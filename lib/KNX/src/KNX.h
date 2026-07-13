#pragma once
/**
 * @name KNX.h
 * @date 25.10.2025
 * @authors Florian Wiesner
 * @details High-level KNX coordinator. Owns the TP-UART2 driver and telegram encoder/decoder,
 *          manages group address bindings, and dispatches decoded events to registered callbacks.
*/

#ifdef _MSC_VER
	#pragma region Libraries //--------------------------------------------------------------------------------------------------
#endif
#include <Arduino.h>
#include "KNX_Defines.h"
#include "KNX_TPUART2.h"
#include "KNX_Telegram.h"

#ifdef _MSC_VER
	#pragma endregion
	#pragma region KNX-Class //--------------------------------------------------------------------------------------------------
#endif
class KNX {
	#ifdef _MSC_VER
		#pragma endregion
		#pragma region private
	#endif
	private:
		//----Definitions----
		static constexpr uint8_t MAX_BINDINGS = 32;

		//----Members----
		KNX_Binding bindings[MAX_BINDINGS];
		uint8_t bindingCount = 0;

		//----Methods----
		static void telegramParsedTrampoline(void* ctx, const ParsedTelegram& tg);
		void handleParsedTelegram(ParsedTelegram tg);

	#ifdef _MSC_VER
		#pragma endregion
		#pragma region public
	#endif
	public:
		//----Constructor----
		/**
		 * @brief Constructs the KNX coordinator and wires the internal driver and telegram objects.
		 * @param physicalAddress Physical address of this device on the KNX bus (e.g. "1.1.5").
		*/
		KNX(String physicalAddress)
			: tpuart(physicalAddress), telegram(&tpuart) {
				tpuart.attachTelegramPointer(&telegram);
				telegram.setParsedCallback(&KNX::telegramParsedTrampoline, this);
			}

		//----Public methods----
		/**
		 * @brief Initialises KNX communication: configures UART, resets TP-UART, sets physical address.
		 * @return true if initialisation succeeded and state indication is OK.
		*/
		bool begin(void);

		/**
		 * @brief Resets the TP-UART and re-applies the physical address.
		 * @return true if the reset succeeded and state indication is OK.
		*/
		bool reset(void);

		/**
		 * @brief Checks the SAVE and TW interrupt flags for bus voltage loss or temperature warning.
		 * @return true if any fault condition is active.
		*/
		bool monitorTPUART(void);

		/**
		 * @brief Processes pending UART RX bytes, handles status codes, and dispatches incoming telegrams.
		 * @return true if a complete telegram was received and processed.
		*/
		bool handleUART(void);

		/**
		 * @brief Registers a callback for a group address / DPT pair.
		 * @param gaStr Group address string (e.g. "0/3/0").
		 * @param dpt   Expected DPT used to decode the payload.
		 * @param cb    Callback invoked with the decoded KnxEvent.
		 * @return true if the binding was added successfully.
		*/
		bool bind(String gaStr, KNX_DPT dpt, KNX_Callback cb);

		/**
		 * @brief Sends a DPT1 switch telegram to a light group address.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param state              true = on, false = off.
		 * @return true on success.
		*/
		bool switchLight(String targetGroupAddress, bool state);

		/**
		 * @brief Sends a DPT3 incremental dimming telegram.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param direction          true = brighter, false = darker.
		 * @param increment          Step size for the dimming command.
		 * @return true on success.
		*/
		bool dimLightInterval(String targetGroupAddress, bool direction, DimmIncrement increment);

		/**
		 * @brief Sends a DPT5 absolute dim-value telegram (0–100 % mapped to 0–255).
		 * @param targetGroupAddress Target KNX group address string.
		 * @param dimValue           Brightness percentage (0–100).
		 * @return true on success.
		*/
		bool dimLightValue(String targetGroupAddress, uint8_t dimValue);

		/**
		 * @brief Sends a DPT1 blind open/close telegram.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param direction          true = up/open, false = down/close.
		 * @return true on success.
		*/
		bool blindOpenClose(String targetGroupAddress, bool direction);

		/**
		 * @brief Sends a DPT1 blind stop/step telegram.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param step               true = short press (step), false = stop.
		 * @return true on success.
		*/
		bool blindStopStep(String targetGroupAddress, bool step);

		/**
		 * @brief Sends a DPT5 absolute blind position telegram (0–100 % mapped to 0–255).
		 * @param targetGroupAddress Target KNX group address string.
		 * @param position           Position percentage (0–100).
		 * @return true on success.
		*/
		bool blindPositionValue(String targetGroupAddress, uint8_t position);

		/**
		 * @brief Sends a DPT9 temperature telegram.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param temperature        Temperature value in °C.
		 * @return true on success.
		*/
		bool sendTemperature(String targetGroupAddress, float temperature);

		/**
		 * @brief Sends a DPT9 humidity telegram.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param humidity           Relative humidity in %.
		 * @return true on success.
		*/
		bool sendHumidity(String targetGroupAddress, float humidity);

		/**
		 * @brief Sends a telegram of arbitrary DPT using a raw value pointer.
		 * @param targetGroupAddress Target KNX group address string.
		 * @param dpt                DPT that determines encoding.
		 * @param value              Pointer to the value to encode and send.
		 * @return true on success.
		*/
		bool sendTelegram(String targetGroupAddress, KNX_DPT dpt, void* value);

		//----Public data----
		KNX_TPUART2 tpuart;
		KNX_Telegram telegram;
};
#ifdef _MSC_VER
	#pragma endregion
#endif
