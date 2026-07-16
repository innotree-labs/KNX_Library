#pragma once
/**
 * @name KNX_Common.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Umbrella header for the shared KNX vocabulary: enums, addresses, passive
 *          telegram/event types and the dependency-inversion interfaces. Include this
 *          from any Arduino-side KNX module. The host-testable value codec (KNX_Value)
 *          deliberately depends on the finer-grained, Arduino-free headers instead.
 *
 *          (Renamed from KNX_Defines — "Defines" implied #defines, but this holds types
 *          and contracts. A KNX_Defines.h shim preserves the old include path.)
*/

// #define DEBUG	// Keep commented out for production — enables Serial debug output across all modules

//---- Shared vocabulary ----
#include "KnxEnums.h"
#include "KnxAddress.h"
#include "KnxTelegramTypes.h"
#include "KnxInterfaces.h"
