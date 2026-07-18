/**
 * @name KnxFrame.cpp
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details See KnxFrame.h. Byte layout follows the KNX standard frame:
 *            [0] control  [1] src hi  [2] src lo  [3] tgt hi  [4] tgt lo
 *            [5] NPCI     [6] APCI hi [7] APCI lo (+ inline data or payload) [..] checksum
*/

//---- Libraries ----
#include "KnxFrame.h"
#include "KnxCodec.h"
#include <cstring>

namespace KnxFrame {

uint8_t checksum(const uint8_t* buf, uint8_t len) {
	uint8_t cs = 0xFF;
	for (uint8_t i = 0; i < len; i++) cs ^= buf[i];
	return cs;
}

uint8_t build(PhysicalAddress source, uint16_t targetGa, const KnxValue& value,
              uint8_t* out, uint8_t maxLen, KnxPriority priority, bool repeat) {
	uint8_t payload[KnxCodec::MAX_PAYLOAD];
	uint8_t plen = KnxCodec::encode(value, payload, sizeof(payload));
	if (plen == 0) return 0;

	bool inline6 = KnxCodec::isInline6(value.dpt);
	uint8_t apduLen  = inline6 ? 2 : (uint8_t)(2 + plen);
	uint8_t frameLen = inline6 ? 8 : (uint8_t)(8 + plen);   // bytes before the checksum
	if ((uint8_t)(frameLen + 1) > maxLen) return 0;

	out[0] = (uint8_t)(0x90 | (repeat ? 0x20 : 0x00) | (uint8_t)priority);
	out[1] = (uint8_t)((source.area << 4) | (source.line & 0x0F));
	out[2] = source.device;
	out[3] = (uint8_t)(targetGa >> 8);      // (main << 3) | middle
	out[4] = (uint8_t)(targetGa & 0xFF);    // sub
	out[5] = (uint8_t)(0xE0 | ((apduLen - 1) & 0x0F));
	out[6] = 0x00;                          // APCI high — GroupValueWrite

	if (inline6) {
		out[7] = (uint8_t)(0x80 | (payload[0] & 0x3F));
	}
	else {
		out[7] = 0x80;
		std::memcpy(&out[8], payload, plen);
	}

	out[frameLen] = checksum(out, frameLen);
	return (uint8_t)(frameLen + 1);
}

bool parse(const uint8_t* buf, uint8_t len, ParsedTelegram& out) {
	out = ParsedTelegram{};

	// Smallest valid frame is inline-6: 6 header + 2 APDU + 1 checksum = 9 bytes.
	if (len < 9) return false;
	if (buf[len - 1] != checksum(buf, (uint8_t)(len - 1))) return false;

	out.priority      = (KnxPriority)(buf[0] & 0x0C);
	out.source.area   = (buf[1] >> 4) & 0x0F;
	out.source.line   = buf[1] & 0x0F;
	out.source.device = buf[2];
	out.target        = unpackGroupAddress((uint16_t)((buf[3] << 8) | buf[4]));

	uint8_t apduLen = (buf[5] & 0x0F) + 1;

	// APCI command: bits 9..6 of ((buf[6]&0x03)<<8 | buf[7]). 0=Read, 1=Response, 2=Write.
	uint16_t apci = (uint16_t)(((buf[6] & 0x03) << 8) | buf[7]);
	uint8_t  cmd  = (apci >> 6) & 0x0F;
	out.type = (cmd == 0) ? GroupValueType::Read
	         : (cmd == 1) ? GroupValueType::Response
	         : (cmd == 2) ? GroupValueType::Write
	         : GroupValueType::Unknown;

	if (apduLen == 2) {
		out._isInline6   = true;
		out.inline6Data  = buf[7] & 0x3F;
		out.payload      = nullptr;
		out.payloadLength = 0;
	}
	else if (apduLen > 2) {
		uint8_t dataLen  = apduLen - 2;
		uint8_t expected = (uint8_t)(6 + apduLen + 1);   // header + APDU + checksum
		if (expected != len) return false;
		if ((uint8_t)(8 + dataLen) > len) return false;
		out.payload      = &buf[8];
		out.payloadLength = dataLen;
	}
	else {
		return false;
	}

	out.dpt = KnxDpt::UNKNOWN;   // the receiving object supplies its DPT to decode
	return true;
}

} // namespace KnxFrame
