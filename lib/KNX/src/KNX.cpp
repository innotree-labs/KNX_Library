/**
 * @name KNX.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KNX.h.
*/

//----Libraries----
#include "KNX.h"

//---- Sending ----

bool KNX::send(uint16_t groupAddress, const KnxValue& value) {
	uint8_t frame[KnxFrame::MAX_FRAME];
	uint8_t len = KnxFrame::build(physicalAddress, groupAddress, value, frame, sizeof(frame));
	if (len == 0) return false;
	return p_driver->sendTelegram(frame, len);
}

//---- Receiver registry (intrusive singly-linked list) ----

void KNX::registerReceiver(IKnxReceiver* receiver) {
	if (!receiver) return;
	// Idempotent: don't double-link an already-registered receiver.
	for (IKnxReceiver* n = receiverHead; n != nullptr; n = n->nextReceiver) {
		if (n == receiver) return;
	}
	receiver->nextReceiver = receiverHead;
	receiverHead = receiver;
}

void KNX::unregisterReceiver(IKnxReceiver* receiver) {
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

void KNX::dispatch(const ParsedTelegram& telegram) {
	uint16_t ga = packGroupAddress(telegram.target);
	for (IKnxReceiver* r = receiverHead; r != nullptr; r = r->nextReceiver) {
		if (r->matches(ga)) r->receive(telegram);
	}
}

//---- Receiving ----

bool KNX::handleUART(void) {
	bool processed = false;
	uint8_t len = 0;

	// Drain every complete frame the driver has reassembled this cycle.
	while (p_driver->poll(rxFrame, sizeof(rxFrame), len)) {
		ParsedTelegram telegram;
		if (!KnxFrame::parse(rxFrame, len, telegram)) continue;

		dispatch(telegram);
		#ifdef ARDUINO
		dispatchLegacy(telegram);
		#endif
		processed = true;
	}
	return processed;
}

//---- Legacy KnxEvent bind path (Arduino only; removed with the device-layer rewrite) ----
#ifdef ARDUINO

bool KNX::bind(String gaStr, KNX_DPT dpt, KNX_Callback cb) {
	if (legacyBindingCount >= MAX_BINDINGS || !cb) return false;
	legacyBindings[legacyBindingCount].ga       = groupAddressFromString(gaStr);
	legacyBindings[legacyBindingCount].dpt      = dpt;
	legacyBindings[legacyBindingCount].callback = cb;
	legacyBindingCount++;
	return true;
}

void KNX::dispatchLegacy(const ParsedTelegram& telegram) {
	for (uint8_t i = 0; i < legacyBindingCount; i++) {
		if (!gaEqual(legacyBindings[i].ga, telegram.target)) continue;

		KNX_DPT dpt = legacyBindings[i].dpt;
		const uint8_t* in;
		uint8_t inLen;
		if (KnxCodec::isInline6(dpt)) { in = &telegram.inline6Data; inLen = 1; }
		else                          { in = telegram.payload;      inLen = telegram.payloadLength; }

		KnxValue kv = KnxCodec::decode(dpt, in, inLen);
		if (!kv.isValid()) continue;

		KnxEvent ev{};
		ev.source = telegram.source;
		ev.target = telegram.target;
		ev.type   = telegram.type;
		ev.dpt    = dpt;

		// Minimal native mapping — DPT1 is the only type the legacy app path exercises.
		switch (dpt) {
			case KNX_DPT::DPT1: ev.value.dpt1 = kv.asBool(); break;
			case KNX_DPT::DPT2: ev.value.dpt2 = kv.asU8();   break;
			case KNX_DPT::DPT3: ev.value.dpt3.direction = kv.asDim().increase;
			                    ev.value.dpt3.step      = kv.asDim().stepcode; break;
			case KNX_DPT::DPT4: ev.value.dpt4 = kv.asChar(); break;
			case KNX_DPT::DPT5: ev.value.dpt5.raw     = kv.asU8();
			                    ev.value.dpt5.percent = kv.asU8() * 100.0f / 255.0f; break;
			default: continue;
		}

		legacyBindings[i].callback(ev);
	}
}

#endif // ARDUINO
