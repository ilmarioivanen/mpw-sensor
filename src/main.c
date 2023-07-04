/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>


char bt_name[] = "test";
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(bt_name) - 1)


/*
 * Set Advertisement data. Based on the Eddystone specification:
 * https://github.com/google/eddystone/blob/master/protocol-specification.md
 * https://github.com/google/eddystone/tree/master/eddystone-url
 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
		      0xaa, 0xfe, /* Eddystone UUID */
		      0x10, /* Eddystone-URL frame type */
		      0x00, /* Calibrated Tx power at 0m */
		      0x01, /* URL Scheme Prefix https://www. */
		      't', 'i', 'n', 'y', 'u', 'r', 'l',
		      0x00, /* .com/ */
			  '3', 'h', 'm', '4', 'v', 'c', 'k', 'x')
};
//https://www.youtube.com/watch?v=y6120QOlsfU -> https://tinyurl.com/3hm4vckx


/* Set Scan Response data */
static const struct bt_data sd[] = {
	
	BT_DATA(BT_DATA_NAME_COMPLETE, bt_name, DEVICE_NAME_LEN),
};

/* Set new Scan Response data */
static const struct bt_data sd2[] = {

	BT_DATA(BT_DATA_NAME_COMPLETE, "test2", DEVICE_NAME_LEN),
};

static void bt_ready(int err)
{
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");


	/* Start advertising */

	/* Make a local copy of the BLE advertising parameters */
	struct bt_le_adv_param adv_params = *(BT_LE_ADV_NCONN_IDENTITY); 
	adv_params.interval_min = 1000;
	adv_params.interval_max = 1000;
	
	err = bt_le_adv_start(&adv_params, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}


	/* For connectable advertising you would use
	 * bt_le_oob_get_local().  For non-connectable non-identity
	 * advertising an non-resolvable private address is used;
	 * there is no API to retrieve that.
	 */

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

	printk("Beacon started, advertising as %s\n", addr_s);

	
	bt_le_adv_update_data(ad, ARRAY_SIZE(ad),
			      sd2, ARRAY_SIZE(sd2));

	
}

int main(void)
{
	int err;

	printk("Starting Beacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
	return 0;
}
