#pragma once
/**
 * @name KnxCoordinator.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details High-level KNX coordinator (the dependency-injection core). Owns an injected
 *          link-layer driver (IKnxDriver), assembles outgoing telegrams from a KnxValue via
 *          KnxFrame, and dispatches incoming telegrams to an intrusive list of IKnxReceiver
 *          objects that decode with their own DPT (PLAN §5–7). Arduino-free and depends only on
 *          the driver interface, so it is host-testable with a mock driver.
 *
 *          This is the class intent objects reference (KnxCoordinator&). End-user sketches use
 *          the `Konnextor` facade (Konnextor.h) — a thin Arduino subclass that owns a KnxDriver and
 *          is built from a physical-address string — so the driver is hidden without giving up
 *          the injectable, testable core here. The String ctor + stateless String send() tier are
 *          Arduino conveniences confined to #ifdef ARDUINO.
*/

//---- Standard / platform libraries ----
#include <cstdint>
#ifdef ARDUINO
#include <Arduino.h>
#endif

//---- Custom shared types + module headers ----
#include "KnxInterfaces.h"   // IKnxDriver, IKnxReceiver
#include "KnxAddress.h"
#include "KnxDebug.h"        // library-wide verbose logging switch
#include "KnxValue.h"
#include "KnxFrame.h"        // framing (pulls in KnxCodec)

class KnxCoordinator {
	private:
		//---- Config ----
		// RX buffer holds a full standard frame (up to 6 header + 16 APDU + checksum).
		static constexpr uint8_t RX_BUFFER_SIZE = 24;

		//---- Members ----
		IKnxDriver*    p_driver;                 // injected link-layer driver
		PhysicalAddress physicalAddress;         // frame source address
		IKnxReceiver*  receiverHead = nullptr;   // intrusive registry head
		IKnxDeviceHandler* p_deviceHandler = nullptr;   // optional point-to-point handler
		uint8_t        rxFrame[RX_BUFFER_SIZE];

		//---- Methods ----
		// Walks the receiver registry and delivers a parsed telegram to every match.
		void dispatch(const ParsedTelegram& telegram);

		// Routes a telegram addressed to an individual address: ours goes to the device
		// handler, anyone else's is foreign traffic.
		void dispatchIndividual(const ParsedTelegram& telegram);

	public:
		//---- Constructor ----
		/**
		 * @brief Constructs the coordinator around an injected driver and this device's address.
		 * @param driver          Link-layer driver (not owned; must outlive the coordinator).
		 * @param physicalAddress Physical address of this device (frame source).
		*/
		KnxCoordinator(IKnxDriver* driver, PhysicalAddress physicalAddress)
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
		 * @brief Installs the handler for telegrams addressed to this device's individual
		 *        address (device management: descriptor read / "ping", transport connect,
		 *        ETS programming). Without one such telegrams are parsed and logged but
		 *        not acted on, which is the default behaviour of a pure group-comms node.
		 * @param handler Handler to install, or nullptr to remove; not owned, must outlive
		 *                the coordinator.
		*/
		void setDeviceHandler(IKnxDeviceHandler* handler) { p_deviceHandler = handler; }

		//---- Point-to-point sending ----
		/**
		 * @brief Sends a point-to-point connectionless telegram carrying a raw APCI to one
		 *        device — e.g. apci 0x300 (A_DeviceDescriptor_Read) to ping it.
		 * @param target        Physical address of the addressed device.
		 * @param apci          Raw 10-bit APCI.
		 * @param payload       Optional APDU payload after the APCI (may be nullptr).
		 * @param payloadLength Number of payload bytes.
		 * @return true if the driver returned a positive L_Data.con.
		*/
		bool sendIndividual(PhysicalAddress target, uint16_t apci,
		                    const uint8_t* payload = nullptr, uint8_t payloadLength = 0);

		/**
		 * @brief Sends a TPCI-only transport control telegram (T_Connect, T_Disconnect,
		 *        T_ACK, T_NAK) to one device.
		 * @param target         Physical address of the addressed device.
		 * @param tpci           Control PDU to emit.
		 * @param sequenceNumber Sequence number for Ack/Nak; ignored otherwise.
		 * @return true if the driver returned a positive L_Data.con.
		*/
		bool sendControl(PhysicalAddress target, KnxTpci tpci, uint8_t sequenceNumber = 0);

		/**
		 * @brief Services the KNX stack: call this every Arduino loop() iteration. It drains every
		 *        complete frame the driver has received, parses each, and dispatches it to the
		 *        registered objects (which decode, cache, and fire their callbacks). Non-blocking.
		 * @return true if at least one telegram was processed this call.
		*/
		bool loop(void);

		/**
		 * @brief This device's physical address (frame source).
		 * @return The stored physical address.
		*/
		PhysicalAddress address(void) const { return physicalAddress; }

		//---- Diagnostics ----
		/**
		 * @brief Turns verbose debug logging on or off: every telegram sent and received
		 *        (including telegrams addressed to other devices), frame build/parse, value
		 *        coding and driver traffic are printed over Serial, prefixed with [knx].
		 *        Intended for diagnosing a misbehaving installation — it is chatty, and the
		 *        printing itself costs time on the receive path.
		 * @param on true to enable logging, false to silence it.
		 * @note  This is a LIBRARY-WIDE switch, not a per-node one: the flag behind it is
		 *        shared, so enabling it on any node enables it everywhere.
		*/
		void enableDebugMode(bool on) { KnxDebug::setEnabled(on); }

// ================= Arduino String conveniences =================
#ifdef ARDUINO
	public:
		/**
		 * @brief Arduino convenience: construct from a driver and a "area.line.device" string.
		*/
		KnxCoordinator(IKnxDriver* driver, String physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddressFromString(physicalAddress)) {}

		/**
		 * @brief Stateless send tier (PLAN §3): encode a value and transmit it to a group address
		 *        given as a "main/middle/sub" string. No receive — use a KnxObject for that.
		*/
		bool send(String ga, const KnxValue& value) { return send(packedGroupAddressFromString(ga), value); }
#endif // ARDUINO
};
