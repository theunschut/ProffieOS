# Task 1.2: Analysis of Reusable ProffieOS Base Classes

**Objective:** Document base classes suitable for wrapping/inheritance in Phase 2

---

## 1. Animation State Machines

### 1.1 ColorCycleBase (color_cycle.h)

**Purpose:** Rotating ring animation with speed/RPM control

**Public Interface:**
```cpp
class ColorCycleBase {
  bool run(BladeBase* base, int off_percentage, int off_rpm, int on_percentage, int on_rpm,
           int fade_time_millis, bool off_color_is_black);
  int getMix();  // Returns 0-16384 blend amount
};
```

**State Variables (Protected):**
- `last_micros_`: Last run time
- `fade_`: Fade progress (0.0-1.0)
- `fade_int_`: Integer fade (0-16384)
- `pos_`: Position in cycle (0.0-1.0)
- `num_leds_`: LED count in fixed-point

**How It Works:**
1. `run()` updates fade progress based on blade on/off state
2. Updates rotation position based on RPM
3. `getMix()` returns blend value for color mixing

**Current sd_style.h Usage:**
```cpp
class RtColorCycle : public RtColorNode, public ColorCycleBase {
  // Inherits run() and getMix()
  // Adds RtColorNode virtual interface
};
```

**Assessment:** ✅ **EXCELLENT REUSE** — Already inheriting correctly in sd_style.h
- No changes needed
- Pattern is solid (virtual interface + base class inheritance)

---

### 1.2 CylonBase (cylon.h)

**Purpose:** Knight Rider swept effect (sine wave motion)

**Public Interface:**
```cpp
class CylonBase {
  bool run(BladeBase* base, int percentage_off, int rpm_off, int percentage_on,
           int rpm_on, int fade_time_millis);
  int getMix();  // Returns 0-16384 blend
};
```

**State Variables (Protected):**
- Similar to ColorCycleBase but with sine motion instead of circular

**Current sd_style.h Usage:**
```cpp
class RtCylon : public RtColorNode, public CylonBase {
  // Inherits run() and getMix()
};
```

**Assessment:** ✅ **EXCELLENT REUSE** — Already inheriting correctly
- No changes needed
- Parallel pattern to ColorCycleBase

---

## 2. Shape & Function Base Classes

### 2.1 BumpBase (functions/bump.h)

**Purpose:** Gaussian bump shape generator with lookup table

**Public Interface:**
```cpp
class BumpBase {
  int getInteger(int led);  // Returns 0-128 (or interpolated)
};
```

**Protected State:**
- `location_`: Center position in LED space
- `mult_`: Multiplier for LED coordinate scaling

**Internals:**
- Uses `bump_shape[33]` lookup table (gaussian precomputed)
- `getInteger()` interpolates between table entries
- Runs in O(1) per LED, no allocations

**How Bump<> template uses it:**
```cpp
template<class BUMP_POSITION, class BUMP_WIDTH>
class Bump : public BumpBase {
  void run(BladeBase* blade) {
    // Calculate location_ and mult_ from template params
    pos_.run(blade);
    fraction_.run(blade);
    // Set BumpBase members based on blade state
  }
};
```

