#pragma once
/**
 * @name KnxFrame.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details Stateless KNX L_Data frame builder/parser: control field, addressing, NPCI,
 *          APDU and checksum. Value coding is delegated to KnxCodec (KnxValue) — this
 *          module owns framing only (PLAN §12). Arduino-free and host-testable; the
 *          group address is the packed uint16_t currency, never a String in the hot path.
*/

//---- Libraries ----
#include <cstdint>
#include "KnxEnums.h"
#include "KnxAddress.h"
#include "KnxTelegramTypes.h"
#include "KnxCodec.h"

namespace KnxFrame {

	// Largest standard frame: 6-byte header + 2-byte APDU + payload + 1-byte checksum.
	static constexpr uint8_t MAX_FRAME = 6 + 2 + KnxCodec::MAX_PAYLOAD + 1;

	/**
	 * @brief Assembles a GroupValueWrite L_Data frame for a value.
	 * @param source   Physical address of this device (frame source).
	 * @param targetGa Packed destination group address.
	 * @param value    The value to encode into the APDU (via KnxCodec).
	 * @param out      Destination buffer.
	 * @param maxLen   Capacity of out.
	 * @param priority Telegram priority (default Normal).
	 * @param repeat   Control-field repeat flag (default true = original, not repeated).
	 * @return Total frame length including checksum, or 0 on encode failure / capacity.
	*/
	uint8_t build(PhysicalAddress source, uint16_t targetGa, const KnxValue& value,
	              uint8_t* out, uint8_t maxLen,
	              KnxPriority priority = KnxPriority::Normal, bool repeat = true);

	/**
	 * @brief Verifies the checksum and parses a raw frame into a ParsedTelegram.
	 *        payload points into the caller's buffer (which must outlive the ParsedTelegram);
	 *        dpt is left UNKNOWN — the receiving object supplies its own DPT to decode.
	 * @param buf Raw frame bytes (checksum included).
	 * @param len Number of bytes in buf.
	 * @param out Destination telegram.
	 * @return true if the checksum is valid and the structure parsed.
	*/
	bool parse(const uint8_t* buf, uint8_t len, ParsedTelegram& out);

	// XOR checksum seeded with 0xFF over the first len bytes (KNX L_Data checksum).
	uint8_t checksum(const uint8_t* buf, uint8_t len);

} // namespace KnxFrame
