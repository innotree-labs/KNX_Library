#pragma once
/**
 * @name KnxEnums.h
 * @date 16.07.2026
 * @authors Florian Wiesner
 * @details KNX protocol enumerations: datapoint types, priorities, service types and
 *          the intent behaviours used by the device layer. Deliberately Arduino-free
 *          (only <cstdint>) so it can be shared with the host-testable value codec.
*/

//---- Libraries ----
#include <cstdint>

/**
 * @brief The datapoint type of a KNX value — what kind of data it holds and how big it is.
 *        You pass one to a generic KnxObject; the named device classes pick it for you.
*/
enum class KnxDpt : uint8_t {
	DPT1	= 1,	///< 1 bit — on/off, true/false
	DPT2	= 2,	///< 2 bit — a value 0..3
	DPT3	= 3,	///< 4 bit — a relative dimming or blind step
	DPT4	= 4,	///< 8 bit — a single character
	DPT5	= 5,	///< 8 bit — unsigned 0..255 (also used for 0..100 %)
	DPT6	= 6,	///< 8 bit — signed -128..127
	DPT7	= 7,	///< 16 bit — unsigned 0..65535
	DPT8	= 8,	///< 16 bit — signed -32768..32767
	DPT9	= 9,	///< 16 bit floating point (e.g. temperature)
	DPT10	= 10,	///< time of day
	DPT11	= 11,	///< calendar date
	DPT12	= 12,	///< 32 bit — unsigned
	DPT13	= 13,	///< 32 bit — signed
	DPT14	= 14,	///< 32 bit floating point
	DPT16	= 16,	///< a short text string
	DPT19	= 19,	///< combined date and time
	DPT232	= 232,	///< RGB colour
	UNKNOWN 		///< no/unknown datapoint type
};

//---- Link-layer acknowledge codes ----
enum class AcknowledgeInfo : uint8_t {
	ACK,
	NACK,
	BUSY
};

//---- Telegram priority (control field bits 3..2) ----
// Values and names per KNX TP1 spec (03_02_02) Figure 28: p1p0 = 00 system,
// 01 normal, 10 urgent, 11 low. The previous names were shifted by one — "High"
// carried the normal-priority value and "Normal" carried the low-priority one.
// Group communication conventionally runs at Low, which is KnxFrame::build's default.
enum class KnxPriority : uint8_t {
	System = 0x00,	// p1p0 = 00 — reserved for system-critical traffic
	Normal = 0x04,	// p1p0 = 01
	Urgent = 0x08,	// p1p0 = 10 (formerly misnamed "Alarm")
	Low    = 0x0C	// p1p0 = 11 — the default for group communication
};

//---- Group value service type ----
enum class GroupValueType : uint8_t {
	Read,
	Response,
	Write,
	Unknown
};

//---- Destination address type (control field octet 5, bit 7 "AT") ----
// TP1 spec (03_02_02) §2.2.4.4: AT = 1 addresses a group, AT = 0 a single device by
// its individual address (device management, e.g. a ping from ETS).
enum class KnxAddressType : uint8_t {
	Group,
	Individual
};

/**
 * @brief Advanced: the transport-layer kind of a point-to-point message. You pass Connect,
 *        Disconnect, Ack or Nak to KnxCoordinator::sendControl(); the Data* kinds identify what
 *        a received message is.
*/
enum class KnxTpci : uint8_t {
	DataGroup,        ///< ordinary group-address message
	DataIndividual,   ///< a message to one device, outside a connection
	DataConnected,    ///< a message to one device, inside a connection
	Connect,          ///< open a connection to a device
	Disconnect,       ///< close a connection to a device
	Ack,              ///< acknowledge a received message
	Nak,              ///< reject a received message
	Unknown           ///< unrecognised
};

//---- Intent behaviours (device layer) ----
enum class SwitchingBehaviour : uint8_t {
	Off    = 0x00,
	On     = 0x01,
	Toggle = 0x02
};

enum class DimmingBehaviour : uint8_t {
	Darker   = 0x00,
	Brighter = 0x01
};

/**
 * @brief How large a dimming step KnxDimmLight::brighter() / darker() takes. A larger percentage
 *        is a coarser step; the default (Percent_100) moves the light in one full step.
*/
enum class DimmIncrement : uint8_t {
	Stop         = 0x00,	///< stop a running dimming ramp
	Percent_100  = 0x01,	///< a full step (100 %)
	Percent_50   = 0x02,	///< a 50 % step
	Percent_25   = 0x03,	///< a 25 % step
	Percent_12_5 = 0x04,	///< a 12.5 % step
	Percent_6    = 0x05,	///< a 6.25 % step
	Percent_3    = 0x06,	///< a 3.125 % step
	Percent_1_5  = 0x07		///< the finest step (1.5625 %)
};
