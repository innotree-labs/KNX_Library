#pragma once
/**
 * @name KnxValue.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details A KnxValue is one typed KNX value: a datapoint type plus its payload. You create one
 *          with a Dpt*() factory (Dpt1(true), Dpt9(21.5f), …) to pass to send() or write(), and
 *          read a received one back with the matching as*() accessor. Building a value through
 *          the factories means the payload can only ever be the type that datapoint expects.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxEnums.h"

//---- Composite value types (passed to and received from the multi-field datapoints) ----

/** @brief A relative dimming step: a direction and how large a step to take. */
struct DptDim {
	bool    increase;   // true = brighter/up, false = darker/down
	uint8_t stepcode;   // 0 = stop, 1..7 = step size (see DimmIncrement)
};

/** @brief A time of day. */
struct DptTime {
	uint8_t weekday;    // 0 = no day, 1 = Monday .. 7 = Sunday
	uint8_t hour;       // 0..23
	uint8_t minute;     // 0..59
	uint8_t second;     // 0..59
};

/** @brief A calendar date. */
struct DptDate {
	uint8_t  day;       // 1..31
	uint8_t  month;     // 1..12
	uint16_t year;      // full year, e.g. 2026
};

/** @brief A combined date and time, with daylight-saving and fault flags. */
struct DptDateTime {
	uint16_t year;      // full year, e.g. 2026
	uint8_t  month;     // 1..12
	uint8_t  day;       // 1..31
	uint8_t  weekday;   // 0 = no day, 1 = Monday .. 7 = Sunday
	uint8_t  hour;      // 0..24
	uint8_t  minute;    // 0..59
	uint8_t  second;    // 0..59
	bool     summerTime;  // daylight saving active
	bool     faultFlag;   // clock/data fault
};

/** @brief An RGB colour, each channel 0..255. */
struct DptColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/**
 * @brief One typed KNX value: a datapoint type plus its payload. Create it with a Dpt*() factory
 *        and read it with the matching as*() accessor.
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

	//---- Accessors: read the value as the type its datapoint uses ----
	bool        asBool(void)     const { return payload.b; }        ///< The value as a boolean.
	uint8_t     asU8(void)       const { return payload.u8; }       ///< The value as an unsigned 8-bit integer.
	int8_t      asI8(void)       const { return payload.i8; }       ///< The value as a signed 8-bit integer.
	char        asChar(void)     const { return payload.ch; }       ///< The value as a character.
	uint16_t    asU16(void)      const { return payload.u16; }      ///< The value as an unsigned 16-bit integer.
	int16_t     asI16(void)      const { return payload.i16; }      ///< The value as a signed 16-bit integer.
	uint32_t    asU32(void)      const { return payload.u32; }      ///< The value as an unsigned 32-bit integer.
	int32_t     asI32(void)      const { return payload.i32; }      ///< The value as a signed 32-bit integer.
	float       asFloat(void)    const { return payload.f; }        ///< The value as a floating-point number.
	DptDim      asDim(void)      const { return payload.dim; }      ///< The value as a dimming step.
	DptTime     asTime(void)     const { return payload.time; }     ///< The value as a time of day.
	DptDate     asDate(void)     const { return payload.date; }     ///< The value as a calendar date.
	DptDateTime asDateTime(void) const { return payload.datetime; } ///< The value as a date and time.
	DptColor    asColor(void)    const { return payload.color; }    ///< The value as an RGB colour.

	/** @brief Whether this value holds a real datapoint (not an empty/unknown value).
	 *  @return true if the value is usable. */
	bool isValid(void) const { return dpt != KnxDpt::UNKNOWN; }
};

//---- Value factories: one per datapoint type; each accepts only that type's payload ----

/** @brief Makes a boolean value (on/off, true/false). @param value The boolean to send. */
inline KnxValue Dpt1(bool value)      { KnxValue v; v.dpt = KnxDpt::DPT1;  v.payload.b   = value; return v; }
/** @brief Makes a 2-bit value (0..3). @param value The value; only the low 2 bits are used. */
inline KnxValue Dpt2(uint8_t value)   { KnxValue v; v.dpt = KnxDpt::DPT2;  v.payload.u8  = value & 0x03; return v; }
/** @brief Makes a relative dimming step. @param increase true = brighter, false = darker.
 *  @param stepcode Step size 1..7, or 0 to stop. */
inline KnxValue Dpt3(bool increase, uint8_t stepcode) { KnxValue v; v.dpt = KnxDpt::DPT3; v.payload.dim = DptDim{ increase, (uint8_t)(stepcode & 0x07) }; return v; }
/** @brief Makes a single-character value. @param value The character to send. */
inline KnxValue Dpt4(char value)      { KnxValue v; v.dpt = KnxDpt::DPT4;  v.payload.ch  = value; return v; }
/** @brief Makes an 8-bit unsigned value (0..255). @param value The value to send. */
inline KnxValue Dpt5(uint8_t value)   { KnxValue v; v.dpt = KnxDpt::DPT5;  v.payload.u8  = value; return v; }
/** @brief Makes an 8-bit signed value (-128..127). @param value The value to send. */
inline KnxValue Dpt6(int8_t value)    { KnxValue v; v.dpt = KnxDpt::DPT6;  v.payload.i8  = value; return v; }
/** @brief Makes a 16-bit unsigned value (0..65535). @param value The value to send. */
inline KnxValue Dpt7(uint16_t value)  { KnxValue v; v.dpt = KnxDpt::DPT7;  v.payload.u16 = value; return v; }
/** @brief Makes a 16-bit signed value (-32768..32767). @param value The value to send. */
inline KnxValue Dpt8(int16_t value)   { KnxValue v; v.dpt = KnxDpt::DPT8;  v.payload.i16 = value; return v; }
/** @brief Makes a 16-bit floating-point value (e.g. a temperature). @param value The value to send. */
inline KnxValue Dpt9(float value)     { KnxValue v; v.dpt = KnxDpt::DPT9;  v.payload.f   = value; return v; }
/** @brief Makes a time-of-day value. @param value The time to send. */
inline KnxValue Dpt10(DptTime value)  { KnxValue v; v.dpt = KnxDpt::DPT10; v.payload.time = value; return v; }
/** @brief Makes a calendar-date value. @param value The date to send. */
inline KnxValue Dpt11(DptDate value)  { KnxValue v; v.dpt = KnxDpt::DPT11; v.payload.date = value; return v; }
/** @brief Makes a 32-bit unsigned value. @param value The value to send. */
inline KnxValue Dpt12(uint32_t value) { KnxValue v; v.dpt = KnxDpt::DPT12; v.payload.u32 = value; return v; }
/** @brief Makes a 32-bit signed value. @param value The value to send. */
inline KnxValue Dpt13(int32_t value)  { KnxValue v; v.dpt = KnxDpt::DPT13; v.payload.i32 = value; return v; }
/** @brief Makes a 32-bit floating-point value. @param value The value to send. */
inline KnxValue Dpt14(float value)    { KnxValue v; v.dpt = KnxDpt::DPT14; v.payload.f   = value; return v; }
/** @brief Makes a combined date-and-time value. @param value The date and time to send. */
inline KnxValue Dpt19(DptDateTime value) { KnxValue v; v.dpt = KnxDpt::DPT19; v.payload.datetime = value; return v; }
/** @brief Makes an RGB colour value. @param r Red 0..255. @param g Green 0..255. @param b Blue 0..255. */
inline KnxValue Dpt232(uint8_t r, uint8_t g, uint8_t b) { KnxValue v; v.dpt = KnxDpt::DPT232; v.payload.color = DptColor{ r, g, b }; return v; }
