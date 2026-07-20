/**
 * @name test_frame.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Host unit tests for KnxFrame (build/parse). Cases are built from the KNX
 *          standard-frame layout: known bytes, build->parse round-trips for inline-6 and
 *          multi-octet DPTs, checksum verification, and structural rejection.
 *          Run with: pio test -e native
*/

#include <unity.h>
#include "KnxFrame.h"
#include "KnxCodec.h"

void setUp(void) {}
void tearDown(void) {}

static const PhysicalAddress SRC = { 1, 1, 5 };   // 1.1.5
static uint8_t frame[KnxFrame::MAX_FRAME];

//---- Known byte layout for a DPT1 switch-on to 0/1/1 ----
void test_build_dpt1_layout(void) {
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 1, 1 });
	uint8_t len = KnxFrame::build(SRC, ga, Dpt1(true), frame, sizeof(frame));
	TEST_ASSERT_EQUAL_UINT8(9, len);                       // inline-6 frame + checksum
	TEST_ASSERT_EQUAL_UINT8(0xBC, frame[0]);               // control: std, not-repeated, Low priority
	TEST_ASSERT_EQUAL_UINT8((1 << 4) | 1, frame[1]);       // source area/line
	TEST_ASSERT_EQUAL_UINT8(5, frame[2]);                  // source device
	TEST_ASSERT_EQUAL_UINT8((0 << 3) | 1, frame[3]);       // target hi: main 0, middle 1 -> 0x01
	TEST_ASSERT_EQUAL_UINT8(1, frame[4]);                  // target lo: sub 1
	TEST_ASSERT_EQUAL_UINT8(0xE1, frame[5]);               // NPCI: group addr, RC 6, apduLen 2 -> 0xE1
	TEST_ASSERT_EQUAL_UINT8(0x81, frame[7]);               // APCI write + data 1
	TEST_ASSERT_EQUAL_UINT8(KnxFrame::checksum(frame, 8), frame[8]);
}

//---- Precise target-address encoding: 0/1/1 -> hi (main<<3|middle)=0x01, lo=sub=0x01 ----
void test_target_address_bytes(void) {
	uint16_t ga = packGroupAddress(GroupAddress{ 5, 3, 200 });
	KnxFrame::build(SRC, ga, Dpt1(true), frame, sizeof(frame));
	TEST_ASSERT_EQUAL_UINT8((5 << 3) | 3, frame[3]);
	TEST_ASSERT_EQUAL_UINT8(200, frame[4]);
}

//---- Round-trip: build then parse recovers source, target, type and value (inline-6) ----
void test_roundtrip_dpt3(void) {
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 0, 1 });
	uint8_t len = KnxFrame::build(SRC, ga, Dpt3(true, 5), frame, sizeof(frame));

	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, len, tg));
	TEST_ASSERT_TRUE(tg._isInline6);
	TEST_ASSERT_EQUAL_UINT8(1, tg.source.area);
	TEST_ASSERT_EQUAL_UINT8(1, tg.source.line);
	TEST_ASSERT_EQUAL_UINT8(5, tg.source.device);
	TEST_ASSERT_TRUE(gaEqual(GroupAddress{ 0, 0, 1 }, tg.target));
	TEST_ASSERT_EQUAL(GroupValueType::Write, tg.type);

	// Receiver-side decode with the known DPT.
	DptDim d = KnxCodec::decode(KnxDpt::DPT3, &tg.inline6Data, 1).asDim();
	TEST_ASSERT_TRUE(d.increase);
	TEST_ASSERT_EQUAL_UINT8(5, d.stepcode);
}

//---- Round-trip: multi-octet payload (DPT9) ----
void test_roundtrip_dpt9(void) {
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 4, 2 });
	uint8_t len = KnxFrame::build(SRC, ga, Dpt9(21.5f), frame, sizeof(frame));
	TEST_ASSERT_EQUAL_UINT8(11, len);   // 8 header/APDU + 2 payload + 1 checksum

	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, len, tg));
	TEST_ASSERT_FALSE(tg._isInline6);
	TEST_ASSERT_EQUAL_UINT8(2, tg.payloadLength);
	TEST_ASSERT_NOT_NULL(tg.payload);
	TEST_ASSERT_TRUE(gaEqual(GroupAddress{ 0, 4, 2 }, tg.target));

	float back = KnxCodec::decode(KnxDpt::DPT9, tg.payload, tg.payloadLength).asFloat();
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, back);
}

