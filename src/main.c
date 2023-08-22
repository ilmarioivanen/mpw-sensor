#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdlib.h>
#include <math.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static uint8_t mfg_data[] = {"  Event: 1"};

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 10),
};

/* Set Scan Response data */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

typedef struct {
    int eventNumber;
    int timestamp;
    float x;
    float y;
    float z;
} SensorEvent;

int mode = 1;
static SensorEvent previousEvent;
float threshold_x = 1;
float threshold_y = 1;
float threshold_z = 1;
int numEvents = 0;
bool isFirstEvent = true;


static void fetch_and_display(const struct device *sensor)
{   
    if (mode == 1) {
        printf("HABANERO \n");
        threshold_x = 0.5;
        threshold_y = 0.5;
        threshold_z = 0.5;
    }
    struct sensor_value accel[3];
    const char *overrun = "";
    int rc = sensor_sample_fetch(sensor);

    if (rc == -EBADMSG) {
        /* Sample overrun.  Ignore in polled mode. */
        if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
            overrun = "[OVERRUN] ";
        }
        rc = 0;
    }
    if (rc == 0) {
        rc = sensor_channel_get(sensor,
                                SENSOR_CHAN_ACCEL_XYZ,
                                accel);
    }
    if (rc < 0) {
        printf("ERROR: Update failed: %d\n", rc);
    } else {
        SensorEvent currentEvent;
        currentEvent.eventNumber = numEvents + 1;
        currentEvent.timestamp = k_uptime_get_32();
        currentEvent.x = sensor_value_to_double(&accel[0]);
        currentEvent.y = sensor_value_to_double(&accel[1]);
        currentEvent.z = sensor_value_to_double(&accel[2]);
 
        if (isFirstEvent) {
            previousEvent = currentEvent;
            isFirstEvent = false;
        } else {
            // Check if the current event is different from the previous event
            if (fabs(currentEvent.x - previousEvent.x) > threshold_x ||
                fabs(currentEvent.y - previousEvent.y) > threshold_y ||
                fabs(currentEvent.z - previousEvent.z) > threshold_z) {
                numEvents++;
                previousEvent = currentEvent;
                printf("Event Number: %d\n", numEvents);
                printf("Timestamp: %d ms\n", currentEvent.timestamp);
                printf("x: %f\n", currentEvent.x);
                printf("y: %f\n", currentEvent.y);
                printf("z: %f\n", currentEvent.z);
                printf("\n");
                
                printk("Sending advertising data: %02X\n", mfg_data[9]);
                
                int err;
                err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
                if (err) {
                    printk("Advertising failed to start (err %d)\n", err);
                    return 0;
                    }

                k_msleep(5000);

                err = bt_le_adv_stop();
                if (err) {
                    printk("Advertising failed to stop (err %d)\n", err);
                    return 0;
                    }
                else {
                    printk("Stopping advertising data: %02X\n", mfg_data[9]);
                }
                }
            }
        }
    }

#ifdef CONFIG_LIS2DH_TRIGGER
static void trigger_handler(const struct device *dev,
                            const struct sensor_trigger *trig)
{
    fetch_and_display(dev);
}
#endif

int main(void)
{   
    const struct device *const sensor = DEVICE_DT_GET_ANY(st_lis2dh);

    if (sensor == NULL) {
        printf("No device found\n");
        return 0;
    }
    if (!device_is_ready(sensor)) {
        printf("Device %s is not ready\n", sensor->name);
        return 0;
    }

    // Initialize Bluetooth
    int err;
    printk("Starting Broadcaster\n");
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
        }
    printk("Bluetooth initialized\n");


    //k_msleep(1000);

#if CONFIG_LIS2DH_TRIGGER
    {
        struct sensor_trigger trig;
        int rc;

        trig.type = SENSOR_TRIG_DATA_READY;
        trig.chan = SENSOR_CHAN_ACCEL_XYZ;

        if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME)) {
            struct sensor_value odr = {
                .val1 = 1,
            };

            rc = sensor_attr_set(sensor, trig.chan,
                                SENSOR_ATTR_SAMPLING_FREQUENCY,
                                &odr);
            if (rc != 0) {
                printf("Failed to set odr: %d\n", rc);
                return 0;
            }
            printf("Sampling at %u Hz\n", odr.val1);
        }

        rc = sensor_trigger_set(sensor, &trig, trigger_handler);
        if (rc != 0) {
            printf("Failed to set trigger: %d\n", rc);
            return 0;
        }

        printf("Waiting for triggers\n");
        while (true) {
            k_sleep(K_MSEC(2000));
        }
    }
    
#else //CONFIG_LIS2DH_TRIGGER
    printf("Polling at 0.5 Hz\n");
    while (true) {
        fetch_and_display(sensor);
        k_sleep(K_MSEC(1000));
    }

#endif /* CONFIG_LIS2DH_TRIGGER */
}