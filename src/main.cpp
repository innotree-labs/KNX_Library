/**
 * @name main.cpp
 * @date 19.07.2026
 * @authors Florian Wiesner
 * @details HARDWARE BENCH TEST for the KNX library — no buttons, no user input.
 *
 *          A single light is toggled every 5 s, and every status telegram the bus sends back is
 *          printed over Serial — both the DPT1 on/off status and the DPT5 brightness the dimmer
 *          publishes. Between them these exercise the two paths host tests cannot reach: the
 *          driver's real transmit path (does the ATTiny accept the frame and return a positive
 *          L_Data.con?) and the receive path (reassemble -> parse -> match -> decode -> callback),
 *          the latter across two objects and two different DPTs.
 *
 *          Watch the serial monitor at 115200 baud. Expected per cycle: one "[send] toggle" line
 *          with "confirmed", then one "[bus]" status line, and — if the actuator ramps — one or
 *          more brightness lines. See the failure guide at the bottom of this file.
*/

//---- Libraries ----
#include <Arduino.h>
#include <Konnextor.h>     // the whole library: driver + coordinator + values + intent objects

//---- Configuration ----
#define BAUDRATE_SERIAL     115200
#define PHYS_ADDR           "1.1.5"   // this device's KNX physical address
#define TOGGLE_INTERVAL_MS  5000      // how often to flip the light

// Flip to true when something misbehaves: traces every telegram sent and received (including
// foreign ones), frame build/parse, value coding and driver traffic, all prefixed with [knx].
// Left off by default — the printing itself costs time on the receive path, so a clean run
// should be established first and the verbose trace used to explain a broken one.
#define KNX_VERBOSE         false

//---- KNX node: one object, address typed once; the bus driver is owned internally ----
Konnextor knx(PHYS_ADDR);

//---- The light under test: (node, switching GA, status GA) ----
// Sends on/off to 1/1/1, listens for switching status on 0/1/1.
KnxLight lamp(knx, "1/1/1", "0/1/1");

//---- Brightness status published by the dimmer on 0/2/1 (DPT5, listen-only here) ----
// Single-GA ctor: command and status GA are the same, so this object matches on 0/2/1. The
// bench test never calls set() — it only reads what the actuator reports.
KnxPercent brightness(knx, "0/2/1");

//---- Non-blocking 5 s cadence (rollover-safe: unsigned subtraction) ----
static uint32_t ts_lastToggle = 0;

//---- Callback handlers: declared here, defined below loop() ----
void onLampChanged(bool on);
void onBrightnessChanged(uint8_t percent);

void setup() {
	Serial.begin(BAUDRATE_SERIAL);
	while (!Serial && millis() < 3000) { }   // give USB CDC a moment, but never hang the board

	Serial.println("\n[boot] KNX bench test — toggling 1/1/1 every 5 s");

	// Must be set before begin() so the driver's own bring-up is traced too.
	knx.enableDebugMode(KNX_VERBOSE);

	// begin() brings up the UART and hands the physical address to the transceiver.
	// If this fails the ATTiny is not answering — nothing below will work, so say so loudly.
	if (knx.begin()) {
		Serial.println("[boot] driver up, bus link ready");
	} else {
		Serial.println("[boot] *** driver FAILED to initialise — check wiring/ATTiny ***");
	}

	lamp.onUpdate(onLampChanged);
	brightness.onUpdate(onBrightnessChanged);
}

void loop() {
	// Service the KNX stack every iteration: parse incoming telegrams and fire callbacks.
	// This is why the cadence below uses millis() and not delay() — a delay would stall the
	// receive path and the status callbacks would arrive late or be dropped.
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

//---- Handlers: run from knx.loop() when a matching telegram arrives ----

// Switching status on 0/1/1, already decoded to a bool.
void onLampChanged(bool on) {
	Serial.printf("[bus]  status 0/1/1 -> lamp is %s\n", on ? "ON" : "OFF");
}

// Brightness status on 0/2/1. The wire carries DPT5 raw 0..255; KnxPercent rescales to 0..100 %
// before this runs, so a full-brightness telegram (0xFF) prints as 100.
void onBrightnessChanged(uint8_t percent) {
	Serial.printf("[bus]  status 0/2/1 -> brightness %u %%\n", percent);
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
 *                               its status GAs differ, or reassembly/parse is failing.
 *   0/1/1 arrives, 0/2/1 not  → RX and dispatch are fine (one object matched); the dimmer is
 *                               simply not publishing brightness, or not on 0/2/1.
 *   brightness always 0 / 100 → DPT5 decode or the 0..255 -> 0..100 rescale is suspect.
 *   [bus] but light unchanged → the telegram is fine; the actuator is not acting on 1/1/1.
 */
