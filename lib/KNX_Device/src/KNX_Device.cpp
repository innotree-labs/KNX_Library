/**
 * @name KNX_Device.cpp
 * @date 25.10.2025
 * @authors Florian Wiesner
 * @details See KNX_Device.h
*/

//----Libraries----
#include "KNX_Device.h"

#ifdef _MSC_VER
	#pragma region SingleButtonOperation
#endif

void SingleButtonOperation::update(bool state) {
	uint32_t now = millis();

	// Debounce
	if (now - ts_lastChange < debounceTime) return;

	// State change
	if (state != lastState) {
		ts_lastChange = now;
		lastState     = state;

		if (state) { // pressed
			pressed        = true;
			longTriggered  = false;
			holdActive     = false;
			ts_pressStart  = now;
			ts_lastHold    = now;
		}
		else { // released
			if (pressed) {
				uint32_t duration = now - ts_pressStart;

				if (holdActive) {
					onHoldRelease();
				}
				else if (duration < longPressTime && !longTriggered) {
					onShortKeystroke();
				}

				pressed       = false;
				longTriggered = false;
				holdActive    = false;
			}
		}
	}

	// Long-press detection
	if (pressed && !longTriggered && (now - ts_pressStart >= longPressTime)) {
		onLongKeystroke();
		longTriggered = true;
		holdActive    = true;
		ts_lastHold   = now;
	}

	// Hold event
	if (pressed && holdActive && (now - ts_lastHold >= holdInterval)) {
		onHoldKeystroke();
		ts_lastHold = now;
	}
}

void SingleButtonOperation::setDebounceTime(uint32_t debounceTimeMs) {
	debounceTime = constrain(debounceTimeMs, 10, 100);
}

void SingleButtonOperation::setHoldInterval(uint32_t holdIntervalMs) {
	holdInterval = constrain(holdIntervalMs, 100, 1000);
}

