#include <application.h>

#define RADIO_DELAY 10000
#define START_DELAY 600


#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)


// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

// Accelerometer
twr_lis2dh12_t acc;
twr_lis2dh12_result_g_t result;

float magnitude;

twr_tick_t startSeconds = 0;
twr_tick_t endSeconds = 0;

bool playing = false;

void lis2_event_handler(twr_lis2dh12_t *self, twr_lis2dh12_event_t event, void *event_param)
{
    float holdTime;

    if (event == TWR_LIS2DH12_EVENT_UPDATE && playing)
    {
        static twr_tick_t radio_delay = 0;

        twr_lis2dh12_get_result_g(&acc, &result);

        magnitude = pow(result.x_axis, 2) + pow(result.y_axis, 2) + pow(result.z_axis, 2);
        magnitude = sqrt(magnitude);


        if(magnitude > 1.19 || magnitude < 0.95)
        {
            endSeconds = twr_tick_get();
            playing = false;

            twr_lis2dh12_set_update_interval(&acc, TWR_TICK_INFINITY);
            twr_led_set_mode(&led, TWR_LED_MODE_OFF);

            if (twr_tick_get() >= radio_delay)
            {
                // Make longer pulse when transmitting
                twr_led_pulse(&led, 100);

                holdTime = ((endSeconds - startSeconds) / (float)1000);

                twr_radio_pub_float("hold-time", &holdTime);

                radio_delay = twr_tick_get() + RADIO_DELAY;
            }
        }
    }
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    static twr_tick_t start_delay = 0;
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        while(twr_tick_get() >= start_delay)
        {
            start_delay = twr_tick_get() + START_DELAY;
        }

        twr_lis2dh12_set_update_interval(&acc, 40);

        playing = true;
        startSeconds = twr_tick_get();

        twr_led_set_mode(&led, TWR_LED_MODE_ON);
    }

}

// This function dispatches battery events
void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    // Update event?
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage;

        // Read battery voltage
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_log_info("APP: Battery voltage = %.2f", voltage);

            // Publish battery voltage
            twr_radio_pub_battery(&voltage);
        }
    }
}

void application_init(void)
{
    // Initialize logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_pulse(&led, 2000);

    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, 0);
    twr_button_set_event_handler(&button, button_event_handler, NULL);


    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    // Send radio pairing request
    twr_radio_pairing_request("still-position-detector", VERSION);

    twr_lis2dh12_init(&acc, TWR_I2C_I2C0, 0x19);
    twr_lis2dh12_set_event_handler(&acc, lis2_event_handler, NULL);
    twr_lis2dh12_set_update_interval(&acc, TWR_TICK_INFINITY);
}
