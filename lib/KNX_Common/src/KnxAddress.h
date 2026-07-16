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

//---- String convenience helpers (Arduino only) ----
#ifdef ARDUINO
#include <Arduino.h>

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

	if (groupAddress.main > 31) {
		groupAddress.main = 31;
		#ifdef DEBUG
		Serial.println("!---Hauptgruppe muss zwischen 0 und 31 liegen---!");
		#endif
	}
	if (groupAddress.middle > 7) {
		groupAddress.middle = 7;
		#ifdef DEBUG
		Serial.println("!---Mittelgruppe muss zwischen 0 und 7 liegen---!");
		#endif
	}
	if (groupAddress.sub > 255) {
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

// Parses a "main/middle/sub" string straight into the packed uint16_t currency.
inline uint16_t packedGroupAddressFromString(String address) {
	return packGroupAddress(groupAddressFromString(address));
}

#endif // ARDUINO
