#include <Arduino.h>
// Import libraries (BLEPeripheral depends on SPI)SPI.h

#define ARDUINO_GENERIC // overwritten
#include <SPI.h>

// Set pins for DigiPot. To see why we selected these numbers, see GPIO Table for Bit/Port number and Arduino Pin number mapping
#define CS_PIN 22 // chip select pin for DigiPot
#define MOSI_PIN 23 // data pin for DigiPot
#define CLK_PIN 9 // clock pin for DigiPot -- also, note that on nRF52DK, this corresponds to LED4, so don't be surprised when it's on
#define DUMMY_PIN 3 // not using MISO on DigiPot

#include <BLEPeripheral.h>


// Set variable that holds value for DigiPot

// Set desired current level, in uA

// Convert this current level to the needed Wiper voltage (W*), in V, which depends on the value of Rsense
float currentAmp = 50; // set amplitude of current in uA. So far using integers only, but this could be changed to non-integers
float Rsense = 10000; // using resistors with a p/m 1%, so really, [9100, 10100] ohms -- opportunity to refine
float voltageW = currentAmp*Rsense/1000000; // in V

// Convert this Wiper voltage to the potentiometer value (D, which goes from [0, 255])
uint8_t pot_val = round((float)voltageW*256/1.2);
//uint8_t pot_val = 0; // Dale test -- loop through values for DigiPot 301.9 602.5 1.201
uint8_t received_pot_value = 0;

// Set time interval (in ms) you would like in between ADC sample measurements
int time_to_measure = 1000;

// Pin to read analog voltage
//int ADC_PIN[4] = {A2, A3, A4, A5}; // These are corresponding to pins P0.28, P0.29, P0.30, P0.31 respectively
int ADC_PIN[4] = {16, 21, 15, 14}; // These are corresponding to pins P0.28, P0.29, P0.30, P0.31 respectively

// Converting ADC to mV and define variable to hold voltage value
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_RESULT) ((ADC_RESULT * 3000/1023)) //DOUBLE CHECK THIS!!! From Segger: ((ADC_RESULT * 6*600/4095)) 

int VOLT_VALUE[4];

// Define integer to display "Waiting for connection" more sparsely
int count = 0;

// Define integer to hold ADC value from pin
int ADC_VALUE[4];

// Custom boards may override default pin definitions with BLEPeripheral(PIN_REQ, PIN_RDY, PIN_RST)
BLEPeripheral blePeripheral = BLEPeripheral();

// Create service
BLEService ADCService = BLEService("A3E0539E-3CF8-867E-2B7B-1EE451EC384B");

// Create characteristics
BLECharacteristic writeCharacteristic = BLECharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLERead | BLENotify | BLEBroadcast,20);
BLECharCharacteristic readCharacteristic = BLECharCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLEWriteWithoutResponse | BLEWrite);

extern const uint32_t g_ADigitalPinMap[]; // This function allows us to print Bit/Arduino pin number mapping

void setup() {

  // Initialize DigiPot
  SPI.setPins(DUMMY_PIN,CLK_PIN,MOSI_PIN); // This needs to happen BEFORE SPI.begin()!!!
  SPI.begin();
  pinMode(CS_PIN,OUTPUT);
  // Program DigiPot to specified value
  digitalWrite(CS_PIN,HIGH); // Enable DigiPot programming
  SPI.transfer(pot_val);
  digitalWrite(CS_PIN,LOW); // Disable DigiPot programming


  Serial.begin(9600);
  delay(1000); // To allow serial monitor to boot up

  // Set LED pin to output mode
  //pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  //pinMode(PIN_LED3, OUTPUT);
  //pinMode(PIN_LED4, OUTPUT);

  // Set ADC pin to input mode -- not really necessary since default is already INPUT
  for (int i = 0; i < 4; i++){
        pinMode(ADC_PIN[i], INPUT);
      }

  // Set advertised local name and service UUID
  blePeripheral.setLocalName("Measure Celia's ADC");
  blePeripheral.setAdvertisedServiceUuid(ADCService.uuid());

  // Add service and characteristic
  blePeripheral.addAttribute(ADCService);
  blePeripheral.addAttribute(writeCharacteristic);
  blePeripheral.addAttribute(readCharacteristic);

  // Begin initialization
  blePeripheral.begin();
  Serial.println(F("BLE ADC Peripheral"));
  Serial.println("Arduino : Bit#"); // print mapping of Arduino to Bit 
  char OutString[200];

  for(int i=0; i < 32; ++i) {
    sprintf(OutString, "%2d : %2d", i, (int)g_ADigitalPinMap[i]);
    Serial.println(OutString);
  }
}


