#pragma once
/**
 * @name KNX.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details The library's single public include. One line gives a sketch the whole surface:
 *
 *              #include <KNX.h>
 *
 *          It pulls in the concrete bus driver (KNX_Driver), the coordinator (KNX), the value
 *          currency (KnxValue + the Dpt*() factories), and every intent object grouped by
 *          domain — so user code writes only `KNX_Driver`, `KNX`, `KnxLight`, `Dpt9(..)`, etc.
 *
 *          Layering note: the coordinator CLASS lives in KnxCoordinator.h, not here. This
 *          facade sits at the top of the dependency DAG (nothing includes it), so it can pull
 *          the whole stack without creating a cycle — the coordinator never includes the object
 *          or driver headers (object -> coordinator is the only arrow). Include a single domain
 *          header (e.g. KnxLighting.h) or KnxCoordinator.h directly for a smaller surface.
*/

//---- Concrete bus driver (ATTiny / TP-UART, target-only) ----
#include "KNX_Driver.h"

//---- Coordinator + value currency ----
#include "KnxCoordinator.h"   // class KNX: send(ga, KnxValue), receiver registry, handleUART
#include "KnxValue.h"         // KnxValue + Dpt1(..)..Dpt232(..) factories

//---- Stateful objects, by domain ----
#include "KnxObject.h"        // raw tier + IKnxReceiver base
#include "KnxLighting.h"      // KnxLight, KnxDimmLight, KnxRGB
#include "KnxCovers.h"        // KnxBlind
#include "KnxClimate.h"       // KnxTemperature, KnxHumidity
#include "KnxDateTime.h"      // KnxTime, KnxDate, KnxDateTime
#include "KnxScalars.h"       // KnxPercent, KnxChar, KnxFloat
