#pragma once
/**
 * @name KNX_Device.h
 * @date 25.10.2025
 * @authors Florian Wiesner
 * @details Button abstraction layer for KNX wall controllers.
 *          Provides single- and two-button base classes with short/long/hold detection,
 *          and concrete subclasses for switching and dimming via KNX.
*/

#ifdef _MSC_VER
	#pragma region Libraries //--------------------------------------------------------------------------------------------------
#endif
#include <Arduino.h>
#include "KNX.h"
#include "KNX_Defines.h"

#ifdef _MSC_VER
	#pragma endregion
	#pragma region SingleButtonOperation //--------------------------------------------------------------------------------------
#endif
class SingleButtonOperation {
	protected:
		//----Members----
		bool lastState      = false;
		bool pressed        = false;
		bool longTriggered  = false;
		bool holdActive     = false;

		uint32_t ts_pressStart = 0;
		uint32_t ts_lastChange = 0;
		uint32_t ts_lastHold   = 0;

		uint32_t debounceTime  = 50;
		uint32_t longPressTime = 400;
		uint32_t holdInterval  = 200;

	public:
		//----Constructor----
		/**
		 * @brief Constructs a SingleButtonOperation with configurable long-press threshold.
		 * @param longPressTimeMs Time in ms before a press is classified as long (default 500).
		*/
		SingleButtonOperation(uint32_t longPressTimeMs = 500)
			: longPressTime(longPressTimeMs) {}

		virtual ~SingleButtonOperation() = default;

		//----Methods----
		/**
		 * @brief Updates the button state machine. Call repeatedly from the main loop.
		 * @param state Current input state (true = pressed, false = released).
		*/
		void update(bool state);

		/**
		 * @brief Overrides the debounce window.
		 * @param debounceTimeMs Debounce time in ms (clamped 10–100).
		*/
		void setDebounceTime(uint32_t debounceTimeMs);

		/**
		 * @brief Overrides the hold-repeat interval.
		 * @param holdIntervalMs Interval in ms between hold events (clamped 100–1000).
		*/
		void setHoldInterval(uint32_t holdIntervalMs);

		/**
		 * @brief Overrides the long-press threshold.
		 * @param longPressTimeMs Threshold in ms (clamped 200–1000).
		*/
		void setLongPressTime(uint32_t longPressTimeMs);

		/** @brief Returns the current debounce window in ms. */
		uint32_t getDebounceTime(void) const { return debounceTime; }

		/** @brief Returns the current hold-repeat interval in ms. */
		uint32_t getHoldInterval(void) const { return holdInterval; }

		/** @brief Returns the current long-press threshold in ms. */
		uint32_t getLongPressTime(void) const { return longPressTime; }

	protected:
		virtual void onShortKeystroke()  = 0;
		virtual void onLongKeystroke()   = 0;
		virtual void onHoldKeystroke()   {}
		virtual void onHoldRelease()     {}
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonOperation //-----------------------------------------------------------------------------------------
#endif
class TwoButtonOperation {
	protected:
		// Wraps SingleButtonOperation and forwards events to TwoButtonOperation parent
		class LeftButton : public SingleButtonOperation {
			TwoButtonOperation* p_parent;
			public:
				LeftButton(TwoButtonOperation* p) : p_parent(p) {}
			protected:
				void onShortKeystroke() override { p_parent->onLeftShortKeystroke(); }
				void onLongKeystroke()  override { p_parent->onLeftLongKeystroke(); }
				void onHoldKeystroke()  override { p_parent->onLeftHoldKeystroke(); }
				void onHoldRelease()    override { p_parent->onLeftHoldRelease(); }
		};

		class RightButton : public SingleButtonOperation {
			TwoButtonOperation* p_parent;
			public:
				RightButton(TwoButtonOperation* p) : p_parent(p) {}
			protected:
				void onShortKeystroke() override { p_parent->onRightShortKeystroke(); }
				void onLongKeystroke()  override { p_parent->onRightLongKeystroke(); }
				void onHoldKeystroke()  override { p_parent->onRightHoldKeystroke(); }
				void onHoldRelease()    override { p_parent->onRightHoldRelease(); }
		};

