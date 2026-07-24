# Getting Started

Talk to a KNX bus from an Arduino sketch in a few lines. No ETS, no KNX-stack
expertise: you describe devices by **what they are** — a light, a blind, a
temperature — and the library handles the datapoints, framing and bus timing.

```cpp
#include <InnotreeKNX.h>

InnotreeKNX knx("1.1.5");                 // this device's KNX address
KnxLight  lamp(knx, "0/1/1", "0/3/0");  // command address, status address

void setup() {
    knx.begin();
}

void loop() {
    knx.loop();
    lamp.toggle();      // flips the lamp on the bus
    delay(5000);
}
```

That is a complete, working sketch. Read on to receive status, use other device
types, and send raw values.

---

## What you need

- A supported Arduino board (developed on the **Seeed XIAO ESP32-C6**).
- The **Innotree STKNX breakout board**, which connects the MCU to the KNX bus.
- This library, added through PlatformIO or the Arduino IDE.

Each device on a KNX bus has a **physical address** like `1.1.5` (area.line.device),
and devices talk to each other through **group addresses** like `0/1/1`
(main/middle/sub). You set the physical address once when you create the node, and
give each device object the group address(es) it uses. Group addresses are written
directly in your sketch — there is no ETS project to configure.

## Installing

**PlatformIO** — add the library to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/innotree-labs/KNX_Library.git
```

**Arduino IDE** — download the library and add it through
*Sketch → Include Library → Add .ZIP Library…*, then `#include <InnotreeKNX.h>`.

## The bus node

Everything starts with one `InnotreeKNX`, created from this device's physical
address. Call `begin()` once in `setup()`, and `loop()` on every pass of your
sketch's `loop()` so the library can receive telegrams:

```cpp
InnotreeKNX knx("1.1.5");

void setup() {
    knx.begin();
}

void loop() {
    knx.loop();     // never block this with long delay()s
}
```

> `loop()` must run often. Do not gate it behind a long `delay()`, or incoming
> status updates will arrive late or be missed. Use `millis()` for timing instead.

## Device objects

A device object is created once, usually as a global, from the group address(es)
it uses. It sends commands, remembers the last value, and calls a function you
provide when the value changes on the bus.

```cpp
InnotreeKNX knx("1.1.5");
KnxLight  lamp(knx, "0/1/1", "0/3/0");   // sends on 0/1/1, reads status on 0/3/0

void onLampChanged(bool on);             // declared here, defined below

void setup() {
    knx.begin();
    lamp.onUpdate(onLampChanged);        // called whenever the lamp changes
}

void loop() {
    knx.loop();
}

void onLampChanged(bool on) {
    // react to the new state
}
```

Commanding the light:

```cpp
lamp.on();          // switch on
lamp.off();         // switch off
lamp.toggle();      // flip, tracking the real bus state
bool state = lamp.isOn();   // last known state, no bus traffic
```

Most device objects take a **command** group address and a **status** group
address. Pass one address to use it for both. Every command returns `true` if the
bus confirmed it.

### Available device objects

| Object | For |
|---|---|
| `KnxLight` | a switchable light |
| `KnxDimmLight` | a light that also dims (brighter / darker / stop) |
| `KnxRGB` | an RGB colour light |
| `KnxBlind` | a blind or roller shutter (up / down / stop) |
| `KnxTemperature`, `KnxHumidity` | publish or read a climate value |
| `KnxTime`, `KnxDate`, `KnxDateTime` | publish or read date and time |
| `KnxPercent` | a 0–100 % value |
| `KnxChar` | a single character |
| `KnxFloat` | a 32-bit number |
| `KnxObject` | any other datapoint — you pick the type |

Each is documented on its own page in the reference, with its constructors and
methods.

## Sending a raw value

For a one-off send to any group address, without keeping a device object, use the
node directly with a typed value:

```cpp
knx.send("0/4/2", Dpt9(21.5f));     // send 21.5 as a floating-point value
knx.send("0/1/1", Dpt1(true));      // send an on/off value
```

The `Dpt*()` value factories (`Dpt1`, `Dpt9`, `Dpt232`, …) each accept only the
data their datapoint expects, so a wrong value type is caught by the compiler
rather than sent as a bad telegram. This send-only path has no status callback —
use a device object when you need to receive.

## Diagnosing a problem

Turn on verbose tracing to see every telegram the library sends and receives,
printed over Serial with a `[knx]` prefix:

```cpp
knx.enableDebugMode(true);   // call before begin() to trace start-up too
```

Leave it off in normal operation — the printing is chatty and slows the receive
path. Turn it on to explain a run that is misbehaving.

---

Full class and method documentation is in the **API reference**.
