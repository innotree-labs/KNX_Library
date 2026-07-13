#pragma once
/**
 * @name KNX_Telegram.h
 * @date 11.10.2025
 * @authors Florian Wiesner
 * @details Encodes and decodes KNX standard telegrams for all supported DPTs.
 *          Handles buffer assembly, checksum, APDU encoding, and callback dispatch.
*/

#ifdef _MSC_VER
	#pragma region Libraries //--------------------------------------------------------------------------------------------------
#endif
#include <Arduino.h>
#include "KNX_Defines.h"

// Forward-declaration -> no cyclic-dependencies
class KNX_TPUART2;

typedef void (*TelegramParsedCallback)(void* ctx, const ParsedTelegram& tg);

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Telegram-Class //---------------------------------------------------------------------------------------------
#endif
class KNX_Telegram {
	#ifdef _MSC_VER
		#pragma endregion
		#pragma region private
	#endif
	private:
		//----Definitions----
		static constexpr uint8_t TELEGRAM_MAX_DATA_SIZE = 23;
		static constexpr uint8_t TELEGRAM_HEADER_SIZE   = 6;
		static constexpr uint8_t TELEGRAM_APDU_SIZE     = 2;
		static constexpr uint8_t MAX_TELEGRAM_SIZE      = TELEGRAM_HEADER_SIZE + TELEGRAM_MAX_DATA_SIZE + 1; // +1 for checksum

		//----Members----
		KNX_TPUART2* p_tpuart;                // Pointer to KNX_TPUART2 instance
		uint8_t buffer[MAX_TELEGRAM_SIZE];
		uint8_t length;
		ParsedTelegram parsed;
		TelegramParsedCallback parsedCb = nullptr;
		void* parsedCtx = nullptr;

		//----Methods----
		void clear(void);
		static void encodeDPT9(float value, uint8_t* data);

		// Sending helpers
		void setControlField(bool repeat=true, KNX_Priority prio=KNX_Priority::Normal);
		void setSourceAddress(PhysicalAddress source);
		void setTargetGroupAddress(String groupAddressString);
		void setNPCI(uint8_t apduLength);
		void setAPDU(uint8_t* data, uint8_t len, bool smaller6Bit);
		void setChecksum(void);
		void createStandardHeader(String targetGroupAddress);
		bool send(void);

		// Receiving helpers
		uint8_t getChecksum(void) const;
		uint8_t calculateChecksum(void) const;
		PhysicalAddress getSourceAddress(void) const;
		GroupAddress getTargetGroupAddress(void) const;
		KNX_Priority getPriority(void) const;

		// DPT decoders
		bool decodeDPT1(ParsedTelegram& tg);
		bool decodeDPT2(ParsedTelegram& tg);
		bool decodeDPT3(ParsedTelegram& tg);
		bool decodeDPT4(ParsedTelegram& tg);
		bool decodeDPT5(ParsedTelegram& tg);

	#ifdef _MSC_VER
		#pragma endregion
		#pragma region public
	#endif

	public:
		//----Constructor----
		/**
		 * @brief Constructs a KNX_Telegram instance bound to a TP-UART2 driver.
		 * @param tpuart Pointer to the KNX_TPUART2 hardware driver used for sending.
		*/
		KNX_Telegram(KNX_TPUART2* tpuart);

		/**
		 * @brief Registers a callback invoked after each successfully parsed incoming telegram.
		 * @param cb  Function pointer called with the parsed telegram.
		 * @param ctx Caller-supplied context pointer forwarded to cb.
		*/
		void setParsedCallback(TelegramParsedCallback cb, void* ctx);

		//----Public methods - Sending----
		/**
		 * @brief Sends a DPT1 telegram (1-bit on/off).
		 * @param targetGroupAddress KNX group address string (e.g. "0/1/1").
		 * @param oneBit             Bit value to send.
		 * @return true if accepted by the TP-UART driver.
		*/
		bool sendDPT1(String targetGroupAddress, bool oneBit);

		/**
		 * @brief Sends a DPT2 telegram (2-bit controlled).
		 * @param targetGroupAddress KNX group address string.
		 * @param twoBit             2-bit value (bits 1..0 used).
		 * @return true on success.
		*/
		bool sendDPT2(String targetGroupAddress, uint8_t twoBit);

		/**
		 * @brief Sends a DPT3 telegram (4-bit dimming/blind step).
		 * @param targetGroupAddress KNX group address string.
		 * @param fourBit            4-bit value (bits 3..0 used).
		 * @return true on success.
		*/
		bool sendDPT3(String targetGroupAddress, uint8_t fourBit);

		/**
		 * @brief Sends a DPT4 telegram (8-bit ASCII character).
		 * @param targetGroupAddress KNX group address string.
		 * @param ascii              ASCII character to send.
		 * @return true on success.
		*/
		bool sendDPT4(String targetGroupAddress, char ascii);

