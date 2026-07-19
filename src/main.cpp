/**
 * @name main.cpp
 * @date 19.07.2026
 * @authors Florian Wiesner
 * @details HARDWARE BENCH TEST for the KNX library — no buttons, no user input.
 *
 *          A single dimmable light is toggled every 5 s, and every status telegram the bus
 *          sends back is printed over Serial. Between them these exercise the two paths that
 *          host tests cannot reach: the driver's real transmit path (does the ATTiny accept the
 *          frame and return a positive L_Data.con?) and the receive path (does a telegram from
 *          the actuator get reassembled, parsed, matched and decoded back into a bool?).
 *
 *          Watch the serial monitor at 115200 baud. Expected per cycle: one "toggle ->" line
 *          with "confirmed", then one "[bus]" line once the actuator reports its new state. See
 *          the failure guide at the bottom of this file for how to read a run that misbehaves.
*/

//---- Libraries ----
#include <Arduino.h>
#include <Konnextor.h>     // the whole library: driver + coordinator + values + intent objects

//---- Configuration ----
#define BAUDRATE_SERIAL     115200
#define PHYS_ADDR           "1.1.5"   // this device's KNX physical address
#define TOGGLE_INTERVAL_MS  5000      // how often to flip the light

//---- KNX node: one object, address typed once; the bus driver is owned internally ----
Konnextor knx(PHYS_ADDR);

//---- The light under test: (node, switching GA, dimming GA, status GA) ----
// Sends on/off to 1/1/1, would send relative dim steps to 0/2/1, listens for status on 0/1/1.
KnxDimmLight lamp(knx, "1/1/1", "0/2/1", "0/1/1");

//---- Non-blocking 5 s cadence (rollover-safe: unsigned subtraction) ----
static uint32_t ts_lastToggle = 0;

//---- Callback handler: declared here, defined below loop() ----
void onLampChanged(bool on);

void setup() {
	Serial.begin(BAUDRATE_SERIAL);
	while (!Serial && millis() < 3000) { }   // give USB CDC a moment, but never hang the board

	Serial.println("\n[boot] KNX bench test — toggling 1/1/1 every 5 s");

	// begin() brings up the UART and hands the physical address to the transceiver.
	// If this fails the ATTiny is not answering — nothing below will work, so say so loudly.
	if (knx.begin()) {
		Serial.println("[boot] driver up, bus link ready");
	} else {
		Serial.println("[boot] *** driver FAILED to initialise — check wiring/ATTiny ***");
	}

	lamp.onUpdate(onLampChanged);
}

void loop() {
	// Service the KNX stack every iteration: parse incoming telegrams and fire callbacks.
	// This is why the cadence below uses millis() and not delay() — a delay would stall the
	// receive path and the status callback would arrive late or be dropped.
	knx.loop();

	if (millis() - ts_lastToggle >= TOGGLE_INTERVAL_MS) {
		ts_lastToggle = millis();

		// toggle() flips relative to the cached, status-fed state and returns the driver's
		// real L_Data.con — "no ACK" here means the transceiver did not confirm the send.
		bool confirmed = lamp.toggle();
		Serial.printf("[send] toggle 1/1/1 -> %s (%s)\n",
			lamp.isOn() ? "ON" : "OFF", confirmed ? "confirmed" : "no ACK");
	}
}

//---- Handler: runs from knx.loop() when a status telegram arrives on 0/1/1 ----

// Already decoded to a bool — no DPT, no manual unpacking on the user side.
void onLampChanged(bool on) {
	Serial.printf("[bus]  status 0/1/1 -> lamp is %s\n", on ? "ON" : "OFF");
}

/*
 * Reading a bad run:
 *
 *   no output at all          → serial/board problem, not KNX.
 *   "driver FAILED"           → ATTiny not answering the U_ reset handshake (wiring, baud, power).
 *   "no ACK" every cycle      → send path reaches the transceiver but no positive L_Data.con.
 *                               Suspect #1 is CON_MASK / CON_PATTERN / CON_POSITIVE in
 *                               KnxDriver.h — spec-derived and never hardware-verified.
 *   "confirmed" but no [bus]  → TX works, RX does not: the actuator is not sending status, or
 *                               its status GA is not 0/1/1, or reassembly/parse is failing.
 *   [bus] but light unchanged → the telegram is fine; the actuator is not acting on 1/1/1.
 */
