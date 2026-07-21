#pragma once
/**
 * @name KnxScalars.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Scalar value objects: KnxPercent (a 0–100 % value), KnxChar (a single character) and
 *          KnxFloat (a 32-bit number). Publish with set(), read the last value with the named
 *          getter, and be notified of changes with onUpdate().
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A percentage from 0 to 100. Publish one with set(), read the last value with percent(),
 *        and be notified of changes with onUpdate().
*/
class KnxPercent : public KnxObject {
	private:
		void (*p_onChange)(uint8_t percent) = nullptr;

		// DPT5.001 scaling: 0..100 % <-> 0..255 raw, rounded to nearest.
		static uint8_t rawFromPercent(uint8_t percent) {
			if (percent >= 100) return 255;
			return (uint8_t)((percent * 255u + 50u) / 100u);
		}
		static uint8_t percentFromRaw(uint8_t raw) {
			return (uint8_t)((raw * 100u + 127u) / 255u);
		}

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(percentFromRaw(cachedValue.asU8()));
		}

	public:
		/**
		 * @brief Creates a percentage that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the value is published to.
		 * @param statusGa   Group address the value is received on.
		*/
		KnxPercent(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT5) {}
		/**
		 * @brief Creates a percentage that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the value.
		*/
		KnxPercent(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT5) {}

		/** @brief Publishes a percentage. @param percent A value from 0 to 100.
		 *  @return true if the bus confirmed it. */
		bool set(uint8_t percent) { return write(Dpt5(rawFromPercent(percent))); }
		/** @brief The last percentage received, from 0 to 100 (no bus traffic).
		 *  @return The cached percentage. */
		uint8_t percent(void) const { return percentFromRaw(cachedValue.asU8()); }

		/**
		 * @brief Registers a function to call whenever a new percentage arrives on the bus.
		 * @param callback Called with the value from 0 to 100. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(uint8_t percent)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a percentage that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/2/1".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/2/2".
		*/
		KnxPercent(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT5) {}
		/**
		 * @brief Creates a percentage that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/2/1".
		*/
		KnxPercent(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT5) {}
#endif
};

/**
 * @brief A single character. Publish one with set(), read the last value with character(), and
 *        be notified of changes with onUpdate().
*/
class KnxChar : public KnxObject {
	private:
		void (*p_onChange)(char character) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asChar());
		}

	public:
		/**
		 * @brief Creates a character value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the character is published to.
		 * @param statusGa   Group address the character is received on.
		*/
		KnxChar(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT4) {}
		/**
		 * @brief Creates a character value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the character.
		*/
		KnxChar(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT4) {}

		/** @brief Publishes a single character. @param character The character to send.
		 *  @return true if the bus confirmed it. */
		bool set(char character) { return write(Dpt4(character)); }
		/** @brief The last character received (no bus traffic). @return The cached character. */
		char character(void) const { return cachedValue.asChar(); }

		/**
		 * @brief Registers a function to call whenever a new character arrives on the bus.
		 * @param callback Called with the new character. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(char character)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a character value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/8/1".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/8/2".
		*/
		KnxChar(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT4) {}
		/**
		 * @brief Creates a character value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/8/1".
		*/
		KnxChar(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT4) {}
#endif
};

/**
 * @brief A 32-bit number (float). Publish one with set(), read the last value with value(), and
 *        be notified of changes with onUpdate().
*/
class KnxFloat : public KnxObject {
	private:
		void (*p_onChange)(float value) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		/**
		 * @brief Creates a number value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the number is published to.
		 * @param statusGa   Group address the number is received on.
		*/
		KnxFloat(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT14) {}
		/**
		 * @brief Creates a number value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the number.
		*/
		KnxFloat(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT14) {}

		/** @brief Publishes a number. @param value The value to send.
		 *  @return true if the bus confirmed it. */
		bool set(float value) { return write(Dpt14(value)); }
		/** @brief The last number received (no bus traffic). @return The cached value. */
		float value(void) const { return cachedValue.asFloat(); }

		/**
		 * @brief Registers a function to call whenever a new number arrives on the bus.
		 * @param callback Called with the new value. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(float value)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a number value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/9/1".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/9/2".
		*/
		KnxFloat(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT14) {}
		/**
		 * @brief Creates a number value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/9/1".
		*/
		KnxFloat(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT14) {}
#endif
};
