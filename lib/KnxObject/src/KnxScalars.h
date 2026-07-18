#pragma once
/**
 * @name KnxScalars.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Scalar value objects (PLAN §10): KnxPercent (DPT5.001, 0..100 %), KnxChar (DPT4),
 *          KnxFloat (DPT14). KnxPercent presents a 0..100 % API and marshals to/from the DPT5
 *          0..255 raw range internally (Arduino-free integer rounding, not Arduino map()).
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A scaled percentage (DPT5.001). The public API is 0..100 %; on the wire it is the
 *        DPT5 0..255 raw range, converted here with rounding.
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
		KnxPercent(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT5) {}
		KnxPercent(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT5) {}

		/** @brief Publishes a percentage in the range 0..100 %. */
		bool set(uint8_t percent) { return write(Dpt5(rawFromPercent(percent))); }
		/** @brief Cached percentage in the range 0..100 % (no bus traffic). */
		uint8_t percent(void) const { return percentFromRaw(cachedValue.asU8()); }

		/** @brief Registers a callback fired with the decoded percentage on every update. */
		void onUpdate(void (*callback)(uint8_t percent)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxPercent(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT5) {}
		KnxPercent(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT5) {}
#endif
};

/**
 * @brief A single ASCII character (DPT4).
*/
class KnxChar : public KnxObject {
	private:
		void (*p_onChange)(char character) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asChar());
		}

	public:
		KnxChar(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT4) {}
		KnxChar(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT4) {}

		/** @brief Publishes a single ASCII character. */
		bool set(char character) { return write(Dpt4(character)); }
		/** @brief Cached character (no bus traffic). */
		char character(void) const { return cachedValue.asChar(); }

		/** @brief Registers a callback fired with the decoded character on every update. */
		void onUpdate(void (*callback)(char character)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxChar(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT4) {}
		KnxChar(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT4) {}
#endif
};

/**
 * @brief A 32-bit IEEE float value (DPT14).
*/
class KnxFloat : public KnxObject {
	private:
		void (*p_onChange)(float value) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		KnxFloat(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT14) {}
		KnxFloat(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT14) {}

		/** @brief Publishes a 32-bit float value. */
		bool set(float value) { return write(Dpt14(value)); }
		/** @brief Cached float value (no bus traffic). */
		float value(void) const { return cachedValue.asFloat(); }

		/** @brief Registers a callback fired with the decoded value on every update. */
		void onUpdate(void (*callback)(float value)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxFloat(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT14) {}
		KnxFloat(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT14) {}
#endif
};
