#pragma once
/**
 * @name KnxClimate.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Climate value objects (PLAN §10): KnxTemperature and KnxHumidity, both DPT9
 *          (2-octet float). A sensor node calls set() to publish a reading; a display node
 *          registers onUpdate() to receive one. Named methods keep the two intents distinct.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A temperature value (DPT9). set() publishes a reading in degrees Celsius; the cache
 *        and onUpdate() callback surface the last value seen on the status GA.
*/
class KnxTemperature : public KnxObject {
	private:
		void (*p_onChange)(float celsius) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		KnxTemperature(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT9) {}
		KnxTemperature(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KNX_DPT::DPT9) {}

		/** @brief Publishes a temperature reading in degrees Celsius. */
		bool set(float celsius) { return write(Dpt9(celsius)); }
		/** @brief Cached temperature in degrees Celsius (no bus traffic). */
		float temperature(void) const { return cachedValue.asFloat(); }

		/** @brief Registers a callback fired with the decoded temperature on every update. */
		void onUpdate(void (*callback)(float celsius)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxTemperature(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT9) {}
		KnxTemperature(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KNX_DPT::DPT9) {}
#endif
};

/**
 * @brief A relative-humidity value (DPT9). set() publishes a reading in percent; the cache
 *        and onUpdate() callback surface the last value seen on the status GA.
*/
class KnxHumidity : public KnxObject {
	private:
		void (*p_onChange)(float percent) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		KnxHumidity(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT9) {}
		KnxHumidity(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KNX_DPT::DPT9) {}

		/** @brief Publishes a relative-humidity reading in percent. */
		bool set(float percent) { return write(Dpt9(percent)); }
		/** @brief Cached relative humidity in percent (no bus traffic). */
		float humidity(void) const { return cachedValue.asFloat(); }

		/** @brief Registers a callback fired with the decoded humidity on every update. */
		void onUpdate(void (*callback)(float percent)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxHumidity(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT9) {}
		KnxHumidity(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KNX_DPT::DPT9) {}
#endif
};