		LeftButton  left;
		RightButton right;

	public:
		//----Constructor----
		/**
		 * @brief Constructs a TwoButtonOperation and binds the internal button instances.
		*/
		TwoButtonOperation() : left(this), right(this) {}

		virtual ~TwoButtonOperation() = default;

		//----Methods----
		/**
		 * @brief Updates both button state machines. Call repeatedly from the main loop.
		 * @param leftState  Current state of the left input (true = pressed).
		 * @param rightState Current state of the right input (true = pressed).
		*/
		void update(bool leftState, bool rightState);

		/**
		 * @brief Overrides the debounce window for both buttons.
		 * @param debounceTimeMs Debounce time in ms (clamped 10–100).
		*/
		void setDebounceTime(uint32_t debounceTimeMs);

		/**
		 * @brief Overrides the hold-repeat interval for both buttons.
		 * @param holdIntervalMs Interval in ms between hold events (clamped 100–1000).
		*/
		void setHoldInterval(uint32_t holdIntervalMs);

		/**
		 * @brief Overrides the long-press threshold for both buttons.
		 * @param longPressTimeMs Threshold in ms (clamped 200–1000).
		*/
		void setLongPressTime(uint32_t longPressTimeMs);

		/** @brief Returns the shared debounce window in ms. */
		uint32_t getDebounceTime(void) const { return left.getDebounceTime(); }

		/** @brief Returns the shared hold-repeat interval in ms. */
		uint32_t getHoldInterval(void) const { return left.getHoldInterval(); }

		/** @brief Returns the shared long-press threshold in ms. */
		uint32_t getLongPressTime(void) const { return left.getLongPressTime(); }

		/**
		 * @brief Called on a short press of the left button. Must be implemented by subclasses.
		*/
		virtual void onLeftShortKeystroke()  = 0;

		/**
		 * @brief Called once when the left button crosses the long-press threshold. Must be implemented by subclasses.
		*/
		virtual void onLeftLongKeystroke()   = 0;

		/**
		 * @brief Called repeatedly while the left button is held (after the long press). Optional override.
		*/
		virtual void onLeftHoldKeystroke()   {}

		/**
		 * @brief Called when the left button is released after a hold. Optional override.
		*/
		virtual void onLeftHoldRelease()     {}

		/**
		 * @brief Called on a short press of the right button. Must be implemented by subclasses.
		*/
		virtual void onRightShortKeystroke() = 0;

		/**
		 * @brief Called once when the right button crosses the long-press threshold. Must be implemented by subclasses.
		*/
		virtual void onRightLongKeystroke()  = 0;

		/**
		 * @brief Called repeatedly while the right button is held (after the long press). Optional override.
		*/
		virtual void onRightHoldKeystroke()  {}

		/**
		 * @brief Called when the right button is released after a hold. Optional override.
		*/
		virtual void onRightHoldRelease()    {}
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Test helpers //------------------------------------------------------------------------------------------------
#endif
class SingleTestButton : public SingleButtonOperation {
	protected:
		void onShortKeystroke() override;
		void onLongKeystroke()  override;
		void onHoldKeystroke()  override;
		void onHoldRelease()    override;
};

class TwoTestButton : public TwoButtonOperation {
	protected:
		void onLeftShortKeystroke()  override { Serial.println("[TEST] Left button short press erkannt!"); }
		void onLeftLongKeystroke()   override { Serial.println("[TEST] Left button long press erkannt!"); }
		void onLeftHoldKeystroke()   override { Serial.println("[TEST] Left button hold press erkannt!"); }
		void onLeftHoldRelease()     override { Serial.println("[TEST] Left button hold release erkannt!"); }
		void onRightShortKeystroke() override { Serial.println("[TEST] Right button short press erkannt!"); }
		void onRightLongKeystroke()  override { Serial.println("[TEST] Right button long press erkannt!"); }
		void onRightHoldKeystroke()  override { Serial.println("[TEST] Right button hold press erkannt!"); }
		void onRightHoldRelease()    override { Serial.println("[TEST] Right button hold release erkannt!"); }
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region SingleButtonSwitching //--------------------------------------------------------------------------------------
#endif
class SingleButtonSwitching : public SingleButtonOperation {
	private:
		//----Members----
		KNX* p_knx;
		String groupAddress;
		bool toggleState = false;

