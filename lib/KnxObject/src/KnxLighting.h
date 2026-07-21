#pragma once
/**
 * @name KnxLighting.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Lighting device objects: KnxLight (on/off), KnxDimmLight (on/off plus relative
 *          brighter/darker dimming), and KnxRGB (colour). Construct one with the group
 *          address(es) it uses, then call its methods to command the light and onUpdate() to be
 *          notified of status changes from the bus.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A switchable light. Send on/off with on()/off()/toggle(), read the current state with
 *        isOn(), and get notified of changes with onUpdate(). toggle() flips the light relative
 *        to the last state reported by the bus, so it stays correct even when the light is also
 *        switched elsewhere.
*/
class KnxLight : public KnxObject {
	private:
		void (*p_onChange)(bool state) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asBool());
		}

	public:
		/**
		 * @brief Creates a light that commands on one group address and reads status on another.
		 * @param knx       The bus node this light belongs to.
		 * @param cmdGa      Group address on/off commands are sent to.
		 * @param statusGa   Group address the light's status is reported on.
		*/
		KnxLight(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT1) {}
		/**
		 * @brief Creates a light that uses one group address for both commands and status.
		 * @param knx  The bus node this light belongs to.
		 * @param ga   Group address used to command and to read status.
		*/
		KnxLight(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT1) {}

		/** @brief Switches the light on. @return true if the bus confirmed the command. */
		bool on(void)          { return write(Dpt1(true)); }
		/** @brief Switches the light off. @return true if the bus confirmed the command. */
		bool off(void)         { return write(Dpt1(false)); }
		/** @brief Switches the light to a given state.
		 *  @param state true = on, false = off. @return true if the bus confirmed the command. */
		bool set(bool state)   { return write(Dpt1(state)); }
		/** @brief Flips the light between on and off. @return true if the bus confirmed the command. */
		bool toggle(void)      { return set(!isOn()); }
		/** @brief The light's current on/off state, from the last bus update (no bus traffic).
		 *  @return true if the light is on. */
		bool isOn(void) const  { return cachedValue.asBool(); }

		/**
		 * @brief Registers a function to call whenever the light's status changes on the bus.
		 * @param callback Called with the new state (true = on). Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(bool state)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates a light that commands on one group address and reads status on another.
		 * @param knx       The bus node this light belongs to.
		 * @param cmdGa      Command group address as "main/middle/sub", e.g. "0/1/1".
		 * @param statusGa   Status group address as "main/middle/sub", e.g. "0/3/0".
		*/
		KnxLight(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT1) {}
		/**
		 * @brief Creates a light that uses one group address for both commands and status.
		 * @param knx  The bus node this light belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/1/1".
		*/
		KnxLight(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT1) {}
#endif
};

/**
 * @brief A dimmable light. It is a KnxLight — on()/off()/toggle()/isOn()/onUpdate() all work the
 *        same — with brighter()/darker()/stopDimming() added for step dimming on a third group
 *        address. Dimming is relative: each call nudges the light up or down, and stopDimming()
 *        ends a ramp that is still running.
*/
class KnxDimmLight : public KnxLight {
	private:
		uint16_t dimGa;

	public:
		/**
		 * @brief Creates a dimmable light with separate switch, dimming and status addresses.
		 * @param knx        The bus node this light belongs to.
		 * @param switchGa    Group address on/off commands are sent to.
		 * @param dimmingGa   Group address relative brighter/darker steps are sent to.
		 * @param statusGa    Group address the light's on/off status is reported on.
		*/
		KnxDimmLight(KnxCoordinator& knx, uint16_t switchGa, uint16_t dimmingGa, uint16_t statusGa)
			: KnxLight(knx, switchGa, statusGa), dimGa(dimmingGa) {}
		/**
		 * @brief Creates a dimmable light whose status is read on its switch address.
		 * @param knx        The bus node this light belongs to.
		 * @param switchGa    Group address on/off commands are sent to (and status read from).
		 * @param dimmingGa   Group address relative brighter/darker steps are sent to.
		*/
		KnxDimmLight(KnxCoordinator& knx, uint16_t switchGa, uint16_t dimmingGa)
			: KnxLight(knx, switchGa), dimGa(dimmingGa) {}

