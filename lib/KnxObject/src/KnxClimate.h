#pragma once
/**
 * @name KnxClimate.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Climate value objects: KnxTemperature and KnxHumidity. A sensor sketch calls set()
 *          to publish a reading; a display sketch registers onUpdate() to receive one. Both read
 *          the last value back with their named getter.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A temperature reading in degrees Celsius. Publish one with set(), read the last value
 *        with temperature(), and be notified of new readings with onUpdate().
*/
class KnxTemperature : public KnxObject {
	private:
		void (*p_onChange)(float celsius) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		/**
		 * @brief Creates a temperature value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address readings are published to.
		 * @param statusGa   Group address readings are received on.
		*/
		KnxTemperature(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT9) {}
		/**
		 * @brief Creates a temperature value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive readings.
		*/
		KnxTemperature(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT9) {}

		/** @brief Publishes a temperature reading.
		 *  @param celsius Temperature in degrees Celsius. @return true if the bus confirmed it. */
		bool set(float celsius) { return write(Dpt9(celsius)); }
		/** @brief The last temperature reading in degrees Celsius (no bus traffic).
		 *  @return The cached temperature. */
		float temperature(void) const { return cachedValue.asFloat(); }

		/**
		 * @brief Registers a function to call whenever a new temperature arrives on the bus.
		 * @param callback Called with the reading in degrees Celsius. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(float celsius)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a temperature value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/6/1".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/6/2".
		*/
		KnxTemperature(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT9) {}
		/**
		 * @brief Creates a temperature value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/6/1".
		*/
		KnxTemperature(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT9) {}
#endif
};

/**
 * @brief A relative-humidity reading in percent. Publish one with set(), read the last value
 *        with humidity(), and be notified of new readings with onUpdate().
*/
class KnxHumidity : public KnxObject {
	private:
		void (*p_onChange)(float percent) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asFloat());
		}

	public:
		/**
		 * @brief Creates a humidity value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address readings are published to.
		 * @param statusGa   Group address readings are received on.
		*/
		KnxHumidity(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT9) {}
		/**
		 * @brief Creates a humidity value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive readings.
		*/
		KnxHumidity(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT9) {}

		/** @brief Publishes a relative-humidity reading.
		 *  @param percent Relative humidity in percent. @return true if the bus confirmed it. */
		bool set(float percent) { return write(Dpt9(percent)); }
		/** @brief The last relative-humidity reading in percent (no bus traffic).
		 *  @return The cached humidity. */
		float humidity(void) const { return cachedValue.asFloat(); }

		/**
		 * @brief Registers a function to call whenever a new humidity reading arrives on the bus.
		 * @param callback Called with the reading in percent. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(float percent)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a humidity value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/6/3".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/6/4".
		*/
		KnxHumidity(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT9) {}
		/**
		 * @brief Creates a humidity value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/6/3".
		*/
		KnxHumidity(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT9) {}
#endif
};
