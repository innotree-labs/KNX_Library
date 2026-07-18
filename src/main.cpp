/**
 * @name main.cpp
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Showcase sketch for the KNX library. Demonstrates the three ways to use it:
 *
 *            1. Intent objects  — KnxLight / KnxDimmLight hide the DPT behind on()/toggle()/
 *                                 brighter(); they cache the bus state and fire typed callbacks.
 *            2. Value objects    — KnxTemperature publishes and receives a float, no DPT in sight.
 *            3. Stateless send   — knx.send("0/4/2", Dpt9(21.5f)) for a one-off to any address.
 *
 *          Objects are declared at global scope: they self-register into the coordinator's
 *          receiver registry on construction and must outlive it (PLAN §6). Wire two momentary
 *          buttons to the pins below to drive the light; incoming status telegrams flow back
 *          through the onUpdate callbacks.
*/

//---- Libraries ----
#include <Arduino.h>
#include <KNX.h>           // the whole library: driver + coordinator + values + intent objects

//---- Configuration ----
#define BAUDRATE_SERIAL 115200
#define PHYS_ADDR       "1.1.5"     // this device's KNX physical address

#define PIN_BTN_TOGGLE  D8          // momentary button: toggle the kitchen light
#define PIN_BTN_BRIGHT  D9          // momentary button: dim the lamp brighter

//---- KNX node: one object, address typed once; the bus driver is owned internally ----
KNX knx(PHYS_ADDR);

//---- Objects: (node, command GA[, status GA]) — declared once, live forever ----
KnxLight       kitchen(knx, "0/1/1", "0/3/0");        // sends on 0/1/1, listens on 0/3/0
KnxDimmLight   lamp(knx, "0/1/2", "0/0/1");           // switch 0/1/2, relative dim 0/0/1
KnxTemperature roomTemp(knx, "0/4/2");                // publish + read a temperature

//---- Simple rising-edge detection for the two demo buttons ----
static bool lastToggleBtn = HIGH;
static bool lastBrightBtn = HIGH;

void setup() {
	Serial.begin(BAUDRATE_SERIAL);
	pinMode(PIN_BTN_TOGGLE, INPUT_PULLUP);
	pinMode(PIN_BTN_BRIGHT, INPUT_PULLUP);

	knx.begin();

	// RX: react to bus feedback with typed callbacks (no DPT, no manual decode).
	// A non-capturing lambda converts to a plain function pointer — no heap, AVR-safe.
	kitchen.onUpdate([](bool on) {
		Serial.printf("[kitchen] bus says the light is now %s\n", on ? "ON" : "OFF");
		// drive your RGB backlight / OLED here
	});

	roomTemp.onUpdate([](float celsius) {
		Serial.printf("[roomTemp] %.1f C\n", celsius);
	});
}

void loop() {
	// 1. Pump the receive path first: parse incoming telegrams and fire callbacks.
	knx.handleUART();

	// 2. Surface bus faults (voltage failure / thermal warning) reported by the driver.
	if (knx.monitorTPUART()) Serial.println("[knx] bus fault reported");

	// 3. Drive the light from the physical buttons (intent methods over the send path).
	bool toggleBtn = digitalRead(PIN_BTN_TOGGLE);
	if (lastToggleBtn == HIGH && toggleBtn == LOW) {
		// toggle() flips relative to the cached, status-fed state — tracks the real bus,
		// and returns the driver's L_Data.con so you know it was confirmed on the wire.
		bool confirmed = kitchen.toggle();
		Serial.printf("[kitchen] toggle -> %s (%s)\n",
			kitchen.isOn() ? "ON" : "OFF", confirmed ? "confirmed" : "no ACK");
	}
	lastToggleBtn = toggleBtn;

	bool brightBtn = digitalRead(PIN_BTN_BRIGHT);
	if (lastBrightBtn == HIGH && brightBtn == LOW) {
		lamp.on();                                   // ensure it is on, then step up
		lamp.brighter(DimmIncrement::Percent_12_5);  // relative DPT3 dim step
	}
	lastBrightBtn = brightBtn;

	// 3rd tier for reference: a one-off value to any address, no object needed.
	// knx.send("0/4/2", Dpt9(21.5f));
}
