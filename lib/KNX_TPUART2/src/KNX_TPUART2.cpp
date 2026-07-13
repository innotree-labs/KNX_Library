/**
 * @name KNX_TPUART2.cpp
 * @date 25.09.2025
 * @authors Florian Wiesner
 * @details See KNX_TPUART2.h
*/

//----Libraries----
#include "KNX_TPUART2.h"
#include "KNX_Telegram.h"

// Initialise the ISR flags (they are static members)
volatile bool KNX_TPUART2::_voltageFailure = false;
volatile bool KNX_TPUART2::_tempWarning    = false;

#ifdef _MSC_VER
	#pragma region Constructor
#endif

KNX_TPUART2::KNX_TPUART2(String physicalAddress)
	: rxPin(ESP32_RX), txPin(ESP32_TX), resnPin(TPUART_RESN),
	  savePin(TPUART_SAVE), twPin(TPUART_TW), baudrate(BAUDRATE_TPUART),
	  physicalAddressString(physicalAddress),
	  physicalAddress(physicalAddressFromString(physicalAddress)) {
	// Configure pins
	pinMode(resnPin, INPUT_PULLUP);
	pinMode(savePin, INPUT_PULLUP);
	pinMode(twPin,   INPUT_PULLDOWN);

	// Configure interrupts for SAVE and TW
	enableInterrupts();
}

void KNX_TPUART2::attachTelegramPointer(KNX_Telegram* telegram) {
	p_telegram = telegram;
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Private methods
#endif

void KNX_TPUART2::sendCommand(const uint8_t* cmd, uint16_t len) {
	uart.write(cmd, len);
}

void KNX_TPUART2::clearBuffer(void) {
	#ifdef DEBUG
	Serial.print("Receive Buffer cleared.");
	#endif

	#ifdef DEBUG
	bool headerPrinted = false;
	#endif

	while (uart.available()) {
		uint8_t buffer = uart.read();
		#ifdef DEBUG
		if (!headerPrinted) { // print the "Bytes:" label only before the first byte
			Serial.print(" Bytes: ");
			headerPrinted = true;
		}
		Serial.printf("0x%02X ", buffer);
		#else
		(void)buffer;
		#endif
	}
	#ifdef DEBUG
	Serial.println("");
	#endif
}

void KNX_TPUART2::enableInterrupts(void) {
	// Configure interrupts for the SAVE and TW pins
	attachInterrupt(savePin, KNX_TPUART2::ISR_voltageFailure, FALLING);
	attachInterrupt(twPin,   KNX_TPUART2::ISR_tempWarning,    RISING);
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region Public methods
#endif

void KNX_TPUART2::begin(void) {
	// Initialise the UART interface with 8E1 and the configured pins
	uart.begin(baudrate, SERIAL_8E1, txPin, rxPin);
	#ifdef DEBUG
	Serial.println("TP-UART2 initialisiert");
	#endif
}

bool KNX_TPUART2::resetRequest(void) {
	sendCommand(&U_RESET_REQ, sizeof(U_RESET_REQ));
	clearBuffer();
	delay(RESPONSE_TIME);

	#ifdef DEBUG
	Serial.println("Reset Request");
	#endif

	// Warten auf Reset.indication vom TP-UART2
	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME) {
		if (uart.available()) {
			uint8_t buffer = uart.read();

			if (buffer != 0x00) { // ignore 0x00 caused by the RX line being pulled LOW
				#ifdef DEBUG
				Serial.print("Reset indication: ");
				Serial.printf("0x%02X\n", buffer);
				#endif

				if (buffer == U_RESET_IND) return true;
			}
		}
	}

	// Reset failed, attempt a hardware reset
	#ifdef DEBUG
	Serial.println("!---Reset fehlgeschlagen---! Versuche Hardware-Reset");
	#endif

	pinMode(resnPin, OUTPUT);
	digitalWrite(resnPin, LOW);
	delayMicroseconds(50);
	pinMode(resnPin, INPUT_PULLUP);

	clearBuffer();
	start = millis();
	while (millis() - start < RESPONSE_TIME) {
		if (uart.available()) {
			uint8_t buffer = uart.read();

			if (buffer != 0x00) {
				#ifdef DEBUG
				Serial.print("Reset indication: ");
				Serial.printf("0x%02X\n", buffer);
				#endif

				if (buffer == U_RESET_IND) return true;
				else {
					#ifdef DEBUG
					Serial.println("!---Hardware-Reset fehlgeschlagen---!");
					#endif
					return false;
				}
			}
		}
	}

	#ifdef DEBUG
	Serial.println("!---Hardware-Reset fehlgeschlagen---! Keine Indication erhalten");
	#endif
	return false;
}

uint8_t KNX_TPUART2::stateRequest(void) {
	sendCommand(&U_STATE_REQ, sizeof(U_STATE_REQ));
	delay(RESPONSE_TIME);
	#ifdef DEBUG
	Serial.print("State Request: ");
	#endif

	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME) {
		if (uart.available()) {
			uint8_t state = uart.read();

			#ifdef DEBUG
			Serial.printf("0x%02X", state);
			switch (state) {
				case U_STATE_IND_OK: Serial.println(" (Normal)");                                      break;
				case U_STATE_IND_TW: Serial.println(" (!---Temperature Warning---!)");                 break;
				case U_STATE_IND_PE: Serial.println(" (!---Protocol Error---!)");                      break;
				case U_STATE_IND_TE: Serial.println(" (!---Transmitter Error---!)");                   break;
				case U_STATE_IND_RE: Serial.println(" (!---Receive Error---! Checksum/Parity/Bit)");   break;
				case U_STATE_IND_SC: Serial.println(" (!---Slave Collision---!)");                     break;
				default:             Serial.println(" (unbekannter Status)");                          break;
			}
			#endif

			return state;
		}
	}
	#ifdef DEBUG
	Serial.println("!---State Request fehlgeschlagen---!");
	#endif
	return 0xFF;
}