//---- Round-trip: largest v1 payload (DPT19, 8 octets) ----
void test_roundtrip_dpt19(void) {
	DptDateTime dt{};
	dt.year = 2026; dt.month = 7; dt.day = 16; dt.weekday = 4;
	dt.hour = 9; dt.minute = 5; dt.second = 30;
	uint16_t ga = packGroupAddress(GroupAddress{ 1, 2, 3 });
	uint8_t len = KnxFrame::build(SRC, ga, Dpt19(dt), frame, sizeof(frame));
	TEST_ASSERT_EQUAL_UINT8(17, len);   // 8 + 8 payload + 1 checksum

	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, len, tg));
	TEST_ASSERT_EQUAL_UINT8(8, tg.payloadLength);
	DptDateTime r = KnxCodec::decode(KnxDpt::DPT19, tg.payload, tg.payloadLength).asDateTime();
	TEST_ASSERT_EQUAL_UINT16(2026, r.year);
	TEST_ASSERT_EQUAL_UINT8(30, r.second);
}

//---- Checksum corruption is rejected ----
void test_parse_rejects_bad_checksum(void) {
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 1, 1 });
	uint8_t len = KnxFrame::build(SRC, ga, Dpt1(true), frame, sizeof(frame));
	frame[len - 1] ^= 0xFF;   // corrupt the checksum
	ParsedTelegram tg;
	TEST_ASSERT_FALSE(KnxFrame::parse(frame, len, tg));
}

//---- Structural rejection: too-short buffer ----
void test_parse_rejects_short(void) {
	uint8_t tiny[4] = { 0xBC, 0x11, 0x05, 0x00 };
	ParsedTelegram tg;
	TEST_ASSERT_FALSE(KnxFrame::parse(tiny, sizeof(tiny), tg));
}

//---- build fails cleanly on an unencodable value ----
void test_build_rejects_unknown_dpt(void) {
	KnxValue bad;   // dpt == UNKNOWN
	TEST_ASSERT_EQUAL_UINT8(0, KnxFrame::build(SRC, 0x0001, bad, frame, sizeof(frame)));
}

//---- parse verified independently of build(): hand-authored raw frames from the spec ----
// Frame skeleton (1.1.5 -> 0/1/1, inline-6): BC 11 05 01 01 E1 00 <apci> <checksum>.
// The APCI low byte selects the service: Write 0x80|d, Response 0x40|d, Read 0x00.

void test_parse_raw_groupvalue_write(void) {
	uint8_t raw[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE1, 0x00, 0x81, 0x37 };
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(raw, sizeof(raw), tg));
	TEST_ASSERT_EQUAL(GroupValueType::Write, tg.type);
	TEST_ASSERT_EQUAL_UINT8(1, tg.source.area);
	TEST_ASSERT_EQUAL_UINT8(1, tg.source.line);
	TEST_ASSERT_EQUAL_UINT8(5, tg.source.device);
	TEST_ASSERT_TRUE(gaEqual(GroupAddress{ 0, 1, 1 }, tg.target));
	TEST_ASSERT_TRUE(tg._isInline6);
	TEST_ASSERT_EQUAL_UINT8(1, tg.inline6Data);
	TEST_ASSERT_TRUE(KnxCodec::decode(KnxDpt::DPT1, &tg.inline6Data, 1).asBool());
}

void test_parse_raw_groupvalue_response(void) {
	// Same frame but APCI 0x41 = GroupValueResponse + data 1 (never emitted by build()).
	uint8_t raw[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE1, 0x00, 0x41, 0xF7 };
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(raw, sizeof(raw), tg));
	TEST_ASSERT_EQUAL(GroupValueType::Response, tg.type);
	TEST_ASSERT_EQUAL_UINT8(1, tg.inline6Data);
}

void test_parse_raw_groupvalue_read(void) {
	// APCI 0x00 = GroupValueRead, no data.
	uint8_t raw[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE1, 0x00, 0x00, 0xB6 };
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(raw, sizeof(raw), tg));
	TEST_ASSERT_EQUAL(GroupValueType::Read, tg.type);
	TEST_ASSERT_EQUAL_UINT8(0, tg.inline6Data);
}

//---- Address type: octet 5 bit 7 selects group vs individual addressing (TP1 §2.2.4.4) ----
// Same skeleton twice, differing only in that bit; note the destination octets are identical,
// so nothing but AT distinguishes "group 0/1/1" from "device 0.1.1".
void test_parse_address_type_group(void) {
	uint8_t raw[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE1, 0x00, 0x81, 0x37 };
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(raw, sizeof(raw), tg));
	TEST_ASSERT_EQUAL_INT((int)KnxAddressType::Group, (int)tg.addressType);
	TEST_ASSERT_EQUAL_INT((int)KnxTpci::DataGroup, (int)tg.tpci);
	TEST_ASSERT_EQUAL_UINT8(1, tg.target.middle);
	TEST_ASSERT_EQUAL_UINT8(1, tg.target.sub);
}

