/**
 * @name KNX_Telegram.cpp
 * @date 11.10.2025
 * @authors Florian Wiesner
 * @details See KNX_Telegram.h
*/

//----Libraries----
#include "KNX_Telegram.h"
#include "KNX_TPUART2.h"

#ifdef _MSC_VER
	#pragma region Constructor
#endif

KNX_Telegram::KNX_Telegram(KNX_TPUART2* tpuart) : p_tpuart(tpuart) {
	clear();
}

void KNX_Telegram::setParsedCallback(TelegramParsedCallback cb, void* ctx) {
	parsedCb = cb;
	parsedCtx = ctx;
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Sending
#endif

// Zeroes the telegram buffer and resets the length
void KNX_Telegram::clear(void) {
	for (int i = 0; i < MAX_TELEGRAM_SIZE; i++) {
		buffer[i] = 0;
	}
	length = 0;
}

// Assembles the control byte (frame type, repeat flag, priority) into the buffer
void KNX_Telegram::setControlField(bool repeat, KNX_Priority priority) {
	// Assemble the control byte and write it into the buffer
	buffer[0] = 0x90 | (repeat << 5) | (uint8_t)priority;
}

// Writes the physical source address into the buffer
void KNX_Telegram::setSourceAddress(PhysicalAddress source) {
	// Write the source address into the buffer
	buffer[1] = (source.area << 4) | source.line; // Source address high byte
	buffer[2] = source.device; // Source address low byte
}

// Parses the group address string and writes it into the buffer
void KNX_Telegram::setTargetGroupAddress(String groupAddressString) {
	GroupAddress groupAddress = groupAddressFromString(groupAddressString);

	// Write the target address into the buffer
	buffer[3] = (groupAddress.main << 3) | groupAddress.middle; // Target address high byte
	buffer[4] = groupAddress.sub; // Target address low byte
}

// Writes the NPCI byte (group-address flag, routing counter, APDU length)
void KNX_Telegram::setNPCI(uint8_t apduLength) {
	// NPCI: target = group address, routing counter = 6
	buffer[5] = 0xE0 | ((apduLength - 1) & 0x0F);
}

// inline6 = true for DPT1/DPT2/DPT3 (data bits in the APDU low byte),
// inline6 = false for DPT4/5/9/... (data bytes after the APDU low byte)
void KNX_Telegram::setAPDU(uint8_t* data, uint8_t len, bool smaller6Bit) {
	buffer[6] = 0x00; // APDU high byte

	// Check the data length
	if (len > TELEGRAM_MAX_DATA_SIZE - TELEGRAM_APDU_SIZE)
		len = TELEGRAM_MAX_DATA_SIZE - TELEGRAM_APDU_SIZE;

	// For DPT1/2/3 (<= 6 bit) write the data into the APDU low byte
	if (smaller6Bit) {
		buffer[7] = 0x80 | (data[0] & 0x3F);
		setNPCI(TELEGRAM_APDU_SIZE);       // APDU length = 2
		length = 8;                        // up to and including the APDU
	}
	else {
		buffer[7] = 0x80;  // Write, no inline data
		memcpy(&buffer[8], data, len);
		setNPCI(TELEGRAM_APDU_SIZE + len); // APDU length = 2 + data bytes
		length = 8 + len;
	}
}

// Computes the XOR checksum over the buffer and appends it
void KNX_Telegram::setChecksum(void) {
	uint8_t checksum = 0xFF;
	for (uint8_t i = 0; i < length; i++) { // XOR over all bytes
		checksum ^= buffer[i];
	}
	buffer[length] = checksum;
	length++;
}

// Builds the standard header (control field, source and target address)
void KNX_Telegram::createStandardHeader(String targetGroupAddress) {
	clear();
	setControlField();
	setSourceAddress(p_tpuart->physicalAddress);
	setTargetGroupAddress(targetGroupAddress);
}

// Hands the assembled buffer to the TP-UART driver for transmission
bool KNX_Telegram::send(void) {
	return p_tpuart->sendTelegram(buffer, length);
}

// Helper: converts a float into the KNX 2-byte float (DPT9)
void KNX_Telegram::encodeDPT9(float value, uint8_t* data) {
	int16_t mantissa;
	int8_t exponent = 0;
	bool negative = (value < 0);
	if (negative) value = -value;

	// Scale until the mantissa fits (max 2047)
	mantissa = (int16_t)(value / 0.01);
	while (mantissa > 2047) {
		mantissa >>= 1;
		exponent++;
	}

	if (negative) mantissa = -mantissa;

	// High byte: exponent & sign + top 3 mantissa bits
	data[0] = ((exponent & 0x0F) << 3) | ((mantissa >> 8) & 0x07);
	if (mantissa < 0) data[0] |= 0x80;

	// Low byte: lower 8 mantissa bits
	data[1] = mantissa & 0xFF;
}

bool KNX_Telegram::sendDPT1(String targetGroupAddress, bool oneBit) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1];
	data[0] = oneBit ? 0x01 : 0x00;
	setAPDU(data, 1, true);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT2(String targetGroupAddress, uint8_t twoBit) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1];
	data[0] = twoBit & 0x03; // only 2 bits allowed
	setAPDU(data, 1, true);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT3(String targetGroupAddress, uint8_t fourBit) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1];
	data[0] = fourBit & 0x0F; // only 4 bits allowed
	setAPDU(data, 1, true);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT4(String targetGroupAddress, char ascii) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1] = { ascii };
	setAPDU(data, 1, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT5(String targetGroupAddress, uint8_t uint8) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1] = { uint8 };
	setAPDU(data, 1, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT6(String targetGroupAddress, int8_t sint8) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[1];
	data[0] = (int8_t)sint8;
	setAPDU(data, 1, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT7(String targetGroupAddress, uint16_t uint16) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[2];
	data[0] = (uint16 >> 8) & 0xFF;
	data[1] = uint16 & 0xFF;
	setAPDU(data, 2, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT8(String targetGroupAddress, int16_t sint16) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[2];
	data[0] = (sint16 >> 8) & 0xFF;
	data[1] = sint16 & 0xFF;
	setAPDU(data, 2, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT9(String targetGroupAddress, float value) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[2];
	encodeDPT9(value, data);       // build the KNX float16
	setAPDU(data, 2, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT10(String targetGroupAddress, uint32_t threeByte) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[3];
	data[0] = (threeByte >> 16) & 0xFF;
	data[1] = (threeByte >> 8) & 0xFF;
	data[2] = threeByte & 0xFF;
	setAPDU(data, 3, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT11(String targetGroupAddress, uint32_t threeByte) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[3];
	data[0] = (threeByte >> 16) & 0xFF;
	data[1] = (threeByte >> 8) & 0xFF;
	data[2] = threeByte & 0xFF;
	setAPDU(data, 3, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT12(String targetGroupAddress, uint32_t uint32) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[4];
	data[0] = (uint32 >> 24) & 0xFF;
	data[1] = (uint32 >> 16) & 0xFF;
	data[2] = (uint32 >> 8) & 0xFF;
	data[3] = uint32 & 0xFF;
	setAPDU(data, 4, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT13(String targetGroupAddress, int32_t sint32) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[4];
	data[0] = (sint32 >> 24) & 0xFF;
	data[1] = (sint32 >> 16) & 0xFF;
	data[2] = (sint32 >> 8) & 0xFF;
	data[3] = sint32 & 0xFF;
	setAPDU(data, 4, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT14(String targetGroupAddress, float float32) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[4];
	memcpy(data, &float32, 4); // IEEE 754 bytes as-is
	setAPDU(data, 4, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT16(String targetGroupAddress, String string14) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[14] = { 0 };
	uint8_t len = constrain(string14.length(), 0, 14);
	for (uint8_t i = 0; i < len; i++)
		data[i] = string14[i];
	setAPDU(data, 14, false);
	setChecksum();
	return send();
}

bool KNX_Telegram::sendDPT19(String targetGroupAddress, uint64_t datetime) {
	createStandardHeader(targetGroupAddress);
	uint8_t data[8];
	for (int i = 0; i < 8; i++) {
		data[7 - i] = (datetime >> (i * 8)) & 0xFF;
	}
	setAPDU(data, 8, false);
	setChecksum();
	return send();
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Receiving
#endif

uint8_t KNX_Telegram::getChecksum(void) const {
	if (length == 0) return 0x00;         // nothing buffered
	return buffer[length - 1];            // last byte is the checksum
}

uint8_t KNX_Telegram::calculateChecksum(void) const {
	// XOR over all bytes (excluding the checksum) starting from 0xFF
	if (length < 2) return 0x00;          // too short to be meaningful

	uint8_t checksum = 0xFF;
	for (uint8_t i = 0; i < (uint8_t)(length - 1); i++) {
		checksum ^= buffer[i];
	}
	return checksum;
}

PhysicalAddress KNX_Telegram::getSourceAddress(void) const {
	PhysicalAddress addr{};

	// Expected: buffer[1] = (area<<4)|line, buffer[2] = device
	if (length < 3) return addr;

	uint8_t high = buffer[1];
	addr.area   = (high >> 4) & 0x0F;
	addr.line   = high & 0x0F;
	addr.device = buffer[2];

	return addr;
}

GroupAddress KNX_Telegram::getTargetGroupAddress(void) const {
	GroupAddress ga{};

	// Expected: buffer[3] = (main<<3)|middle, buffer[4] = sub
	if (length < 5) return ga;

	uint8_t high = buffer[3];
	ga.main   = (high >> 3) & 0x1F; // 5 bits
	ga.middle = high & 0x07;        // 3 bits
	ga.sub    = buffer[4];          // 8 bits

	return ga;
}

KNX_Priority KNX_Telegram::getPriority(void) const {
	if (length < 1) return KNX_Priority::Normal;
	return (KNX_Priority)(buffer[0] & 0x0C);
}

bool KNX_Telegram::receiveIncomingTelegram(uint8_t* rxBuffer, uint8_t rxLength) {
	if (rxLength == 0) return false;
	if (rxLength > MAX_TELEGRAM_SIZE) return false;

	clear();
	memcpy(buffer, rxBuffer, rxLength);
	length = rxLength;

	if (getChecksum() != calculateChecksum()) {
		clear();
		parsed = ParsedTelegram{};
		return false;
	}

	return parseIncomingTelegram();
}

bool KNX_Telegram::parseIncomingTelegram(void) {
	parsed = ParsedTelegram{};

	parsed.source   = getSourceAddress();
	parsed.target   = getTargetGroupAddress();
	parsed.priority = getPriority();
	parsed.dpt      = KNX_DPT::UNKNOWN;

	uint8_t apduLo  = buffer[7];
	uint8_t apduLen = (buffer[5] & 0x0F) + 1;

	if (apduLen == 2) {
		parsed._isInline6   = true;
		parsed.inline6Data = apduLo & 0x3F;
		parsed.payload      = nullptr;
		parsed.payloadLength = 0;
	}
	else if (apduLen > 2) {
		uint8_t dataLen  = apduLen - 2;
		uint8_t expected = 6 + apduLen + 1;

		if (expected != length) return false;
		if ((uint8_t)(8 + dataLen) > length) return false;

		parsed.payload       = &buffer[8];
		parsed.payloadLength = dataLen;
	}

	if (parsedCb) parsedCb(parsedCtx, parsed);
	return true;
}

void KNX_Telegram::outputParsedTelegram(void) {
	Serial.print("KNX Telegram received: ");
	for (uint8_t i = 0; i < length; i++) Serial.printf("0x%02X ", buffer[i]);

	Serial.print("  Type: ");
	Serial.print((uint8_t)parsed.type);

	Serial.print("  Source: ");
	Serial.print(stringFromPhysicalAddress(parsed.source));

	Serial.print("  Target: ");
	Serial.print(stringFromGroupAddress(parsed.target));

	if (parsed._isInline6) {
		Serial.print("  inline6: ");
		Serial.print(parsed.inline6Data);
	}
	else {
		Serial.print("  PayloadLen: ");
		Serial.print(parsed.payloadLength);
	}
	Serial.println();
}

bool KNX_Telegram::decode(ParsedTelegram& tg) {
	tg.decodedValid = false;

	switch (tg.dpt) {
		case KNX_DPT::DPT1: tg.decodedValid = decodeDPT1(tg); break;
		case KNX_DPT::DPT2: tg.decodedValid = decodeDPT2(tg); break;
		case KNX_DPT::DPT3: tg.decodedValid = decodeDPT3(tg); break;
		case KNX_DPT::DPT4: tg.decodedValid = decodeDPT4(tg); break;
		case KNX_DPT::DPT5: tg.decodedValid = decodeDPT5(tg); break;
		default: tg.decodedValid = false; break;
	}

	return tg.decodedValid;
}

// Decodes a DPT1 (1-bit) value from the inline-6 APDU data
bool KNX_Telegram::decodeDPT1(ParsedTelegram& tg) {
	if (!tg._isInline6) return false;
	tg.decoded.dpt1 = (tg.inline6Data & 0x01) != 0;
	return true;
}

// Decodes a DPT2 (2-bit controlled) value from the inline-6 APDU data
bool KNX_Telegram::decodeDPT2(ParsedTelegram& tg) {
	if (!tg._isInline6) return false;
	tg.decoded.dpt2 = tg.inline6Data & 0x03;
	return true;
}

// Decodes a DPT3 (4-bit dim/step) value: bit 3 = direction, bits 2..0 = step
bool KNX_Telegram::decodeDPT3(ParsedTelegram& tg) {
	if (!tg._isInline6) return false;
	tg.decoded.dpt3.direction = (tg.inline6Data & 0x08) != 0;
	tg.decoded.dpt3.step      = tg.inline6Data & 0x07;
	return true;
}

// Decodes a DPT4 (8-bit ASCII character) value from the payload byte
bool KNX_Telegram::decodeDPT4(ParsedTelegram& tg) {
	if (tg._isInline6) return false;
	if (!tg.payload || tg.payloadLength < 1) return false;
	tg.decoded.dpt4 = (char)tg.payload[0];
	return true;
}

// Decodes a DPT5 (8-bit unsigned) value and its 0..100 % representation
bool KNX_Telegram::decodeDPT5(ParsedTelegram& tg) {
	if (tg._isInline6) return false;
	if (!tg.payload || tg.payloadLength < 1) return false;

	uint8_t raw = tg.payload[0];
	tg.decoded.dpt5.raw     = raw;
	tg.decoded.dpt5.percent = (raw * 100.0f) / 255.0f;
	return true;
}

#ifdef _MSC_VER
	#pragma endregion
#endif
