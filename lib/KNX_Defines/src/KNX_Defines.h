#pragma once
/**
 * @name KNX_Defines.h
 * @date 11.10.2025
 * @authors Florian Wiesner
 * @details Shared types, enums, structs, and inline helper functions used across the entire KNX stack.
 *          Header-only — include in every KNX module.
*/

#ifdef _MSC_VER
	#pragma region DEBUG //------------------------------------------------------------------------------------------------------
#endif
// #define DEBUG	// Keep commented out for production — enables Serial debug output across all modules

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Libraries //--------------------------------------------------------------------------------------------------
#endif
#include <Arduino.h>

#ifdef _MSC_VER
	#pragma endregion
	#pragma region enums //------------------------------------------------------------------------------------------------------
#endif
enum class AcknowledgeInfo : uint8_t {
	ACK,
	NACK,
	BUSY
};

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
	UNKNOWN
};

enum class KNX_Priority : uint8_t {
	System = 0x00,
	Alarm  = 0x08,
	High   = 0x04,
	Normal = 0x0C
};

enum class GroupValueType : uint8_t {
	Read,
	Response,
	Write,
	Unknown
};

enum class SwitchingBehaviour : uint8_t {
	Off    = 0x00,
	On     = 0x01,
	Toggle = 0x02
};

enum class DimmingBehaviour : uint8_t {
	Darker   = 0x00,
	Brighter = 0x01
};

enum class DimmIncrement : uint8_t {
	Stop         = 0x00,
	Percent_1_5  = 0x00,
	Percent_3    = 0x01,
	Percent_6    = 0x02,
	Percent_12_5 = 0x03,
	Percent_25   = 0x04,
	Percent_50   = 0x05,
	Percent_100  = 0x06
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region structs //----------------------------------------------------------------------------------------------------
#endif
struct GroupAddress {
	uint8_t main;   // Main group (0-31)
	uint8_t middle; // Middle group (0-7)
	uint8_t sub;    // Sub group (0-255)
};

struct PhysicalAddress {
	uint8_t area;   // Area (1-15)
	uint8_t line;   // Line (1-15)
	uint8_t device; // Device (1-255)
};

union KnxTelegramData {
	bool dpt1;
	uint8_t dpt2;
	struct { bool direction; uint8_t step; } dpt3;
	char dpt4;
	struct { uint8_t raw; float percent; } dpt5;
};

struct ParsedTelegram {
	PhysicalAddress source{};
	GroupAddress target{};
	GroupValueType type;
	KNX_DPT dpt = KNX_DPT::UNKNOWN;
	KNX_Priority priority{};
	const uint8_t* payload = nullptr;
	uint8_t payloadLength  = 0;
	uint8_t inline6Data    = 0;
	bool _isInline6        = false;
	bool decodedValid      = false;
	KnxTelegramData decoded;
};

struct KnxEvent {
	PhysicalAddress source;
	GroupAddress target;
	GroupValueType type;
	KNX_DPT dpt;
	KnxTelegramData value;
};

using KNX_Callback = void (*)(const KnxEvent& event);
struct KNX_Binding {
	GroupAddress ga;
	KNX_DPT dpt;
	KNX_Callback callback;
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Functions //--------------------------------------------------------------------------------------------------
#endif
// Converts the string representation of a physical address into a PhysicalAddress struct
inline PhysicalAddress physicalAddressFromString(String address) {
	PhysicalAddress physAddr;

	uint8_t firstDot = address.indexOf('.');
	uint8_t lastDot  = address.lastIndexOf('.');
	if (firstDot == -1 || lastDot == -1 || firstDot == lastDot) {
		#ifdef DEBUG
		Serial.println("!---Falsches Adressformat---! Richtig: Bereich.Linie.Geraet");
		#endif
	}
	physAddr.area   = address.substring(0, firstDot).toInt();
	physAddr.line   = address.substring(firstDot + 1, lastDot).toInt();
	physAddr.device = address.substring(lastDot + 1).toInt();

	if (physAddr.area <= 0 || physAddr.area > 15) {
		physAddr.area = 15;
		#ifdef DEBUG
		Serial.println("!---Bereich muss zwischen 1 und 15 liegen---!");
		#endif
	}
	if (physAddr.line <= 0 || physAddr.line > 15) {
		physAddr.line = 15;
		#ifdef DEBUG
		Serial.println("!---Linie muss zwischen 1 und 15 liegen---!");
		#endif
	}
	if (physAddr.device <= 0 || physAddr.device > 255) {
		physAddr.device = 255;
		#ifdef DEBUG
		Serial.println("!---Geraet muss zwischen 1 und 255 liegen---!");
		#endif
	}

	return physAddr;
}

// Converts the string representation of a group address into a GroupAddress struct
inline GroupAddress groupAddressFromString(String address) {
	GroupAddress groupAddress;

	uint8_t firstSlash = address.indexOf('/');
	uint8_t lastSlash  = address.lastIndexOf('/');
	if (firstSlash == -1 || lastSlash == -1 || firstSlash == lastSlash) {
		#ifdef DEBUG
		Serial.println("!---Falsches Adressformat---! Richtig: Hauptgruppe/Mittelgruppe/Gruppenaddresse");
		#endif
	}
	groupAddress.main   = address.substring(0, firstSlash).toInt();
	groupAddress.middle = address.substring(firstSlash + 1, lastSlash).toInt();
	groupAddress.sub    = address.substring(lastSlash + 1).toInt();

	if (groupAddress.main < 0 || groupAddress.main > 31) {
		groupAddress.main = 31;
		#ifdef DEBUG
		Serial.println("!---Hauptgruppe muss zwischen 0 und 31 liegen---!");
		#endif
	}
	if (groupAddress.middle < 0 || groupAddress.middle > 7) {
		groupAddress.middle = 7;
		#ifdef DEBUG
		Serial.println("!---Mittelgruppe muss zwischen 0 und 7 liegen---!");
		#endif
	}
	if (groupAddress.sub < 0 || groupAddress.sub > 255) {
		groupAddress.sub = 255;
		#ifdef DEBUG
		Serial.println("!---Untergruppe muss zwischen 0 und 255 liegen---!");
		#endif
	}
	if (groupAddress.main == 0 && groupAddress.middle == 0 && groupAddress.sub == 0) {
		groupAddress.sub = 1; // 0/0/0 is invalid
		#ifdef DEBUG
		Serial.println("!---0/0/0 ist keine gueltige Gruppenadresse---! Setze auf 0/0/1");
		#endif
	}

	return groupAddress;
}

inline String stringFromPhysicalAddress(PhysicalAddress address) {
	String s_address = "";
	s_address += address.area;
	s_address += ".";
	s_address += address.line;
	s_address += ".";
	s_address += address.device;
	return s_address;
}

inline String stringFromGroupAddress(GroupAddress address) {
	String s_address = "";
	s_address += address.main;
	s_address += "/";
	s_address += address.middle;
	s_address += "/";
	s_address += address.sub;
	return s_address;
}

inline bool gaEqual(const GroupAddress& a, const GroupAddress& b) {
	return a.main == b.main && a.middle == b.middle && a.sub == b.sub;
}
