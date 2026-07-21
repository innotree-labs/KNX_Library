#pragma once
/**
 * @name KnxCoordinator.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The KNX bus node. It sends values to group addresses, receives telegrams and delivers
 *          them to the device objects attached to it, and exposes begin(), loop() and the debug
 *          switch a sketch calls on its `knx` object.
 *
 *          Most sketches never name this class directly: they create a `Konnextor` (Konnextor.h),
 *          which is a KnxCoordinator that also owns its bus driver, so the driver is set up for
 *          you. Use KnxCoordinator directly only for the advanced case of supplying your own
 *          driver (IKnxDriver) — for example a custom transceiver or a mock in a host test.
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
		 * @brief Advanced: creates a bus node driven by a bus driver you provide. Most sketches
		 *        use Konnextor instead, which supplies the driver for you.
		 * @param driver          The bus driver to use; not owned, and must outlive this node.
		 * @param physicalAddress This device's physical address.
		*/
		KnxCoordinator(IKnxDriver* driver, PhysicalAddress physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddress) {}

		//---- Lifecycle ----
		/**
		 * @brief Starts the bus connection. Call once in setup().
		 * @return true if the bus came up successfully.
		*/
		bool begin(void) { return p_driver->begin(); }

		/**
		 * @brief Restarts the bus connection.
		 * @return true if the reset succeeded.
		*/
		bool reset(void) { return p_driver->reset(); }

		//---- Sending ----
		/**
		 * @brief Sends a value to a group address.
		 * @param groupAddress Destination group address.
		 * @param value        The value to send.
		 * @return true if the bus confirmed the send.
		*/
		bool send(uint16_t groupAddress, const KnxValue& value);

		//---- Receiving ----
		/**
		 * @brief Attaches a device object so it receives matching telegrams. Objects do this
		 *        themselves when created, so you rarely call it directly.
		 * @param receiver The object to attach; not owned, and must outlive this node.
		*/
		void registerReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Detaches a device object (safe if it was not attached). Objects do this
		 *        themselves when destroyed.
		 * @param receiver The object to detach.
		*/
		void unregisterReceiver(IKnxReceiver* receiver);

		/**
		 * @brief Advanced: installs a handler for telegrams sent to this device by its physical
		 *        address (device management, such as a ping from ETS). Without one, such
		 *        telegrams are ignored.
		 * @param handler The handler to install, or nullptr to remove it; not owned, and must
		 *                outlive this node.
		*/
		void setDeviceHandler(IKnxDeviceHandler* handler) { p_deviceHandler = handler; }

		//---- Point-to-point sending (advanced) ----
		/**
		 * @brief Advanced: sends a message to a single device addressed by its physical address,
		 *        for example to ping it.
		 * @param target        Physical address of the device to send to.
		 * @param apci          Application service code to send.
		 * @param payload       Optional data after the service code (may be nullptr).
		 * @param payloadLength Number of payload bytes.
		 * @return true if the bus confirmed the send.
		*/
		bool sendIndividual(PhysicalAddress target, uint16_t apci,
		                    const uint8_t* payload = nullptr, uint8_t payloadLength = 0);

		/**
		 * @brief Advanced: sends a transport control message (connect, disconnect, acknowledge)
		 *        to a single device addressed by its physical address.
		 * @param target         Physical address of the device to send to.
		 * @param tpci           The control message to send.
		 * @param sequenceNumber Sequence number, used only for acknowledge messages.
		 * @return true if the bus confirmed the send.
		*/
		bool sendControl(PhysicalAddress target, KnxTpci tpci, uint8_t sequenceNumber = 0);

		/**
		 * @brief Services the bus: call this on every iteration of loop(). It receives waiting
		 *        telegrams and fires the callbacks of the objects they are addressed to. It never
		 *        blocks, so do not replace it with delay()-based timing.
		 * @return true if at least one telegram was handled this call.
		*/
		bool loop(void);

		/**
		 * @brief This device's physical address.
		 * @return The physical address the node was created with.
		*/
		PhysicalAddress address(void) const { return physicalAddress; }

		//---- Diagnostics ----
		/**
		 * @brief Turns verbose tracing on or off. When on, every telegram sent and received
		 *        (including traffic for other devices), and the library's internal steps, are
		 *        printed over Serial with a [knx] prefix. Useful for diagnosing a problem; leave
		 *        it off in normal operation, as the printing is chatty and slows the receive path.
		 * @param on true to enable tracing, false to turn it off.
		 * @note  This affects the whole library, not just this node: turning it on for one node
		 *        turns it on everywhere.
		*/
		void enableDebugMode(bool on) { KnxDebug::setEnabled(on); }

// ================= Arduino String conveniences =================
#ifdef ARDUINO
	public:
		/**
		 * @brief Advanced: creates a bus node from a driver you provide and an address string.
		 * @param driver          The bus driver to use; not owned, and must outlive this node.
		 * @param physicalAddress This device's physical address as "area.line.device", e.g. "1.1.5".
		*/
		KnxCoordinator(IKnxDriver* driver, String physicalAddress)
			: p_driver(driver), physicalAddress(physicalAddressFromString(physicalAddress)) {}

		/**
		 * @brief Sends a value to a group address given as a string, without needing a device
		 *        object. This only sends; to receive, use a device object.
		 * @param ga    Destination group address as "main/middle/sub", e.g. "0/4/2".
		 * @param value The value to send.
		 * @return true if the bus confirmed the send.
		*/
		bool send(String ga, const KnxValue& value) { return send(packedGroupAddressFromString(ga), value); }
#endif // ARDUINO
};
