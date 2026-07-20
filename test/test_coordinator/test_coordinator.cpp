/**
 * @name test_coordinator.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Host unit tests for the KNX coordinator, driven by a MockDriver and TestReceiver.
 *          Targets the coordinator's OWN logic (not KnxFrame, which is tested separately):
 *          L_Data.con propagation, build-failure short-circuit, the RX drain loop, receiver
 *          match selectivity, and registry idempotency. Run with: pio test -e native
*/

#include <unity.h>
#include <cstring>
#include "KnxCoordinator.h"
#include "KnxFrame.h"
#include "KnxCodec.h"

//---- Test doubles ----

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
};

class TestReceiver : public IKnxReceiver {
	public:
		uint16_t listenGa;
		int      receiveCount = 0;
		bool     lastBool = false;

		explicit TestReceiver(uint16_t ga) : listenGa(ga) {}
		bool matches(uint16_t ga) const override { return ga == listenGa; }
		void receive(const ParsedTelegram& tg) override {
			receiveCount++;
			lastBool = KnxCodec::decode(KnxDpt::DPT1, &tg.inline6Data, 1).asBool();
		}
};

class TestDeviceHandler : public IKnxDeviceHandler {
	public:
		int      receiveCount = 0;
		KnxTpci  lastTpci = KnxTpci::Unknown;
		uint16_t lastApci = 0;
		bool     lastHadApci = false;

		void receiveIndividual(const ParsedTelegram& tg) override {
			receiveCount++;
			lastTpci    = tg.tpci;
			lastApci    = tg.apci;
			lastHadApci = tg.hasApci;
		}
};

static const PhysicalAddress SELF = { 1, 1, 5 };

void setUp(void) {}
void tearDown(void) {}

//---- send() returns the driver's real L_Data.con result (PLAN §9) ----
void test_send_con_true(void) {
	MockDriver mock; mock.sendResult = true;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 1, 1 });
	TEST_ASSERT_TRUE(knx.send(ga, Dpt1(true)));
	TEST_ASSERT_EQUAL_INT(1, mock.sendCount);
	// Confirm it framed THIS address/value without recomputing the whole frame.
	TEST_ASSERT_EQUAL_UINT8(0xBC, mock.lastSent[0]);
	TEST_ASSERT_EQUAL_UINT8(0x01, mock.lastSent[3]);  // (main 0 << 3) | middle 1
	TEST_ASSERT_EQUAL_UINT8(0x01, mock.lastSent[4]);  // sub 1
}

void test_send_con_false(void) {
	MockDriver mock; mock.sendResult = false;
	KnxCoordinator knx(&mock, SELF);
	TEST_ASSERT_FALSE(knx.send(packGroupAddress(GroupAddress{ 0, 1, 1 }), Dpt1(true)));
	TEST_ASSERT_EQUAL_INT(1, mock.sendCount);
}

//---- An unencodable value short-circuits: driver is never called ----
void test_send_build_failure(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	KnxValue bad;   // dpt == UNKNOWN
	TEST_ASSERT_FALSE(knx.send(0x0001, bad));
	TEST_ASSERT_EQUAL_INT(0, mock.sendCount);
}

//---- One queued frame dispatches to the matching receiver with the decoded value ----
void test_dispatch_single(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });
	TestReceiver rx(ga);
	knx.registerReceiver(&rx);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);

	TEST_ASSERT_TRUE(knx.loop());
	TEST_ASSERT_EQUAL_INT(1, rx.receiveCount);
	TEST_ASSERT_TRUE(rx.lastBool);
}

//---- Two queued frames drain in a single loop() call ----
void test_drain_loop_two_frames(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });
	TestReceiver rx(ga);
	knx.registerReceiver(&rx);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);
	mock.queueFrame(f, n);

	TEST_ASSERT_TRUE(knx.loop());
	TEST_ASSERT_EQUAL_INT(2, rx.receiveCount);
}

//---- Only the receiver whose GA matches is fired ----
void test_match_selectivity(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t gaA = packGroupAddress(GroupAddress{ 0, 3, 0 });
	uint16_t gaB = packGroupAddress(GroupAddress{ 0, 3, 1 });
	TestReceiver rxA(gaA), rxB(gaB);
	knx.registerReceiver(&rxA);
	knx.registerReceiver(&rxB);

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, gaA, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);

	knx.loop();
	TEST_ASSERT_EQUAL_INT(1, rxA.receiveCount);
	TEST_ASSERT_EQUAL_INT(0, rxB.receiveCount);
}

