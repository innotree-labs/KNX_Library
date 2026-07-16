/**
 * @name test_reassembler.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Host unit tests for KnxReassembler — the byte-stream -> complete-frame state
 *          machine. Cases: clean frame, leading noise, split delivery, back-to-back frames,
 *          and invalid-length rejection with recovery. Run with: pio test -e native
*/

#include <unity.h>
#include "KnxReassembler.h"
#include "KnxFrame.h"

void setUp(void) {}
void tearDown(void) {}

// A valid inline-6 GroupValueWrite frame (1.1.5 -> 0/1/1, DPT1 on): 9 bytes incl. checksum.
static const uint8_t FRAME[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE1, 0x00, 0x81, 0x37 };

// Feeds bytes; returns number of completed frames, and copies the last one out.
static int feedAll(KnxReassembler& r, const uint8_t* bytes, uint8_t n,
                   uint8_t* lastFrame, uint8_t* lastLen) {
	int completions = 0;
	for (uint8_t i = 0; i < n; i++) {
		if (r.feed(bytes[i])) {
			completions++;
			if (lastFrame) {
				*lastLen = r.length();
				for (uint8_t j = 0; j < r.length(); j++) lastFrame[j] = r.frame()[j];
			}
		}
	}
	return completions;
}

//---- A clean frame completes exactly once with the right bytes ----
void test_clean_frame(void) {
	KnxReassembler r;
	uint8_t out[32]; uint8_t len = 0;
	int n = feedAll(r, FRAME, sizeof(FRAME), out, &len);
	TEST_ASSERT_EQUAL_INT(1, n);
	TEST_ASSERT_EQUAL_UINT8(sizeof(FRAME), len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(FRAME, out, sizeof(FRAME));
}

//---- Bytes before the 0xBC start byte are ignored ----
void test_leading_noise(void) {
	KnxReassembler r;
	const uint8_t noise[] = { 0x00, 0xFF, 0x0B, 0xAB };
	uint8_t out[32]; uint8_t len = 0;
	TEST_ASSERT_EQUAL_INT(0, feedAll(r, noise, sizeof(noise), nullptr, nullptr));
	TEST_ASSERT_EQUAL_INT(1, feedAll(r, FRAME, sizeof(FRAME), out, &len));
	TEST_ASSERT_EQUAL_UINT8_ARRAY(FRAME, out, sizeof(FRAME));
}

//---- Partial input yields no completion until the final byte ----
void test_split_delivery(void) {
	KnxReassembler r;
	for (uint8_t i = 0; i < sizeof(FRAME) - 1; i++) {
		TEST_ASSERT_FALSE(r.feed(FRAME[i]));
	}
	TEST_ASSERT_TRUE(r.feed(FRAME[sizeof(FRAME) - 1]));
	TEST_ASSERT_EQUAL_UINT8(sizeof(FRAME), r.length());
}

//---- Two frames back-to-back both complete ----
void test_back_to_back(void) {
	KnxReassembler r;
	uint8_t stream[2 * sizeof(FRAME)];
	for (uint8_t i = 0; i < sizeof(FRAME); i++) {
		stream[i] = FRAME[i];
		stream[sizeof(FRAME) + i] = FRAME[i];
	}
	TEST_ASSERT_EQUAL_INT(2, feedAll(r, stream, sizeof(stream), nullptr, nullptr));
}

//---- A multi-octet frame (DPT9) reassembles to its full length ----
void test_multibyte_frame(void) {
	uint8_t built[KnxFrame::MAX_FRAME];
	uint8_t blen = KnxFrame::build(PhysicalAddress{ 1, 1, 5 },
	                               packGroupAddress(GroupAddress{ 0, 4, 2 }),
	                               Dpt9(21.5f), built, sizeof(built));
	KnxReassembler r;
	uint8_t out[32]; uint8_t len = 0;
	TEST_ASSERT_EQUAL_INT(1, feedAll(r, built, blen, out, &len));
	TEST_ASSERT_EQUAL_UINT8(blen, len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(built, out, blen);
}

//---- An invalid NPCI length (apduLen 1 -> frame < 9) is rejected, then recovers ----
void test_invalid_length_recovers(void) {
	KnxReassembler r;
	// buffer[5]=0xE0 -> apduLen 1 -> expectedLen 8 (< MIN_FRAME) -> reset on the 6th byte.
	const uint8_t bad[] = { 0xBC, 0x11, 0x05, 0x01, 0x01, 0xE0 };
	TEST_ASSERT_EQUAL_INT(0, feedAll(r, bad, sizeof(bad), nullptr, nullptr));
	// A valid frame afterwards still reassembles.
	uint8_t out[32]; uint8_t len = 0;
	TEST_ASSERT_EQUAL_INT(1, feedAll(r, FRAME, sizeof(FRAME), out, &len));
	TEST_ASSERT_EQUAL_UINT8_ARRAY(FRAME, out, sizeof(FRAME));
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_clean_frame);
	RUN_TEST(test_leading_noise);
	RUN_TEST(test_split_delivery);
	RUN_TEST(test_back_to_back);
	RUN_TEST(test_multibyte_frame);
	RUN_TEST(test_invalid_length_recovers);
	return UNITY_END();
}
