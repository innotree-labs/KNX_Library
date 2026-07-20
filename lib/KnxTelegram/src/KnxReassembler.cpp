/**
 * @name KnxReassembler.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxReassembler.h.
*/

//---- Libraries ----
#include "KnxReassembler.h"

void KnxReassembler::reset(void) {
	index        = 0;
	expectedLen  = 0;
	inFrame      = false;
}

bool KnxReassembler::feed(uint8_t byte) {
	// Idle: wait for any legal L_Data_Standard control byte, ignore everything else.
	if (!inFrame) {
		if (!isControlByte(byte)) return false;
		index       = 0;
		expectedLen = 0;
		inFrame     = true;
	}

	// Guard against a runaway frame (never saw a valid length).
	if (index >= CAPACITY) {
		reset();
		return false;
	}

	buffer[index++] = byte;

	// Once the header through the NPCI byte is in, the total length is known.
	if (expectedLen == 0 && index >= 6) {
		uint8_t apduLen = (buffer[5] & 0x0F) + 1;
		expectedLen = (uint8_t)(6 + apduLen + 1);   // header + APDU + checksum
		if (expectedLen < MIN_FRAME || expectedLen > CAPACITY) {
			reset();
			return false;
		}
	}

	// Complete frame buffered — hand it off and arm for the next one.
	if (expectedLen != 0 && index >= expectedLen) {
		completedLen = index;
		inFrame      = false;
		return true;
	}

	return false;
}
