/**
 * @name KnxCoordinator.cpp
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details See KnxCoordinator.h.
*/

//----Libraries----
#include "KnxCoordinator.h"

//---- Sending ----

bool KnxCoordinator::send(uint16_t groupAddress, const KnxValue& value) {
	GroupAddress ga = unpackGroupAddress(groupAddress);
	KnxDebug::log("TX  -> GA %u/%u/%u, DPT %u",
		(unsigned)ga.main, (unsigned)ga.middle, (unsigned)ga.sub, (unsigned)value.dpt);

	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::build(physicalAddress, groupAddress, value, frame, sizeof(frame));
	if (len == 0) {
		KnxDebug::log("TX  !! build failed (bad value or frame too long)");
		return false;
	}

	bool confirmed = p_driver->sendTelegram(frame, len);
	KnxDebug::log("TX  %s", confirmed ? "confirmed (L_Data.con)" : "!! NOT confirmed");
	return confirmed;
}

//---- Receiver registry (intrusive singly-linked list) ----

void KnxCoordinator::registerReceiver(IKnxReceiver* receiver) {
	if (!receiver) return;
	// Idempotent: don't double-link an already-registered receiver.
	for (IKnxReceiver* n = receiverHead; n != nullptr; n = n->nextReceiver) {
		if (n == receiver) return;
	}
	receiver->nextReceiver = receiverHead;
	receiverHead = receiver;
}

void KnxCoordinator::unregisterReceiver(IKnxReceiver* receiver) {
	IKnxReceiver** link = &receiverHead;
	while (*link != nullptr) {
		if (*link == receiver) {
			*link = receiver->nextReceiver;
			receiver->nextReceiver = nullptr;
			return;
		}
		link = &(*link)->nextReceiver;
	}
}

void KnxCoordinator::dispatch(const ParsedTelegram& telegram) {
	uint16_t ga = packGroupAddress(telegram.target);

	uint8_t delivered = 0;
	for (IKnxReceiver* r = receiverHead; r != nullptr; r = r->nextReceiver) {
		if (r->matches(ga)) {
			r->receive(telegram);
			delivered++;
		}
	}

	// Logged for every telegram on the bus, including those addressed to other devices —
	// "no receiver" is normal foreign traffic, not an error.
	KnxDebug::log("RX  <- %u.%u.%u to GA %u/%u/%u -> %u receiver(s)",
		(unsigned)telegram.source.area, (unsigned)telegram.source.line,
		(unsigned)telegram.source.device,
		(unsigned)telegram.target.main, (unsigned)telegram.target.middle,
		(unsigned)telegram.target.sub, (unsigned)delivered);
}

//---- Receiving ----

bool KnxCoordinator::loop(void) {
	bool processed = false;
	uint8_t len = 0;

	// Drain every complete frame the driver has reassembled this cycle.
	while (p_driver->poll(rxFrame, sizeof(rxFrame), len)) {
		KnxDebug::logBytes("RX  frame:", rxFrame, len);

		ParsedTelegram telegram;
		if (!KnxFrame::parse(rxFrame, len, telegram)) {
			KnxDebug::log("RX  !! parse failed (%u bytes)", (unsigned)len);
			continue;
		}

		dispatch(telegram);
		processed = true;
	}
	return processed;
}
