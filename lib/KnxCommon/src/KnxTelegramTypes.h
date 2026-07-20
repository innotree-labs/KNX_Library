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
//
// Two destination flavours share this struct, discriminated by addressType (TP1 §2.2.4.4):
//   Group      — `target` holds the group address; this is ordinary group communication and
//                the only kind the IKnxReceiver registry dispatches.
//   Individual — `individualTarget` holds the addressed device; `target` is meaningless.
//                These are device-management telegrams (ping, descriptor read, ETS
//                programming) and go to the coordinator's device handler instead.
// The group fields keep their original names and positions so every existing consumer of a
// group telegram compiles and behaves unchanged.
struct ParsedTelegram {
	PhysicalAddress source{};
	GroupAddress target{};                                  // valid when addressType == Group
	GroupValueType type;
	KnxDpt dpt = KnxDpt::UNKNOWN;
	KnxPriority priority{};
	const uint8_t* payload = nullptr;   // multi-octet APDU payload (nullptr for inline-6 DPTs)
	uint8_t payloadLength  = 0;
	uint8_t inline6Data    = 0;         // 6-bit APDU data for the inline DPTs (DPT1/2/3)
	bool _isInline6        = false;

	//---- Point-to-point addressing (device management) ----
	KnxAddressType  addressType = KnxAddressType::Group;
	PhysicalAddress individualTarget{};                     // valid when addressType == Individual
	KnxTpci         tpci           = KnxTpci::DataGroup;
	uint8_t         sequenceNumber = 0;                     // TPCI seq no on numbered PDUs
	uint16_t        apci           = 0;                     // raw 10-bit APCI (0 on control PDUs)
	bool            hasApci        = false;                 // false on TPCI-only control PDUs
};
