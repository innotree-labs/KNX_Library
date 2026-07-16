#pragma once
/**
 * @name KnxInterfaces.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Abstract contracts used for dependency inversion (PLAN §12). They sit below
 *          their consumers so both sides can see them without a cyclic include:
 *          the coordinator holds an injected IKnxDriver* and an intrusive list of
 *          IKnxReceiver*, and never needs to include the concrete driver or object headers.
 *          Pure virtual, no data members other than the intrusive list link.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxTelegramTypes.h"

/**
 * @brief Link-layer transport contract implemented by the concrete bus driver
 *        (ATTiny co-processor / TP-UART). The coordinator talks only to this.
*/
class IKnxDriver {
	public:
		virtual ~IKnxDriver() = default;

		// Bring the transport up (UART config, transceiver reset, physical address).
		virtual bool begin(void) = 0;

		// Reset the transceiver and re-apply configuration.
		virtual bool reset(void) = 0;

		// Transmit an assembled telegram. Returns the real L_Data.con result reported by
		// the transceiver (true = confirmed on the bus) — never a hard-coded true (PLAN §9).
		virtual bool sendTelegram(const uint8_t* frame, uint8_t length) = 0;

		// Pump the receive path: consume available transport bytes and, if a complete frame
		// was reassembled, copy it into out (up to maxLen), set outLen, and return true.
		// Delivers at most one frame per call — loop until it returns false to drain the FIFO.
		virtual bool poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) = 0;

		// True if a bus-voltage failure or temperature warning has occurred since last checked.
		virtual bool faultPending(void) = 0;
};

/**
 * @brief A bus-event receiver that owns its listen address, DPT and cache, so it can
 *        decode an incoming telegram without the user restating either (PLAN §6).
 *        Nodes link themselves into the coordinator's intrusive registry via nextReceiver.
*/
class IKnxReceiver {
	public:
		virtual ~IKnxReceiver() = default;

		// True if this receiver wants telegrams addressed to the given packed group address.
		virtual bool matches(uint16_t packedGroupAddress) const = 0;

		// Handle a matched telegram: decode with the receiver's own DPT, update the
		// cached value, and fire the user callback.
		virtual void receive(const ParsedTelegram& telegram) = 0;

		// Intrusive singly-linked-list link; owned by the coordinator's registry.
		IKnxReceiver* nextReceiver = nullptr;
};
