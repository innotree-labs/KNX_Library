#pragma once
/**
 * @name KNX_TPUART2.h
 * @date 25.09.2025
 * @authors Florian Wiesner
 * @details Hardware driver for the Siemens TP-UART2 transceiver.
 *          Manages UART initialisation, telegram TX/RX framing, state requests,
 *          and interrupt-driven monitoring of bus voltage and temperature.
*/

#ifdef _MSC_VER
	#pragma region Libraries //--------------------------------------------------------------------------------------------------
#endif
#include <Arduino.h>
#include "KNX_Defines.h"

// Forward-declaration -> no cyclic-dependencies
class KNX_Telegram;

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TPUART2-Class //----------------------------------------------------------------------------------------------
#endif
class KNX_TPUART2 {
	#ifdef _MSC_VER
		#pragma endregion
		#pragma region private
	#endif
	private:
		//----Definitions — TP-UART2 commands----
		static constexpr uint8_t U_RESET_REQ          = 0x01; // Reset.request
		static constexpr uint8_t U_STATE_REQ          = 0x02; // State.request
		static constexpr uint8_t U_MONITOR_REQ        = 0x05; // Monitor.request
		static constexpr uint8_t U_ACK_INFO_ACK       = 0x11; // ACK
		static constexpr uint8_t U_ACK_INFO_BUSY      = 0x12; // BUSY
		static constexpr uint8_t U_ACK_INFO_NACK      = 0x14; // NACK
		static constexpr uint8_t U_PRODUCT_ID_REQ     = 0x20; // ProductID.request
		static constexpr uint8_t U_BUSY_MODE_REQ      = 0x21; // ActivateBusyMode
		static constexpr uint8_t U_RESET_BUSY_MODE_REQ = 0x22; // ResetBusyMode
		static constexpr uint8_t U_SET_MAX_REP_COUNT  = 0x24; // SetMaxRepeatCount
		static constexpr uint8_t U_ACTIVATE_CRC       = 0x25; // ActivateCRC
		static constexpr uint8_t U_SET_ADDRESS        = 0x28; // SetPhysicalAddress
		static constexpr uint8_t U_DATA_START_CONTINUE = 0x80; // L_DataStart
		static constexpr uint8_t U_DATA_END           = 0x40; // L_DataEnd

		//----Definitions — TP-UART2 response codes----
		static constexpr uint8_t U_RESET_IND     = 0x03; // Reset.indication
		static constexpr uint8_t U_STATE_IND_OK  = 0x07; // State OK
		static constexpr uint8_t U_STATE_IND_TW  = 0x0F; // Temperature Warning
		static constexpr uint8_t U_STATE_IND_PE  = 0x17; // Protocol Error
		static constexpr uint8_t U_STATE_IND_TE  = 0x27; // Transmitter Error
		static constexpr uint8_t U_STATE_IND_RE  = 0x47; // Receive Error
		static constexpr uint8_t U_STATE_IND_SC  = 0x87; // Slave Collision

		//----Definitions — pin map and timing (span include boundaries)----
		#define ESP32_RX         D6
		#define ESP32_TX         D7
		#define TPUART_RESN      D0
		#define TPUART_SAVE      D1
		#define TPUART_TW        D2
		#define BAUDRATE_TPUART  19200
		#define RESPONSE_TIME    10   // ms — wait for TP-UART response
		#define MAX_TELEGRAM_LENGTH 30

		//----Members----
		HardwareSerial uart = HardwareSerial(1);
		KNX_Telegram* p_telegram = nullptr;

		uint8_t rxPin;
		uint8_t txPin;
		uint8_t resnPin;  // Hard-reset pin
		uint8_t savePin;  // LOW when bus voltage absent >1.5 ms
		uint8_t twPin;    // HIGH when TP-UART temperature is too high
		uint16_t baudrate;
		String physicalAddressString;

		static volatile bool _tempWarning;
		static volatile bool _voltageFailure;

