/**
 * @name main.cpp
 * @date 23.10.2025
 * @authors Florian Wiesner
 * @details Application entry point: wires the KNX coordinator to a two-button dimming rocker
 *          and registers the light-status feedback callback.
*/

#include <Arduino.h>
#include <KNX.h>
#include <KNX_Device.h>

#define TPUART_PHYS_ADDR "1.1.5"
#define BAUDRATE_SERIAL 115200

KNX knx = KNX(TPUART_PHYS_ADDR);
TwoButtonDimming rocker1_TwoButtonDimming(&knx);

void onStatusLightA(const KnxEvent& event);

void setup() {
	rocker1_TwoButtonDimming.setLongPressTime(400);
	rocker1_TwoButtonDimming.configureSwitchingBehaviour("0/1/1", SwitchingBehaviour::Off,
		SwitchingBehaviour::On);
	rocker1_TwoButtonDimming.configureDimmingBehaviour("0/0/1", DimmIncrement::Percent_6,
		DimmingBehaviour::Darker, DimmingBehaviour::Brighter);

	Serial.begin(BAUDRATE_SERIAL);
	knx.begin();

	knx.bind("0/3/0", KNX_DPT::DPT1, onStatusLightA);
}

void loop() {
	knx.monitorTPUART();
	knx.handleUART();
}

void onStatusLightA(const KnxEvent& event) {
	return;
}