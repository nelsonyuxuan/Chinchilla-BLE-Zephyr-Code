/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/services/ias.h>

/* Including additional packages from ADC codes */
#include <inttypes.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/util.h>

/* ADC Initialization Codes */
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};
/* END of ADC Initialization Codes */

/* Change the given UUID to the provided board */
/* Custom Service Variables */
/* From Celia's code, converts the string format to the BT_UUID_128_ENCODE() format */

/* Previously successful version of the characteristic value */
// #define BT_UUID_CUSTOM_SERVICE_VAL \
// 	BT_UUID_128_ENCODE(0xA3E0539E, 0x3CF8, 0x867E, 0x2B7B, 0x1EE451EC384B)

/* Based on the print out of Celia's code */
/* No much difference, will need to modify later */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0xDE495ED4, 0xC63C, 0xE850, 0x4B9B, 0xAF8F1053D8D6)

static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	BT_UUID_CUSTOM_SERVICE_VAL);

/* Use the read characteristic as encrypted characteristic */
static struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x6E400002, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E));

/* Use the write characteristic as authenticated characteristic */
static struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E));

/* Just a reverse version for the purpose of debugging */
// static struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
// 	BT_UUID_128_ENCODE(0x6E400002, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E));

// static struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
// 	BT_UUID_128_ENCODE(0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E))

#define VND_MAX_LEN 20

/* Is the problem static? */
// static uint8_t vnd_value[VND_MAX_LEN + 1] = { 'V', 'e', 'n', 'd', 'o', 'r'};
static uint8_t vnd_value[VND_MAX_LEN + 1] = {"0000 0000 0000 0001"};
static uint8_t vnd_auth_value[VND_MAX_LEN + 1] = {"0000 0000 0000 0002"};

/* The handler of the reading, the buffer contains the data to write, and len contains the length of the data */
static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

/* The handler of the writing, the buffer contains the data to write, and len contains the length of the data */
/* The offset helps handle and align the written data properly within the value */
static ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > VND_MAX_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);
	value[offset + len] = 0;

	return len;
}

static uint8_t simulate_vnd;


/* Important for paring. Withouth this, struct bleak client cannot connect to the board */
static void vnd_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	simulate_vnd = (value == BT_GATT_CCC_INDICATE) ? 1 : 0;
}

/* Vendor Primary Service Declaration */
BT_GATT_SERVICE_DEFINE(vnd_svc,
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),
	/* Specify the functionality of the read characteristic */
	/* The last three parameters speicfy the corresponding callback functions for: read, write, and a pointer to the data that the characteristic represents. */
	/* Therefore, for reading characteristic, just use read_vnd, and write just use the write_vnd */
	BT_GATT_CHARACTERISTIC(&vnd_enc_uuid.uuid,
			       BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE |
				   BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_WRITE |
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, write_vnd, vnd_value),

	BT_GATT_CCC(vnd_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),

	/* Specify the funcionality of the write characteristic */
	/* Do the same as above */
	/* Use the same test message of vnd_value defined as above */
	BT_GATT_CHARACTERISTIC(&vnd_auth_uuid.uuid,
			       BT_GATT_CHRC_READ | 
				   BT_GATT_CHRC_INDICATE |
				   BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_AUTHEN |
			       BT_GATT_PERM_READ,
			       read_vnd, NULL, vnd_value)
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_CTS_VAL)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL),
};

/* Updated when connected to the bluetooth */
void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	printk("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
}

/* Registers a set of callback handlers for different GATT events */
static struct bt_gatt_cb gatt_callbacks = {
	/* Called when the ATT MTU is updated during connection */
	.att_mtu_updated = mtu_updated

};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}


BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;


	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* Starts connectable BLE advertising using the given advertising data buffer. The advertising data would typically contain something like the device name */
	/* In this general example, no scan response data is provided */
	/* Allow for further customization: 
		- Choosing advertising type 
		- Setting advertising and scan response data 
		- Configuring advertising intervals/duration
		- Restarting advertising as needed 
		- Registering an advertising stop callback */
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	/* Gets the address of the remote device in the connection as a string */
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

/* Registers callback handlers for authentication events like pairing */
static struct bt_conn_auth_cb auth_cb_display = {
	/* Show passkey during pairing */
	.passkey_display = auth_passkey_display,

