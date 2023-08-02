# https://www.linuxfixes.com/2022/02/solved-cannot-connect-to-arduino-over.html
import asyncio

from aioconsole import ainput
from bleak import BleakClient, discover
import os
from datetime import datetime

# Define path and name of output file
#root_path = os.environ["HOME"]
#output_file = r"C:\Users\Celia\Desktop\ble data.csv"
current_time = datetime.now()
time_stamp = current_time.timestamp()
date_time = datetime.fromtimestamp(time_stamp)
str_date_time = date_time.strftime("%Y%m%d")
#path1 = r"C:\\Users\\Celia\\Desktop\\"
path1 = r"C:\\Users\\Celia\\OneDrive - Johns Hopkins\\00. Chinchilla Current Source\\Data\\"
path2 = str_date_time
output_file = path1 + path2 + "Data.csv"



async def data_client(device):

    #column_names = ["time", "delay", "Ch0", "Ch1", "Ch2", "Ch3"]
    
    def handle_rx(_: int, data: bytearray):
        print("received:", data)
        column_names = ["Date","Time","Ch0", "Ch1", "Ch2", "Ch3"]
        f=open(output_file, "a+")
        if os.stat(output_file).st_size == 0:
            print("Created file.")
            f.write(",".join([str(name) for name in column_names]) + ",\n")
        else:
            #print(data,"\n")
            datastr = bytearray.decode(data)
            current_time = datetime.now()
            time_stamp = current_time.timestamp()
            date_time = datetime.fromtimestamp(time_stamp)
            str_date_time = date_time.strftime("%d-%m-%Y, %H:%M:%S")
            f.write(f"{str_date_time},{datastr[0:4].strip()},{datastr[5:9]},{datastr[10:14].strip()},{datastr[15:19].strip()},\n")
        
    async with BleakClient(device,timeout=30) as client:
        #print('\nCheckpoint 2 COMPLETE')
        await client.start_notify(read_characteristic, handle_rx)
        #print('\nCheckpoint 3 COMPLETE')
        while client.is_connected:
            await asyncio.sleep(1)
            input_str = await ainput("Enter command: ")
            bytes_to_send = input_str.encode()
            if input_str == 'e':
                await client.stop_notify(read_characteristic)
                await client.disconnect()
            else:
                await client.write_gatt_char(write_characteristic, bytes_to_send)


async def select_device():
    print("Scanning for Bluetooh LE hardware...")
    await asyncio.sleep(2.0)  # Wait for BLE to initialize.
    devices = await discover()

    print("Please select device: ")
    for i, device in enumerate(devices):
        print(f"{i}: {device.name}")
    print("99: Exit program")
    print("-1: Re-scan for BLE devices")
    response = await ainput("Select device: ")
    try:
        response = int(response.strip())
    except ValueError:
        print("Please make valid selection.")
        response = -1
    print('response', type(response), response)
    if -1 < response < len(devices):
        return devices[response].address
    elif response == 99:
        return 99
    print("Please make valid selection.")


async def main():
    # device = "A3E0539E-3CF8-867E-2B7B-1EE451EC384B" ## UUID for nRF52DK on-board chip
    #device = "F182A17D-0E1C-61E7-27B9-B75D127C16BD" #F7271A17-39B3-88FB-F069-52806225AA94" ## UUID's for 2 different EV on-board chip
    #device = "0279880A-5E98-E7A3-23AE-E1BC4CCA98C2" ## UUID for BLE V2 PCB chip!
    # device = "DE:D3:4D:CA:60:FD" # in Windows, for my custom BC832
    #device = "F1:A3:5B:56:23:79"; # in Windows, for the nRF52DK on-board module
    device = None
    keep_alive = True
    while keep_alive:
        print('Device:', device)
        if device is None:
            device = await select_device()
            if device == 99:
                keep_alive = False
        elif device:
            #print('\nCheckpoint 1 COMPLETE')
            await data_client(device)
            device = None
            print('Device disconnected.\n')


# For nRF52DK on-board chip:
write_characteristic = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" #ARDUINO
read_characteristic = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" #ARDUINO
#write_characteristic = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" # SEGGER
#read_characteristic = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" # SEGGER

# For BC832 chip:
#write_characteristic = ""
#read_characteristic = ""

if __name__ == "__main__":
    asyncio.run(main())