		/**
		 * @brief Steps the light brighter.
		 * @param step How large a step to take; defaults to the largest (a full step).
		 * @return true if the bus confirmed the command.
		*/
		bool brighter(DimmIncrement step = DimmIncrement::Percent_100) {
			return p_knx->send(dimGa, Dpt3(true, (uint8_t)step));
		}
		/**
		 * @brief Steps the light darker.
		 * @param step How large a step to take; defaults to the largest (a full step).
		 * @return true if the bus confirmed the command.
		*/
		bool darker(DimmIncrement step = DimmIncrement::Percent_100) {
			return p_knx->send(dimGa, Dpt3(false, (uint8_t)step));
		}
		/** @brief Stops a dimming ramp that is currently running.
		 *  @return true if the bus confirmed the command. */
		bool stopDimming(void) {
			return p_knx->send(dimGa, Dpt3(true, (uint8_t)DimmIncrement::Stop));
		}

#ifdef ARDUINO
		/**
		 * @brief Creates a dimmable light with separate switch, dimming and status addresses.
		 * @param knx        The bus node this light belongs to.
		 * @param switchGa    Switch group address as "main/middle/sub", e.g. "0/1/1".
		 * @param dimmingGa   Dimming group address as "main/middle/sub", e.g. "0/1/2".
		 * @param statusGa    Status group address as "main/middle/sub", e.g. "0/3/0".
		*/
		KnxDimmLight(KnxCoordinator& knx, String switchGa, String dimmingGa, String statusGa)
			: KnxLight(knx, switchGa, statusGa),
			  dimGa(packedGroupAddressFromString(dimmingGa)) {}
		/**
		 * @brief Creates a dimmable light whose status is read on its switch address.
		 * @param knx        The bus node this light belongs to.
		 * @param switchGa    Switch group address as "main/middle/sub", e.g. "0/1/1".
		 * @param dimmingGa   Dimming group address as "main/middle/sub", e.g. "0/1/2".
		*/
		KnxDimmLight(KnxCoordinator& knx, String switchGa, String dimmingGa)
			: KnxLight(knx, switchGa),
			  dimGa(packedGroupAddressFromString(dimmingGa)) {}
#endif
};

/**
 * @brief An RGB colour light. Set a colour with set(r, g, b), read the last colour with color(),
 *        and get notified of changes with onUpdate(). Each channel is 0–255.
*/
class KnxRGB : public KnxObject {
	private:
		void (*p_onChange)(uint8_t r, uint8_t g, uint8_t b) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) {
				DptColor c = cachedValue.asColor();
				p_onChange(c.r, c.g, c.b);
			}
		}

	public:
		/**
		 * @brief Creates an RGB light that commands on one group address and reads status on another.
		 * @param knx       The bus node this light belongs to.
		 * @param cmdGa      Group address colours are sent to.
		 * @param statusGa   Group address the colour status is reported on.
		*/
		KnxRGB(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT232) {}
		/**
		 * @brief Creates an RGB light that uses one group address for both commands and status.
		 * @param knx  The bus node this light belongs to.
		 * @param ga   Group address used to set and to read the colour.
		*/
		KnxRGB(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KnxDpt::DPT232) {}

		/**
		 * @brief Sets the light's colour.
		 * @param r Red channel, 0–255.
		 * @param g Green channel, 0–255.
		 * @param b Blue channel, 0–255.
		 * @return true if the bus confirmed the command.
		*/
		bool set(uint8_t r, uint8_t g, uint8_t b) { return write(Dpt232(r, g, b)); }
		/** @brief The light's current colour, from the last bus update (no bus traffic).
		 *  @return The colour as an {r, g, b} value. */
		DptColor color(void) const { return cachedValue.asColor(); }

		/**
		 * @brief Registers a function to call whenever the colour changes on the bus.
		 * @param callback Called with the new r, g, b channels. Pass nullptr to remove it.
		*/
		void onUpdate(void (*callback)(uint8_t r, uint8_t g, uint8_t b)) { p_onChange = callback; }

#ifdef ARDUINO
		/**
		 * @brief Creates an RGB light that commands on one group address and reads status on another.
		 * @param knx       The bus node this light belongs to.
		 * @param cmdGa      Command group address as "main/middle/sub", e.g. "0/5/1".
		 * @param statusGa   Status group address as "main/middle/sub", e.g. "0/5/2".
		*/
		KnxRGB(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KnxDpt::DPT232) {}
		/**
		 * @brief Creates an RGB light that uses one group address for both commands and status.
		 * @param knx  The bus node this light belongs to.
		 * @param ga   Group address as "main/middle/sub", e.g. "0/5/1".
		*/
		KnxRGB(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KnxDpt::DPT232) {}
#endif
};
