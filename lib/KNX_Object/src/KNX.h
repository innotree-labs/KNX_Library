#pragma once
/**
 * @name KNX.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The library's single public include. One line gives a sketch the whole surface:
 *
 *              #include <KNX.h>
 *              KNX knx("1.1.5");            // driver hidden; address typed once
 *              KnxLight kitchen(knx, "0/1/1", "0/3/0");
 *
 *          It pulls in the concrete bus driver (KNX_Driver), the coordinator core, the value
 *          currency (KnxValue + the Dpt*() factories), and every intent object grouped by domain,
 *          then defines the user-facing `KNX` node class below.
 *
 *          Layering note: the dependency-injection CORE is `KnxCoordinator` (KnxCoordinator.h) —
 *          Arduino-free, host-testable with a mock driver, and the type intent objects reference.
 *          The `KNX` class here is a thin Arduino subclass that OWNS a concrete KNX_Driver, so the
 *          user never instantiates or injects a driver. This facade sits at the top of the
 *          dependency DAG (nothing includes it), so it can bundle the whole stack without a cycle.
 *          Advanced users can still inject their own IKnxDriver by using `KnxCoordinator` directly.
*/

//---- Concrete bus driver (ATTiny / TP-UART, target-only) ----
#include "KNX_Driver.h"

//---- Coordinator + value currency ----
#include "KnxCoordinator.h"   // dependency-injection core (KnxCoordinator)
#include "KnxValue.h"         // KnxValue + Dpt1(..)..Dpt232(..) factories

//---- Stateful objects, by domain ----
#include "KnxObject.h"        // raw tier + IKnxReceiver base
#include "KnxLighting.h"      // KnxLight, KnxDimmLight, KnxRGB
#include "KnxCovers.h"        // KnxBlind
#include "KnxClimate.h"       // KnxTemperature, KnxHumidity
#include "KnxDateTime.h"      // KnxTime, KnxDate, KnxDateTime
#include "KnxScalars.h"       // KnxPercent, KnxChar, KnxFloat

/**
 * @brief End-user KNX node: a KnxCoordinator that owns its concrete bus driver, built from just
 *        this device's physical address. The driver is hidden — you give the address once and it
 *        reaches both the frame source (KnxCoordinator) and the transceiver (KNX_Driver) inside.
 *        Because it IS-A KnxCoordinator, intent objects (KnxLight, …) accept it directly.
*/
class KNX : public KnxCoordinator {
	private:
		KNX_Driver driverImpl;   // concrete link-layer driver, owned by this node

	public:
		/**
		 * @brief Constructs the node from its physical address, e.g. KNX knx("1.1.5").
		 * @param physicalAddress "area.line.device" string identifying this device on the bus.
		*/
		explicit KNX(const String& physicalAddress)
			: KnxCoordinator(&driverImpl, physicalAddress), driverImpl(physicalAddress) {}
};
