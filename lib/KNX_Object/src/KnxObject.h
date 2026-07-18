#pragma once
/**
 * @name KnxObject.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Stateful base object (PLAN §5-7). A KnxObject knows its own command GA, status
 *          (listen) GA and DPT, so it can decode an incoming telegram without the user
 *          restating either. It self-registers into the coordinator's intrusive IKnxReceiver
 *          registry on construction and unlinks in its destructor, so a scoped object can
 *          never dangle. write() sends through the one KNX::send() encode path and
 *          optimistically updates the cache; receive() overwrites it authoritatively from the
 *          bus. Intent subclasses (KnxLight, ...) hide the DPT behind intent methods and
 *          translate the cached value into a typed callback via the onValueUpdated() hook.
 *
 *          Header-only by design: the LDF compiles every .cpp in a pulled library, so keeping
 *          this Arduino-bound layer .cpp-free keeps the native codec tests green.
*/

//---- Standard / platform libraries ----
#include <cstdint>

//---- Custom module headers ----
#include "KNX.h"          // coordinator: send() + register/unregister
#include "KnxValue.h"     // value currency + Dpt*() factories
#include "KnxCodec.h"     // decode()/isInline6()

/**
 * @brief Raw KNX object tier: a receiver bound to one status GA / DPT with a value cache and a
 *        generic KnxValue callback. Covers datapoints not wrapped by an intent class — the user
 *        supplies the DPT once and gets generic write()/onUpdate() methods.
*/
class KnxObject : public IKnxReceiver {
	protected:
		//---- Members ----
		KNX*     p_knx;                    // coordinator (not owned; must outlive this object)
		uint16_t commandGa;                // packed GA that write() transmits to
		uint16_t statusGa;                 // packed GA this object listens on
		KNX_DPT  objectDpt;                // DPT used to encode writes and decode receives
		KnxValue cachedValue;              // last known value (send-optimistic or status-fed)
		void (*p_onValue)(const KnxValue&) = nullptr;   // generic raw-tier callback

		//---- Methods ----
		// Fired after the cache is updated on receive. Base delivers the generic callback;
		// intent subclasses override to translate the cached value into a typed callback.
		virtual void onValueUpdated(void) {
			if (p_onValue) p_onValue(cachedValue);
		}

	public:
		//---- Constructors ----
		/**
		 * @brief Constructs an object with distinct command and status group addresses.
		 * @param knx       Coordinator to send through and register with (must outlive this).
		 * @param cmdGa      Packed group address writes are sent to.
		 * @param statusGa   Packed group address this object listens on for feedback.
		 * @param dpt        Datapoint type used to encode writes and decode receives.
		*/
		KnxObject(KNX& knx, uint16_t cmdGa, uint16_t statusGa, KNX_DPT dpt)
			: p_knx(&knx), commandGa(cmdGa), statusGa(statusGa), objectDpt(dpt) {
			p_knx->registerReceiver(this);
		}

		/**
		 * @brief Constructs an object whose status GA defaults to its command GA.
		*/
		KnxObject(KNX& knx, uint16_t ga, KNX_DPT dpt)
			: KnxObject(knx, ga, ga, dpt) {}

		//---- Destructor: unlink from the registry so a scoped object cannot dangle ----
		~KnxObject() override { p_knx->unregisterReceiver(this); }

		//---- IKnxReceiver ----
		bool matches(uint16_t packedGroupAddress) const override {
			return packedGroupAddress == statusGa;
		}

		void receive(const ParsedTelegram& telegram) override {
			const uint8_t* in;
			uint8_t inLen;
			if (KnxCodec::isInline6(objectDpt)) { in = &telegram.inline6Data; inLen = 1; }
			else                                { in = telegram.payload;      inLen = telegram.payloadLength; }

			KnxValue decoded = KnxCodec::decode(objectDpt, in, inLen);
			if (!decoded.isValid()) return;

			cachedValue = decoded;
			onValueUpdated();
		}

		//---- Sending ----
		/**
		 * @brief Encodes and transmits a value to the command GA, updating the cache optimistically.
		 * @param value Value to send; only cached if its DPT matches this object's DPT.
		 * @return The driver's L_Data.con result (PLAN §9).
		*/
		bool write(const KnxValue& value) {
			// Optimistic cache: lets toggle/read-back work before status feedback arrives; a
			// later receive() on the status GA overwrites it with the authoritative bus state.
			if (value.dpt == objectDpt) cachedValue = value;
			return p_knx->send(commandGa, value);
		}

		//---- Reading ----
		/**
		 * @brief Last known value with no bus traffic (send-optimistic or status-fed).
		*/
		KnxValue value(void) const { return cachedValue; }

		/**
		 * @brief Registers a generic callback fired with the decoded value on every status update.
		*/
		void onUpdate(void (*callback)(const KnxValue&)) { p_onValue = callback; }

// ================= Arduino String conveniences (config-time only) =================
#ifdef ARDUINO
	public:
		KnxObject(KNX& knx, String cmdGa, String statusGa, KNX_DPT dpt)
			: KnxObject(knx, packedGroupAddressFromString(cmdGa),
			            packedGroupAddressFromString(statusGa), dpt) {}
		KnxObject(KNX& knx, String ga, KNX_DPT dpt)
			: KnxObject(knx, packedGroupAddressFromString(ga), dpt) {}
#endif
};
