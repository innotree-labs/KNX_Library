/**
 * @name KnxCodec.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxCodec.h. Symmetric native <-> wire coding, big-endian for multi-octet
 *          datapoints. No <Arduino.h> — pure protocol math.
*/

//---- Libraries ----
#include "KnxCodec.h"
#include <cmath>
#include <cstring>

namespace KnxCodec {

//---- Big-endian octet writers ----
static void putU16(uint8_t* p, uint16_t v) {
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v & 0xFF);
}

static void putU32(uint8_t* p, uint32_t v) {
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v & 0xFF);
}

static uint16_t getU16(const uint8_t* p) {
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t getU32(const uint8_t* p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

//---- DPT9 2-octet float (S EEEE MMMMMMMMMMM, value = 0.01 * M * 2^E) ----
uint16_t encodeFloat16(float value) {
	// Mantissa in hundredths; keep halving until it fits the signed 12-bit range.
	int32_t m = (int32_t)lroundf(value * 100.0f);
	uint8_t exp = 0;
	while (m > 2047 || m < -2048) {
		m /= 2;              // truncates toward zero; exponent compensates
		if (++exp >= 15) break;   // 4-bit exponent saturates
	}

	uint16_t sign = (m < 0) ? 0x8000 : 0x0000;
	uint16_t mant = (uint16_t)(m & 0x07FF);         // low 11 bits (two's complement)
	return sign | ((uint16_t)(exp & 0x0F) << 11) | mant;
}

float decodeFloat16(uint16_t raw) {
	uint8_t  exp  = (raw >> 11) & 0x0F;
	int16_t  mant = raw & 0x07FF;
	if (raw & 0x8000) mant -= 2048;                 // sign-extend the 12-bit mantissa
	return 0.01f * (float)mant * (float)(1UL << exp);
}

bool isInline6(KNX_DPT dpt) {
	return dpt == KNX_DPT::DPT1 || dpt == KNX_DPT::DPT2 || dpt == KNX_DPT::DPT3;
}

uint8_t encode(const KnxValue& v, uint8_t* out, uint8_t maxLen) {
	switch (v.dpt) {
		case KNX_DPT::DPT1:
			if (maxLen < 1) return 0;
			out[0] = v.payload.b ? 0x01 : 0x00;
			return 1;

		case KNX_DPT::DPT2:
			if (maxLen < 1) return 0;
			out[0] = v.payload.u8 & 0x03;
			return 1;

		case KNX_DPT::DPT3:
			if (maxLen < 1) return 0;
			out[0] = (uint8_t)((v.payload.dim.increase ? 0x08 : 0x00) | (v.payload.dim.stepcode & 0x07));
			return 1;

		case KNX_DPT::DPT4:
			if (maxLen < 1) return 0;
			out[0] = (uint8_t)v.payload.ch;
			return 1;

		case KNX_DPT::DPT5:
			if (maxLen < 1) return 0;
			out[0] = v.payload.u8;
			return 1;

		case KNX_DPT::DPT6:
			if (maxLen < 1) return 0;
			out[0] = (uint8_t)v.payload.i8;
			return 1;

		case KNX_DPT::DPT7:
			if (maxLen < 2) return 0;
			putU16(out, v.payload.u16);
			return 2;

		case KNX_DPT::DPT8:
			if (maxLen < 2) return 0;
			putU16(out, (uint16_t)v.payload.i16);
			return 2;

		case KNX_DPT::DPT9:
			if (maxLen < 2) return 0;
			putU16(out, encodeFloat16(v.payload.f));
			return 2;

		case KNX_DPT::DPT10: {
			if (maxLen < 3) return 0;
			const DptTime& t = v.payload.time;
			out[0] = (uint8_t)(((t.weekday & 0x07) << 5) | (t.hour & 0x1F));
			out[1] = t.minute & 0x3F;
			out[2] = t.second & 0x3F;
			return 3;
		}

		case KNX_DPT::DPT11: {
			if (maxLen < 3) return 0;
			const DptDate& d = v.payload.date;
			out[0] = d.day & 0x1F;
			out[1] = d.month & 0x0F;
			out[2] = (uint8_t)(d.year % 100);       // 0..89 => 2000s, 90..99 => 1900s
			return 3;
		}

		case KNX_DPT::DPT12:
			if (maxLen < 4) return 0;
			putU32(out, v.payload.u32);
			return 4;

		case KNX_DPT::DPT13:
			if (maxLen < 4) return 0;
			putU32(out, (uint32_t)v.payload.i32);
			return 4;

		case KNX_DPT::DPT14: {
			if (maxLen < 4) return 0;
			uint32_t bits;
			std::memcpy(&bits, &v.payload.f, 4);    // IEEE-754, then emit big-endian
			putU32(out, bits);
			return 4;
		}

		case KNX_DPT::DPT19: {
			if (maxLen < 8) return 0;
			const DptDateTime& dt = v.payload.datetime;
			out[0] = (uint8_t)(dt.year - 1900);
			out[1] = dt.month & 0x0F;
			out[2] = dt.day & 0x1F;
			out[3] = (uint8_t)(((dt.weekday & 0x07) << 5) | (dt.hour & 0x1F));
			out[4] = dt.minute & 0x3F;
			out[5] = dt.second & 0x3F;
			out[6] = (uint8_t)((dt.faultFlag ? 0x80 : 0x00) | (dt.summerTime ? 0x01 : 0x00));
			out[7] = 0x00;
			return 8;
		}

		case KNX_DPT::DPT232: {
			if (maxLen < 3) return 0;
			const DptColor& c = v.payload.color;
			out[0] = c.r;
			out[1] = c.g;
			out[2] = c.b;
			return 3;
		}

		default:
			return 0;
	}
}

KnxValue decode(KNX_DPT dpt, const uint8_t* in, uint8_t len) {
	switch (dpt) {
		case KNX_DPT::DPT1:
			if (len < 1) break;
			return Dpt1((in[0] & 0x01) != 0);

		case KNX_DPT::DPT2:
			if (len < 1) break;
			return Dpt2(in[0] & 0x03);

		case KNX_DPT::DPT3:
			if (len < 1) break;
			return Dpt3((in[0] & 0x08) != 0, in[0] & 0x07);

		case KNX_DPT::DPT4:
			if (len < 1) break;
			return Dpt4((char)in[0]);

		case KNX_DPT::DPT5:
			if (len < 1) break;
			return Dpt5(in[0]);

		case KNX_DPT::DPT6:
			if (len < 1) break;
			return Dpt6((int8_t)in[0]);

		case KNX_DPT::DPT7:
			if (len < 2) break;
			return Dpt7(getU16(in));

		case KNX_DPT::DPT8:
			if (len < 2) break;
			return Dpt8((int16_t)getU16(in));

		case KNX_DPT::DPT9:
			if (len < 2) break;
			return Dpt9(decodeFloat16(getU16(in)));

		case KNX_DPT::DPT10: {
			if (len < 3) break;
			DptTime t;
			t.weekday = (in[0] >> 5) & 0x07;
			t.hour    = in[0] & 0x1F;
			t.minute  = in[1] & 0x3F;
			t.second  = in[2] & 0x3F;
			return Dpt10(t);
		}

		case KNX_DPT::DPT11: {
			if (len < 3) break;
			DptDate d;
			d.day   = in[0] & 0x1F;
			d.month = in[1] & 0x0F;
			uint8_t yy = in[2] & 0x7F;
			d.year  = (yy < 90) ? (uint16_t)(2000 + yy) : (uint16_t)(1900 + yy);
			return Dpt11(d);
		}

		case KNX_DPT::DPT12:
			if (len < 4) break;
			return Dpt12(getU32(in));

		case KNX_DPT::DPT13:
			if (len < 4) break;
			return Dpt13((int32_t)getU32(in));

		case KNX_DPT::DPT14: {
			if (len < 4) break;
			uint32_t bits = getU32(in);
			float f;
			std::memcpy(&f, &bits, 4);
			return Dpt14(f);
		}

		case KNX_DPT::DPT19: {
			if (len < 8) break;
			DptDateTime dt;
			dt.year       = (uint16_t)(1900 + in[0]);
			dt.month      = in[1] & 0x0F;
			dt.day        = in[2] & 0x1F;
			dt.weekday    = (in[3] >> 5) & 0x07;
			dt.hour       = in[3] & 0x1F;
			dt.minute     = in[4] & 0x3F;
			dt.second     = in[5] & 0x3F;
			dt.faultFlag  = (in[6] & 0x80) != 0;
			dt.summerTime = (in[6] & 0x01) != 0;
			return Dpt19(dt);
		}

		case KNX_DPT::DPT232:
			if (len < 3) break;
			return Dpt232(in[0], in[1], in[2]);

		default:
			break;
	}

	return KnxValue{}; // dpt == UNKNOWN on failure
}

} // namespace KnxCodec
