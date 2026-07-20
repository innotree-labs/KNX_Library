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
#include "KnxDebug.h"
#include <cstring>

namespace KnxFrame {

uint8_t checksum(const uint8_t* buf, uint8_t len) {
	uint8_t cs = 0xFF;
	for (uint8_t i = 0; i < len; i++) cs ^= buf[i];
	return cs;
}

// Writes octets 0..5 — control field, source, destination and NPCI — which are common to
// every L_Data_Standard frame. The caller fills the APDU from octet 6 and appends the
// checksum. Hop count is fixed at the conventional Network Layer default of 6.
static void writeHeader(uint8_t* out, PhysicalAddress source, uint16_t rawTarget,
                        KnxAddressType addressType, uint8_t apduLen,
                        KnxPriority priority, bool repeat) {
	out[0] = (uint8_t)(0x90 | (repeat ? 0x20 : 0x00) | (uint8_t)priority);
	out[1] = (uint8_t)((source.area << 4) | (source.line & 0x0F));
	out[2] = source.device;
	out[3] = (uint8_t)(rawTarget >> 8);
	out[4] = (uint8_t)(rawTarget & 0xFF);
	uint8_t at = (addressType == KnxAddressType::Group) ? 0x80 : 0x00;
	out[5] = (uint8_t)(at | 0x60 | ((apduLen - 1) & 0x0F));
}

uint8_t build(PhysicalAddress source, uint16_t targetGa, const KnxValue& value,
              uint8_t* out, uint8_t maxLen, KnxPriority priority, bool repeat) {
	uint8_t payload[KnxCodec::MAX_PAYLOAD];
	uint8_t plen = KnxCodec::encode(value, payload, sizeof(payload));
	if (plen == 0) {
		KnxDebug::log("FRM !! encode failed for DPT %u", (unsigned)value.dpt);
		return 0;
	}

	bool inline6 = KnxCodec::isInline6(value.dpt);
	uint8_t apduLen  = inline6 ? 2 : (uint8_t)(2 + plen);
	uint8_t frameLen = inline6 ? 8 : (uint8_t)(8 + plen);   // bytes before the checksum
	if ((uint8_t)(frameLen + 1) > maxLen) {
		KnxDebug::log("FRM !! frame %u B exceeds buffer %u B", (unsigned)(frameLen + 1), (unsigned)maxLen);
		return 0;
	}

	KnxDebug::log("FRM build: %s, APDU %u B, payload %u B",
		inline6 ? "inline-6" : "multi-octet", (unsigned)apduLen, (unsigned)plen);

	writeHeader(out, source, targetGa, KnxAddressType::Group, apduLen, priority, repeat);
	out[6] = 0x00;                          // TPCI T_Data_Group + APCI high — GroupValueWrite

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

//---- Point-to-point (individually addressed) framing ----

uint8_t buildIndividual(PhysicalAddress source, PhysicalAddress target, uint16_t apci,
                        const uint8_t* payload, uint8_t payloadLength,
                        uint8_t* out, uint8_t maxLen, KnxPriority priority, bool repeat) {
	if (payloadLength > 0 && payload == nullptr) return 0;

	uint8_t apduLen  = (uint8_t)(2 + payloadLength);   // APCI occupies octets 6..7
	uint8_t frameLen = (uint8_t)(6 + apduLen);         // bytes before the checksum
	if (apduLen > 16 || (uint8_t)(frameLen + 1) > maxLen) {
		KnxDebug::log("FRM !! individual frame %u B exceeds buffer %u B",
			(unsigned)(frameLen + 1), (unsigned)maxLen);
		return 0;
	}

	writeHeader(out, source, packPhysicalAddress(target), KnxAddressType::Individual,
		apduLen, priority, repeat);

	// TPCI T_Data_Individual (unnumbered data, all six upper bits zero) carries the top two
	// APCI bits in octet 6; the remaining eight sit in octet 7.
	out[6] = (uint8_t)((apci >> 8) & 0x03);
	out[7] = (uint8_t)(apci & 0xFF);
	for (uint8_t i = 0; i < payloadLength; i++) out[8 + i] = payload[i];

	KnxDebug::log("FRM build: individual -> %u.%u.%u, APCI 0x%03X, payload %u B",
		(unsigned)target.area, (unsigned)target.line, (unsigned)target.device,
		(unsigned)apci, (unsigned)payloadLength);

	out[frameLen] = checksum(out, frameLen);
	return (uint8_t)(frameLen + 1);
}

uint8_t buildControl(PhysicalAddress source, PhysicalAddress target, KnxTpci tpci,
                     uint8_t sequenceNumber, uint8_t* out, uint8_t maxLen,
                     KnxPriority priority, bool repeat) {
	uint8_t tpciByte = 0;
	switch (tpci) {
		case KnxTpci::Connect:    tpciByte = 0x80; break;
		case KnxTpci::Disconnect: tpciByte = 0x81; break;
		case KnxTpci::Ack:        tpciByte = (uint8_t)(0xC2 | ((sequenceNumber & 0x0F) << 2)); break;
		case KnxTpci::Nak:        tpciByte = (uint8_t)(0xC3 | ((sequenceNumber & 0x0F) << 2)); break;
		default:
			KnxDebug::log("FRM !! buildControl called with a non-control TPCI");
			return 0;
	}

	// A control PDU ends after octet 6: APDU length 1, so LG = 0.
	uint8_t frameLen = 7;   // bytes before the checksum
	if ((uint8_t)(frameLen + 1) > maxLen) return 0;

	writeHeader(out, source, packPhysicalAddress(target), KnxAddressType::Individual,
		1, priority, repeat);
	out[6] = tpciByte;

	KnxDebug::log("FRM build: control TPCI 0x%02X -> %u.%u.%u",
		(unsigned)tpciByte, (unsigned)target.area, (unsigned)target.line,
		(unsigned)target.device);

	out[frameLen] = checksum(out, frameLen);
	return (uint8_t)(frameLen + 1);
}

// Classifies the transport control field (octet 6) per Transport Layer spec 03_03_04,
// Figure 3. Sets seqNo for the numbered PDUs and leaves it 0 for the rest.
static KnxTpci decodeTpci(uint8_t b, KnxAddressType addressType, uint8_t& seqNo) {
	seqNo = 0;

	if ((b & 0xC0) == 0x00) {   // unnumbered data — bits 1..0 are the top APCI bits
		return (addressType == KnxAddressType::Group) ? KnxTpci::DataGroup
		                                              : KnxTpci::DataIndividual;
	}
	if ((b & 0xC0) == 0x40) {   // numbered data, inside a transport connection
		seqNo = (uint8_t)((b >> 2) & 0x0F);
		return KnxTpci::DataConnected;
	}

	// Control PDUs. Connect/Disconnect carry no sequence number, so match them exactly.
	if (b == 0x80) return KnxTpci::Connect;
	if (b == 0x81) return KnxTpci::Disconnect;
	if ((b & 0xC3) == 0xC2) { seqNo = (uint8_t)((b >> 2) & 0x0F); return KnxTpci::Ack; }
	if ((b & 0xC3) == 0xC3) { seqNo = (uint8_t)((b >> 2) & 0x0F); return KnxTpci::Nak; }
	return KnxTpci::Unknown;
}

bool parse(const uint8_t* buf, uint8_t len, ParsedTelegram& out) {
	out = ParsedTelegram{};

	// Smallest valid frame is a TPCI-only control PDU (T_Connect/T_Disconnect/T_ACK):
	// 6 header + 1 TPCI + 1 checksum = 8 bytes (TP1 §2.2.4.5, length 0).
	if (len < 8) {
		KnxDebug::log("FRM !! runt frame, %u B (min 8)", (unsigned)len);
		return false;
	}
	if (buf[len - 1] != checksum(buf, (uint8_t)(len - 1))) {
		KnxDebug::log("FRM !! checksum: got 0x%02X, want 0x%02X",
			(unsigned)buf[len - 1], (unsigned)checksum(buf, (uint8_t)(len - 1)));
		return false;
	}

	out.priority      = (KnxPriority)(buf[0] & 0x0C);
	out.source.area   = (buf[1] >> 4) & 0x0F;
	out.source.line   = buf[1] & 0x0F;
	out.source.device = buf[2];

	// Octet 5 bit 7 is the Address Type: 1 = group, 0 = a single device (TP1 §2.2.4.4).
	uint16_t rawTarget = (uint16_t)((buf[3] << 8) | buf[4]);
	out.addressType    = (buf[5] & 0x80) ? KnxAddressType::Group : KnxAddressType::Individual;
	if (out.addressType == KnxAddressType::Group) {
		out.target = unpackGroupAddress(rawTarget);
	}
	else {
		out.individualTarget = unpackPhysicalAddress(rawTarget);
	}

	// The frame length is fully determined by LG, so verify it for every flavour rather
	// than only for the multi-octet branch.
	uint8_t apduLen  = (uint8_t)((buf[5] & 0x0F) + 1);
	uint8_t expected = (uint8_t)(6 + apduLen + 1);   // header + APDU + checksum
	if (expected != len) {
		KnxDebug::log("FRM !! length mismatch: LG implies %u B, got %u B",
			(unsigned)expected, (unsigned)len);
		return false;
	}

	out.tpci = decodeTpci(buf[6], out.addressType, out.sequenceNumber);

	// A TPCI-only control PDU (T_Connect/T_Disconnect/T_ACK/T_NAK) ends after octet 6 and
	// carries no APCI, hence no service type and no payload.
	if (apduLen < 2) {
		out.hasApci = false;
		out.type    = GroupValueType::Unknown;
		out.dpt     = KnxDpt::UNKNOWN;
		return true;
	}

	// APCI command: bits 9..6 of ((buf[6]&0x03)<<8 | buf[7]). 0=Read, 1=Response, 2=Write.
	out.apci    = (uint16_t)(((buf[6] & 0x03) << 8) | buf[7]);
	out.hasApci = true;
	uint8_t cmd = (uint8_t)((out.apci >> 6) & 0x0F);
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
	else {
		uint8_t dataLen = (uint8_t)(apduLen - 2);
		if ((uint8_t)(8 + dataLen) > len) return false;
		out.payload      = &buf[8];
		out.payloadLength = dataLen;
	}

	out.dpt = KnxDpt::UNKNOWN;   // the receiving object supplies its DPT to decode
	return true;
}

} // namespace KnxFrame
