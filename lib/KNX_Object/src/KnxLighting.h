#pragma once
/**
 * @name KnxLighting.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Lighting intent objects (PLAN §10): KnxLight (DPT1 on/off), KnxDimmLight
 *          (DPT1 switching + DPT3 relative dimming), KnxRGB (DPT232 colour). Each hides its
 *          DPT and routes through the KnxObject / KNX::send() encode path, so the wire format
 *          is the one the codec tests already cover.
*/

//---- Custom module headers ----
#include "KnxObject.h"

/**
 * @brief A switchable light (DPT1). Sends on/off to the command GA and tracks the status GA;
 *        toggle() flips relative to the cached (status-fed) state, not just what was last sent.
*/
class KnxLight : public KnxObject {
	private:
		void (*p_onChange)(bool state) = nullptr;

	protected:
		void onValueUpdated(void) override {
			if (p_onChange) p_onChange(cachedValue.asBool());
		}

	public:
		KnxLight(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT1) {}
		KnxLight(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KNX_DPT::DPT1) {}

		/** @brief Switches the light on. */
		bool on(void)          { return write(Dpt1(true)); }
		/** @brief Switches the light off. */
		bool off(void)         { return write(Dpt1(false)); }
		/** @brief Sets the light to an explicit state. */
		bool set(bool state)   { return write(Dpt1(state)); }
		/** @brief Flips the light relative to the cached state. */
		bool toggle(void)      { return set(!isOn()); }
		/** @brief Cached on/off state (no bus traffic). */
		bool isOn(void) const  { return cachedValue.asBool(); }

		/** @brief Registers a callback fired with the decoded state on every status update. */
		void onUpdate(void (*callback)(bool state)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxLight(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT1) {}
		KnxLight(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KNX_DPT::DPT1) {}
#endif
};

/**
 * @brief A dimmable light: DPT1 switching (inherited from KnxLight, cmd + status) plus a
 *        separate DPT3 control-dimming GA for relative brighter/darker/stop commands.
*/
class KnxDimmLight : public KnxLight {
	private:
		uint16_t dimGa;   // packed GA for DPT3 relative dimming

	public:
		KnxDimmLight(KnxCoordinator& knx, uint16_t switchGa, uint16_t dimmingGa, uint16_t statusGa)
			: KnxLight(knx, switchGa, statusGa), dimGa(dimmingGa) {}
		KnxDimmLight(KnxCoordinator& knx, uint16_t switchGa, uint16_t dimmingGa)
			: KnxLight(knx, switchGa), dimGa(dimmingGa) {}

		/** @brief Sends a relative brighten step (DPT3). Default is a full 100 % interval. */
		bool brighter(DimmIncrement step = DimmIncrement::Percent_100) {
			return p_knx->send(dimGa, Dpt3(true, (uint8_t)step));
		}
		/** @brief Sends a relative darken step (DPT3). Default is a full 100 % interval. */
		bool darker(DimmIncrement step = DimmIncrement::Percent_100) {
			return p_knx->send(dimGa, Dpt3(false, (uint8_t)step));
		}
		/** @brief Sends the DPT3 break/stop command, ending a running dim ramp. */
		bool stopDimming(void) {
			return p_knx->send(dimGa, Dpt3(true, (uint8_t)DimmIncrement::Stop));
		}

#ifdef ARDUINO
		KnxDimmLight(KnxCoordinator& knx, String switchGa, String dimmingGa, String statusGa)
			: KnxLight(knx, switchGa, statusGa),
			  dimGa(packedGroupAddressFromString(dimmingGa)) {}
		KnxDimmLight(KnxCoordinator& knx, String switchGa, String dimmingGa)
			: KnxLight(knx, switchGa),
			  dimGa(packedGroupAddressFromString(dimmingGa)) {}
#endif
};

/**
 * @brief An RGB colour object (DPT232.600). Sends a 3-byte R/G/B value and caches the last
 *        colour seen on the status GA.
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
		KnxRGB(KnxCoordinator& knx, uint16_t cmdGa, uint16_t statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT232) {}
		KnxRGB(KnxCoordinator& knx, uint16_t ga)
			: KnxObject(knx, ga, KNX_DPT::DPT232) {}

		/** @brief Sends an RGB colour. */
		bool set(uint8_t r, uint8_t g, uint8_t b) { return write(Dpt232(r, g, b)); }
		/** @brief Cached colour (no bus traffic). */
		DptColor color(void) const { return cachedValue.asColor(); }

		/** @brief Registers a callback fired with the decoded colour on every status update. */
		void onUpdate(void (*callback)(uint8_t r, uint8_t g, uint8_t b)) { p_onChange = callback; }

#ifdef ARDUINO
		KnxRGB(KnxCoordinator& knx, String cmdGa, String statusGa)
			: KnxObject(knx, cmdGa, statusGa, KNX_DPT::DPT232) {}
		KnxRGB(KnxCoordinator& knx, String ga)
			: KnxObject(knx, ga, KNX_DPT::DPT232) {}
#endif
};
