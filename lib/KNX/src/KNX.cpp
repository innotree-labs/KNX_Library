/**
 * @name KNX.cpp
 * @date 25.10.2025
 * @authors Florian Wiesner
 * @details See KNX.h
*/

//----Libraries----
#include "KNX.h"

#ifdef _MSC_VER
	#pragma region Public methods
#endif

bool KNX::begin(void) {
	tpuart.begin();
	bool error = tpuart.resetRequest();
	tpuart.setPhysicalAddress();

	if (tpuart.stateRequest() == 0x07 && !error) return true;
	else return false;
}

bool KNX::reset(void) {
	bool error = tpuart.resetRequest();
	tpuart.setPhysicalAddress();

	if (tpuart.stateRequest() == 0x07 && !error) return true;
	else return false;
}

bool KNX::monitorTPUART(void) {
	return tpuart.watchBusVoltage() || tpuart.watchTemperature();
}

bool KNX::handleUART(void) {
	return tpuart.checkUART();
}

bool KNX::switchLight(String targetGroupAddress, bool state) {
	return telegram.sendDPT1(targetGroupAddress, state);
}

bool KNX::dimLightInterval(String targetGroupAddress, bool direction, DimmIncrement increment) {
	// direction = 1 -> heller, 0 -> dunkler; increment = step size (3 bit)
	uint8_t data = ((direction & 0x01) << 3) | ((uint8_t)increment & 0x07);
	return telegram.sendDPT3(targetGroupAddress, data);
}

bool KNX::dimLightValue(String targetGroupAddress, uint8_t dimValue) {
	uint8_t value = map(dimValue, 0, 100, 0, 255); // KNX 0..255
	return telegram.sendDPT5(targetGroupAddress, value);
}

bool KNX::blindOpenClose(String targetGroupAddress, bool direction) {
	return telegram.sendDPT1(targetGroupAddress, direction);
}

bool KNX::blindStopStep(String targetGroupAddress, bool step) {
	return telegram.sendDPT1(targetGroupAddress, step);
}

bool KNX::blindPositionValue(String targetGroupAddress, uint8_t position) {
	uint8_t value = map(position, 0, 100, 0, 255);
	return telegram.sendDPT5(targetGroupAddress, value);
}

bool KNX::sendTemperature(String targetGroupAddress, float temperature) {
	return telegram.sendDPT9(targetGroupAddress, temperature);
}

bool KNX::sendHumidity(String targetGroupAddress, float humidity) {
	return telegram.sendDPT9(targetGroupAddress, humidity);
}

bool KNX::bind(String gaStr, KNX_DPT dpt, KNX_Callback cb) {
	if (bindingCount >= MAX_BINDINGS) return false;
	if (!cb) return false;

	bindings[bindingCount].ga       = groupAddressFromString(gaStr);
	bindings[bindingCount].dpt      = dpt;
	bindings[bindingCount].callback = cb;

	bindingCount++;
	return true;
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Private methods
#endif

// Static trampoline: forwards the C-style callback to the owning KNX instance
void KNX::telegramParsedTrampoline(void* ctx, const ParsedTelegram& tg) {
	if (!ctx) return;
	static_cast<KNX*>(ctx)->handleParsedTelegram(tg);
}

// Matches the telegram against all bindings, decodes it, and fires the callback
void KNX::handleParsedTelegram(ParsedTelegram tg) {
	for (uint8_t i = 0; i < bindingCount; i++) {
		if (!gaEqual(bindings[i].ga, tg.target)) continue;

		tg.dpt = bindings[i].dpt; // Set DPT from the registry (important!)

		if (!telegram.decode(tg)) {
			#ifdef DEBUG
			Serial.println("Received telegram data is invalid!");
			#endif
			continue;
		}

		KnxEvent ev{};
		ev.source = tg.source;
		ev.target = tg.target;
		ev.type   = tg.type;
		ev.dpt    = tg.dpt;
		ev.value  = tg.decoded;

		bindings[i].callback(ev);
	}
}

#ifdef _MSC_VER
	#pragma endregion
#endif
