/**
 * @name test_object.cpp
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Host unit tests for the KnxObject base and intent classes, driven by a MockDriver
 *          and a real KNX coordinator. Targets the object layer's OWN logic (not the frame
 *          codec, tested separately): receive -> decode -> cache -> typed callback, write ->
 *          command-GA send + optimistic cache + L_Data.con propagation, status-fed toggle,
 *          command/status GA split, match selectivity, and the destructor unlink that keeps a
 *          scoped object from dangling in the registry. Run with: pio test -e native
*/

#include <unity.h>
#include <cstring>
#include "KnxCoordinator.h"
#include "KnxFrame.h"
#include "KnxCodec.h"
#include "KnxObject.h"
#include "KnxLighting.h"
#include "KnxClimate.h"
#include "KnxScalars.h"

//---- Test double: a driver that records sends and replays queued RX frames ----

class MockDriver : public IKnxDriver {
	public:
		bool    sendResult = true;      // simulated L_Data.con
		uint8_t lastSent[32] = {0};
		uint8_t lastLen = 0;
		int     sendCount = 0;

		static constexpr int MAXQ = 8;
		uint8_t rxq[MAXQ][32];
		uint8_t rxqLen[MAXQ] = {0};
		int     qHead = 0, qTail = 0;

		void queueFrame(const uint8_t* f, uint8_t n) {
			std::memcpy(rxq[qTail], f, n);
			rxqLen[qTail] = n;
			qTail = (qTail + 1) % MAXQ;
		}

		bool begin(void) override { return true; }
		bool reset(void) override { return true; }
		bool sendTelegram(const uint8_t* frame, uint8_t length) override {
			std::memcpy(lastSent, frame, length);
			lastLen = length;
			sendCount++;
			return sendResult;
		}
		bool poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) override {
			if (qHead == qTail) return false;
			uint8_t n = rxqLen[qHead];
			if (n > maxLen) { qHead = (qHead + 1) % MAXQ; return false; }
			std::memcpy(out, rxq[qHead], n);
			outLen = n;
			qHead = (qHead + 1) % MAXQ;
			return true;
		}
		bool faultPending(void) override { return false; }
};

static const PhysicalAddress SELF = { 1, 1, 5 };

// Decodes the bool a DPT1 telegram the mock last transmitted carries.
static bool lastSentBool(const MockDriver& mock) {
	ParsedTelegram tg;
	KnxFrame::parse(mock.lastSent, mock.lastLen, tg);
	return KnxCodec::decode(KNX_DPT::DPT1, &tg.inline6Data, 1).asBool();
}

//---- Callback capture (plain function pointers, so state lives in file-scope globals) ----
static int  g_lightCbCount = 0;
static bool g_lightCbState = false;
static void lightCb(bool state) { g_lightCbCount++; g_lightCbState = state; }

static int   g_tempCbCount = 0;
static float g_tempCbValue = 0.0f;
static void  tempCb(float c) { g_tempCbCount++; g_tempCbValue = c; }

static int     g_pctCbCount = 0;
static uint8_t g_pctCbValue = 0;
static void    pctCb(uint8_t p) { g_pctCbCount++; g_pctCbValue = p; }

void setUp(void) {
	g_lightCbCount = 0; g_lightCbState = false;
	g_tempCbCount = 0;  g_tempCbValue = 0.0f;
	g_pctCbCount = 0;   g_pctCbValue = 0;
}
void tearDown(void) {}

//---- receive() decodes with the object's own DPT, caches, and fires the typed callback ----
void test_light_receive_decodes_caches_fires(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });
	KnxLight light(knx, ga);
	light.onUpdate(lightCb);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);

	TEST_ASSERT_TRUE(knx.handleUART());
	TEST_ASSERT_EQUAL_INT(1, g_lightCbCount);
	TEST_ASSERT_TRUE(g_lightCbState);
	TEST_ASSERT_TRUE(light.isOn());       // cache updated from the bus
}

