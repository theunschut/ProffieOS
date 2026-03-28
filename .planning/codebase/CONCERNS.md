# ProffieOS Codebase Concerns

This document catalogs technical concerns, risks, and areas requiring attention identified during codebase analysis. It is intended to inform planning and guide reviewers working on the SD style loader project and future features.

---

## 1. Memory Pressure and Heap Fragmentation

**Severity: High**

ProffieOS targets microcontrollers (Proffieboard v2/v3, STM32-based) with constrained RAM (typically 192–256 KB). Several patterns introduce fragmentation risk or excessive heap usage:

- `ScopedPtr<T>` and dynamic `malloc`/`free` are used throughout style instantiation. Long-running sessions with repeated style switches can fragment the heap without a garbage-collected allocator.
- `StyleFactory` allocates new `BladeStyle*` objects via `get()` on each call. Callers are responsible for deallocation, but the contract is not always explicit.
- `BladeStyleParser` constructs deeply nested style trees from string input; any parse error mid-tree can leave partially-allocated objects if exception safety is not enforced (and on embedded targets, C++ exceptions are typically disabled).
- `ProffieOSStyleManager` on ESP32 uses `ps_malloc` (PSRAM) for large buffers; the non-ESP32 path must fit everything in internal SRAM.

**Recommendation:** Audit allocation sites for corresponding frees; consider an arena/pool allocator for style objects to avoid fragmentation.

---

## 2. Style String Parsing: No Formal Grammar or Error Recovery

**Severity: High**

The style string parser (`BladeStyleParser` / `style_parser.h`) is hand-rolled and recursive. Concerns:

- No formal grammar document exists; the parser is the specification.
- Error messages are minimal; malformed SD-card style strings will silently produce null/default styles or undefined behavior.
- Recursive descent without depth limits means a pathologically nested style string (e.g., from a malicious or corrupted SD file) could exhaust the call stack on a device with a 4–8 KB stack.
- Template argument parsing relies on string matching against registered type names; a typo in an SD-card file produces no useful diagnostic.

**Recommendation:** Add a maximum recursion depth guard. Add structured error codes returned up the call chain. Consider a simple validation pass before full parsing.

---

## 3. SD Card File I/O: Blocking Operations in Audio/Effect Callbacks

**Severity: High**

SD file access (`FileReader`, `BufferedFileReader`, `ProffieOSFiles`) uses synchronous/blocking reads. On FAT-formatted SD cards, directory enumeration and file open latencies are non-deterministic (can be tens of milliseconds).

- Style loading from SD is intended to occur at blade-on or prop transitions, but if triggered inside an ISR or a tight DMA callback, it will cause audio glitches or missed PWM updates.
- The ESP32 path adds `BufferedFileReader` with a background task, but the STM32 path has no equivalent.
- `FileReader::Open()` calls directly into the FatFs/SD driver which may block on SD card bus arbitration.

**Recommendation:** Ensure SD-based style loading is always deferred to the main loop (not ISR context). Add an assertion or compile-time guard to detect accidental use in interrupt context. Document the threading/context requirements explicitly.

---

## 4. Style Reloading and Hot-Swap: Dangling Pointer Risk

**Severity: High**

When a blade style is replaced at runtime (e.g., via SD reload or prop command):

- `BladeBase::SetStyle()` takes a raw `BladeStyle*`. The old style object must be deleted by the caller; there is no ref-counting or ownership transfer mechanism.
- If a blade is mid-run (inside `run()` or `IsOn()`) when its style pointer is replaced, the next `run()` tick may access a freed object.
- `ProffieOSStyleManager::SetStyle()` wraps this but the window between `delete old_style` and updating the pointer is not protected against concurrent access (on ESP32 with FreeRTOS, this is a real race).

**Recommendation:** Use a swap-buffer or double-pointer pattern so the old style is only freed at a safe quiesce point. On ESP32, protect the swap with a critical section or mutex.

---

## 5. No Style Object Lifecycle Tests

**Severity: Medium**

The test suite (`tests/`) covers blade logic and some prop behavior, but there are no tests that:

- Construct a style from a string, run it for N frames, then destroy and rebuild it.
- Verify that repeated style switches do not leak memory.
- Exercise the parser error paths with malformed input.
- Test concurrent style swap and blade run on the ESP32 FreeRTOS target.

**Recommendation:** Add unit tests for `BladeStyleParser` covering error inputs. Add a lifecycle test that allocates/frees styles in a loop and checks heap usage before/after.

---

## 6. `CONFIG_TOP` / `CONFIG_PRESETS` Macro Expansion Complexity

**Severity: Medium**

ProffieOS configuration is driven by macros in user config files (`CONFIG_TOP`, `CONFIG_PRESETS`, `CONFIG_PROP`, etc.). These expand to large inline declarations. Concerns:

