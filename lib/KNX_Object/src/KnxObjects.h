#pragma once
/**
 * @name KnxObjects.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details One-include umbrella for the whole stateful-object surface: the KnxObject base and
 *          every intent class, grouped by domain. Including this transitively pulls in the KNX
 *          coordinator and the KnxValue currency, so a user sketch needs only:
 *
 *              #include <KNX_Driver.h>   // the concrete bus driver
 *              #include <KnxObjects.h>   // coordinator + values + intent objects
 *
 *          The coordinator header (KNX.h) never includes the object headers — the dependency
 *          runs one way (object -> KNX) so the intrusive receiver registry stays acyclic
 *          (PLAN §12). Include a single domain header instead if you want only part of the API.
*/

#include "KnxObject.h"      // raw tier + IKnxReceiver base (pulls in KNX.h + KnxValue)
#include "KnxLighting.h"    // KnxLight, KnxDimmLight, KnxRGB
#include "KnxCovers.h"      // KnxBlind
#include "KnxClimate.h"     // KnxTemperature, KnxHumidity
#include "KnxDateTime.h"    // KnxTime, KnxDate, KnxDateTime
#include "KnxScalars.h"     // KnxPercent, KnxChar, KnxFloat
