#pragma once
/**
 * @name KNX_Defines.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Backward-compatibility shim. The shared vocabulary moved to KNX_Common.h and
 *          its split headers. Existing modules that still #include "KNX_Defines.h" keep
 *          working; new code should include "KNX_Common.h" (or a finer header) directly.
*/
#include "KNX_Common.h"
