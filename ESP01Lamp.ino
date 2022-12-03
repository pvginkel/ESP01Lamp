#include <EEPROM.h>
#include <Bounce2.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#include "Support.h"

#define TOGGLE_DIRECTION_MS 2000
#define DIMMING_DELAY_MS 600

// Invert the level the led is set to.
#define INVERT false
#define SCALE false

#if SCALE
// Increase resolution because of scaling.
#define LEVEL_MAX (255 * 3)
#define STEP_MS 5
#else
#define LEVEL_MAX 255
#define STEP_MS 15
#endif

#define FADE_STEP_MS 3

#ifdef ESP8266
#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3

#define TX GPIO1
#define RX GPIO3

#define IO_LED GPIO0
#define IO_PB RX

#else
#define IO_LED 6
#define IO_PB 4

typedef unsigned long time_t;
#endif

enum class Dimming {
	None,
	GoingUp,
	GoingDown
};

bool isOn = false;
bool wasOn = false;
bool maxSignaled = false;
int level = LEVEL_MAX;
Bounce bounce(IO_PB, 50);
time_t lastDownChange = 0;
time_t lastLevelChange = 0;
bool isDown = false;
bool isFading = false;
time_t fadeLastMillis = 0;
int fadeTarget = 0;
int fadeLevel;
auto dimming = Dimming::None;

static void setLevel(int value, bool fade = false);

void setup() {
	while (!Serial);

	pinMode(IO_LED, OUTPUT);

#ifdef ESP8266
	WiFi.mode(WIFI_OFF);

	// Disable RX so that we can use it for the button.
	Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

	// Turn off the blue led.
	pinMode(GPIO2, OUTPUT);
	// GPIO2 has a pull up resistor to Vcc, so we need to pull it
	// high to turn off the led
	digitalWrite(GPIO2, HIGH);
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
	if (isFading) {
		auto currentMillis = millis();
		if (currentMillis - fadeLastMillis >= FADE_STEP_MS) {
			if (fadeTarget == fadeLevel) {
				DEBUG(F("Ended fading"));
				isFading = false;
			}
			else {
				fadeLastMillis = currentMillis;

				if (fadeTarget < fadeLevel) {
					fadeLevel--;
				}
				else {
					fadeLevel++;
				}
			}

			setLevel(fadeLevel);
		}

		// Disable the rest of the algorithm while fading.
		return;
	}

	bounce.update();

	if (bounce.read()) {
		auto currentMillis = millis();

		if (!isDown) {
			DEBUG(F("Staring down"));

			if (currentMillis - lastDownChange < TOGGLE_DIRECTION_MS) {
				// Initializes Dimming::None to Dimming::GoingUp also.
				dimming = dimming == Dimming::GoingUp
					? Dimming::GoingDown
					: Dimming::GoingUp;

				DEBUG(F("Toggling dimming direction to "), dimming == Dimming::GoingUp ? F("GoingUp") : F("GoingDown"));
			}
			else {
				dimming = Dimming::GoingUp;
			}

			isDown = true;
			lastDownChange = currentMillis;
			lastLevelChange = currentMillis;
			wasOn = isOn;

			if (!isOn) {
				DEBUG(F("Turning on"));

				if (level == 0) {
					level = 5;
				}
				setLevel(level, true);

				isOn = true;
				dimming = Dimming::None;
			}
		}

		if (
			currentMillis - lastDownChange >= DIMMING_DELAY_MS &&
			currentMillis - lastLevelChange >= STEP_MS
		) {
			if (dimming == Dimming::GoingUp) {
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
			else if (dimming == Dimming::GoingDown) {
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
			if (currentMillis - lastDownChange < DIMMING_DELAY_MS && wasOn) {
				DEBUG(F("Turning off"));

				setLevel(0, true);

				isOn = false;
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

void setLevel(int value, bool fade) {
	if (fade) {
		isFading = true;
		fadeLastMillis = 0; // Trigger the algorithm on the first loop.
		fadeTarget = value;
		fadeLevel = isOn ? level : 0;
#if INVERT
		fadeLevel = 255 - fadeLevel;
		fadeTarget = 255 - fadeTarget;
#endif

		DEBUG(F("Start fading from "), fadeLevel, F(" to "), fadeTarget);

		// Let the fade algorithm manage the rest.
		return;
	}

#if SCALE
	float fraction = value / ((float)(LEVEL_MAX));

	// Use a parabolic function to make the level more natural.
	fraction = fraction * fraction;

	int actualactualValueLevel = (int)(fraction * 255);
	if (actualValue == 0 && value > 0) {
		actualValue = 1;
	}
#else
	int actualValue = value;
#endif

#if INVERT
	actualValue = 255 - actualValue;
#endif
	 
	analogWrite(IO_LED, actualValue);
}