- Compilation errors in user config files produce deeply nested template error messages that are nearly unreadable.
- The SD style loader feature adds a new runtime path that must interoperate with compile-time preset declarations; the boundary between "compiled-in preset" and "SD-loaded preset" is not yet formally defined.
- Conditional compilation (`#ifdef ENABLE_SD_STYLE_LOADER`) scattered across multiple files makes it easy for a new contributor to miss a required guard.

**Recommendation:** Centralize the feature flag in a single header. Add a static_assert or linker symbol that fails the build cleanly if incompatible options are combined.

---

## 7. ESP32 / STM32 Divergence Without Abstraction Layer

**Severity: Medium**

Recent commits (e.g., `ESP32: faster file and directory abstractions`, `ESP32: fix scoped cycle counter`) add ESP32-specific code paths directly alongside STM32 code. Patterns observed:

- `#ifdef PROFFIE_ESP32` / `#ifdef TEENSYDUINO` guards are used ad hoc rather than routing through a hardware abstraction layer (HAL).
- File I/O, timers, and atomic operations each have separate `#ifdef` branches rather than a unified interface.
- This makes it easy to fix a bug on one platform and silently leave the other platform broken.

**Recommendation:** Define a minimal HAL interface for: file I/O, high-resolution timer, atomic operations, and heap allocation. Route all platform-specific code through that interface. Run CI on both targets.

---

## 8. `ScopedCycleCounter` and Profiling Overhead in Production Builds

**Severity: Low–Medium**

`ScopedCycleCounter` is used for performance profiling. On the STM32 target it accesses DWT cycle counter registers. Concerns:

- If left enabled in production firmware it adds register read overhead to every instrumented scope.
- The ESP32 fix (`ESP32: fix scoped cycle counter`) suggests the counter was silently incorrect on that platform prior to the fix — any profiling data collected before the fix is unreliable.
- There is no compile-time knob to strip all profiling instrumentation for release builds.

**Recommendation:** Wrap `ScopedCycleCounter` use in `#ifdef PROFILING_ENABLED` and ensure the default release build strips it.

---

## 9. Dither Matrix as `const` Data in Flash

**Severity: Low**

The recent commit `make dither matrix const` moves the dither matrix to flash (read-only data). On STM32, this is correct and saves SRAM. However:

- If any code path ever writes to the matrix (e.g., a future customization feature), it will cause a hard fault on hardware that enforces flash write protection.
- The const enforcement is correct today but should be explicitly documented so future contributors do not add mutable accessors.

**Recommendation:** Add a comment at the definition site explaining that this data must remain const and reside in flash.

---

## 10. Fett263 Prop Integration Surface Area

**Severity: Low–Medium**

The Fett263 prop (`props/fett263.h`) is a large, feature-rich prop file used by many users (including the project owner). It interacts with style loading through:

- Direct preset manipulation (`next_preset()`, `set_preset()`)
- Font and style change events that must coordinate with SD-based style loading
- Battle mode and gesture callbacks that may trigger style transitions at high frequency

If the SD style loader introduces latency in style transitions, Fett263 gesture response times will degrade noticeably. This prop also has the largest surface area for prop/style interaction bugs.

**Recommendation:** Test the SD style loader integration specifically against a Fett263 prop configuration. Define maximum acceptable latency for style transitions (e.g., < 50 ms) and measure against it.

---

## 11. No Explicit Ownership Semantics Documentation

**Severity: Low–Medium**

Throughout the codebase, raw pointers are used for style objects, blade objects, and prop objects. There is no consistent documentation (comments, naming conventions, or smart pointer usage) that communicates:

- Who owns a given object.
- Who is responsible for deletion.
- Whether a pointer may be null.

This makes it difficult for new contributors to reason about lifetimes, especially in the SD style loader which introduces a new dynamic ownership path.

**Recommendation:** Adopt a lightweight convention (e.g., `owner_ptr<T>` typedef = `T*` with a comment, or actual `std::unique_ptr` where the toolchain supports it) and document it in CONVENTIONS.md.

---

## 12. Serial/USB Debug Output Left Enabled by Default

**Severity: Low**

`Serial.println` and `STDOUT` debug output is gated on `#ifdef ENABLE_DEVELOPER_COMMANDS` and similar guards, but some diagnostic paths output unconditionally via `PVLOG_DEBUG`. On a production saber, this wastes CPU cycles and may interfere with real-time audio timing.

**Recommendation:** Audit all unconditional output paths. Ensure release builds compile to zero debug output.

---

*Document generated: 2026-03-28. Based on static codebase analysis of the `claude/flamboyant-dirac` branch.*
