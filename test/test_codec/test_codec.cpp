/**
 * @name test_codec.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Host unit tests for the KNX_Value codec (KnxCodec) and value type (KnxValue).
 *          The cases are built from the DPT specifications — expected wire bytes and
 *          round-trips — so a wrong implementation fails instead of being ratified.
 *          Run with: pio test -e native
*/

#include <unity.h>
#include "KnxCodec.h"
#include "KnxEnums.h"
#include "KnxAddress.h"

void setUp(void) {}
void tearDown(void) {}

//---- Helpers ----
static uint8_t buf[KnxCodec::MAX_PAYLOAD];

//---- DimmIncrement enum: the DPT3.007 stepcode table that previously collided ----
void test_dimincrement_enum_fixed(void) {
	// Stop and the finest step must be distinct; ordering runs 100 % (1) -> 1.5 % (7).
	TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)DimmIncrement::Stop);
	TEST_ASSERT_EQUAL_UINT8(1, (uint8_t)DimmIncrement::Percent_100);
	TEST_ASSERT_EQUAL_UINT8(2, (uint8_t)DimmIncrement::Percent_50);
	TEST_ASSERT_EQUAL_UINT8(3, (uint8_t)DimmIncrement::Percent_25);
	TEST_ASSERT_EQUAL_UINT8(4, (uint8_t)DimmIncrement::Percent_12_5);
	TEST_ASSERT_EQUAL_UINT8(5, (uint8_t)DimmIncrement::Percent_6);
	TEST_ASSERT_EQUAL_UINT8(6, (uint8_t)DimmIncrement::Percent_3);
	TEST_ASSERT_EQUAL_UINT8(7, (uint8_t)DimmIncrement::Percent_1_5);
}

//---- inline-6 classification ----
void test_isinline6(void) {
	TEST_ASSERT_TRUE(KnxCodec::isInline6(KNX_DPT::DPT1));
	TEST_ASSERT_TRUE(KnxCodec::isInline6(KNX_DPT::DPT2));
	TEST_ASSERT_TRUE(KnxCodec::isInline6(KNX_DPT::DPT3));
	TEST_ASSERT_FALSE(KnxCodec::isInline6(KNX_DPT::DPT5));
	TEST_ASSERT_FALSE(KnxCodec::isInline6(KNX_DPT::DPT9));
}

