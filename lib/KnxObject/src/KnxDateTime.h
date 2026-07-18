#pragma once
/**
 * @name KnxDateTime.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Date/time value objects (PLAN §10): KnxTime (DPT10), KnxDate (DPT11),
 *          KnxDateTime (DPT19). Each carries its composite struct as the value currency.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A time-of-day value (DPT10.001): weekday + hour/minute/second.
*/
class KnxTime : public KnxObject {
	private:
		void (*p_onChange)(DptTime time) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asTime());
		}

	public:
		KnxTime(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT10) {}
		KnxTime(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT10) {}

		/** @brief Publishes a time of day. */
		bool set(DptTime time) { return write(Dpt10(time)); }
		/** @brief Cached time of day (no bus traffic). */
		DptTime time(void) const { return cachedValue.asTime(); }

		/** @brief Registers a callback fired with the decoded time on every update. */
		void onUpdate(void (*callback)(DptTime time)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxTime(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT10) {}
		KnxTime(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT10) {}
#endif
};

/**
 * @brief A calendar-date value (DPT11.001): day/month/year.
*/
class KnxDate : public KnxObject {
	private:
		void (*p_onChange)(DptDate date) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asDate());
		}

	public:
		KnxDate(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT11) {}
		KnxDate(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT11) {}

		/** @brief Publishes a calendar date. */
		bool set(DptDate date) { return write(Dpt11(date)); }
		/** @brief Cached calendar date (no bus traffic). */
		DptDate date(void) const { return cachedValue.asDate(); }

		/** @brief Registers a callback fired with the decoded date on every update. */
		void onUpdate(void (*callback)(DptDate date)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxDate(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT11) {}
		KnxDate(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT11) {}
#endif
};

/**
 * @brief A combined date + time value (DPT19.001) with validity/status flags.
*/
class KnxDateTime : public KnxObject {
	private:
		void (*p_onChange)(DptDateTime datetime) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asDateTime());
		}

	public:
		KnxDateTime(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT19) {}
		KnxDateTime(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT19) {}

		/** @brief Publishes a combined date and time. */
		bool set(DptDateTime datetime) { return write(Dpt19(datetime)); }
		/** @brief Cached date and time (no bus traffic). */
		DptDateTime dateTime(void) const { return cachedValue.asDateTime(); }

		/** @brief Registers a callback fired with the decoded date-time on every update. */
		void onUpdate(void (*callback)(DptDateTime datetime)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxDateTime(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT19) {}
		KnxDateTime(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT19) {}
#endif
};
