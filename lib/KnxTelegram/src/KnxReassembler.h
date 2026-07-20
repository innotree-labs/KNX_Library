#pragma once
/**
 * @name KnxReassembler.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Reassembles a complete KNX standard frame from a byte stream. The driver feeds
 *          received bytes one at a time; the reassembler locks onto an L_Data_Standard
 *          control byte, derives the frame length from the NPCI field, and signals when a full
 *          frame (through the checksum) is buffered — ready for KnxFrame::parse. Arduino-free
 *          and host-testable; this is the RX accumulation logic the old TP-UART checkUART owned.
*/

//---- Libraries ----
#include <cstdint>

class KnxReassembler {
	private:
		//---- Config ----
		// Largest standard frame: 6-byte header + up to 16-octet APDU + checksum.
		static constexpr uint8_t CAPACITY    = 6 + 16 + 1;

		// An L_Data_Standard control field is '1 0 r 1 p1 p0 0 0' (TP1 spec 03_02_02,
		// Figure 28): bit 5 is the repeat flag and bits 3..2 the priority, so eight byte
		// values are legal — 0xB0/B4/B8/BC (original) and 0x90/94/98/9C (repeated). Matching
		// the single value 0xBC, as this did before, silently dropped every repeated frame
		// (i.e. exactly the retransmission sent after a lost original) and everything not at
		// low priority. Mask off the five invariant bits (7,6,4,1,0) and compare instead;
		// this also still rejects extended (bit 7 = 0), acknowledge and poll frames.
		static constexpr uint8_t CTRL_MASK    = 0xD3;
		static constexpr uint8_t CTRL_PATTERN = 0x90;

		// Shortest legal frame is a TPCI-only control PDU (T_Connect/T_Disconnect/T_ACK):
		// 6 header + 1 TPCI + checksum. TP1 §2.2.4.5 — length 0 ends after the sixth octet.
		static constexpr uint8_t MIN_FRAME   = 8;

		// True if b can start an L_Data_Standard frame.
		static bool isControlByte(uint8_t b) { return (b & CTRL_MASK) == CTRL_PATTERN; }

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