void KNX_TPUART2::activateBusmonitorMode(void) {
	sendCommand(&U_MONITOR_REQ, sizeof(U_MONITOR_REQ));
	#ifdef DEBUG
	Serial.println("Busmonitor Mode aktiviert");
	#endif
}

uint8_t KNX_TPUART2::productIDRequest(void) {
	sendCommand(&U_PRODUCT_ID_REQ, sizeof(U_PRODUCT_ID_REQ));
	delay(RESPONSE_TIME);
	#ifdef DEBUG
	Serial.print("Product ID Request: ");
	#endif

	uint32_t start = millis();
	while (millis() - start < RESPONSE_TIME) {
		if (uart.available()) {
			uint8_t productID = uart.read();
			#ifdef DEBUG
			Serial.printf("0x%02X\n", productID);
			#endif
			return productID;
		}
	}
	#ifdef DEBUG
	Serial.println("!---Product ID Request fehlgeschlagen---!");
	#endif
	return 0xFF;
}

void KNX_TPUART2::activateBusyMode(void) {
	sendCommand(&U_BUSY_MODE_REQ, sizeof(U_BUSY_MODE_REQ));
	#ifdef DEBUG
	Serial.println("BUSY Mode aktiviert");
	#endif
}

void KNX_TPUART2::resetBusyMode(void) {
	sendCommand(&U_RESET_BUSY_MODE_REQ, sizeof(U_RESET_BUSY_MODE_REQ));
	#ifdef DEBUG
	Serial.println("BUSY Mode deaktiviert");
	#endif
}

void KNX_TPUART2::acknowledgeInformation(AcknowledgeInfo info) {
	#ifdef DEBUG
	Serial.print("Send AckInformation: ");
	#endif

	switch (info) {
		case AcknowledgeInfo::ACK:
			sendCommand(&U_ACK_INFO_ACK,  sizeof(U_ACK_INFO_ACK));
			#ifdef DEBUG
			Serial.println("ACK");
			#endif
			break;
		case AcknowledgeInfo::BUSY:
			sendCommand(&U_ACK_INFO_BUSY, sizeof(U_ACK_INFO_BUSY));
			#ifdef DEBUG
			Serial.println("BUSY");
			#endif
			break;
		case AcknowledgeInfo::NACK:
			sendCommand(&U_ACK_INFO_NACK, sizeof(U_ACK_INFO_NACK));
			#ifdef DEBUG
			Serial.println("NACK");
			#endif
			break;
		default:
			#ifdef DEBUG
			Serial.println("!---Falscher AckInformation Parameter---!");
			#endif
			break;
	}
}