		/**
		 * @brief Sends a DPT5 telegram (8-bit unsigned, 0..255: position, percent, brightness).
		 * @param targetGroupAddress KNX group address string.
		 * @param uint8              Value to send.
		 * @return true on success.
		*/
		bool sendDPT5(String targetGroupAddress, uint8_t uint8);

		/**
		 * @brief Sends a DPT6 telegram (8-bit signed, -128..127: temperature delta, correction).
		 * @param targetGroupAddress KNX group address string.
		 * @param sint8              Signed value to send.
		 * @return true on success.
		*/
		bool sendDPT6(String targetGroupAddress, int8_t sint8);

		/**
		 * @brief Sends a DPT7 telegram (16-bit unsigned, 0..65535: pulses, counters).
		 * @param targetGroupAddress KNX group address string.
		 * @param uint16             Value to send.
		 * @return true on success.
		*/
		bool sendDPT7(String targetGroupAddress, uint16_t uint16);

		/**
		 * @brief Sends a DPT8 telegram (16-bit signed, -32768..32767).
		 * @param targetGroupAddress KNX group address string.
		 * @param sint16             Signed value to send.
		 * @return true on success.
		*/
		bool sendDPT8(String targetGroupAddress, int16_t sint16);

		/**
		 * @brief Sends a DPT9 telegram (2-byte KNX float: temperature, brightness, humidity).
		 * @param targetGroupAddress KNX group address string.
		 * @param float16            Value encoded as KNX DPT9 2-byte float.
		 * @return true on success.
		*/
		bool sendDPT9(String targetGroupAddress, float float16);

		/**
		 * @brief Sends a DPT10 telegram (3-byte time: hour, minute, second).
		 * @param targetGroupAddress KNX group address string.
		 * @param threeByte          Time packed in the low 24 bits.
		 * @return true on success.
		*/
		bool sendDPT10(String targetGroupAddress, uint32_t threeByte);

		/**
		 * @brief Sends a DPT11 telegram (3-byte date: day, month, year).
		 * @param targetGroupAddress KNX group address string.
		 * @param threeByte          Date packed in the low 24 bits.
		 * @return true on success.
		*/
		bool sendDPT11(String targetGroupAddress, uint32_t threeByte);

		/**
		 * @brief Sends a DPT12 telegram (32-bit unsigned, 0..4294967295: counter, energy counter).
		 * @param targetGroupAddress KNX group address string.
		 * @param uint32             Value to send.
		 * @return true on success.
		*/
		bool sendDPT12(String targetGroupAddress, uint32_t uint32);

		/**
		 * @brief Sends a DPT13 telegram (32-bit signed, -2147483648..2147483647).
		 * @param targetGroupAddress KNX group address string.
		 * @param sint32             Signed value to send.
		 * @return true on success.
		*/
		bool sendDPT13(String targetGroupAddress, int32_t sint32);

		/**
		 * @brief Sends a DPT14 telegram (32-bit IEEE float: precise measurement values).
		 * @param targetGroupAddress KNX group address string.
		 * @param float32            Precise floating-point value.
		 * @return true on success.
		*/
		bool sendDPT14(String targetGroupAddress, float float32);

		/**
		 * @brief Sends a DPT16 telegram (14-character ASCII string).
		 * @param targetGroupAddress KNX group address string.
		 * @param string14           String to send; truncated to 14 characters.
		 * @return true on success.
		*/
		bool sendDPT16(String targetGroupAddress, String string14);

		/**
		 * @brief Sends a DPT19 telegram (8-byte date/time: year, month, day, hour, minute, second, weekday).
		 * @param targetGroupAddress KNX group address string.
		 * @param datetime           Date/time packed into 64 bits.
		 * @return true on success.
		*/
		bool sendDPT19(String targetGroupAddress, uint64_t datetime);

		//----Public methods - Receiving----
		/**
		 * @brief Copies raw received bytes into the telegram buffer and validates the checksum.
		 * @param rxBuffer Pointer to the received raw bytes.
		 * @param rxLength Number of bytes in rxBuffer.
		 * @return true if the checksum is valid and parsing succeeded.
		*/
		bool receiveIncomingTelegram(uint8_t* rxBuffer, uint8_t rxLength);

		/**
		 * @brief Parses the current buffer into the internal ParsedTelegram struct and fires the callback.
		 * @return true if the telegram structure is valid.
		*/
		bool parseIncomingTelegram(void);

		/**
		 * @brief Prints all fields of the last parsed telegram to the serial monitor.
		*/
		void outputParsedTelegram(void);

		/**
		 * @brief Decodes the payload of a parsed telegram according to its DPT.
		 * @param tg Reference to the ParsedTelegram; decoded result written to tg.decoded.
		 * @return true if decoding succeeded and tg.decodedValid is set.
		*/
		bool decode(ParsedTelegram& tg);
};
#ifdef _MSC_VER
	#pragma endregion
#endif