	public:
		//----Constructor----
		/**
		 * @brief Constructs a SingleButtonSwitching bound to a KNX instance and group address.
		 * @param knxInstance       Pointer to the active KNX coordinator.
		 * @param targetGroupAddress KNX group address to switch.
		 * @param longPressMs       Long-press threshold in ms (default 500).
		*/
		SingleButtonSwitching(KNX* knxInstance, String targetGroupAddress, uint32_t longPressMs = 500)
			: SingleButtonOperation(longPressMs), p_knx(knxInstance), groupAddress(targetGroupAddress) {}

		//----Methods----
		/** @brief Returns the switched KNX group address. */
		String getGroupAddress(void) const { return groupAddress; }

		/** @brief Sets the switched KNX group address. */
		void setGroupAddress(String groupAddressString) { groupAddress = groupAddressString; }

	protected:
		void onShortKeystroke() override;
		void onLongKeystroke()  override;
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonSwitching //-----------------------------------------------------------------------------------------
#endif
class TwoButtonSwitching : public TwoButtonOperation {
	private:
		//----Members----
		KNX* p_knx;
		String groupAddress;
		SwitchingBehaviour leftSwitchBehaviour;
		SwitchingBehaviour rightSwitchBehaviour;
		bool lastToggleState = false;

	public:
		//----Constructor----
		/**
		 * @brief Constructs a TwoButtonSwitching bound to a KNX instance.
		 * @param knxInstance Pointer to the active KNX coordinator.
		*/
		TwoButtonSwitching(KNX* knxInstance) : p_knx(knxInstance) {}

		//----Methods----
		/**
		 * @brief Configures the switching behaviour for both buttons.
		 * @param switchingGroupAddress KNX group address for switch commands.
		 * @param leftBehaviour         Behaviour for the left button (Off, On, Toggle).
		 * @param rightBehaviour        Behaviour for the right button (Off, On, Toggle).
		*/
		void configureSwitchingBehaviour(String switchingGroupAddress, SwitchingBehaviour leftBehaviour, SwitchingBehaviour rightBehaviour);

		/** @brief Returns the switching KNX group address. */
		String getGroupAddress(void) const { return groupAddress; }

		/** @brief Sets the switching KNX group address. */
		void setGroupAddress(String groupAddressString) { groupAddress = groupAddressString; }

		/** @brief Returns the left button's switching behaviour. */
		SwitchingBehaviour getLeftSwitchBehaviour(void) const { return leftSwitchBehaviour; }

		/** @brief Sets the left button's switching behaviour. */
		void setLeftSwitchBehaviour(SwitchingBehaviour behaviour) { leftSwitchBehaviour = behaviour; }

		/** @brief Returns the right button's switching behaviour. */
		SwitchingBehaviour getRightSwitchBehaviour(void) const { return rightSwitchBehaviour; }

		/** @brief Sets the right button's switching behaviour. */
		void setRightSwitchBehaviour(SwitchingBehaviour behaviour) { rightSwitchBehaviour = behaviour; }

	protected:
		void onLeftShortKeystroke()  override;
		void onLeftLongKeystroke()   override;
		void onRightShortKeystroke() override;
		void onRightLongKeystroke()  override;
};

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonDimming //-------------------------------------------------------------------------------------------
#endif
class TwoButtonDimming : public TwoButtonOperation {
	private:
		//----Members----
		KNX* p_knx;
		String groupAddressSwitching;
		String groupAddressDimming;
		SwitchingBehaviour leftSwitchBehaviour;
		SwitchingBehaviour rightSwitchBehaviour;
		DimmIncrement increment;
		DimmingBehaviour leftDimmBehaviour;
		DimmingBehaviour rightDimmBehaviour;
		bool lastToggleState = false;