void KNX_TPUART2::setMaxRepeatCount(uint8_t busyCount, uint8_t nackCount) {
	busyCount &= 0x07;
	nackCount &= 0x07;

	uint8_t MxRstCnt = (busyCount << 5) | nackCount;
	uint8_t cmd[2]   = { U_SET_MAX_REP_COUNT, MxRstCnt };
	sendCommand(cmd, sizeof(cmd));

	#ifdef DEBUG
	Serial.println("Max Repeat Count gesetzt: BUSY=" + String(busyCount) + ", NACK=" + String(nackCount));
	#endif
}

void KNX_TPUART2::activateCRC(void) {
	sendCommand(&U_ACTIVATE_CRC, sizeof(U_ACTIVATE_CRC));
	#ifdef DEBUG
	Serial.println("CRC aktiviert");
	#endif
}

void KNX_TPUART2::setPhysicalAddress(void) {
	uint8_t addressHigh = (physicalAddress.area << 4) | physicalAddress.line;
	uint8_t addressLow  = physicalAddress.device;
	uint8_t cmd[3]      = { U_SET_ADDRESS, addressHigh, addressLow };
	sendCommand(cmd, sizeof(cmd));
}

bool KNX_TPUART2::sendTelegram(uint8_t* buffer, uint8_t length) {
	uint8_t sendBuffer[2];
	for (uint8_t i = 0; i < length; i++) {
		sendBuffer[0] = (i == length - 1) ? U_DATA_END : U_DATA_START_CONTINUE;
		sendBuffer[0] |= i;          // L_Data_Continue index
		sendBuffer[1]  = buffer[i];
		sendCommand(sendBuffer, sizeof(sendBuffer));
	}
	return true;
}

bool KNX_TPUART2::checkUART(void) {
	if (!p_telegram) return false;
	bool delivered = false;

	while (uart.available()) {
		uint8_t b = uart.read();

		if (!_incomingTelegram && b == 0xBC) {
			rxIndex        = 0;
			rxExpectedLen  = 0;
			_incomingTelegram = true;
		}

		if (!_incomingTelegram) continue;

		if (rxIndex >= sizeof(rxBuffer)) {
			IncomingReset();
			continue;
		}

		rxBuffer[rxIndex++] = b;

		// Determine the length once the header is complete (up to NPCI)
		if (rxExpectedLen == 0 && rxIndex >= 6) {
			uint8_t apduLen  = (rxBuffer[5] & 0x0F) + 1;
			rxExpectedLen    = 6 + apduLen + 1; // header + apdu + checksum

			if (rxExpectedLen < 8 || rxExpectedLen > sizeof(rxBuffer)) {
				IncomingReset();
				continue;
			}
		}

		if (rxExpectedLen != 0 && rxIndex >= rxExpectedLen) {
			p_telegram->receiveIncomingTelegram(rxBuffer, rxIndex);
			IncomingReset();
			delivered = true;
		}
	}
	return delivered;
}

void KNX_TPUART2::IncomingReset(void) {
	rxIndex           = 0;
	rxExpectedLen     = 0;
	_incomingTelegram = false;
	for (uint8_t i = 0; i < sizeof(rxBuffer); i++) rxBuffer[i] = 0;
}

bool KNX_TPUART2::watchBusVoltage(void) {
	if (_voltageFailure) {
		#ifdef DEBUG
		Serial.println("!---Bus Voltage Failure---!");
		#endif
		_voltageFailure = false;
		return true;
	}
	return false;
}

bool KNX_TPUART2::watchTemperature(void) {
	if (_tempWarning) {
		if (digitalRead(twPin)) {
			#ifdef DEBUG
			Serial.println("!---Temperature Warning---!");
			#endif
			_tempWarning = false;
			return true;
		}
		else {
			_tempWarning = false;
			return false;
		}
	}
	return false;
}

#ifdef _MSC_VER
	#pragma endregion
	#pragma region ISR
#endif

void KNX_TPUART2::ISR_voltageFailure(void) {
	_voltageFailure = true;
}

void KNX_TPUART2::ISR_tempWarning(void) {
	_tempWarning = true;
}

#ifdef _MSC_VER
	#pragma endregion
#endif
