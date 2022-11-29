#include <EEPROM.h>
#include <Bounce2.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#include "Support.h"

#define TOGGLE_DIRECTION_MS 2000
#define TURN_OFF_MS 1000

#define LEVEL_MAX (255 * 3)
#define STEP_MS 5

#define IS_MOSFET true

#ifdef ESP8266
#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3

#define TX GPIO1
#define RX GPIO3

#define IO_LED GPIO0
#define IO_PB RX

#define INVERT (!IS_MOSFET)
#else
#define IO_LED 6
#define IO_PB 4

#define INVERT false

typedef unsigned long time_t;
#endif

bool isOn = false;
bool wasOn = false;
bool maxSignaled = false;
int level = LEVEL_MAX;
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

	// Disable RX so that we can use it for the button.
	Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else
	Serial.begin(115200);
#endif

	EEPROM.begin(2);
	EEPROM.get(0, level);
	if (level < 0 || level > LEVEL_MAX) {
		level = LEVEL_MAX;
	}

	setLevel(0);

	DEBUG(F("Initializing with level "), level);
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
				if (level == 0) {
					level = 5;
				}
				setLevel(level);
			}
		}

		if (
			currentMillis - lastDownChange >= TURN_OFF_MS &&
			currentMillis - lastLevelChange >= STEP_MS
		) {
			if (goingUp) {
				if (level < LEVEL_MAX) {
					DEBUG(F("Stepping to "), level);

					level++;
					setLevel(level);
				}
				else if (!maxSignaled) {
					maxSignaled = true;

					setLevel(0);
					delay(200);
					setLevel(level);
				}
			}
			else {
				if (level > 1) {
					DEBUG(F("Stepping to "), level);

					level--;
					setLevel(level);
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
				setLevel(0);
			}
			else {
				DEBUG(F("Writing current level "), level);

				int oldLevel;
				EEPROM.get(0, oldLevel);
				if (level != oldLevel) {
					EEPROM.put(0, level);
					EEPROM.commit();
				}
			}

			isDown = false;
			maxSignaled = false;
			lastDownChange = millis();
		}
	}
}

void setLevel(int level) {
	float fraction = level / ((float)(LEVEL_MAX));

	// Use a parabolic function to make the level more natural.
	fraction = fraction * fraction;

	int actualLevel = (int)(fraction * 255);
	if (actualLevel == 0 && level > 0) {
		actualLevel = 1;
	}

#if INVERT
	actualLevel = 255 - actualLevel;
#endif
	 
	analogWrite(IO_LED, actualLevel);
}
