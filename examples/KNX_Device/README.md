# KNX_Device — legacy thesis button layer (NOT built, does NOT compile as-is)

This is the original thesis button state machine (`SingleButtonOperation`,
`TwoButtonOperation`, `TwoButtonDimming`, `TwoButtonSwitching`, `SingleButtonSwitching`,
plus the serial test buttons). It is kept here as a **historical reference only**.

- It is under `examples/`, so PlatformIO's LDF **does not compile it** (`pio run` /
  `pio test` exclude this directory).
- It **will not compile against the current library**: it drives KNX through the removed
  transitional shims (`KNX::switchLight`, `KNX::dimLightInterval`) and the old
  `KNX_Defines.h` include, all deleted when the coordinator/object redesign landed.

## Porting it back (PLAN §8, not yet done)

The button state machine itself (`SingleButtonOperation` / `TwoButtonOperation` short/long/
hold detection) is still good. To revive it, rewire the concrete subclasses to hold a
`KnxLight&` / `KnxDimmLight&` / `KnxObject&` instead of a `KNX*` + `String` GAs, and replace
each `p_knx->switchLight(...)` / `dimLightInterval(...)` call with the intent method
(`light.toggle()`, `dimm.brighter()`, `dimm.stopDimming()`, …). See `src/main.cpp` for the
intent-object API this layer would sit on top of.