//---- Registry is idempotent and supports unregister ----
void test_registry_idempotent_and_unregister(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	uint16_t ga = packGroupAddress(GroupAddress{ 0, 3, 0 });
	TestReceiver rx(ga);
	knx.registerReceiver(&rx);
	knx.registerReceiver(&rx);   // double-register must not double-link

	uint8_t f[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::build(SELF, ga, Dpt1(true), f, sizeof(f));
	mock.queueFrame(f, n);
	knx.loop();
	TEST_ASSERT_EQUAL_INT(1, rx.receiveCount);   // fired once, not twice

	knx.unregisterReceiver(&rx);
	mock.queueFrame(f, n);
	knx.loop();
	TEST_ASSERT_EQUAL_INT(1, rx.receiveCount);   // no longer registered
}

//---- A telegram addressed to OUR individual address reaches the device handler ----
void test_individual_to_us_reaches_handler(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	TestDeviceHandler handler;
	knx.setDeviceHandler(&handler);

	// A_DeviceDescriptor_Read (APCI 0x300) from 1.1.9 to us — an ETS-style ping.
	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::buildIndividual(PhysicalAddress{ 1, 1, 9 }, SELF,
	                                      0x300, nullptr, 0, frame, sizeof(frame));
	mock.queueFrame(frame, n);

	TEST_ASSERT_TRUE(knx.loop());
	TEST_ASSERT_EQUAL_INT(1, handler.receiveCount);
	TEST_ASSERT_TRUE(handler.lastHadApci);
	TEST_ASSERT_EQUAL_UINT16(0x300, handler.lastApci);
	TEST_ASSERT_EQUAL_INT((int)KnxTpci::DataIndividual, (int)handler.lastTpci);
}

//---- The same telegram addressed to a different device is foreign traffic ----
void test_individual_to_other_device_ignored(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	TestDeviceHandler handler;
	knx.setDeviceHandler(&handler);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::buildIndividual(PhysicalAddress{ 1, 1, 9 },
	                                      PhysicalAddress{ 1, 1, 7 },
	                                      0x300, nullptr, 0, frame, sizeof(frame));
	mock.queueFrame(frame, n);

	TEST_ASSERT_TRUE(knx.loop());          // consumed...
	TEST_ASSERT_EQUAL_INT(0, handler.receiveCount);   // ...but not delivered
}

//---- Point-to-point telegrams must never leak into the group receiver registry ----
// The group and individual address spaces overlap numerically: 1.1.5 packs to the same
// uint16_t as group address 2/1/5, so routing by address type is what keeps them apart.
void test_individual_never_reaches_group_registry(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	TestReceiver receiver(packPhysicalAddress(SELF));   // same packed value as our address
	knx.registerReceiver(&receiver);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::buildIndividual(PhysicalAddress{ 1, 1, 9 }, SELF,
	                                      0x300, nullptr, 0, frame, sizeof(frame));
	mock.queueFrame(frame, n);

	TEST_ASSERT_TRUE(knx.loop());
	TEST_ASSERT_EQUAL_INT(0, receiver.receiveCount);
}

//---- A TPCI-only control PDU reaches the handler with no APCI ----
void test_control_pdu_reaches_handler(void) {
	MockDriver mock;
	KnxCoordinator knx(&mock, SELF);
	TestDeviceHandler handler;
	knx.setDeviceHandler(&handler);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t n = KnxFrame::buildControl(PhysicalAddress{ 1, 1, 9 }, SELF,
	                                   KnxTpci::Connect, 0, frame, sizeof(frame));
	mock.queueFrame(frame, n);

	TEST_ASSERT_TRUE(knx.loop());
	TEST_ASSERT_EQUAL_INT(1, handler.receiveCount);
	TEST_ASSERT_FALSE(handler.lastHadApci);
	TEST_ASSERT_EQUAL_INT((int)KnxTpci::Connect, (int)handler.lastTpci);
}

int main(int, char**) {
	UNITY_BEGIN();
	RUN_TEST(test_send_con_true);
	RUN_TEST(test_send_con_false);
	RUN_TEST(test_send_build_failure);
	RUN_TEST(test_dispatch_single);
	RUN_TEST(test_drain_loop_two_frames);
	RUN_TEST(test_match_selectivity);
	RUN_TEST(test_registry_idempotent_and_unregister);
	RUN_TEST(test_individual_to_us_reaches_handler);
	RUN_TEST(test_individual_to_other_device_ignored);
	RUN_TEST(test_individual_never_reaches_group_registry);
	RUN_TEST(test_control_pdu_reaches_handler);
	return UNITY_END();
}
