#pragma once
/**
 * @name KnxReassembler.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Reassembles a complete KNX standard frame from a byte stream. The driver feeds
 *          received bytes one at a time; the reassembler locks onto the 0xBC start byte,
 *          derives the frame length from the NPCI field, and signals when a full frame
 *          (through the checksum) is buffered — ready for KnxFrame::parse. Arduino-free and
 *          host-testable; this is the RX accumulation logic the old TP-UART checkUART owned.
*/

//---- Libraries ----
#include <cstdint>

class KnxReassembler {
	private:
		//---- Config ----
		// Largest standard frame: 6-byte header + up to 16-octet APDU + checksum.
		static constexpr uint8_t CAPACITY    = 6 + 16 + 1;
		static constexpr uint8_t START_BYTE  = 0xBC;
		static constexpr uint8_t MIN_FRAME   = 9;   // inline-6: 6 header + 2 APDU + checksum

		//---- Members ----
		uint8_t buffer[CAPACITY];
		uint8_t index        = 0;
		uint8_t expectedLen  = 0;
		uint8_t completedLen = 0;
		bool    inFrame      = false;

	public:
		//---- Constructor ----
		/**
		 * @brief Constructs an empty reassembler ready to lock onto the next start byte.
		*/
		KnxReassembler(void) = default;

		/**
		 * @brief Feeds one received byte into the state machine.
		 * @param byte The byte read from the transport.
		 * @return true when a complete frame is now available via frame()/length();
		 *         the frame stays valid until the next feed() begins a new frame.
		*/
		bool feed(uint8_t byte);

		/**
		 * @brief Pointer to the last completed frame buffer (valid after feed() returned true).
		 * @return Pointer to the internal frame buffer.
		*/
		const uint8_t* frame(void) const { return buffer; }

		/**
		 * @brief Length of the last completed frame, including the checksum byte.
		 * @return Frame length in bytes.
		*/
		uint8_t length(void) const { return completedLen; }

		/**
		 * @brief Discards any partially accumulated frame and returns to the idle state.
		*/
		void reset(void);
};
