#pragma once
/**
 * @name KnxCovers.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Cover device object: KnxBlind. Construct it with the group address that moves the
 *          blind and the one that stops it, then call up()/down() to move and stop() to halt.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A blind or roller shutter. up() and down() start a full move; stop() halts a move that
 *        is still running.
*/
class KnxBlind : public KnxObject {
	private:
		uint16_t stopGa;

	public:
		/**
		 * @brief Creates a blind with separate move and stop addresses.
		 * @param knx         The bus node this blind belongs to.
		 * @param moveGa       Group address up/down moves are sent to.
		 * @param stepStopGa   Group address the stop command is sent to.
		*/
		KnxBlind(KnxCoordinator& knx, uint16_t moveGa, uint16_t stepStopGa)
			: KnxObject(knx, moveGa, KnxDpt::DPT1), stopGa(stepStopGa) {}

		/** @brief Moves the blind down. @return true if the bus confirmed the command. */
		bool down(void) { return write(Dpt1(true)); }
		/** @brief Moves the blind up. @return true if the bus confirmed the command. */
		bool up(void)   { return write(Dpt1(false)); }
		/** @brief Stops a move that is currently running. @return true if the bus confirmed the command. */
		bool stop(void) { return p_knx->send(stopGa, Dpt1(true)); }

#ifdef ARDUINO
		/**
		 * @brief Creates a blind with separate move and stop addresses.
		 * @param knx         The bus node this blind belongs to.
		 * @param moveGa       Move group address as "main/middle/sub", e.g. "0/4/1".
		 * @param stepStopGa   Stop group address as "main/middle/sub", e.g. "0/4/2".
		*/
		KnxBlind(KnxCoordinator& knx, String moveGa, String stepStopGa)
			: KnxObject(knx, packedGroupAddressFromString(moveGa), KnxDpt::DPT1),
			  stopGa(packedGroupAddressFromString(stepStopGa)) {}
#endif
};