		uint8_t rxBuffer[MAX_TELEGRAM_LENGTH];
		uint8_t rxIndex        = 0;
		uint8_t rxExpectedLen  = 0;
		bool _incomingTelegram = false;

		//----Methods----
		// Sends a byte array to the TP-UART over UART.
		void sendCommand(const uint8_t* cmd, uint16_t len);
		// Sets max BUSY/NACK repeat counts on the TP-UART.
		void setMaxRepeatCount(uint8_t busyCount, uint8_t nackCount);
		// Activates CRC checking for received telegrams.
		void activateCRC(void);
		// Flushes all pending bytes from the UART receive buffer.
		void clearBuffer(void);
		// Sends ACK, NACK, or BUSY after address match — timing-critical.
		void acknowledgeInformation(AcknowledgeInfo info);
		// Attaches FALLING/RISING interrupts to SAVE and TW pins.
		void enableInterrupts(void);
		// Resets incoming telegram state machine.
		void IncomingReset(void);

		static void ISR_voltageFailure(void);
		static void ISR_tempWarning(void);

	#ifdef _MSC_VER
		#pragma endregion
		#pragma region public
	#endif
	public:
		//----Constructor----
		/**
		 * @brief Constructs the TP-UART2 driver and configures hardware pins.
		 * @param physicalAddress Physical address of this device (e.g. "1.1.5").
		*/
		KNX_TPUART2(String physicalAddress);

		//----Methods----
		/**
		 * @brief Initialises the UART peripheral (8E1, configured RX/TX pins, baud rate).
		*/
		void begin(void);

		/**
		 * @brief Injects the KNX_Telegram pointer for forwarding received frames.
		 * @param telegram Pointer to the active KNX_Telegram instance.
		*/
		void attachTelegramPointer(KNX_Telegram* telegram);

		/**
		 * @brief Sends a Reset.request and waits for Reset.indication; falls back to hard reset.
		 * @return true if Reset.indication was received.
		*/
		bool resetRequest(void);

		/**
		 * @brief Sends a State.request and returns the raw State.indication byte.
		 * @return State byte (0x07 = OK, 0xFF = no response), see TP-UART2 datasheet for error codes.
		*/
		uint8_t stateRequest(void);

		/**
		 * @brief Activates bus monitor mode on the TP-UART.
		*/
		void activateBusmonitorMode(void);

		/**
		 * @brief Requests the product ID from the TP-UART.
		 * @return Product ID byte (0x41 for Release A, 0xFF if no response).
		*/
		uint8_t productIDRequest(void);

		/**
		 * @brief Activates BUSY mode: the TP-UART replies BUSY to addressed telegrams for up to 700 ms.
		*/
		void activateBusyMode(void);

		/**
		 * @brief Deactivates BUSY mode.
		*/
		void resetBusyMode(void);

		/**
		 * @brief Sends the physical address stored in physicalAddress to the TP-UART.
		*/
		void setPhysicalAddress(void);

		/**
		 * @brief Reads pending UART bytes, reassembles KNX frames, and forwards complete telegrams.
		 * @return true if a complete telegram was delivered to the KNX_Telegram layer.
		*/
		bool checkUART(void);

		/**
		 * @brief Returns and clears the bus voltage failure flag set by the SAVE interrupt.
		 * @return true if a voltage failure event occurred since last call.
		*/
		bool watchBusVoltage(void);

		/**
		 * @brief Returns and clears the temperature warning flag set by the TW interrupt.
		 * @return true if a temperature warning event occurred since last call.
		*/
		bool watchTemperature(void);

		/**
		 * @brief Sends a raw KNX telegram byte buffer via the TP-UART L_Data framing.
		 * @param buffer Pointer to the telegram bytes.
		 * @param length Number of bytes to send.
		 * @return true (always — no per-byte ACK checked here).
		*/
		bool sendTelegram(uint8_t* buffer, uint8_t length);

		//----Public data----
		PhysicalAddress physicalAddress;
};
#ifdef _MSC_VER
	#pragma endregion
#endif
