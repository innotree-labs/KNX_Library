#pragma once
/**
 * @name KnxDateTime.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Date and time value objects: KnxTime (time of day), KnxDate (calendar date) and
 *          KnxDateTime (both together). Publish with set(), read the last value with the named
 *          getter, and be notified of changes with onUpdate(). Each value is a small struct
 *          (DptTime, DptDate, DptDateTime) defined in KnxValue.h.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A time of day (weekday, hour, minute, second). Publish one with set(), read the last
 *        value with time(), and be notified of changes with onUpdate().
*/
class KnxTime : public KnxObject {
	private:
		void (*p_onChange)(DptTime time) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asTime());
		}

	public:
		/**
		 * @brief Creates a time value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the time is published to.
		 * @param statusGa   Group address the time is received on.
		*/
		KnxTime(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT10) {}
		/**
		 * @brief Creates a time value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the time.
		*/
		KnxTime(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT10) {}

		/** @brief Publishes a time of day. @param time The time to send.
		 *  @return true if the bus confirmed it. */
		bool set(DptTime time) { return write(Dpt10(time)); }
		/** @brief The last time of day received (no bus traffic). @return The cached time. */
		DptTime time(void) const { return cachedValue.asTime(); }

		/**
		 * @brief Registers a function to call whenever a new time arrives on the bus.
		 * @param callback Called with the new time. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(DptTime time)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a time value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/7/1".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/7/2".
		*/
		KnxTime(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT10) {}
		/**
		 * @brief Creates a time value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/7/1".
		*/
		KnxTime(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT10) {}
#endif
};

/**
 * @brief A calendar date (day, month, year). Publish one with set(), read the last value with
 *        date(), and be notified of changes with onUpdate().
*/
class KnxDate : public KnxObject {
	private:
		void (*p_onChange)(DptDate date) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asDate());
		}

	public:
		/**
		 * @brief Creates a date value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the date is published to.
		 * @param statusGa   Group address the date is received on.
		*/
		KnxDate(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT11) {}
		/**
		 * @brief Creates a date value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the date.
		*/
		KnxDate(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT11) {}

		/** @brief Publishes a calendar date. @param date The date to send.
		 *  @return true if the bus confirmed it. */
		bool set(DptDate date) { return write(Dpt11(date)); }
		/** @brief The last calendar date received (no bus traffic). @return The cached date. */
		DptDate date(void) const { return cachedValue.asDate(); }

		/**
		 * @brief Registers a function to call whenever a new date arrives on the bus.
		 * @param callback Called with the new date. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(DptDate date)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a date value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/7/3".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/7/4".
		*/
		KnxDate(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT11) {}
		/**
		 * @brief Creates a date value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/7/3".
		*/
		KnxDate(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT11) {}
#endif
};

/**
 * @brief A combined date and time, with daylight-saving and fault flags. Publish one with set(),
 *        read the last value with dateTime(), and be notified of changes with onUpdate().
*/
class KnxDateTime : public KnxObject {
	private:
		void (*p_onChange)(DptDateTime datetime) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asDateTime());
		}

	public:
		/**
		 * @brief Creates a date-time value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Group address the date-time is published to.
		 * @param statusGa   Group address the date-time is received on.
		*/
		KnxDateTime(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT19) {}
		/**
		 * @brief Creates a date-time value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address used to publish and to receive the date-time.
		*/
		KnxDateTime(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT19) {}

		/** @brief Publishes a combined date and time. @param datetime The date and time to send.
		 *  @return true if the bus confirmed it. */
		bool set(DptDateTime datetime) { return write(Dpt19(datetime)); }
		/** @brief The last date and time received (no bus traffic). @return The cached date-time. */
		DptDateTime dateTime(void) const { return cachedValue.asDateTime(); }

		/**
		 * @brief Registers a function to call whenever a new date-time arrives on the bus.
		 * @param callback Called with the new date and time. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(DptDateTime datetime)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a date-time value that publishes on one address and reads on another.
		 * @param knx       The bus node this value belongs to.
		 * @param cmdGa      Publish group address as "main/middle/sub", e.g. "0/7/5".
		 * @param statusGa   Receive group address as "main/middle/sub", e.g. "0/7/6".
		*/
		KnxDateTime(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT19) {}
		/**
		 * @brief Creates a date-time value that uses one address to publish and receive.
		 * @param knx  The bus node this value belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/7/5".
		*/
		KnxDateTime(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT19) {}
#endif
};
