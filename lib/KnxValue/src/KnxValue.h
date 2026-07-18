#pragma once
/**
 * @name KnxValue.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details The library's single value currency (PLAN §4). A KnxValue is a tagged union:
 *          one KnxDpt tag plus a payload sized to the largest v1 datapoint. Typed
 *          constructors (Dpt1(..) .. Dpt232(..)) populate it so a payload/DPT mismatch is a
 *          compile error, not a silent wrong telegram. Named accessors read it back.
 *
 *          Deliberately Arduino-free: depends only on <cstdint> and KnxEnums.h so it can be
 *          compiled and unit-tested on a host. The wire encode/decode lives in KnxCodec.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxEnums.h"

//---- Composite payloads (named so future intent classes KnxTime/KnxDate/... don't clash) ----

// DPT3.007 control-dimming: a direction bit and a 3-bit step code (0 = stop).
struct DptDim {
	bool    increase;   // true = brighter/up, false = darker/down
	uint8_t stepcode;   // 0..7, see DimmIncrement
};

// DPT10.001 time of day.
struct DptTime {
	uint8_t weekday;    // 0 = no day, 1 = Monday .. 7 = Sunday
	uint8_t hour;       // 0..23
	uint8_t minute;     // 0..59
	uint8_t second;     // 0..59
};

// DPT11.001 date.
struct DptDate {
	uint8_t  day;       // 1..31
	uint8_t  month;     // 1..12
	uint16_t year;      // full year, e.g. 2026
};

// DPT19.001 date + time with the common validity/status flags.
struct DptDateTime {
	uint16_t year;      // full year, e.g. 2026
	uint8_t  month;     // 1..12
	uint8_t  day;       // 1..31
	uint8_t  weekday;   // 0 = no day, 1 = Monday .. 7 = Sunday
	uint8_t  hour;      // 0..24
	uint8_t  minute;    // 0..59
	uint8_t  second;    // 0..59
	bool     summerTime;  // DST active
	bool     faultFlag;   // clock/data fault
};

// DPT232.600 RGB colour.
struct DptColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/**
 * @brief One typed KNX value: its datapoint type plus the correctly-typed native payload.
 *        Construct via the DptN(..) factories; read via the asX() accessors.
*/
struct KnxValue {
	KnxDpt dpt = KnxDpt::UNKNOWN;
	union Payload {
		bool        b;      // DPT1
		uint8_t     u8;     // DPT2, DPT5
		int8_t      i8;     // DPT6
		char        ch;     // DPT4
		uint16_t    u16;    // DPT7
		int16_t     i16;    // DPT8
		uint32_t    u32;    // DPT12
		int32_t     i32;    // DPT13
		float       f;      // DPT9, DPT14
		DptDim      dim;    // DPT3
		DptTime     time;   // DPT10
		DptDate     date;   // DPT11
		DptDateTime datetime; // DPT19 (largest v1 payload)
		DptColor    color;  // DPT232
		Payload() : u32(0) {}
	} payload;

	//---- Named accessors (read the payload for the value's DPT) ----
	bool        asBool(void)     const { return payload.b; }
	uint8_t     asU8(void)       const { return payload.u8; }
	int8_t      asI8(void)       const { return payload.i8; }
	char        asChar(void)     const { return payload.ch; }
	uint16_t    asU16(void)      const { return payload.u16; }
	int16_t     asI16(void)      const { return payload.i16; }
	uint32_t    asU32(void)      const { return payload.u32; }
	int32_t     asI32(void)      const { return payload.i32; }
	float       asFloat(void)    const { return payload.f; }
	DptDim      asDim(void)      const { return payload.dim; }
	DptTime     asTime(void)     const { return payload.time; }
	DptDate     asDate(void)     const { return payload.date; }
	DptDateTime asDateTime(void) const { return payload.datetime; }
	DptColor    asColor(void)    const { return payload.color; }

	bool isValid(void) const { return dpt != KnxDpt::UNKNOWN; }
};

//---- Typed constructors: one per datapoint type, payload can only be its native type ----

inline KnxValue Dpt1(bool value)      { KnxValue v; v.dpt = KnxDpt::DPT1;  v.payload.b   = value; return v; }
inline KnxValue Dpt2(uint8_t value)   { KnxValue v; v.dpt = KnxDpt::DPT2;  v.payload.u8  = value & 0x03; return v; }
inline KnxValue Dpt3(bool increase, uint8_t stepcode) { KnxValue v; v.dpt = KnxDpt::DPT3; v.payload.dim = DptDim{ increase, (uint8_t)(stepcode & 0x07) }; return v; }
inline KnxValue Dpt4(char value)      { KnxValue v; v.dpt = KnxDpt::DPT4;  v.payload.ch  = value; return v; }
inline KnxValue Dpt5(uint8_t value)   { KnxValue v; v.dpt = KnxDpt::DPT5;  v.payload.u8  = value; return v; }
inline KnxValue Dpt6(int8_t value)    { KnxValue v; v.dpt = KnxDpt::DPT6;  v.payload.i8  = value; return v; }
inline KnxValue Dpt7(uint16_t value)  { KnxValue v; v.dpt = KnxDpt::DPT7;  v.payload.u16 = value; return v; }
inline KnxValue Dpt8(int16_t value)   { KnxValue v; v.dpt = KnxDpt::DPT8;  v.payload.i16 = value; return v; }
inline KnxValue Dpt9(float value)     { KnxValue v; v.dpt = KnxDpt::DPT9;  v.payload.f   = value; return v; }
inline KnxValue Dpt10(DptTime value)  { KnxValue v; v.dpt = KnxDpt::DPT10; v.payload.time = value; return v; }
inline KnxValue Dpt11(DptDate value)  { KnxValue v; v.dpt = KnxDpt::DPT11; v.payload.date = value; return v; }
inline KnxValue Dpt12(uint32_t value) { KnxValue v; v.dpt = KnxDpt::DPT12; v.payload.u32 = value; return v; }
inline KnxValue Dpt13(int32_t value)  { KnxValue v; v.dpt = KnxDpt::DPT13; v.payload.i32 = value; return v; }
inline KnxValue Dpt14(float value)    { KnxValue v; v.dpt = KnxDpt::DPT14; v.payload.f   = value; return v; }
inline KnxValue Dpt19(DptDateTime value) { KnxValue v; v.dpt = KnxDpt::DPT19; v.payload.datetime = value; return v; }
inline KnxValue Dpt232(uint8_t r, uint8_t g, uint8_t b) { KnxValue v; v.dpt = KnxDpt::DPT232; v.payload.color = DptColor{ r, g, b }; return v; }
