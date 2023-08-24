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
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <inttypes.h>


/* Set up leds */
/* 1000 msec = 1 sec */
#define LED_SLEEP_MS 1000
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
struct gpio_dt_spec leds[3] = {led_red, led_blue, led_green};
int led_index = 0;
int last_led_index = 0;

/* Set up button */
#define BUTTON_SLEEP_MS	1
#define SW0_NODE	DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

/* Set up mag switch */
#define SW1_NODE	DT_ALIAS(sw1)
static const struct gpio_dt_spec mag_switch = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static struct gpio_callback switch_cb_data;

/* Set up Bluetooth */
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME /* Device name is located in prj.conf */
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static uint8_t mfg_data[] = {"  Chair "};

/* Set ad data */
static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 8),
};

/* Set scan response data */
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
int last_mode = 0;
bool door_open = true;
static SensorEvent previousEvent;
float threshold_x = 1;
float threshold_y = 1;
float threshold_z = 1;
int numEvents = 0;
bool isFirstEvent = true;
int check = 1;
int coffee_check = 0;
int twice = 0;

/* Function for gathering and processing sensor data */
static void fetch_and_display(const struct device *sensor)
{   
    /* Check if mode has changed */
    if (last_mode != mode) { 

        last_mode = mode;

        if (mode == 1) {
            /* Chair mode */
            strcpy(mfg_data, "  Chair ");
            threshold_x = 0.5;
            threshold_y = 0.5;
            threshold_z = 0.5;
        }
        else if (mode == 2) {
            /* Door mode */
            strcpy(mfg_data, "  Door  ");
            threshold_x = 5000;
            threshold_y = 5000;
            threshold_z = 5000;
        }
        else if (mode == 3) {
            /* Coffee mode */
             strcpy(mfg_data, "  Coffee");
            threshold_x = 10;
            threshold_y = 10;
            threshold_z = 0.2;
        }
    }

    struct sensor_value accel[3];
    const char *overrun = "";
    int rc = sensor_sample_fetch(sensor);

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
            
            if ((mode == 2 && door_open)                             ||
                fabs(currentEvent.x - previousEvent.x) > threshold_x ||
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
                
                if (mode == 3 && coffee_check < 11) {
                    coffee_check++;
                }
                else if (twice < 1) {
                    twice++;
                }
                else {
                
                twice = 0;
                coffee_check = 0;

                printk("Sending advertising data: %s\n", mfg_data);
                
                /* Send advertisement */
                int err;
                err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
                if (err) {
                    printk("Advertising failed to start (err %d)\n", err);
                    return 0;
                    }

                k_msleep(3000);

                /* Stop advertisement*/
                err = bt_le_adv_stop();
                if (err) {
                    printk("Advertising failed to stop (err %d)\n", err);
                    return 0;
                    }
                else {
                    printk("Stopping advertising data: %s\n", mfg_data);
                }
                /*
                if (check % 2 == 0 && mode == 3) {
                    k_msleep(10000);
                }
                check++;
                */
                }
                }
            }
        }
}

/* Leds init */
int led_init(void)
{
    for (int i = 0; i < 3; i++) {

        int ret;
        const struct gpio_dt_spec led = leds[i];
         
        if (!gpio_is_ready_dt(&led)) {
            return 0;
        }

        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        if (ret < 0) {
            return 0;
        }
        gpio_pin_set_dt(&led, 0);
        gpio_pin_set_dt(&led_red, 1);

    }
    return 0;
}

/* Function for when button is pressed */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{ 
    last_led_index = led_index;
    last_mode = mode;

    mode++;
    led_index++;

    if (led_index >= 3) {
        led_index = 0;
        mode = 1;
    }

    const struct gpio_dt_spec led = leds[led_index];
    const struct gpio_dt_spec last_led = leds[last_led_index];

    gpio_pin_set_dt(&last_led, 0);
	gpio_pin_set_dt(&led, 1);

    k_msleep(BUTTON_SLEEP_MS);
}

/* Button init */
int button_init(void) 
{

    int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	return 0;
}

void magnet_close(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int val = gpio_pin_get_raw(mag_switch.port, mag_switch.pin);

    if (val == 0) {
        door_open = false;
    } 
    else if (val == 1) {
        door_open = true;
    }

}

int magnet_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&mag_switch)) {
        return 0;
    }

    // Configure the GPIO pin as an input
    ret = gpio_pin_configure_dt(&mag_switch, GPIO_INPUT);
    if (ret != 0) {
        return 0;
    }

    ret = gpio_pin_interrupt_configure_dt(&mag_switch, GPIO_INT_EDGE_BOTH);

	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, mag_switch.port->name, mag_switch.pin);
		return 0;
    }

    gpio_init_callback(&switch_cb_data, magnet_close, BIT(mag_switch.pin));
	gpio_add_callback(mag_switch.port, &switch_cb_data);
}


/* Main */

int main(void)
{   

    led_init();

    button_init();

    magnet_init();

    const struct device *const sensor = DEVICE_DT_GET_ANY(st_lis2dh);

    if (sensor == NULL) {
        printf("No device found\n");
        return 0;
    }
    if (!device_is_ready(sensor)) {
        printf("Device %s is not ready\n", sensor->name);
        return 0;
    }

    /* Initialize Bluetooth */
    int err;
    printk("Starting Broadcaster\n");
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
        }
    printk("Bluetooth initialized\n");
    
    printf("Polling at 0.5 Hz\n");
    while (true) {
        fetch_and_display(sensor);
        k_sleep(K_MSEC(1000));
        }
    }