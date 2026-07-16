#pragma once
/**
 * @name KnxTelegramTypes.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Passive value types describing a parsed telegram and a decoded bus event.
 *          These carry no behaviour; the coordinator and telegram codec populate them.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxEnums.h"
#include "KnxAddress.h"

// Decoded native payload for the low DPTs the legacy decoder path understands.
// Superseded as the send-side currency by KnxValue (KNX_Value); retained for the
// existing receive path until the coordinator is rebuilt on KnxObject.
union KnxTelegramData {
	bool dpt1;
	uint8_t dpt2;
	struct { bool direction; uint8_t step; } dpt3;
	char dpt4;
	struct { uint8_t raw; float percent; } dpt5;
};

struct ParsedTelegram {
	PhysicalAddress source{};
	GroupAddress target{};
	GroupValueType type;
	KNX_DPT dpt = KNX_DPT::UNKNOWN;
	KNX_Priority priority{};
	const uint8_t* payload = nullptr;
	uint8_t payloadLength  = 0;
	uint8_t inline6Data    = 0;
	bool _isInline6        = false;
	bool decodedValid      = false;
	KnxTelegramData decoded;
};

struct KnxEvent {
	PhysicalAddress source;
	GroupAddress target;
	GroupValueType type;
	KNX_DPT dpt;
	KnxTelegramData value;
};

// Legacy binding currency — removed once the coordinator moves to the intrusive
// IKnxReceiver registry (PLAN §7). Kept so the current KNX::bind path still builds.
using KNX_Callback = void (*)(const KnxEvent& event);
struct KNX_Binding {
	GroupAddress ga;
	KNX_DPT dpt;
	KNX_Callback callback;
};
