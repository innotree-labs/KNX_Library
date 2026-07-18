#pragma once
/**
 * @name KnxCovers.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Cover intent objects (PLAN §10): KnxBlind (DPT1). Move up/down rides on the command
 *          GA (DPT1.008: 0 = up, 1 = down); a short step/stop rides on a separate GA.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A blind / roller shutter (DPT1). up()/down() send a long move on the command GA;
 *        stop() sends a step/stop on a separate GA to halt a running move.
*/
class KnxBlind : public KnxObject {
	private:
		uint16_t stopGa;   // packed GA for the DPT1 step/stop command

	public:
		KnxBlind(KnxCoordinator& knx, uint16_t moveGa, uint16_t stepStopGa)
			: KnxObject(knx, moveGa, KNX_DPT::DPT1), stopGa(stepStopGa) {}

		/** @brief Moves the blind down (DPT1.008 value 1). */
		bool down(void) { return write(Dpt1(true)); }
		/** @brief Moves the blind up (DPT1.008 value 0). */
		bool up(void)   { return write(Dpt1(false)); }
		/** @brief Halts a running move (step/stop on the separate GA). */
		bool stop(void) { return p_knx->send(stopGa, Dpt1(true)); }

#ifdef ARDUINO
		KnxBlind(KnxCoordinator& knx, String moveGa, String stepStopGa)
			: KnxObject(knx, packedGroupAddressFromString(moveGa), KNX_DPT::DPT1),
			  stopGa(packedGroupAddressFromString(stepStopGa)) {}
#endif
};