void test_parse_address_type_individual(void) {
	// AT cleared (0xE1 -> 0x61) and the checksum recomputed over the changed octet.
	uint8_t raw[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0x61, 0x00, 0x81, 0x00 };
	raw[8] = KnxFrame::checksum(raw, 8);
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(raw, sizeof(raw), tg));
	TEST_ASSERT_EQUAL_INT((int)KnxAddressType::Individual, (int)tg.addressType);
	TEST_ASSERT_EQUAL_INT((int)KnxTpci::DataIndividual, (int)tg.tpci);
	TEST_ASSERT_EQUAL_UINT8(0, tg.individualTarget.area);
	TEST_ASSERT_EQUAL_UINT8(1, tg.individualTarget.line);
	TEST_ASSERT_EQUAL_UINT8(1, tg.individualTarget.device);
}

//---- Every transport control PDU round-trips through build -> parse ----
void test_control_pdu_roundtrip(void) {
	struct Case { KnxTpci tpci; uint8_t seq; };
	const Case cases[] = {
		{ KnxTpci::Connect,    0 },
		{ KnxTpci::Disconnect, 0 },
		{ KnxTpci::Ack,        5 },
		{ KnxTpci::Nak,        3 },
	};

	for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		uint8_t len = KnxFrame::buildControl(SRC, PhysicalAddress{ 1, 1, 7 },
		                                     cases[i].tpci, cases[i].seq,
		                                     frame, sizeof(frame));
		TEST_ASSERT_EQUAL_UINT8(8, len);   // 6 header + TPCI + checksum

		ParsedTelegram tg;
		TEST_ASSERT_TRUE(KnxFrame::parse(frame, len, tg));
		TEST_ASSERT_EQUAL_INT((int)KnxAddressType::Individual, (int)tg.addressType);
		TEST_ASSERT_EQUAL_INT((int)cases[i].tpci, (int)tg.tpci);
		TEST_ASSERT_FALSE(tg.hasApci);     // control PDUs carry no application service
		if (cases[i].tpci == KnxTpci::Ack || cases[i].tpci == KnxTpci::Nak) {
			TEST_ASSERT_EQUAL_UINT8(cases[i].seq, tg.sequenceNumber);
		}
	}
}

//---- A point-to-point data telegram round-trips with its APCI and payload ----
void test_individual_apci_roundtrip(void) {
	const uint8_t payload[] = { 0x12, 0x34 };
	uint8_t len = KnxFrame::buildIndividual(SRC, PhysicalAddress{ 1, 1, 7 }, 0x300,
	                                        payload, sizeof(payload), frame, sizeof(frame));
	ParsedTelegram tg;
	TEST_ASSERT_TRUE(KnxFrame::parse(frame, len, tg));
	TEST_ASSERT_EQUAL_INT((int)KnxTpci::DataIndividual, (int)tg.tpci);
	TEST_ASSERT_TRUE(tg.hasApci);
	TEST_ASSERT_EQUAL_UINT16(0x300, tg.apci);
	TEST_ASSERT_EQUAL_UINT8(sizeof(payload), tg.payloadLength);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, tg.payload, sizeof(payload));
	TEST_ASSERT_EQUAL_UINT8(1, tg.individualTarget.area);
	TEST_ASSERT_EQUAL_UINT8(7, tg.individualTarget.device);
}

//---- buildControl refuses a TPCI that is not a control PDU ----
void test_build_control_rejects_data_tpci(void) {
	TEST_ASSERT_EQUAL_UINT8(0, KnxFrame::buildControl(SRC, PhysicalAddress{ 1, 1, 7 },
	                                                  KnxTpci::DataGroup, 0,
	                                                  frame, sizeof(frame)));
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_build_dpt1_layout);
	RUN_TEST(test_target_address_bytes);
	RUN_TEST(test_roundtrip_dpt3);
	RUN_TEST(test_roundtrip_dpt9);
	RUN_TEST(test_roundtrip_dpt19);
	RUN_TEST(test_parse_rejects_bad_checksum);
	RUN_TEST(test_parse_rejects_short);
	RUN_TEST(test_build_rejects_unknown_dpt);
	RUN_TEST(test_parse_raw_groupvalue_write);
	RUN_TEST(test_parse_raw_groupvalue_response);
	RUN_TEST(test_parse_raw_groupvalue_read);
	RUN_TEST(test_parse_address_type_group);
	RUN_TEST(test_parse_address_type_individual);
	RUN_TEST(test_control_pdu_roundtrip);
	RUN_TEST(test_individual_apci_roundtrip);
	RUN_TEST(test_build_control_rejects_data_tpci);
	return UNITY_END();
}
