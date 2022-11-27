#include <EEPROM.h>
#include <Bounce2.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#include "Support.h"

#define TOGGLE_DIRECTION_MS 2000
#define TURN_OFF_MS 1000
#define STEP_MS 30

#ifdef ESP8266
#define IO_LED 2
#define IO_PB 3
#else
#define IO_LED 6
#define IO_PB 4

typedef unsigned long time_t;
#endif

bool isOn = false;
bool wasOn = false;
int level = 255;
Bounce bounce(IO_PB, 50);
time_t lastDownChange = 0;
time_t lastLevelChange = 0;
bool isDown = false;
bool goingUp = true;

void setup() {
	while (!Serial);

	pinMode(IO_LED, OUTPUT);

#ifdef ESP8266
	WiFi.mode(WIFI_OFF);
#endif

	EEPROM.get(0, level);

	DEBUG(F("Initializing with level "), level);

	Serial.begin(115200);
}

void loop() {
	bounce.update();

	if (bounce.read()) {
		auto currentMillis = millis();

		if (!isDown) {
			DEBUG(F("Staring down"));

			if (currentMillis - lastDownChange < TOGGLE_DIRECTION_MS) {
				goingUp = !goingUp;

				DEBUG(F("Toggling going up to "), goingUp);
			}
			else {
				goingUp = true;
			}

			isDown = true;
			lastDownChange = currentMillis;
			lastLevelChange = currentMillis;

			wasOn = isOn;

			if (!isOn) {
				DEBUG(F("Turning on"));

				isOn = true;
				analogWrite(IO_LED, level);
			}
		}

		if (currentMillis - lastLevelChange >= STEP_MS) {
			if (goingUp) {
				if (level < 255) {
					DEBUG(F("Stepping to "), level);

					level++;
					analogWrite(IO_LED, level);
				}
			}
			else {
				if (level > 0) {
					DEBUG(F("Stepping to "), level);

					level--;
					analogWrite(IO_LED, level);
				}
			}

			lastLevelChange = currentMillis;
		}
	}
	else {
		if (isDown) {
			DEBUG(F("End down"));

			auto currentMillis = millis();
			if (currentMillis - lastDownChange < TURN_OFF_MS && wasOn) {
				DEBUG(F("Turning off"));

				isOn = false;
				analogWrite(IO_LED, 0);
			}
			else {
				DEBUG(F("Writing current level "), level);

				EEPROM.put(0, level);
			}

			isDown = false;
			lastDownChange = millis();
		}
	}
}