void SingleButtonOperation::setLongPressTime(uint32_t longPressTimeMs) {
	longPressTime = constrain(longPressTimeMs, 200, 1000);
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonOperation
#endif

void TwoButtonOperation::update(bool leftState, bool rightState) {
	left.update(leftState);
	right.update(rightState);
}

void TwoButtonOperation::setDebounceTime(uint32_t debounceTimeMs) {
	left.setDebounceTime(debounceTimeMs);
	right.setDebounceTime(debounceTimeMs);
}

void TwoButtonOperation::setHoldInterval(uint32_t holdIntervalMs) {
	left.setHoldInterval(holdIntervalMs);
	right.setHoldInterval(holdIntervalMs);
}

void TwoButtonOperation::setLongPressTime(uint32_t longPressTimeMs) {
	left.setLongPressTime(longPressTimeMs);
	right.setLongPressTime(longPressTimeMs);
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region SingleTestButton
#endif

void SingleTestButton::onShortKeystroke() { Serial.println("[TEST] Short Keystroke erkannt!"); }
void SingleTestButton::onLongKeystroke()  { Serial.println("[TEST] Long Keystroke erkannt!"); }
void SingleTestButton::onHoldKeystroke()  { Serial.println("[TEST] Hold Keystroke erkannt!"); }
void SingleTestButton::onHoldRelease()    { Serial.println("[TEST] Hold Release erkannt!"); }

#ifdef _MSC_VER
	#pragma endregion
	#pragma region SingleButtonSwitching
#endif

void SingleButtonSwitching::onShortKeystroke() {
	toggleState = !toggleState;
	p_knx->switchLight(groupAddress, toggleState);
}

void SingleButtonSwitching::onLongKeystroke() {
	toggleState = !toggleState;
	p_knx->switchLight(groupAddress, toggleState);
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonSwitching
#endif

void TwoButtonSwitching::configureSwitchingBehaviour(String switchingGroupAddress, SwitchingBehaviour leftBehaviour, SwitchingBehaviour rightBehaviour) {
	groupAddress          = switchingGroupAddress;
	leftSwitchBehaviour   = leftBehaviour;
	rightSwitchBehaviour  = rightBehaviour;
}

void TwoButtonSwitching::onLeftShortKeystroke() {
	switch (leftSwitchBehaviour) {
		case SwitchingBehaviour::Off:    p_knx->switchLight(groupAddress, false);              break;
		case SwitchingBehaviour::On:     p_knx->switchLight(groupAddress, true);               break;
		case SwitchingBehaviour::Toggle: p_knx->switchLight(groupAddress, !lastToggleState);   break;
		default: break;
	}
}

void TwoButtonSwitching::onLeftLongKeystroke() {
	onLeftShortKeystroke();
}

void TwoButtonSwitching::onRightShortKeystroke() {
	switch (rightSwitchBehaviour) {
		case SwitchingBehaviour::Off:    p_knx->switchLight(groupAddress, false);              break;
		case SwitchingBehaviour::On:     p_knx->switchLight(groupAddress, true);               break;
		case SwitchingBehaviour::Toggle: p_knx->switchLight(groupAddress, !lastToggleState);   break;
		default: break;
	}
}

void TwoButtonSwitching::onRightLongKeystroke() {
	onRightShortKeystroke();
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region TwoButtonDimming
#endif

void TwoButtonDimming::configureSwitchingBehaviour(String switchingGroupAddress, SwitchingBehaviour leftBehaviour, SwitchingBehaviour rightBehaviour) {
	groupAddressSwitching = switchingGroupAddress;
	leftSwitchBehaviour   = leftBehaviour;
	rightSwitchBehaviour  = rightBehaviour;
}

void TwoButtonDimming::configureDimmingBehaviour(String dimmingGroupAddress, DimmIncrement dimIncrement, DimmingBehaviour leftBehaviour, DimmingBehaviour rightBehaviour) {
	groupAddressDimming = dimmingGroupAddress;
	increment           = dimIncrement;
	leftDimmBehaviour   = leftBehaviour;
	rightDimmBehaviour  = rightBehaviour;
}

void TwoButtonDimming::onLeftShortKeystroke() {
	switch (leftSwitchBehaviour) {
		case SwitchingBehaviour::Off:    p_knx->switchLight(groupAddressSwitching, false);             break;
		case SwitchingBehaviour::On:     p_knx->switchLight(groupAddressSwitching, true);              break;
		case SwitchingBehaviour::Toggle: p_knx->switchLight(groupAddressSwitching, !lastToggleState);  break;
		default: break;
	}
}

void TwoButtonDimming::onLeftLongKeystroke() {
	switch (leftDimmBehaviour) {
		case DimmingBehaviour::Darker:   p_knx->dimLightInterval(groupAddressDimming, false, increment); break;
		case DimmingBehaviour::Brighter: p_knx->dimLightInterval(groupAddressDimming, true,  increment); break;
		default: break;
	}
}

void TwoButtonDimming::onLeftHoldKeystroke() {
	onLeftLongKeystroke();
}

void TwoButtonDimming::onLeftHoldRelease() {
	p_knx->dimLightInterval(groupAddressDimming, false, DimmIncrement::Stop);
}

void TwoButtonDimming::onRightShortKeystroke() {
	switch (rightSwitchBehaviour) {
		case SwitchingBehaviour::Off:    p_knx->switchLight(groupAddressSwitching, false);             break;
		case SwitchingBehaviour::On:     p_knx->switchLight(groupAddressSwitching, true);              break;
		case SwitchingBehaviour::Toggle: p_knx->switchLight(groupAddressSwitching, !lastToggleState);  break;
		default: break;
	}
}

void TwoButtonDimming::onRightLongKeystroke() {
	switch (rightDimmBehaviour) {
		case DimmingBehaviour::Darker:   p_knx->dimLightInterval(groupAddressDimming, false, increment); break;
		case DimmingBehaviour::Brighter: p_knx->dimLightInterval(groupAddressDimming, true,  increment); break;
		default: break;
	}
}

void TwoButtonDimming::onRightHoldKeystroke() {
	onRightLongKeystroke();
}

void TwoButtonDimming::onRightHoldRelease() {
	p_knx->dimLightInterval(groupAddressDimming, false, DimmIncrement::Stop);
}

#ifdef _MSC_VER
	#pragma endregion
#endif