	/* Enter passkey received from peer */
	.passkey_entry = NULL,

	/* Canel ongoing pairing process */
	.cancel = auth_cancel,
};

int main(void)
{
	struct bt_gatt_attr *vnd_ind_attr;
	char str[BT_UUID_STR_LEN];
	int err;

	uint32_t count = 0;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!device_is_ready(adc_channels[i].dev)) {
			printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			printk("Could not setup channel #%d (%d)\n", i, err);
			return 0;
		}
	}

	/* Initializes the buetooth stack */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth init successful (err %d)\n", err);

	/* Makes the app BLE-ready once Bluetooth is enabled */
	bt_ready();

	/* Registers a set of callback functions for GATT events */
	bt_gatt_cb_register(&gatt_callbacks);

	/* Registers callbacks for authentication events like pairing */
	// bt_conn_auth_cb_register(&auth_cb_display);

	/* Find the attribute for the vnd_enc service's characteristic UUID */
	/* In this case, use the attribute of the specific characteristic that was updated, not the service's general UUID */
	/* Use the write characteristic here for testing */
	vnd_ind_attr = bt_gatt_find_by_uuid(vnd_svc.attrs, vnd_svc.attr_count,
					    &vnd_enc_uuid.uuid);

	/* Converts the UUID to a string for printing */
	bt_uuid_to_str(&vnd_enc_uuid.uuid, str, sizeof(str));
	printk("Indicate VND attr %p (UUID %s)\n", vnd_ind_attr, str);

	/* Find the attribute of reading from the corresponding UUID */
	// read_attr = bt_gatt_find_by_uuid(vnd_svc.attrs, vnd_svc.attr_count,
	// 				    &vnd_auth_uuid.uuid);

	int32_t adc_final_reading[ARRAY_SIZE(adc_channels)];

	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */

	// NRF_UART0->ENABLE = 0;

	while (1) {
		k_sleep(K_SECONDS(1));


		for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {

			int32_t val_mv;


			(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

			err = adc_read(adc_channels[i].dev, &sequence);


			/*
			 * If using differential mode, the 16 bit value
			 * in the ADC sample buffer should be a signed 2's
			 * complement value.
			 */
			// if (adc_channels[i].channel_cfg.differential) {
			// 	val_mv = (int32_t)((int16_t)buf);
			// } else {
			// 	val_mv = (int32_t)buf;
			// }

			val_mv = (int32_t)buf;
	
			err = adc_raw_to_millivolts_dt(&adc_channels[i],
						       &val_mv);
			/* conversion to mV may not be supported, skip if not */
			// if (err < 0) {
			// 	printk(" (value in mV not available)\n");
			// } else {
			// 	printk(" = %"PRId32" mV\n", val_mv);
			// }

			/* Store the ADC result in each of the channel */
			adc_final_reading[i] = val_mv;
			
			/* The printing statement works */
			// printk("%04d", adc_final_reading[i]);
			
			/* Simply transmit the data during ADC reading? */
			/* Gives an error */
			// sprintf(vnd_value, val_mv);
			
		}

		/* Test printing the entire array */
		// printk("%04d %04d", adc_final_reading[0], adc_final_reading[1]);
		// printk("%"PRId32"", adc_final_reading[0]);

		/* Test for loop just for testing the dynamical assginment of the numbers */
		// for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		// 	adc_final_reading[i] = i;
		// }

		/* Update the value of the characteristic */
		// strcpy(vnd_value, "Values have been updated");
		sprintf(vnd_value, "%04d %04d %04d %04d", adc_final_reading[0], adc_final_reading[1], adc_final_reading[2], adc_final_reading[3]);
		// sprintf(vnd_value, "%04d %04d %04d %04d", val1, val2, val3, val4);
		// sprintf(vnd_value, "%"PRId32" %"PRId32" %"PRId32" %"PRId32"", adc_final_reading[0], adc_final_reading[1], adc_final_reading[2], adc_final_reading[3]);
		
		/* Notify connected devices of the change */
		bt_gatt_notify(NULL, &vnd_ind_attr->uuid, &vnd_value, strlen(vnd_value));

	}
	return 0;
}