void loop() {
  BLECentral central = blePeripheral.central();
  
  if (central) {

    // When the central gets originally connected to peripheral
    Serial.print(F("Connected to central! "));

    Serial.println("Wiper voltage calc = ");
    Serial.print(currentAmp*Rsense/1000000);
    Serial.println("Wiper voltage = ");
    Serial.print(voltageW);
    Serial.println("Pot Val = ");
    Serial.print(pot_val);

    // Print out full UUID and MAC address.
    Serial.println("Peripheral advertising info: ");
    Serial.print("Name: ");
    Serial.println("Celia's ADC");
    Serial.print("Service UUID: ");
    Serial.println(ADCService.uuid());
    Serial.print("rxCharacteristic UUID: ");
    Serial.println(readCharacteristic.uuid());
    Serial.print("txCharacteristics UUID: ");
    Serial.println(writeCharacteristic.uuid());

    while (central.connected()) { // Central still connected to peripheral

      digitalWrite(CS_PIN,HIGH); // Enable DigiPot programming
      //SPI.transfer(pot_val++);
      SPI.transfer(pot_val);
      digitalWrite(CS_PIN,LOW); // Disable DigiPot programming

      // Every [time interval specified above] [time units], sample ADC and convert to voltage
      delay(time_to_measure);
      for (int i = 0; i < 4; i++){
        ADC_VALUE[i] = analogRead(ADC_PIN[i]); // Sample ADC from pin
        VOLT_VALUE[i] = ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE[i]); // Convert from ADC to mV
      }

      // Also save the values into a string that will be converted to a bytearray to send over to my python script
      char sendchar[20];
      sprintf(sendchar, "%04d %04d %04d %04d",VOLT_VALUE[0],VOLT_VALUE[1],VOLT_VALUE[2],VOLT_VALUE[3]);

      // Convert string to byte array
      byte sendbytes[20];
      String sendstring = String(sendchar);
      sendstring.getBytes(sendbytes,20);

      // And send them over to central device
      writeCharacteristic.setValue(sendbytes,20);

      // Then print bytes to serial monitor for debugging
      Serial.write(sendbytes,20);

      // Read anything that might have been sent over from central
      //String new_pot_string = String(readCharacteristic.value); // Still under development, don't really need this at this point

      //idle_state_handle()
    }

    // Exit the while loop when the central becomes disconnected
    Serial.print(F("Disconnected from central!"));

  } else { // While waiting for connection to central, display message and blink LED
    count = count + 1;
    if (count > 50) {
      Serial.println(F("Waiting for connection..."));
      count = 0;
    }

    // Blink LED2, which is P0.18 (SWO castellated pin on BC832)
    digitalWrite(PIN_LED2, HIGH); // This actually turns LED off
    delay(50);
    digitalWrite(PIN_LED2, LOW); // This actually turns LED on
    delay(50);


    /* This is for debugging purposes

    // Debugging DigiPot -- Dale Test
    digitalWrite(CS_PIN,HIGH); // Enable DigiPot programming
    //SPI.transfer(0x34); // See OneNote notes from Th 09/08/22 to see what the scope output should look like for this transfer
    SPI.transfer(pot_val++);
    digitalWrite(CS_PIN,LOW); // Disable DigiPot programming
    
    //Flashing all of the LED's
    digitalWrite(PIN_LED1, HIGH);
    digitalWrite(PIN_LED2, HIGH);
    digitalWrite(PIN_LED3, HIGH);
    digitalWrite(PIN_LED4, HIGH);
    delay(50);
    digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, LOW);
    digitalWrite(PIN_LED3, LOW);
    digitalWrite(PIN_LED4, LOW);
    delay(50);

    */
  }
}