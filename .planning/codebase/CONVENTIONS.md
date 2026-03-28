# Coding Conventions

**Analysis Date:** 2026-03-28

## Naming Patterns

**Files:**
- Header guards: `#ifndef [DIRECTORY]_[FILENAME]_H` / `#define [DIRECTORY]_[FILENAME]_H`
  - Example: `#ifndef COMMON_COLOR_H` in `common/color.h`
  - Example: `#ifndef BUTTONS_BUTTON_BASE_H` in `buttons/button_base.h`
- Implementation files: `.cpp` for standalone compile units, `.ino` for Arduino sketch
- Test files: `tests.cpp` (one per module) containing all tests for a subsystem
  - Example: `common/tests.cpp`, `blades/tests.cpp`, `buttons/tests.cpp`

**Classes:**
- PascalCase for class names
  - Examples: `Color8`, `Angle`, `BladeBase`, `BladeStyle`, `ButtonBase`, `LinkedPtr`
- Base classes often have "Base" suffix
  - Examples: `BladeBase`, `ButtonBase`, `SaberBase`
- Template implementations often have "Impl" suffix
  - Example: `StyleFactoryImpl` in `styles/blade_style.h`

**Methods:**
- camelCase for method names
  - Examples: `printTo()`, `clampi32()`, `combine_byteorder()`, `getColor()`
  - Protected/private methods: same camelCase
  - Examples: `Read() override` in `buttons/button.h`, `DebouncedRead()` in `button_base.h`
- Virtual methods marked with `override` keyword
- Getter methods typically prefix with `get_` or just return without prefix
  - Examples: `get_max_arg()`, `max_arg()`, `fixed()`, `fixed0()`
- State machine methods: `Loop()`, `Send()`, `SendClick()`

**Member Variables:**
- Private/protected members use trailing underscore (`_`) convention
  - Examples: `ptr_`, `pin_`, `angle_`, `name_`, `button_`, `saved_event_`
  - Local state variables: `push_millis_`, `press_count_`, `prev_`, `next_`
- Public struct members: no trailing underscore (direct data members)
  - Examples: `r`, `g`, `b` in `Color8`; `ohm`, `presets`, `num_presets` in `BladeConfig`

**Constants and Macros:**
- Config constants: SCREAMING_SNAKE_CASE
  - Examples: `BUTTON_DOUBLE_CLICK_TIMEOUT`, `BUTTON_HELD_TIMEOUT`, `ENABLE_AUDIO`, `DISABLE_MOTION`
- Macro functions: mix of styles depending on purpose
  - Config-like: SCREAMING_SNAKE_CASE (`#define NELEM`, `#define PONUA`)
  - Technical macros: various styles
    - Examples: `PROFFIEOS_ASSERT`, `STATE_MACHINE_BEGIN`, `YIELD()`, `LOCK_SD(X)`
  - Enum-like macros: SCREAMING_SNAKE_CASE
    - Examples: `ADC_SAMPLE_TIME_2_5`, `RGB`, `BGR`, `WRGB`
- Static constants: camelCase or SCREAMING_SNAKE_CASE depending on type
  - Example: `constexpr float MaxAmps` (in test structs), `static constexpr int inline_num_bytes()`

**Enums:**
- Enum class names: PascalCase
  - Examples: `enum Byteorder`, `enum class LayerRunResult`, `enum class FunctionRunResult`
- Enum values: SCREAMING_SNAKE_CASE
  - Examples: `RGB=0x123`, `WRGB=0x1234`, `OPAQUE_BLACK_UNTIL_IGNITION`, `ZERO_UNTIL_IGNITION`

**Functions:**
- Global functions: camelCase
  - Examples: `fract()`, `clampi32()`, `itoa()`, `millis()`, `micros()`
- Arduino-like global functions: lowercase with underscores
  - Examples: `digitalRead()`, `digitalWrite()`, `pinMode()`, `pulseIn()`

## Code Style

**Formatting:**
- No auto-formatter detected (no .clang-format, .prettierrc, or eslint config found)
- Manual style observed:
  - Indentation: 2 spaces (sometimes 4 in some files, appears to be historical)
  - Line length: generally 80-100 characters, but no strict enforcement
  - Brace style: Allman style (opening brace on new line for functions and control flow)
    ```cpp
    class Color8 {
    public:
      constexpr Color8() : r(0), g(0), b(0) {}
      Color8 mix(const Color8& other, int x) const {
        return Color8(...);
      }
    };
    ```
  - Constructor member initialization lists: aligned on continuation lines
    ```cpp
    ButtonBase(const char* name, enum BUTTON button)
      : Looper(),
        CommandParser(),
        name_(name),
        button_(button) {
    }
    ```