//---- DPT1 ----
void test_dpt1(void) {
	TEST_ASSERT_EQUAL_UINT8(1, KnxCodec::encode(Dpt1(true), buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_UINT8(0x01, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(1, KnxCodec::encode(Dpt1(false), buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_UINT8(0x00, buf[0]);

	uint8_t on = 0x01;
	TEST_ASSERT_TRUE(KnxCodec::decode(KNX_DPT::DPT1, &on, 1).asBool());
	uint8_t off = 0x00;
	TEST_ASSERT_FALSE(KnxCodec::decode(KNX_DPT::DPT1, &off, 1).asBool());
}

//---- DPT3: direction bit (0x08) + 3-bit stepcode ----
void test_dpt3(void) {
	TEST_ASSERT_EQUAL_UINT8(1, KnxCodec::encode(Dpt3(true, 1), buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_UINT8(0x09, buf[0]);          // increase + stepcode 1
	KnxCodec::encode(Dpt3(false, 7), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x07, buf[0]);          // darker + stepcode 7
	KnxCodec::encode(Dpt3(false, 0), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x00, buf[0]);          // stop

	uint8_t wire = 0x09;
	DptDim d = KnxCodec::decode(KNX_DPT::DPT3, &wire, 1).asDim();
	TEST_ASSERT_TRUE(d.increase);
	TEST_ASSERT_EQUAL_UINT8(1, d.stepcode);
}

//---- DPT5: unsigned 0..255, boundaries ----
void test_dpt5(void) {
	KnxCodec::encode(Dpt5(0), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x00, buf[0]);
	KnxCodec::encode(Dpt5(255), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0xFF, buf[0]);
	uint8_t w = 200;
	TEST_ASSERT_EQUAL_UINT8(200, KnxCodec::decode(KNX_DPT::DPT5, &w, 1).asU8());
}

//---- DPT6: signed 8-bit ----
void test_dpt6(void) {
	KnxCodec::encode(Dpt6(-1), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0xFF, buf[0]);
	uint8_t w = 0x80;
	TEST_ASSERT_EQUAL_INT8(-128, KnxCodec::decode(KNX_DPT::DPT6, &w, 1).asI8());
}

//---- DPT7 / DPT8: 16-bit big-endian ----
void test_dpt7_dpt8_bigendian(void) {
	KnxCodec::encode(Dpt7(0x1234), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x12, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x34, buf[1]);
	TEST_ASSERT_EQUAL_UINT16(0x1234, KnxCodec::decode(KNX_DPT::DPT7, buf, 2).asU16());

	KnxCodec::encode(Dpt8(-2), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0xFF, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0xFE, buf[1]);
	TEST_ASSERT_EQUAL_INT16(-2, KnxCodec::decode(KNX_DPT::DPT8, buf, 2).asI16());
}

//---- DPT9: 2-octet float ----
void test_dpt9_known_bytes(void) {
	// 21.5 -> mantissa 2150, halved once (exp 1) -> 1075 -> word 0x0C33
	KnxCodec::encode(Dpt9(21.5f), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x0C, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x33, buf[1]);
}

void test_dpt9_roundtrip(void) {
	// Small-magnitude values sit at exponent 0 (0.01 resolution).
	const float samples[] = { 0.0f, 21.5f, -5.0f, 23.44f, -20.48f, 20.47f };
	for (float s : samples) {
		KnxCodec::encode(Dpt9(s), buf, sizeof(buf));
		float back = KnxCodec::decode(KNX_DPT::DPT9, buf, 2).asFloat();
		TEST_ASSERT_FLOAT_WITHIN(0.01f, s, back);
	}
}

void test_dpt9_large_magnitude_resolution(void) {
	// DPT9 is a lossy 2-octet float: at large magnitude the exponent grows and the step
	// coarsens (0.01 * 2^E). -273.0 is not representable; the nearest value is -272.96.
	KnxCodec::encode(Dpt9(-273.0f), buf, sizeof(buf));
	float back = KnxCodec::decode(KNX_DPT::DPT9, buf, 2).asFloat();
	TEST_ASSERT_FLOAT_WITHIN(0.16f, -273.0f, back);   // resolution at exponent 4
	TEST_ASSERT_FLOAT_WITHIN(0.001f, -272.96f, back); // exact representable value
}

//---- DPT12 / DPT13: 32-bit big-endian ----
void test_dpt12_dpt13_bigendian(void) {
	KnxCodec::encode(Dpt12(0xDEADBEEF), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0xDE, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0xAD, buf[1]);
	TEST_ASSERT_EQUAL_UINT8(0xBE, buf[2]);
	TEST_ASSERT_EQUAL_UINT8(0xEF, buf[3]);
	TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, KnxCodec::decode(KNX_DPT::DPT12, buf, 4).asU32());

	KnxCodec::encode(Dpt13(-1), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT32(-1, KnxCodec::decode(KNX_DPT::DPT13, buf, 4).asI32());
}

//---- DPT14: IEEE-754 big-endian ----
void test_dpt14(void) {
	KnxCodec::encode(Dpt14(1.0f), buf, sizeof(buf));      // 0x3F800000
	TEST_ASSERT_EQUAL_UINT8(0x3F, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x80, buf[1]);
	TEST_ASSERT_EQUAL_UINT8(0x00, buf[2]);
	TEST_ASSERT_EQUAL_UINT8(0x00, buf[3]);
	TEST_ASSERT_EQUAL_FLOAT(1.0f, KnxCodec::decode(KNX_DPT::DPT14, buf, 4).asFloat());
	TEST_ASSERT_EQUAL_FLOAT(-3.14159f, KnxCodec::decode(KNX_DPT::DPT14,
		(KnxCodec::encode(Dpt14(-3.14159f), buf, sizeof(buf)), buf), 4).asFloat());
}

//---- DPT10: time ----
void test_dpt10(void) {
	DptTime t{ 3, 14, 30, 45 };                          // Wed 14:30:45
	KnxCodec::encode(Dpt10(t), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8((3 << 5) | 14, buf[0]);
	DptTime r = KnxCodec::decode(KNX_DPT::DPT10, buf, 3).asTime();
	TEST_ASSERT_EQUAL_UINT8(3, r.weekday);
	TEST_ASSERT_EQUAL_UINT8(14, r.hour);
	TEST_ASSERT_EQUAL_UINT8(30, r.minute);
	TEST_ASSERT_EQUAL_UINT8(45, r.second);
}

//---- DPT11: date, incl. 1990s/2000s year mapping ----
void test_dpt11_year_mapping(void) {
	DptDate d1{ 16, 7, 2026 };
	KnxCodec::encode(Dpt11(d1), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(26, buf[2]);
	TEST_ASSERT_EQUAL_UINT16(2026, KnxCodec::decode(KNX_DPT::DPT11, buf, 3).asDate().year);

	DptDate d2{ 1, 1, 1995 };
	KnxCodec::encode(Dpt11(d2), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(95, buf[2]);
	TEST_ASSERT_EQUAL_UINT16(1995, KnxCodec::decode(KNX_DPT::DPT11, buf, 3).asDate().year);
}

//---- DPT19: datetime round-trip ----
void test_dpt19(void) {
	DptDateTime dt{};
	dt.year = 2026; dt.month = 7; dt.day = 16; dt.weekday = 4;
	dt.hour = 9; dt.minute = 5; dt.second = 30;
	dt.summerTime = true; dt.faultFlag = false;
	TEST_ASSERT_EQUAL_UINT8(8, KnxCodec::encode(Dpt19(dt), buf, sizeof(buf)));
	DptDateTime r = KnxCodec::decode(KNX_DPT::DPT19, buf, 8).asDateTime();
	TEST_ASSERT_EQUAL_UINT16(2026, r.year);
	TEST_ASSERT_EQUAL_UINT8(7, r.month);
	TEST_ASSERT_EQUAL_UINT8(16, r.day);
	TEST_ASSERT_EQUAL_UINT8(4, r.weekday);
	TEST_ASSERT_EQUAL_UINT8(9, r.hour);
	TEST_ASSERT_EQUAL_UINT8(5, r.minute);
	TEST_ASSERT_EQUAL_UINT8(30, r.second);
	TEST_ASSERT_TRUE(r.summerTime);
	TEST_ASSERT_FALSE(r.faultFlag);
}

//---- DPT232: RGB ----
void test_dpt232(void) {
	KnxCodec::encode(Dpt232(0x11, 0x22, 0x33), buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8(0x11, buf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x22, buf[1]);
	TEST_ASSERT_EQUAL_UINT8(0x33, buf[2]);
	DptColor c = KnxCodec::decode(KNX_DPT::DPT232, buf, 3).asColor();
	TEST_ASSERT_EQUAL_UINT8(0x11, c.r);
	TEST_ASSERT_EQUAL_UINT8(0x22, c.g);
	TEST_ASSERT_EQUAL_UINT8(0x33, c.b);
}

//---- Failure paths: too-small buffer, short payload, unknown DPT ----
void test_failure_paths(void) {
	uint8_t tiny[1];
	TEST_ASSERT_EQUAL_UINT8(0, KnxCodec::encode(Dpt9(1.0f), tiny, 1));   // needs 2
	uint8_t one = 0x00;
	TEST_ASSERT_FALSE(KnxCodec::decode(KNX_DPT::DPT9, &one, 1).isValid()); // needs 2
	TEST_ASSERT_FALSE(KnxCodec::decode(KNX_DPT::UNKNOWN, &one, 1).isValid());
}

//---- Group address packing: hot-path currency for every send and RX match ----
void test_ga_pack_roundtrip(void) {
	const GroupAddress cases[] = {
		{ 0, 0, 0 },        // edge: all-zero
		{ 31, 7, 255 },     // edge: all-max (5/3/8 bits)
		{ 0, 3, 0 },        // 0/3/0 — the status GA used in the app
		{ 18, 5, 200 },     // arbitrary mid value
	};
	for (const GroupAddress& ga : cases) {
		uint16_t packed = packGroupAddress(ga);
		GroupAddress back = unpackGroupAddress(packed);
		TEST_ASSERT_TRUE(gaEqual(ga, back));
	}
}

void test_ga_pack_bit_layout(void) {
	// main[5] << 11 | middle[3] << 8 | sub[8]
	TEST_ASSERT_EQUAL_UINT16(0x0000, packGroupAddress(GroupAddress{ 0, 0, 0 }));
	TEST_ASSERT_EQUAL_UINT16(0xFFFF, packGroupAddress(GroupAddress{ 31, 7, 255 }));
	TEST_ASSERT_EQUAL_UINT16((18u << 11) | (5u << 8) | 200u, packGroupAddress(GroupAddress{ 18, 5, 200 }));
}

void test_ga_equal(void) {
	TEST_ASSERT_TRUE(gaEqual(GroupAddress{ 0, 3, 0 }, GroupAddress{ 0, 3, 0 }));
	TEST_ASSERT_FALSE(gaEqual(GroupAddress{ 0, 3, 0 }, GroupAddress{ 0, 3, 1 }));
	TEST_ASSERT_FALSE(gaEqual(GroupAddress{ 0, 3, 0 }, GroupAddress{ 1, 3, 0 }));
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_dimincrement_enum_fixed);
	RUN_TEST(test_isinline6);
	RUN_TEST(test_dpt1);
	RUN_TEST(test_dpt3);
	RUN_TEST(test_dpt5);
	RUN_TEST(test_dpt6);
	RUN_TEST(test_dpt7_dpt8_bigendian);
	RUN_TEST(test_dpt9_known_bytes);
	RUN_TEST(test_dpt9_roundtrip);
	RUN_TEST(test_dpt9_large_magnitude_resolution);
	RUN_TEST(test_dpt12_dpt13_bigendian);
	RUN_TEST(test_dpt14);
	RUN_TEST(test_dpt10);
	RUN_TEST(test_dpt11_year_mapping);
	RUN_TEST(test_dpt19);
	RUN_TEST(test_dpt232);
	RUN_TEST(test_failure_paths);
	RUN_TEST(test_ga_pack_roundtrip);
	RUN_TEST(test_ga_pack_bit_layout);
	RUN_TEST(test_ga_equal);
	return UNITY_END();
}
