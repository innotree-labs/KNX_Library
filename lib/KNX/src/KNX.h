#pragma once
/**
 * @name KNX.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details High-level KNX coordinator. Owns an injected link-layer driver (IKnxDriver),
 *          assembles outgoing telegrams from a KnxValue via KnxFrame, and dispatches incoming
 *          telegrams to an intrusive list of IKnxReceiver objects that decode with their own
 *          DPT (PLAN §5–7). The core is Arduino-free and depends only on the driver interface,
 *          so it is host-testable with a mock driver; the String conveniences and the legacy
 *          KnxEvent bind path are transitional and confined to #ifdef ARDUINO.
*/

//---- Standard / platform libraries ----
#include <cstdint>
#ifdef ARDUINO
#include <Arduino.h>
#endif

//---- Custom shared types + module headers ----
#include "KnxInterfaces.h"   // IKnxDriver, IKnxReceiver
#include "KnxAddress.h"
#include "KnxValue.h"
#include "KnxFrame.h"        // framing (pulls in KnxCodec)

class KNX {
	private:
		//---- Config ----
		// RX buffer holds a full standard frame (up to 6 header + 16 APDU + checksum).
		static constexpr uint8_t RX_BUFFER_SIZE = 24;

		//---- Members ----
		IKnxDriver*    p_driver;                 // injected link-layer driver
		PhysicalAddress physicalAddress;         // frame source address
		IKnxReceiver*  receiverHead = nullptr;   // intrusive registry head
		uint8_t        rxFrame[RX_BUFFER_SIZE];

		//---- Methods ----
		// Walks the receiver registry and delivers a parsed telegram to every match.
		void dispatch(const ParsedTelegram& telegram);

	public:
		//---- Constructor ----
		/**
		 * @brief Constructs the coordinator around an injected driver and this device's address.
		 * @param driver          Link-layer driver (not owned; must outlive the coordinator).
		 * @param physicalAddress Physical address of this device (frame source).
		*/
		KNX(IKnxDriver* driver, PhysicalAddress physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddress) {}

		//---- Lifecycle (delegated to the driver) ----
		/**
		 * @brief Brings the link layer up.
		 * @return true if the driver initialised successfully.
		*/
		bool begin(void) { return p_driver->begin(); }

		/**
		 * @brief Resets the link layer.
		 * @return true if the reset succeeded.
		*/
		bool reset(void) { return p_driver->reset(); }

		/**
		 * @brief Checks for a bus-voltage or temperature fault reported by the driver.
		 * @return true if a fault occurred since the last call.
		*/
		bool monitorTPUART(void) { return p_driver->faultPending(); }

		//---- Sending ----
		/**
		 * @brief Encodes a value and transmits it to a packed group address.
		 * @param groupAddress Packed destination group address.
		 * @param value        Typed value to send.
		 * @return true if the driver returned a positive L_Data.con (PLAN §9).
		*/
		bool send(uint16_t groupAddress, const KnxValue& value);

		//---- Receiving ----
		/**
		 * @brief Links a receiver into the intrusive registry (idempotent).
		 * @param receiver Receiver to register; not owned, must outlive the coordinator.
		*/
		void registerReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Unlinks a receiver from the registry (safe if not registered).
		 * @param receiver Receiver to remove.
		*/
		void unregisterReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Drains complete frames from the driver, parses and dispatches each.
		 * @return true if at least one telegram was processed this call.
		*/
		bool handleUART(void);

		/**
		 * @brief This device's physical address (frame source).
		 * @return The stored physical address.
		*/
		PhysicalAddress address(void) const { return physicalAddress; }

// ================= Transitional Arduino conveniences (migrate to KnxObject) =================
#ifdef ARDUINO
	public:
		/**
		 * @brief Arduino convenience: construct from a driver and a "area.line.device" string.
		*/
		KNX(IKnxDriver* driver, String physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddressFromString(physicalAddress)) {}

		// Stateless one-liners over send() — thin sugar kept so the legacy device/app layer
		// builds until it is rebuilt on KnxObject (PLAN §8).
		bool send(String ga, const KnxValue& value)                { return send(packedGroupAddressFromString(ga), value); }
		bool switchLight(String ga, bool state)                    { return send(ga, Dpt1(state)); }
		bool dimLightInterval(String ga, bool dir, DimmIncrement i) { return send(ga, Dpt3(dir, (uint8_t)i)); }
		bool dimLightValue(String ga, uint8_t percent)             { return send(ga, Dpt5((uint8_t)map(percent, 0, 100, 0, 255))); }
		bool blindOpenClose(String ga, bool direction)             { return send(ga, Dpt1(direction)); }
		bool blindStopStep(String ga, bool step)                   { return send(ga, Dpt1(step)); }
		bool blindPositionValue(String ga, uint8_t position)       { return send(ga, Dpt5((uint8_t)map(position, 0, 100, 0, 255))); }
		bool sendTemperature(String ga, float temperature)         { return send(ga, Dpt9(temperature)); }
		bool sendHumidity(String ga, float humidity)               { return send(ga, Dpt9(humidity)); }

		/**
		 * @brief Legacy binding: registers a KnxEvent callback for a group address / DPT.
		 * @return true if the binding was stored.
		*/
		bool bind(String gaStr, KNX_DPT dpt, KNX_Callback cb);

	private:
		//---- Legacy KnxEvent bind path (deleted with the device-layer rewrite) ----
		static constexpr uint8_t MAX_BINDINGS = 8;
		KNX_Binding legacyBindings[MAX_BINDINGS];
		uint8_t     legacyBindingCount = 0;
		// Decodes and fires any legacy bindings matching a parsed telegram.
		void dispatchLegacy(const ParsedTelegram& telegram);
#endif // ARDUINO
};
