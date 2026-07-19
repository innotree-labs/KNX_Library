#pragma once
/**
 * @name KnxDriver.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Concrete link-layer driver for the STKNX-behind-ATTiny front end, which mirrors
 *          the Siemens TP-UART2 UART protocol. Implements IKnxDriver: UART bring-up, reset/
 *          state handshake, per-byte telegram transmission with a real L_Data.con result
 *          (PLAN §9), and byte-stream RX via KnxReassembler.
 *          Replaces the thesis KNX_TPUART2 and is decoupled from the telegram/coordinator
 *          layers — the coordinator injects it as an IKnxDriver*.
 *
 *          Hardware paths (UART, pins, timing, con-byte values) follow the TP-UART2 spec and
 *          the thesis implementation; they are code-review-only and unverified without the board.
*/

//---- Standard / platform libraries ----
#include <Arduino.h>

//---- Custom shared types ----
#include "KnxInterfaces.h"
#include "KnxAddress.h"
#include "KnxEnums.h"
#include "KnxDebug.h"

//---- Other custom module headers ----
#include "KnxReassembler.h"

class KnxDriver : public IKnxDriver {
	private:
		//---- TP-UART2 command bytes ----
		static constexpr uint8_t U_RESET_REQ           = 0x01;
		static constexpr uint8_t U_STATE_REQ           = 0x02;
		static constexpr uint8_t U_ACK_INFO_ACK        = 0x11;
		static constexpr uint8_t U_ACK_INFO_BUSY       = 0x12;
		static constexpr uint8_t U_ACK_INFO_NACK       = 0x14;
		static constexpr uint8_t U_SET_ADDRESS         = 0x28;
		static constexpr uint8_t U_DATA_START_CONTINUE = 0x80;
		static constexpr uint8_t U_DATA_END            = 0x40;

		//---- TP-UART2 response codes ----
		static constexpr uint8_t U_RESET_IND    = 0x03;
		static constexpr uint8_t U_STATE_IND_OK = 0x07;

		// L_Data.con: fixed low-nibble pattern 0x_B; bit 7 = positive confirmation.
		// NOTE: spec-derived — validate against the actual ATTiny/TP-UART2 firmware.
		static constexpr uint8_t CON_MASK      = 0x7F;
		static constexpr uint8_t CON_PATTERN   = 0x0B;
		static constexpr uint8_t CON_POSITIVE  = 0x80;

		//---- Timing ----
		static constexpr uint8_t  RESPONSE_TIME_MS = 10;   // U_ command response window
		static constexpr uint16_t CON_TIMEOUT_MS   = 100;  // L_Data.con window after TX

		//---- Pin map (XIAO ESP32-C6) ----
		#define KNX_DRIVER_RX    D6
		#define KNX_DRIVER_TX    D7
		#define KNX_DRIVER_RESN  D0
		#define KNX_DRIVER_BAUD  19200

		//---- Members ----
		HardwareSerial uart = HardwareSerial(1);
		KnxReassembler reassembler;

		uint8_t rxPin;
		uint8_t txPin;
		uint8_t resnPin;   // Hard-reset pin
		uint16_t baudrate;
		PhysicalAddress physicalAddress;

		//---- Private methods ----
		// Writes a raw command byte sequence to the transceiver.
		void sendCommand(const uint8_t* cmd, uint16_t len);
		// Sends Reset.request, waits for Reset.indication, falls back to a hardware reset.
		bool resetRequest(void);
		// Sends State.request and returns the raw State.indication byte (0xFF on timeout).
		uint8_t stateRequest(void);
		// Sends the stored physical address to the transceiver (address filtering / ACK).
		void applyPhysicalAddress(void);
		// Flushes pending RX bytes.
		void clearBuffer(void);
		// Reads the post-transmission L_Data.con within CON_TIMEOUT_MS.
		bool awaitConfirmation(void);
		// Classifies a byte as an L_Data.con and its polarity.
		static bool isConfirmation(uint8_t b);
		static bool isPositiveConfirmation(uint8_t b);

	public:
		//---- Constructor ----
		/**
		 * @brief Constructs the driver, latches the physical address and configures the pins.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		*/
		KnxDriver(String physicalAddress);

		//---- IKnxDriver ----
		/**
		 * @brief Initialises the UART (8E1), resets the transceiver and applies the address.
		 * @return true if the transceiver reports State OK after bring-up.
		*/
		bool begin(void) override;

		/**
		 * @brief Resets the transceiver and re-applies the physical address.
		 * @return true if the transceiver reports State OK.
		*/
		bool reset(void) override;

		/**
		 * @brief Transmits an assembled telegram byte-by-byte and reads the L_Data.con.
		 * @param frame  Pointer to the assembled frame bytes (incl. checksum).
		 * @param length Number of bytes in frame.
		 * @return true if the transceiver returned a positive L_Data.con (PLAN §9).
		*/
		bool sendTelegram(const uint8_t* frame, uint8_t length) override;

		/**
		 * @brief Consumes available RX bytes; on a complete reassembled frame copies it out.
		 * @param out    Destination buffer for a complete frame.
		 * @param maxLen Capacity of out.
		 * @param outLen Set to the frame length when true is returned.
		 * @return true if a complete frame is available this call (at most one per call).
		*/
		bool poll(uint8_t* out, uint8_t maxLen, uint8_t& outLen) override;
};