**Linting:**
- No linter detected in build system
- Code follows general C++ best practices but relies on manual review

## Import Organization

**Order:**
1. Standard library headers: `#include <algorithm>`, `#include <type_traits>`, `#include <vector>`, `#include <stdio.h>`
2. Platform-specific headers (Arduino): `#include <Arduino.h>`, `#include <DMAChannel.h>`
3. Local project headers: `#include "common/color.h"`, `#include "monitoring.h"`
4. Conditional/ifdef blocks for platform-specific includes

**Pattern:**
- Headers guard against double inclusion
- No namespace pollution (all code is in global namespace or uses classes to organize)
- Example from `ProffieOS.ino`:
  ```cpp
  #include "common/resources.h"
  #define CONFIG_TOP
  #include CONFIG_FILE    // Dynamic include based on config
  #undef CONFIG_TOP
  #include "common/capabilities.h"
  ```

**Path Aliases:**
- No path aliases detected (no using namespace, no import aliases)
- Relative includes used throughout: `#include "common/color.h"`, `#include "button_base.h"`

## Error Handling

**Assertion Macro:**
- Used for debug-only assertions: `PROFFIEOS_ASSERT(X)` defined in `common/common.h`
- Disabled in release builds (`#ifndef ENABLE_DEBUG`)
  ```cpp
  #ifdef ENABLE_DEBUG
  #define PROFFIEOS_ASSERT(X) do {
    if (!(X)) {
      interrupts();
      if (!(X)) STDERR << "ASSERT " << #X << " FAILED @ " << __FILE__ << ":" << __LINE__ << "\n";
      while(true);  // Halt on assertion failure
    }
  } while(0)
  #else
  #define PROFFIEOS_ASSERT(X) do {} while(0)
  #endif
  ```
- Strategy: Check condition, enable interrupts if disabled, log to STDERR, infinite loop (safe for embedded)

**Return Codes:**
- Methods return `bool` to indicate success/failure
  - Example: `bool Load(int preset_num)` in `current_preset.h`
- Some methods return `nullptr` or null objects to indicate failure
  - Example: `const char* GetSaveDir()` returns `NULL`
- No exception handling used (typical for embedded/Arduino)

**Null Checks:**
- Explicit pointer checks: `if (ptr)`, `if (!ptr)`, `if (ptr == nullptr)`
- Example: `if (tmp) free(tmp);` in `current_preset.h`

## Logging

**Framework:** Custom `STDOUT` and `STDERR` via `Print` class abstraction

**Pattern:**
- `STDOUT` for normal output: `STDOUT.print()`, `STDOUT.println()`, `STDOUT << value`
- `STDERR` for error output: `STDERR << "message"`
- Operator overloading via `operator<<` for fluent logging
  - Example: `STDERR << "ASSERT " << #X << " FAILED @ " << __FILE__ << ":" << __LINE__ << "\n"`

**Logging Locations:**
- `common/stdout.h`: Main logging infrastructure with `ConsoleHelper` class
- `common/monitoring.h`: Monitoring/debug output control
- Battery/status output uses STDOUT: `STDOUT.print("Battery voltage: ");`
- Configuration output uses STDOUT: `STDOUT << variable_name << " " << max_arg << "\n"`

**Levels:**
- No structured log levels (no DEBUG, INFO, WARN, ERROR explicitly)
- Logging based on code path: status updates, configuration, debug info when enabled
- Monitoring can filter output: `monitor.IsMonitoring(Monitoring::MonitorSerial)`

## Comments

**When to Comment:**
- Configuration parameters: extensive comments above `#define` blocks
- Algorithms: comments above complex calculation code
- Embedded constraints: notes about memory/performance
- TODO/FIXME: inline notes for future work
  - Example: `// TODO: activate/deactivate aren't required anymore...` in `styles/blade_style.h`
- Test helper explanations: comments before test functions

**JSDoc/Doxygen:**
- Not used in this codebase
- Method purposes documented via inline comments and class-level documentation
- Example: Block comment above class explaining purpose in `buttons/button.h`

## Function Design

**Size:**
- Methods generally kept short (10-30 lines)
- Complex state machines use helper methods: `Send()`, `SendClick()` in `button_base.h`
- Inline state machine expansion via `STATE_MACHINE_BEGIN()` / `YIELD()` macros

**Parameters:**
- Methods prefer const references for complex objects: `Color8 mix(const Color8& other, int x) const`
- Simple types passed by value: `int, float, uint8_t`
- Pointers used sparingly, typically for optional parameters
- Example constructor: `Button(enum BUTTON button, int pin, const char* name)`

