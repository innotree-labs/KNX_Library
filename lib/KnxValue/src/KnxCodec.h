#pragma once
/**
 * @name KnxCodec.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Symmetric encode/decode between a native KnxValue and its KNX wire bytes —
 *          the single source of truth for datapoint coding (PLAN §9). Multi-octet DPTs are
 *          big-endian per KNX. Arduino-free: this is the host-testable protocol core.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxValue.h"

namespace KnxCodec {

	// Maximum wire payload of any v1 datapoint (DPT19 = 8 octets).
	static constexpr uint8_t MAX_PAYLOAD = 8;

	/**
	 * @brief True for the datapoints whose value rides in the low 6 bits of the APDU
	 *        (DPT1/2/3). The telegram layer needs this to choose the APDU packing.
	*/
	bool isInline6(KnxDpt dpt);

	/**
	 * @brief Encodes a native value into its KNX wire payload (big-endian).
	 * @param value  The typed value to encode.
	 * @param out    Destination buffer for the payload octets.
	 * @param maxLen Capacity of out.
	 * @return Number of octets written, or 0 on unknown DPT / insufficient capacity.
	*/
	uint8_t encode(const KnxValue& value, uint8_t* out, uint8_t maxLen);

	/**
	 * @brief Decodes a KNX wire payload into a native value for the given DPT.
	 *        For the inline-6 datapoints pass the 6-bit APDU data as in[0] (len = 1).
	 * @param dpt DPT that determines the interpretation.
	 * @param in  Payload octets.
	 * @param len Number of octets in in.
	 * @return Decoded value; dpt == KnxDpt::UNKNOWN if decoding failed.
	*/
	KnxValue decode(KnxDpt dpt, const uint8_t* in, uint8_t len);

	//---- DPT9 2-octet float helpers (exposed for targeted testing) ----
	uint16_t encodeFloat16(float value);
	float    decodeFloat16(uint16_t raw);

} // namespace KnxCodec
