#pragma once
/**
 * @name Konnextor.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The only header a sketch includes. It brings in the whole KNX library — the bus node,
 *          the value types (KnxValue and the Dpt*() factories), and every device object (KnxLight,
 *          KnxBlind, KnxTemperature, …) — so a sketch needs just:
 *
 *              #include <Konnextor.h>
 *              Konnextor knx("1.1.5");                 // this device's KNX address
 *              KnxLight kitchen(knx, "0/1/1", "0/3/0");
*/

//---- Bus driver ----
#include "KnxDriver.h"

//---- Bus node + value types ----
#include "KnxCoordinator.h"
#include "KnxValue.h"         // KnxValue + Dpt1(..)..Dpt232(..) factories

//---- Device objects, by domain ----
#include "KnxObject.h"        // generic object tier
#include "KnxLighting.h"      // KnxLight, KnxDimmLight, KnxRGB
#include "KnxCovers.h"        // KnxBlind
#include "KnxClimate.h"       // KnxTemperature, KnxHumidity
#include "KnxDateTime.h"      // KnxTime, KnxDate, KnxDateTime
#include "KnxScalars.h"       // KnxPercent, KnxChar, KnxFloat

/**
 * @brief Your device on the KNX bus. Create one per sketch from this device's physical address;
 *        it drives the transceiver for you and every device object (KnxLight, KnxBlind, …) is
 *        attached to it. Call begin() once in setup() and loop() every iteration.
*/
class Konnextor : public KnxCoordinator {
	private:
		KnxDriver driverImpl;

	public:
		/**
		 * @brief Creates the bus node for this device.
		 * @param physicalAddress This device's KNX physical address as "area.line.device",
		 *                        e.g. "1.1.5".
		*/
		explicit Konnextor(const String& physicalAddress)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress) {}
};