//---- write() sends to the COMMAND GA, caches optimistically, and returns the con result ----
void test_light_write_command_ga_and_con(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t cmd = packGroupAddress(GroupAddress{ 0, 1, 1 });
	uint16_t status = packGroupAddress(GroupAddress{ 0, 3, 0 });
	KnxLight light(knx, cmd, status);

	TEST_ASSERT_TRUE(light.on());
	TEST_ASSERT_EQUAL_INT(1, mock.sendCount);
	TEST_ASSERT_EQUAL_UINT8(0x01, mock.lastSent[3]);   // (main 0 << 3) | middle 1 = command GA hi
	TEST_ASSERT_EQUAL_UINT8(0x01, mock.lastSent[4]);   // sub 1
	TEST_ASSERT_TRUE(lastSentBool(mock));
	TEST_ASSERT_TRUE(light.isOn());                    // optimistic cache

	mock.sendResult = false;                           // driver reports a negative con
	TEST_ASSERT_FALSE(light.off());
}

//---- toggle() flips relative to the status-fed cache, not what we last sent ----
void test_toggle_tracks_status_cache(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t cmd = packGroupAddress(GroupAddress{ 0, 1, 1 });
	uint16_t status = packGroupAddress(GroupAddress{ 0, 3, 0 });
	KnxLight light(knx, cmd, status);

	// Bus reports the light is ON via the status GA.
	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, status, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);
	knx.handleUART();
	TEST_ASSERT_TRUE(light.isOn());

	// toggle() must now command OFF.
	TEST_ASSERT_TRUE(light.toggle());
	TEST_ASSERT_FALSE(lastSentBool(mock));
}

//---- Only telegrams on the object's status GA are decoded ----
void test_match_selectivity(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });
	uint16_t other = packGroupAddress(GroupAddress{ 0, 3, 1 });
	KnxLight light(knx, ga);
	light.onUpdate(lightCb);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, other, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);

	knx.handleUART();
	TEST_ASSERT_EQUAL_INT(0, g_lightCbCount);
}

//---- Destructor unlinks from the registry: a scoped object cannot dangle (PLAN §6) ----
void test_destructor_unlinks(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });

	{
		KnxLight scoped(knx, ga);
		scoped.onUpdate(lightCb);
	} // scoped destructs here -> must unlink itself

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);

	knx.handleUART();                       // must not dispatch to the dead object, must not crash
	TEST_ASSERT_EQUAL_INT(0, g_lightCbCount);
}

//---- Raw KnxObject exposes a generic KnxValue callback decoded with its DPT ----
void test_raw_object_generic_callback(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 4, 2 });
	KnxTemperature temp(knx, ga);
	temp.onUpdate(tempCb);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt9(21.5f), f, sizeof(f));
	mock.queueFrame(f, n);

	knx.handleUART();
	TEST_ASSERT_EQUAL_INT(1, g_tempCbCount);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, g_tempCbValue);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, temp.temperature());
}

//---- KnxPercent marshals the 0..100 % API to/from the DPT5 0..255 raw range ----
void test_percent_scaling_roundtrip(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 2, 5 });
	KnxPercent pct(knx, ga);
	pct.onUpdate(pctCb);

	// set(50 %) should transmit raw ~128.
	TEST_ASSERT_TRUE(pct.set(50));
	ParsedTelegram tg;
	KnxFrame::parse(mock.lastSent, mock.lastLen, tg);
	uint8_t raw = KnxCodec::decode(KNX_DPT::DPT5, tg.payload, tg.payloadLength).asU8();
	TEST_ASSERT_UINT8_WITHIN(1, 128, raw);

	// Receiving raw 255 should surface as 100 %.
	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt5(255), f, sizeof(f));
	mock.queueFrame(f, n);
	knx.handleUART();
	TEST_ASSERT_EQUAL_INT(1, g_pctCbCount);
	TEST_ASSERT_EQUAL_UINT8(100, g_pctCbValue);
	TEST_ASSERT_EQUAL_UINT8(100, pct.percent());
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_light_receive_decodes_caches_fires);
	RUN_TEST(test_light_write_command_ga_and_con);
	RUN_TEST(test_toggle_tracks_status_cache);
	RUN_TEST(test_match_selectivity);
	RUN_TEST(test_destructor_unlinks);
	RUN_TEST(test_raw_object_generic_callback);
	RUN_TEST(test_percent_scaling_roundtrip);
	return UNITY_END();
}