**Return Values:**
- Boolean for success/failure: `bool Load()`, `bool DebouncedRead()`
- Value types for queries: `int fixed()`, `float battery()`
- Void for state-changing operations: `void Loop() override`
- Const return for immutability: `const char* name() override`

## Module Design

**Exports:**
- Headers define public interfaces (classes, functions, macros)
- Implementation files (.cpp) contain standalone code (tests, main functions)
- No namespace wrapping (all in global namespace)

**Barrel Files:**
- Not used; each module has its own header

**Interdependencies:**
- Clear dependency direction: `common/` used by all, `blades/` used by core logic
- Circular dependencies avoided via forward declarations
  - Example: `class BladeBase;` in `common/blade_config.h`
- Configuration-driven composition: `ProffieOS.ino` pulls all necessary pieces via CONFIG_FILE

**Module Organization Pattern:**
- Each subsystem (blades, buttons, styles, sound, etc.) is self-contained
- Headers in subsystem directory, tests in same directory as `tests.cpp`
- Example: `blades/blade_base.h`, `blades/simple_blade.h`, `blades/tests.cpp`

## Memory Management

**Approach:** Mix of stack allocation, manual heap allocation, and smart pointers for embedded constraints

**Stack Allocation:**
- Preferred for small structures and local variables
- Examples: `Color8`, `Angle`, local state machines in `ButtonBase`
- Embedded constraint: RAM is limited (STM32L4, Teensy)

**Heap Allocation:**
- Used for large collections and dynamic objects: `BladeStyle* new BladeStyle()` in `StyleFactoryImpl`
- Example in `style_blade.h`:
  ```cpp
  template<class STYLE>
  class StyleFactoryImpl : public StyleFactory {
    BladeStyle* make() override {
      STDERR << "Style RAM = " << sizeof(STYLE) << "\n";
      return new STYLE();
    }
  };
  ```
- Typically not deleted (lifetime tied to saber initialization)
- No delete calls detected in most code (rely on process termination)

**LinkedPtr Pattern:**
- Custom reference-counted pointer using doubly-linked list (no ref count overhead)
- Located in `common/linked_ptr.h`
- Used for string management: prevents memory leaks without atomic reference counts
  - Example: `LinkedPtr<const char*>` for preset names, fonts, tracks
- Provides RAII semantics: destructor handles cleanup
  ```cpp
  template<class T, class Free>
  class LinkedPtr {
  public:
    ~LinkedPtr() { leave(); }
    LinkedPtr(LinkedPtr const& other) { join(other); }
    LinkedPtr& operator=(LinkedPtr const& other) { ... }
  };
  ```

**Allocation Macros:**
- Placement new used for embedded union types: `new (&sf_file_) SerialFlashFile;`
- No custom allocator macros detected

**Embedded Constraints:**
- No `std::vector` in main code (used only in POSIX tests)
- Static arrays preferred: `uint8_t data[MAX_SIZE]`
- Pre-allocated buffers checked for overflow: `if (bufsize_ > 1) { ... }`

## Inlining and Performance

**Inline Hints:**
- `__attribute__((always_inline))` used for performance-critical methods
  - Example: `static constexpr int inline_num_bytes(int byteorder) __attribute__((always_inline))`
- Constexpr used for compile-time evaluation
  - Examples: `constexpr Angle()`, `constexpr float TRUNC(float f)`

**Optimization Attributes:**
- Careful use of `[[no_unique_address]]` for zero-cost abstractions (C++20)
- Macro wrapper: `#define PONUA [[no_unique_address]]` with fallback for older compilers
  - Saves RAM when empty base classes are used

## State Machine Pattern

**Framework:** Manual state machine via `Loop()` override + `STATE_MACHINE_BEGIN()` / `YIELD()` macros

**Example** from `buttons/button_base.h`:
```cpp
void Loop() override {
  STATE_MACHINE_BEGIN();
  while (true) {
    while (!DebouncedRead()) {
      if (saved_event_ && millis() - push_millis_ > BUTTON_DOUBLE_CLICK_TIMEOUT) {
        Send(saved_event_);
        saved_event_ = 0;;
      }
      YIELD();
    }
    // ... more state transitions
  }
}
```

**Pattern:**
- `STATE_MACHINE_BEGIN()` initializes state tracking
- `YIELD()` pauses execution and returns control to scheduler
- Variables persist across yields (saved in class members)
- No external state machine library used

---

*Convention analysis: 2026-03-28*