	public:
		//----Constructor----
		/**
		 * @brief Constructs a TwoButtonDimming bound to a KNX instance.
		 * @param knxInstance Pointer to the active KNX coordinator.
		*/
		TwoButtonDimming(KNX* knxInstance) : p_knx(knxInstance) {}

		//----Methods----
		/**
		 * @brief Configures the switching behaviour for both buttons.
		 * @param switchingGroupAddress KNX group address for switch commands.
		 * @param leftBehaviour         Switching behaviour for the left button (Off, On, Toggle).
		 * @param rightBehaviour        Switching behaviour for the right button (Off, On, Toggle).
		*/
		void configureSwitchingBehaviour(String switchingGroupAddress, SwitchingBehaviour leftBehaviour, SwitchingBehaviour rightBehaviour);

		/**
		 * @brief Configures the dimming behaviour for both buttons.
		 * @param dimmingGroupAddress KNX group address for incremental dim commands.
		 * @param dimIncrement        Step size (1.5 %, 3 %, 6 %, 12.5 %, 25 %, 50 %, 100 %).
		 * @param leftBehaviour       Dimming direction for the left button (Darker, Brighter).
		 * @param rightBehaviour      Dimming direction for the right button (Darker, Brighter).
		*/
		void configureDimmingBehaviour(String dimmingGroupAddress, DimmIncrement dimIncrement, DimmingBehaviour leftBehaviour, DimmingBehaviour rightBehaviour);

		/** @brief Returns the switching KNX group address. */
		String getSwitchingGroupAddress(void) const { return groupAddressSwitching; }

		/** @brief Sets the switching KNX group address. */
		void setSwitchingGroupAddress(String groupAddressString) { groupAddressSwitching = groupAddressString; }

		/** @brief Returns the dimming KNX group address. */
		String getDimmingGroupAddress(void) const { return groupAddressDimming; }

		/** @brief Sets the dimming KNX group address. */
		void setDimmingGroupAddress(String groupAddressString) { groupAddressDimming = groupAddressString; }

		/** @brief Returns the left button's switching behaviour. */
		SwitchingBehaviour getLeftSwitchBehaviour(void) const { return leftSwitchBehaviour; }

		/** @brief Sets the left button's switching behaviour. */
		void setLeftSwitchBehaviour(SwitchingBehaviour behaviour) { leftSwitchBehaviour = behaviour; }

		/** @brief Returns the right button's switching behaviour. */
		SwitchingBehaviour getRightSwitchBehaviour(void) const { return rightSwitchBehaviour; }

		/** @brief Sets the right button's switching behaviour. */
		void setRightSwitchBehaviour(SwitchingBehaviour behaviour) { rightSwitchBehaviour = behaviour; }

		/** @brief Returns the dimming step size. */
		DimmIncrement getIncrement(void) const { return increment; }

		/** @brief Sets the dimming step size. */
		void setIncrement(DimmIncrement dimIncrement) { increment = dimIncrement; }

		/** @brief Returns the left button's dimming direction. */
		DimmingBehaviour getLeftDimmBehaviour(void) const { return leftDimmBehaviour; }

		/** @brief Sets the left button's dimming direction. */
		void setLeftDimmBehaviour(DimmingBehaviour behaviour) { leftDimmBehaviour = behaviour; }

		/** @brief Returns the right button's dimming direction. */
		DimmingBehaviour getRightDimmBehaviour(void) const { return rightDimmBehaviour; }

		/** @brief Sets the right button's dimming direction. */
		void setRightDimmBehaviour(DimmingBehaviour behaviour) { rightDimmBehaviour = behaviour; }

	protected:
		void onLeftShortKeystroke()  override;
		void onLeftLongKeystroke()   override;
		void onLeftHoldKeystroke()   override;
		void onLeftHoldRelease()     override;
		void onRightShortKeystroke() override;
		void onRightLongKeystroke()  override;
		void onRightHoldKeystroke()  override;
		void onRightHoldRelease()    override;
};
#ifdef _MSC_VER
	#pragma endregion
#endif
