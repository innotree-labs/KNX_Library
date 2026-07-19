#pragma once
/**
 * @name KnxDebug.h
 * @date 19.07.2026
 * @authors Florian Wiesner
 * @details Library-wide runtime debug logging. One switch turns on verbose tracing of
 *          everything the library does: bytes in and out of the driver, frame build/parse,
 *          value coding, and dispatch of every incoming telegram — including telegrams
 *          addressed to other devices, which the coordinator sees and discards.
 *
 *          Two guards live in here, on purpose, so that call sites elsewhere stay readable
 *          and free of preprocessor noise:
 *            1. `#ifdef ARDUINO` — on a host build the bodies compile to nothing, so the
 *               Arduino-free layers (KnxValue, KnxTelegram, KnxCoordinator) stay Arduino-free
 *               and the native test suite is unaffected.
 *            2. the runtime flag — when logging is off, a call costs one bool test.
 *
 *          The flag is a single library-wide `bool` held in a function-local static, so the
 *          stateless namespaces (KnxFrame, KnxCodec) can log without an object to hang it on.
 *          This is a deliberate, agreed exception to the project's no-globals rule: it is a
 *          log level, not a service locator. Consequence to know: enabling debug on ONE node
 *          enables it for the whole library, not just that instance.
 *
 *          Note the arguments of a log call are still evaluated when logging is off. Keep
 *          call sites cheap, or guard an expensive one with `if (KnxDebug::isEnabled())`.
*/

//---- Standard / platform libraries ----
#include <cstdint>
#include <stdarg.h>
#include <stdio.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace KnxDebug {

	//---- Config ----
	// Formatting buffer, stack-allocated per call — no dynamic allocation.
	static constexpr uint8_t LOG_BUFFER = 96;

	// The single library-wide switch. A function-local static gives one shared instance
	// across all translation units without needing a .cpp (KnxCommon stays header-only).
	inline bool& flag(void) {
		static bool enabled = false;
		return enabled;
	}

	/** @brief True if verbose logging is currently on. Use to guard expensive call sites. */
	inline bool isEnabled(void) { return flag(); }

	/** @brief Turns verbose logging on or off for the whole library. */
	inline void setEnabled(bool on) { flag() = on; }

	/** @brief Prints one printf-style trace line, prefixed with [knx]. No-op when disabled. */
	inline void log(const char* fmt, ...) {
		if (!flag()) return;
#ifdef ARDUINO
		char buffer[LOG_BUFFER];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		Serial.print("[knx] ");
		Serial.println(buffer);
#else
		(void)fmt;   // host build: the call compiles away
#endif
	}

	/**
	 * @brief Prints a labelled hex dump. Emits byte by byte rather than formatting into a
	 *        buffer first, so a long frame cannot overflow LOG_BUFFER and nothing is built
	 *        when logging is off.
	*/
	inline void logBytes(const char* label, const uint8_t* data, uint8_t length) {
		if (!flag()) return;
#ifdef ARDUINO
		Serial.print("[knx] ");
		Serial.print(label);
		for (uint8_t i = 0; i < length; i++) {
			Serial.print(' ');
			if (data[i] < 0x10) Serial.print('0');
			Serial.print(data[i], HEX);
		}
		Serial.println();
#else
		(void)label; (void)data; (void)length;
#endif
	}

}   // namespace KnxDebug
