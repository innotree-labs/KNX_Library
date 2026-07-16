#pragma once
/**
 * @name KnxEnums.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details KNX protocol enumerations: datapoint types, priorities, service types and
 *          the intent behaviours used by the device layer. Deliberately Arduino-free
 *          (only <cstdint>) so it can be shared with the host-testable value codec.
*/

//---- Libraries ----
#include <cstdint>

//---- Datapoint types ----
enum class KNX_DPT : uint8_t {
	DPT1	= 1,	// 1 bit, binary values
	DPT2	= 2,	// 2 bit, 0..4, states
	DPT3	= 3,	// 4 bit, 0..15, dimming or blind steps
	DPT4	= 4,	// 8 bit, 0..255, ASCII characters
	DPT5	= 5,	// 8 bit, 0..255, unsigned integer
	DPT6	= 6,	// 8 bit, -128..127, signed integer
	DPT7	= 7,	// 16 bit, 0..65535, unsigned integer
	DPT8	= 8,	// 16 bit, -32768..32767, signed integer
	DPT9	= 9,	// 16 bit float
	DPT10	= 10,	// 3-byte time: hours, minutes, seconds
	DPT11	= 11,	// 3-byte date: day, month, year
	DPT12	= 12,	// 32 bit, 0..4294967295, unsigned integer
	DPT13	= 13,	// 32 bit, -2147483648..2147483647, signed integer
	DPT14	= 14,	// 32 bit float
	DPT16	= 16,	// 14-byte ASCII string
	DPT19	= 19,	// 8-byte datetime
	DPT232	= 232,	// 3-byte RGB colour (DPT232.600)
	UNKNOWN
};

//---- Link-layer acknowledge codes ----
enum class AcknowledgeInfo : uint8_t {
	ACK,
	NACK,
	BUSY
};

//---- Telegram priority (control field bits) ----
enum class KNX_Priority : uint8_t {
	System = 0x00,
	Alarm  = 0x08,
	High   = 0x04,
	Normal = 0x0C
};

//---- Group value service type ----
enum class GroupValueType : uint8_t {
	Read,
	Response,
	Write,
	Unknown
};

//---- Intent behaviours (device layer) ----
enum class SwitchingBehaviour : uint8_t {
	Off    = 0x00,
	On     = 0x01,
	Toggle = 0x02
};

enum class DimmingBehaviour : uint8_t {
	Darker   = 0x00,
	Brighter = 0x01
};

// DPT3.007 step code (bits 2..0 of the 4-bit control-dimming value).
// stepcode encodes the number of dimming intervals: 2^(stepcode-1). The interval
// size in percent is therefore 100 % / 2^(stepcode-1) — larger stepcode = finer step.
// stepcode 0 is the "break"/stop command. This ordering fixes the earlier enum where
// Stop and the finest step both mapped to 0x00 and the percentages ran backwards.
enum class DimmIncrement : uint8_t {
	Stop         = 0x00,	// break — stop dimming
	Percent_100  = 0x01,	// 1 interval    -> 100 %
	Percent_50   = 0x02,	// 2 intervals   -> 50 %
	Percent_25   = 0x03,	// 4 intervals   -> 25 %
	Percent_12_5 = 0x04,	// 8 intervals   -> 12.5 %
	Percent_6    = 0x05,	// 16 intervals  -> 6.25 %
	Percent_3    = 0x06,	// 32 intervals  -> 3.125 %
	Percent_1_5  = 0x07		// 64 intervals  -> 1.5625 %
};
