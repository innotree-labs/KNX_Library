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
	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::build(physicalAddress, groupAddress, value, frame, sizeof(frame));
	if (len == 0) return false;
	return p_driver->sendTelegram(frame, len);
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
	for (IKnxReceiver* r = receiverHead; r != nullptr; r = r->nextReceiver) {
		if (r->matches(ga)) r->receive(telegram);
	}
}

//---- Receiving ----

bool KnxCoordinator::loop(void) {
	bool processed = false;
	uint8_t len = 0;

	// Drain every complete frame the driver has reassembled this cycle.
	while (p_driver->poll(rxFrame, sizeof(rxFrame), len)) {
		ParsedTelegram telegram;
		if (!KnxFrame::parse(rxFrame, len, telegram)) continue;

		dispatch(telegram);
		processed = true;
	}
	return processed;
}
