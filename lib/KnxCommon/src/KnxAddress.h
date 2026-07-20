#pragma once
/**
 * @name KnxAddress.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details KNX group and physical address types. The packed uint16_t representation and
 *          its pack/unpack math are Arduino-free so address handling stays host-testable;
 *          only the String parse/format convenience helpers require <Arduino.h>.
*/

//---- Libraries ----
#include <cstdint>

//---- Address structs ----
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

//---- Packed group address (3-level: main[5] | middle[3] | sub[8]) ----
// A single uint16_t is the hot-path currency: constructed once from a String at config
// time, then compared and transmitted without re-parsing (PLAN §8).
inline uint16_t packGroupAddress(const GroupAddress& ga) {
	return (uint16_t)(((ga.main & 0x1F) << 11) | ((ga.middle & 0x07) << 8) | ga.sub);
}

inline GroupAddress unpackGroupAddress(uint16_t packed) {
	GroupAddress ga;
	ga.main   = (packed >> 11) & 0x1F;
	ga.middle = (packed >> 8) & 0x07;
	ga.sub    = packed & 0xFF;
	return ga;
}

inline bool gaEqual(const GroupAddress& a, const GroupAddress& b) {
	return a.main == b.main && a.middle == b.middle && a.sub == b.sub;
}

//---- Packed physical address (area[4] | line[4] | device[8]) ----
// Same wire layout as the group address occupies in octets 3..4 of a frame; kept as a
// symmetric pair so the framing layer never open-codes the shifts.
inline uint16_t packPhysicalAddress(const PhysicalAddress& pa) {
	return (uint16_t)(((pa.area & 0x0F) << 12) | ((pa.line & 0x0F) << 8) | pa.device);
}

inline PhysicalAddress unpackPhysicalAddress(uint16_t packed) {
	PhysicalAddress pa;
	pa.area   = (packed >> 12) & 0x0F;
	pa.line   = (packed >> 8) & 0x0F;
	pa.device = packed & 0xFF;
	return pa;
}

inline bool paEqual(const PhysicalAddress& a, const PhysicalAddress& b) {
	return a.area == b.area && a.line == b.line && a.device == b.device;
}

//---- String convenience helpers (Arduino only) ----
#ifdef ARDUINO
#include <Arduino.h>
#include "KnxDebug.h"   // validation warnings go through the library-wide debug switch

// Converts the string representation of a physical address into a PhysicalAddress struct
inline PhysicalAddress physicalAddressFromString(String address) {
	PhysicalAddress physAddr;

	uint8_t firstDot = address.indexOf('.');
	uint8_t lastDot  = address.lastIndexOf('.');
	if (firstDot == -1 || lastDot == -1 || firstDot == lastDot) {
		KnxDebug::log("ADR !! bad physical address format, expected area.line.device");
	}
	physAddr.area   = address.substring(0, firstDot).toInt();
	physAddr.line   = address.substring(firstDot + 1, lastDot).toInt();
	physAddr.device = address.substring(lastDot + 1).toInt();

	if (physAddr.area <= 0 || physAddr.area > 15) {
		physAddr.area = 15;
		KnxDebug::log("ADR !! area out of range (1-15), clamped to 15");
	}
	if (physAddr.line <= 0 || physAddr.line > 15) {
		physAddr.line = 15;
		KnxDebug::log("ADR !! line out of range (1-15), clamped to 15");
	}
	if (physAddr.device <= 0 || physAddr.device > 255) {
		physAddr.device = 255;
		KnxDebug::log("ADR !! device out of range (1-255), clamped to 255");
	}

	return physAddr;
}

// Converts the string representation of a group address into a GroupAddress struct
inline GroupAddress groupAddressFromString(String address) {
	GroupAddress groupAddress;

	uint8_t firstSlash = address.indexOf('/');
	uint8_t lastSlash  = address.lastIndexOf('/');
	if (firstSlash == -1 || lastSlash == -1 || firstSlash == lastSlash) {
		KnxDebug::log("ADR !! bad group address format, expected main/middle/sub");
	}
	groupAddress.main   = address.substring(0, firstSlash).toInt();
	groupAddress.middle = address.substring(firstSlash + 1, lastSlash).toInt();
	groupAddress.sub    = address.substring(lastSlash + 1).toInt();

	if (groupAddress.main > 31) {
		groupAddress.main = 31;
		KnxDebug::log("ADR !! main group out of range (0-31), clamped to 31");
	}
	if (groupAddress.middle > 7) {
		groupAddress.middle = 7;
		KnxDebug::log("ADR !! middle group out of range (0-7), clamped to 7");
	}
	if (groupAddress.sub > 255) {
		groupAddress.sub = 255;
		KnxDebug::log("ADR !! sub group out of range (0-255), clamped to 255");
	}
	if (groupAddress.main == 0 && groupAddress.middle == 0 && groupAddress.sub == 0) {
		groupAddress.sub = 1; // 0/0/0 is invalid
		KnxDebug::log("ADR !! 0/0/0 is not a valid group address, using 0/0/1");
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

// Parses a "main/middle/sub" string straight into the packed uint16_t currency.
inline uint16_t packedGroupAddressFromString(String address) {
	return packGroupAddress(groupAddressFromString(address));
}

#endif // ARDUINO
