#pragma once
/**
 * @name KnxObject.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The generic device object, for datapoints without a dedicated class (KnxLight,
 *          KnxTemperature, …). Give it a group address and a datapoint type; then write() sends
 *          a value, value() reads back the last one, and onUpdate() notifies you when a new value
 *          arrives on the bus. The dedicated device classes are built on top of this and hide the
 *          datapoint type behind named methods.
*/

//---- Standard / platform libraries ----
#include <cstdint>

//---- Custom module headers ----
#include "KnxCoordinator.h"   // coordinator: send() + register/unregister
#include "KnxValue.h"     // value currency + Dpt*() factories
#include "KnxCodec.h"     // decode()/isInline6()
#include "KnxDebug.h"     // library-wide verbose logging

/**
 * @brief A generic device object for any datapoint type. Supply the datapoint type once at
 *        construction, then use write() to send a value, value() to read the last one, and
 *        onUpdate() to be notified of changes. Use this for datapoints the named device classes
 *        (KnxLight, KnxTemperature, …) do not cover.
*/
class KnxObject : public IKnxReceiver {
	protected:
		//---- Members ----
		KnxCoordinator* p_knx;             // coordinator (not owned; must outlive this object)
		uint16_t commandGa;                // packed GA that write() transmits to
		uint16_t statusGa;                 // packed GA this object listens on
		KnxDpt  objectDpt;                // DPT used to encode writes and decode receives
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
		 * @brief Creates an object that sends on one group address and reads status on another.
		 * @param knx       The bus node this object belongs to.
		 * @param cmdGa      Group address values are sent to.
		 * @param statusGa   Group address this object reads status on.
		 * @param dpt        Datapoint type of the values sent and received.
		*/
		KnxObject(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa, KnxDpt dpt)
			: p_knx(&knx), commandGa(cmdGa), statusGa(statusGa), objectDpt(dpt) {
			p_knx->registerReceiver(this);
		}

		/**
		 * @brief Creates an object that uses one group address to send and read status.
		 * @param knx  The bus node this object belongs to.
		 * @param ga   Group address used to send and to read status.
		 * @param dpt  Datapoint type of the values sent and received.
		*/
		KnxObject(KnxCoordinator& knx, uint16_t ga, KnxDpt dpt)
			: KnxObject(knx, ga, ga, dpt) {}

		~KnxObject() override { p_knx->unregisterReceiver(this); }

		//---- IKnxReceiver ----
		/** @brief Called by the bus node to test whether this object handles a group address.
		 *  You do not call this yourself. */
		bool matches(uint16_t packedGroupAddress) const override {
			return packedGroupAddress == statusGa;
		}

		/** @brief Called by the bus node to deliver a matching telegram to this object.
		 *  You do not call this yourself. */
		void receive(const ParsedTelegram& telegram) override {
			const uint8_t* in;
			uint8_t inLen;
			if (KnxCodec::isInline6(objectDpt)) { in = &telegram.inline6Data; inLen = 1; }
			else                                { in = telegram.payload;      inLen = telegram.payloadLength; }

			KnxValue decoded = KnxCodec::decode(objectDpt, in, inLen);
			if (!decoded.isValid()) {
				// The object matched the GA but could not decode it — usually the object's DPT
				// disagrees with what the actuator actually publishes on that address.
				KnxDebug::log("OBJ !! matched GA but decode failed (object DPT %u)",
					(unsigned)objectDpt);
				return;
			}

			cachedValue = decoded;
			KnxDebug::log("OBJ update: DPT %u cached, firing callback", (unsigned)objectDpt);
			onValueUpdated();
		}

		//---- Sending ----
		/**
		 * @brief Sends a value to this object's group address.
		 * @param value Value to send; it must match this object's datapoint type.
		 * @return true if the bus confirmed the send.
		*/
		bool write(const KnxValue& value) {
			// Optimistic cache: lets toggle/read-back work before status feedback arrives; a
			// later receive() on the status GA overwrites it with the authoritative bus state.
			if (value.dpt == objectDpt) cachedValue = value;
			return p_knx->send(commandGa, value);
		}

		//---- Reading ----
		/**
		 * @brief The last value sent or received, with no bus traffic.
		 * @return The cached value.
		*/
		KnxValue value(void) const { return cachedValue; }

		/**
		 * @brief Registers a function to call whenever a new value arrives on the bus.
		 * @param callback Called with the decoded value. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(const KnxValue&)) { p_onValue = callback; }

// ================= Arduino String conveniences (config-time only) =================
#ifdef ARDUINO
	public:
		/**
		 * @brief Creates an object that sends on one group address and reads status on another.
		 * @param knx       The bus node this object belongs to.
		 * @param cmdGa      Command group address as "main/middle/sub".
		 * @param statusGa   Status group address as "main/middle/sub".
		 * @param dpt        Datapoint type of the values sent and received.
		*/
		KnxObject(KnxCoordinator& knx, String cmdGa, String statusGa, KnxDpt dpt)
			: KnxObject(knx, packedGroupAddressFromString(cmdGa),
			            packedGroupAddressFromString(statusGa), dpt) {}
		/**
		 * @brief Creates an object that uses one group address to send and read status.
		 * @param knx  The bus node this object belongs to.
		 * @param ga   Group address as "main/middle/sub".
		 * @param dpt  Datapoint type of the values sent and received.
		*/
		KnxObject(KnxCoordinator& knx, String ga, KnxDpt dpt)
			: KnxObject(knx, packedGroupAddressFromString(ga), dpt) {}
#endif
};
