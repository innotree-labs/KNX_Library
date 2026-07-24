# Examples {#examples}

Three ways to use the library, from the most convenient to the most general.

## Device object

A device object is the usual way: create one for each thing on the bus, command it
with named methods, and register a callback for status changes. Here a light is
switched and its status is tracked.

```cpp
#include <InnotreeKNX.h>

InnotreeKNX knx("1.1.5");                 // this device's KNX address
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // command address, status address

void onLampChanged(bool on);            // defined below

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);       // called when the lamp changes on the bus
}

void loop() {
    knx.loop();                         // receives telegrams, fires callbacks
    lamp.toggle();                      // command the light
    delay(5000);                        // (a real sketch would use millis())
}

void onLampChanged(bool on) {
    // react to the light's new state
}
```

The object remembers the last value, so `lamp.isOn()` reads it back without any bus
traffic, and `lamp.toggle()` flips relative to the real bus state.

## Stateless send

To send a one-off value to any group address without keeping an object, use the node
directly with a typed value. There is no status callback on this path — it only sends.

```cpp
#include <InnotreeKNX.h>

InnotreeKNX knx("1.1.5");

void setup() {
    knx.begin();
    knx.send("0/1/1", Dpt1(true));      // an on/off value
    knx.send("0/4/2", Dpt9(21.5f));     // a temperature, as a floating-point value
}

void loop() {
    knx.loop();
}
```

Each `Dpt*()` factory accepts only the data its datapoint expects, so a wrong value
type is a compile error rather than a bad telegram on the bus.

## Custom KnxObject

For a datapoint that has no dedicated class, use the generic `KnxObject`: give it the
group address and the datapoint type once, then send with `write()` and receive with
`onUpdate()`. Here a 16-bit counter (DPT 7) is published and read.

```cpp
#include <InnotreeKNX.h>

InnotreeKNX knx("1.1.5");
KnxObject counter(knx, "0/5/0", KnxDpt::DPT7);   // 16-bit unsigned value

void onCounter(const KnxValue& value);

void setup() {
    knx.begin();
    counter.onUpdate(onCounter);
    counter.write(Dpt7(1000));          // send a value
}

void loop() {
    knx.loop();
}

void onCounter(const KnxValue& value) {
    uint16_t count = value.asU16();     // read the received value as its type
}
```

`KnxObject` hands your callback a generic ::KnxValue; read it with the accessor for
its type (`asU16()`, `asFloat()`, `asBool()`, …).
