/*
* Blink
* Turns on an LED on for one second,
* then off for one second, repeatedly.
*/

#include <Arduino.h>
#include <Zigbee.h>

/* Zigbee light bulb configuration */
#define ZIGBEE_LIGHT_ENDPOINT 10
uint8_t led = LED_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeLight zbLight = ZigbeeLight(ZIGBEE_LIGHT_ENDPOINT);

/********************* RGB LED functions **************************/
void setLED(bool value) {
    // On XIAO ESP32-C6, LED is active LOW (LOW = ON, HIGH = OFF)
    digitalWrite(led, value ? LOW : HIGH);
}

/********************* Arduino functions **************************/
void setup() {
    Serial.begin(115200);
    
    // ESP32-C6 needs a delay for USB CDC to initialize
    delay(1000);

    // Init LED and turn it OFF (if LED_PIN == RGB_BUILTIN, the rgbLedWrite() will be used under the hood)
    pinMode(led, OUTPUT);
    digitalWrite(led, HIGH);  // HIGH = LED OFF on XIAO ESP32-C6

    // Init button for factory reset
    pinMode(button, INPUT_PULLUP);

    //Optional: set Zigbee device name and model
    zbLight.setManufacturerAndModel("Espressif", "ZBLightBulb");

    // Set callback function for light change
    zbLight.onLightChange(setLED);

    //Add endpoint to Zigbee Core
    Zigbee.addEndpoint(&zbLight);

    // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
    if (!Zigbee.begin()) {
        Serial.println("Zigbee failed to start!");
        Serial.println("Rebooting...");
        ESP.restart();
    }
    Serial.print("Connecting to network.");
    while (!Zigbee.connected()) {
        Serial.print(".");
        delay(100);
    }
    Serial.println("connected!");
}

void loop() {
    // Checking button for factory reset
    if (digitalRead(button) == LOW) {  // Push button pressed
        Serial.println("Button pressed!");
        // Key debounce handling
        delay(100);
        int startTime = millis();
        while (digitalRead(button) == LOW) {
            delay(50);
            if ((millis() - startTime) > 3000) {
                // If key pressed for more than 3secs, factory reset Zigbee and reboot
                Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
                delay(1000);
                Zigbee.factoryReset();
            }
        }
        // Toggle light by pressing the button
        zbLight.setLight(!zbLight.getLightState());
        Serial.println("Light toggled!");
    }
    delay(100);
}