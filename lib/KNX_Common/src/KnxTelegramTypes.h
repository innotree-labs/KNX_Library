#pragma once
/**
 * @name KnxTelegramTypes.h
 * @date 18.07.2026
 * @authors Florian Wiesner
 * @details Passive value type describing a parsed telegram. It carries no behaviour; the
 *          telegram codec (KnxFrame) populates it and the coordinator dispatches it to the
 *          matching IKnxReceiver, which decodes the payload with its own DPT (PLAN §6).
*/

//---- Libraries ----
#include <cstdint>
#include "KnxEnums.h"
#include "KnxAddress.h"

// A parsed L_Data standard frame: addressing + service metadata + a pointer to the raw APDU
// payload. Value decoding is deferred to the receiver's DPT, so no decoded union lives here.
struct ParsedTelegram {
	PhysicalAddress source{};
	GroupAddress target{};
	GroupValueType type;
	KNX_DPT dpt = KNX_DPT::UNKNOWN;
	KNX_Priority priority{};
	const uint8_t* payload = nullptr;   // multi-octet APDU payload (nullptr for inline-6 DPTs)
	uint8_t payloadLength  = 0;
	uint8_t inline6Data    = 0;         // 6-bit APDU data for the inline DPTs (DPT1/2/3)
	bool _isInline6        = false;
};
