/**
 * @name KnxDriver.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxDriver.h. Adapted from the thesis KNX_TPUART2 driver; the telegram-layer
 *          coupling is removed (RX is exposed as reassembled frames via poll()) and the send
 *          path now reads the real L_Data.con instead of hard-returning true (PLAN §9).
*/

//----Libraries----
#include "KnxDriver.h"

KnxDriver::KnxDriver(String physicalAddress)
	: rxPin(KNX_DRIVER_RX), txPin(KNX_DRIVER_TX), resnPin(KNX_DRIVER_RESN),
	  baudrate(KNX_DRIVER_BAUD),
	  physicalAddress(physicalAddressFromString(physicalAddress)) {
	pinMode(resnPin, INPUT_PULLUP);
}

//---- Private methods ----

void KnxDriver::sendCommand(const uint8_t* cmd, uint16_t len) {
	uart.write(cmd, len);
}

void KnxDriver::clearBuffer(void) {
	while (uart.available()) (void)uart.read();
}

bool KnxDriver::resetRequest(void) {
	sendCommand(&U_RESET_REQ, sizeof(U_RESET_REQ));
	clearBuffer();
	delay(RESPONSE_TIME_MS);

	// Wait for Reset.indication.
	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME_MS) {
		if (uart.available()) {
			uint8_t b = uart.read();
			if (b != 0x00 && b == U_RESET_IND) return true;   // ignore RX-idle 0x00
		}
	}

	// Fall back to a hardware reset pulse on /RESET.
	pinMode(resnPin, OUTPUT);
	digitalWrite(resnPin, LOW);
	delayMicroseconds(50);
	pinMode(resnPin, INPUT_PULLUP);

	clearBuffer();
	start = millis();
	while (millis() - start < RESPONSE_TIME_MS) {
		if (uart.available()) {
			uint8_t b = uart.read();
			if (b != 0x00) return (b == U_RESET_IND);
		}
	}
	return false;
}

uint8_t KnxDriver::stateRequest(void) {
	sendCommand(&U_STATE_REQ, sizeof(U_STATE_REQ));
	delay(RESPONSE_TIME_MS);

	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME_MS) {
		if (uart.available()) return uart.read();
	}
	return 0xFF;
}

void KnxDriver::applyPhysicalAddress(void) {
	uint8_t addressHigh = (physicalAddress.area << 4) | physicalAddress.line;
	uint8_t addressLow  = physicalAddress.device;
	uint8_t cmd[3]      = { U_SET_ADDRESS, addressHigh, addressLow };
	sendCommand(cmd, sizeof(cmd));
}

bool KnxDriver::isConfirmation(uint8_t b) {
	return (b & CON_MASK) == CON_PATTERN;
}

bool KnxDriver::isPositiveConfirmation(uint8_t b) {
	return (b & CON_POSITIVE) != 0;
}

bool KnxDriver::awaitConfirmation(void) {
	uint32_t start = millis();
	while (millis() - start < CON_TIMEOUT_MS) {
		if (uart.available()) {
			uint8_t b = uart.read();
			if (isConfirmation(b)) {
				// The con byte itself is logged: these are the spec-derived CON_* constants,
				// the least verified part of the driver, so show the raw byte behind the verdict.
				KnxDebug::log("DRV con byte 0x%02X -> %s", (unsigned)b,
					isPositiveConfirmation(b) ? "positive" : "NEGATIVE");
				return isPositiveConfirmation(b);
			}
			KnxDebug::log("DRV ignored byte 0x%02X while awaiting con", (unsigned)b);
			// Any other byte in this window (e.g. an echoed data octet) is ignored.
		}
	}
	KnxDebug::log("DRV !! no con within %u ms", (unsigned)CON_TIMEOUT_MS);
	return false;   // no confirmation -> report failure, never a blind success
}

//---- IKnxDriver ----

bool KnxDriver::begin(void) {
	uart.begin(baudrate, SERIAL_8E1, txPin, rxPin);
	KnxDebug::log("DRV uart up @ %u baud (rx %u, tx %u)",
		(unsigned)baudrate, (unsigned)rxPin, (unsigned)txPin);

	bool reset_ok = resetRequest();
	applyPhysicalAddress();
	uint8_t state = stateRequest();
	KnxDebug::log("DRV begin: reset %s, state 0x%02X",
		reset_ok ? "ok" : "FAILED", (unsigned)state);
	return reset_ok && (state == U_STATE_IND_OK);
}

bool KnxDriver::reset(void) {
	bool reset_ok = resetRequest();
	applyPhysicalAddress();
	uint8_t state = stateRequest();
	KnxDebug::log("DRV reset: %s, state 0x%02X",
		reset_ok ? "ok" : "FAILED", (unsigned)state);
	return reset_ok && (state == U_STATE_IND_OK);
}

bool KnxDriver::sendTelegram(const uint8_t* frame, uint8_t length) {
	KnxDebug::logBytes("DRV tx frame:", frame, length);

	uint8_t pair[2];
	for (uint8_t i = 0; i < length; i++) {
		pair[0] = (uint8_t)((i == length - 1 ? U_DATA_END : U_DATA_START_CONTINUE) | (i & 0x3F));
		pair[1] = frame[i];
		sendCommand(pair, sizeof(pair));
	}
	return awaitConfirmation();
}

bool KnxDriver::poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) {
	while (uart.available()) {
		uint8_t b = uart.read();
		if (reassembler.feed(b)) {
			uint8_t n = reassembler.length();
			if (n > maxLen) {          // caller buffer too small — drop, stay in sync
				reassembler.reset();
				return false;
			}
			for (uint8_t i = 0; i < n; i++) out[i] = reassembler.frame()[i];
			outLen = n;
			return true;               // one frame per call; loop poll() to drain more
		}
	}
	return false;
}