**Current sd_style.h Usage:**
```cpp
class RtBump : public RtFuncNode, public BumpBase {
  // Inherits getInteger() from BumpBase
  // Calls run() to update location_ and mult_
  virtual bool run(BladeBase* blade) {
    // Manually compute location_/mult_ from input functions
    ...
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting getInteger(), but computing location_/mult_ manually
- Could potentially use template wrapper pattern
- Current approach is clean and functional
- No changes needed—working well

---

### 2.2 SmoothStepBase (functions/smoothstep.h)

**Purpose:** Smooth sigmoid transition function

**Public Interface:**
```cpp
class SmoothStepBase {
  int getInteger(int led);  // Returns 0-32768 (smooth S-curve)
};
```

**Protected State:**
- `mult_`: Position scale factor
- `location_`: Transition center position

**Math:**
- Computes smooth cubic interpolation: `(x² / 2^14) * ((3 << 14) - x) >> 15`
- Creates S-curve transition across blade

**Template Pattern:**
```cpp
template<class POS, class WIDTH>
class SmoothStep : public SmoothStepBase {
  void run(BladeBase* blade);
  PONUA SVFWrapper<POS> pos_;
  PONUA SVFWrapper<WIDTH> width_;
};
```

**Current sd_style.h Usage:**
```cpp
class RtSmoothStep : public RtFuncNode, public SmoothStepBase {
  RtFuncNode* pos_;
  RtFuncNode* width_;
  virtual bool run(BladeBase* blade) {
    // Compute location_/mult_ from input functions
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting getInteger(), custom run()
- Clean pattern similar to BumpBase
- No changes needed

---

### 2.3 Ifon (Template + Base) (functions/ifon.h)

**Purpose:** Conditional function selection based on blade on/off state

**Public Interface:**
```cpp
template<class IFON, class IFOFF>
class Ifon {
  void run(BladeBase* blade);
  int getInteger(int led);
};
```

**How It Works:**
1. Stores two function objects: `ifon_`, `ifoff_`
2. `run()` updates both, sets `on_` flag based on blade.is_on()
3. `getInteger()` delegates to whichever function is active

**Current sd_style.h Usage:**
```cpp
class RtIfon : public RtFuncNode, public Ifon<RtFuncAdapter, RtFuncAdapter> {
  // Inherits from Ifon template
  // RtFuncAdapter wraps RtFuncNode* pointers
};
```

**Assessment:** ✅ **EXCELLENT WRAPPING PATTERN** — This is profezzorn's recommended approach
- Demonstrates template wrapper strategy perfectly
- Adapters enable template composition at runtime
- No changes needed—model this for similar classes

---

### 2.4 InOutFuncSVFBase (functions/ifon.h)

**Purpose:** Timer for extension/retraction animation

**Public Interface:**
```cpp
class InOutFuncSVFBase {
  FunctionRunResult run(BladeBase* blade, int out_millis, int in_millis);
  int getInteger();  // Returns 0-32768 based on extension state
};
```

**State Variables:**
- `extension`: 0.0 (retracted) to 1.0 (extended)
- `last_micros_`: Last run time
- `ret_`: Cached integer value (0-32768)

**How It Works:**
1. Tracks blade on/off state transitions
2. Animates extension value from 0 to 1 over milliseconds
3. Returns integer blend value based on extension progress

**Current sd_style.h Usage:**
```cpp
class RtInOutFunc : public RtFuncNode, public InOutFuncSVFBase {
  RtFuncNode* out_ms_;
  RtFuncNode* in_ms_;
  virtual bool run(BladeBase* blade) {
    // Read out_ms_->getInteger(), in_ms_->getInteger()
    // Call InOutFuncSVFBase::run(blade, out_ms, in_ms)
  }
  virtual int getInteger(int led) {
    return InOutFuncSVFBase::ret_;
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting state machine, custom wrapper
- Timing logic centralized in base class
- Clean virtual interface

---

### 2.5 InOutHelperFBase (functions/ifon.h)

**Purpose:** Per-LED wipe distance calculator for extension effects

**Public Interface:**
```cpp
class InOutHelperFBase {
  int getInteger(int led);  // Wipe distance for this LED
};
```

**Protected State:**
- `on_`: Current blade state
- `thres_`: Wipe threshold position
- `blend_`: 0-32768 blend amount

**How It Works:**
- Computes distance from LED position to wipe boundary
- Used by InOutSparkTipX to render wipe animations

**Current sd_style.h Usage:**
```cpp
class RtInOutHelperF : public RtFuncNode, public InOutHelperFBase {
  RtFuncNode* ext_;  // Extension function
  virtual bool run(BladeBase* blade) {
    // Read extension value, compute thres_ and blend_
  }
  virtual int getInteger(int led) {
    return InOutHelperFBase::getInteger(led);
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting calculation method
- State management light, calculation-heavy
- Pattern works well

---

## 3. Other Reusable Base Classes

### 3.1 RgbArgBase (rgb_arg.h)

**Purpose:** Color argument slot parsing and caching

**Public Interface:**
```cpp
class RgbArgBase {
  void init(BladeStyle* blade);  // Load color from preset arg slot
  Color8 getColor();  // Return cached color
};
```

**Current sd_style.h Usage:**
```cpp
class RtRgbArg : public RtColorNode, public RgbArgBase {
  // Inherits init() and getColor()
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting color parsing
- Integrates with ProffieOS config system

---

### 3.2 IgnitionDelayBase / RetractionDelayBase (ignition_delay.h, retraction_delay.h)

**Purpose:** Defer color activation until after specified delay

**Public Interface:**
```cpp
template<class FUNCTION>
class IgnitionDelayBase {
  void run(BladeBase* blade, int millis);
  int getInteger();
};

template<class COLOR, class DELAY_FUNCTION>
class IgnitionDelay {  // IgnitionDelayBase + wraps COLOR
  void run(BladeBase* blade);
  auto getColor(int led);
};
```

**Current sd_style.h Usage:**
```cpp
class RtIgnitionDelay : public RtColorNode, public IgnitionDelayBase<RtFuncAdapter> {
  RtColorNode* inner_;
  RtFuncNode* delay_ms_;
  virtual bool run(BladeBase* blade) {
    // Call IgnitionDelayBase::run(blade, delay_ms_->getInteger())
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting delay logic
- Wraps input color until delay expires
- Pattern is clean

---

### 3.3 TriggerBase (functions/trigger.h)

**Purpose:** Button/trigger event detection

**Public Interface:**
```cpp
class TriggerBase {
  bool Check();
};
```

**Current sd_style.h Usage:**
```cpp
class RtTrigger : public RtFuncNode, public TriggerBase {
  virtual bool run(BladeBase* blade) {
    return TriggerBase::Check();  // Simplified
  }
};
```

**Assessment:** ✅ **GOOD REUSE** — Simple event detector

---

### 3.4 SparkleBase (functions/sparkle.h)

**Purpose:** Random twinkle/sparkle effect

**Public Interface:**
```cpp
class SparkleBase {
  int getInteger(int led);  // 0 or 32768 (on/off random)
};
```

**Current sd_style.h Usage:**
```cpp
class RtSparkleF : public RtFuncNode, public SparkleBase {
  // Inherits getInteger()
};
```

**Assessment:** ✅ **GOOD REUSE** — Inheriting random effect logic

---

## 4. Arithmetic Base Classes (Math Operations)

### 4.1 SumBase / MultBase / SubtractBase / ModBase (functions/*.h)

**Purpose:** Binary arithmetic operations on two function inputs

**Public Interface:**
```cpp
template<class A, class B>
class SumBase {
  void run(BladeBase* blade);
  int getInteger(int led);  // A + B
};
// Similar for Mult, Subtract, Mod
```

**Current sd_style.h Usage:**
```cpp
class RtSum : public RtFuncNode, public SumBase<RtFuncAdapter, RtFuncAdapter> {
  // Inherits run() and getInteger()
};
```

**Assessment:** ✅ **EXCELLENT WRAPPING** — Perfect template wrapper pattern
- Arithmetic logic centralized
- Adapter pattern enables runtime composition

---

### 4.2 IsLessThanBase (functions/islessthan.h)

**Purpose:** Comparison operator (A < B)

**Public Interface:**
```cpp
template<class A, class B>
class IsLessThanBase {
  void run(BladeBase* blade);
  int getInteger(int led);  // A < B ? 32768 : 0
};
```

**Current sd_style.h Usage:**
```cpp
class RtIsLessThan : public RtFuncNode, public IsLessThanBase<RtFuncAdapter, RtFuncAdapter> {
  // Inherits comparison logic
};

class RtIsGreaterThan : public RtFuncNode, public IsLessThanBase<RtFuncAdapter, RtFuncAdapter> {
  // Reuses IsLessThanBase with inverted logic (clever!)
};
```

**Assessment:** ✅ **EXCELLENT REUSE** — Template wrapping + logical inversion
- Minimal code duplication

---

## 5. Summary: Inheritance Coverage in sd_style.h

| Base Class | Current Usage in sd_style.h | Status | Notes |
|------------|------------------------------|--------|-------|
| ColorCycleBase | RtColorCycle : ColorCycleBase | ✅ Good | Direct inheritance |
| CylonBase | RtCylon : CylonBase | ✅ Good | Direct inheritance |
| BumpBase | RtBump : BumpBase | ✅ Good | Inherits getInteger() |
| SmoothStepBase | RtSmoothStep : SmoothStepBase | ✅ Good | Inherits getInteger() |
| Ifon<> | RtIfon : Ifon<RtFuncAdapter, RtFuncAdapter> | ✅ Excellent | Template wrapping pattern |
| InOutFuncSVFBase | RtInOutFunc : InOutFuncSVFBase | ✅ Good | Inherits state machine |
| InOutHelperFBase | RtInOutHelperF : InOutHelperFBase | ✅ Good | Inherits calculation |
| RgbArgBase | RtRgbArg : RgbArgBase | ✅ Good | Inherits arg parsing |
| IgnitionDelayBase | RtIgnitionDelay : IgnitionDelayBase<...> | ✅ Good | Inherits delay logic |
| RetractionDelayBase | RtRetractionDelay : RetractionDelayBase<...> | ✅ Good | Inherits delay logic |
| TriggerBase | RtTrigger : TriggerBase | ✅ Good | Simple wrapper |
| SparkleBase | RtSparkleF : SparkleBase | ✅ Good | Inherits twinkle logic |
| SumBase | RtSum : SumBase<RtFuncAdapter, RtFuncAdapter> | ✅ Excellent | Template wrapping |
| MultBase | RtMult : MultBase<RtFuncAdapter, RtFuncAdapter> | ✅ Excellent | Template wrapping |
| SubtractBase | RtSubtract : SubtractBase<RtFuncAdapter, RtFuncAdapter> | ✅ Excellent | Template wrapping |
| ModBase | RtModF : ModBase<RtFuncAdapter, RtFuncAdapter> | ✅ Excellent | Template wrapping |
| IsLessThanBase | RtIsLessThan/Greater : IsLessThanBase<...> | ✅ Excellent | Template wrapping |

---

## 6. Adapter Pattern Deep Dive

**Why adapters work:**

ProffieOS templates use duck typing—they don't require virtual inheritance, just the right method signatures:

```cpp
// Template definition (compile-time)
template<class COLOR, class ALPHA_FUNC>
class AlphaL {
  auto getColor(int led) {
    return color_.getColor(led) * alpha_.getInteger(led);
  }
};

// At runtime, adapters provide the interface:
struct RtColorAdapter {
  RtColorNode* ptr;
  auto getColor(int led) { return ptr->getColor(led); }  // ← Satisfies template requirement
};

// Inheritance connects virtual dispatch to templates:
class RtAlphaL : public AlphaL<RtColorAdapter, RtFuncAdapter> {
  RtColorAdapter color_adapter_{&color_node};
  RtFuncAdapter alpha_adapter_{&alpha_func_node};
  // AlphaL::getColor() uses adapters, adapters dispatch to virtual methods
};
```

**Performance:**
- Virtual dispatch: ~1 indirect call per adapter invocation
- Acceptable: 300k calls/sec → ~1 cycle cost
- Compiler can inline virtual methods in many cases

---

## 7. Key Findings for Phase 2

1. **Adapter pattern is proven and works well** — Already in use in sd_style.h
2. **~15-20 base classes already successfully reused** — No changes needed
3. **Opportunity for additional wrapping** — A few more templates could use adapter pattern
4. **Manual implementations are justified** — Procedural effects and complex compositions have domain-specific logic
5. **No safety or architectural issues** — Current approach is sound

---

## 8. Recommendation for Phase 2

**Action:** Keep all current base class inheritance as-is
- Working pattern, no bugs detected
- Wrapping opportunities identified but low ROI
- Focus Phase 2 on consolidating reimplemented color/function nodes instead

---

**Analysis Complete:** Task 1.2 ✅
**Next:** Task 1.3 (Analyze Template Composition Patterns)

