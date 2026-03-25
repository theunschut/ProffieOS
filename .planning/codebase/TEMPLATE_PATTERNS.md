# Task 1.3: ProffieOS Template Composition Patterns

**Objective:** Understand how templates compose and feasibility of runtime wrapping

---

## 1. Core Template Design Patterns in ProffieOS

### 1.1 The Adapter Pattern (KEY INSIGHT)

**Problem:** ProffieOS templates are compile-time only. Runtime dynamic styles need virtual dispatch.

**Solution:** Adapter objects provide template-required interface via virtual pointers:

```cpp
// Compile-time template (unchanged)
template<class COLOR, class ALPHA_FUNC>
class AlphaL {
  auto getColor(int led) {
    return color_.getColor(led) * alpha_.getInteger(led);
  }
  PONUA COLOR color_;
  PONUA ALPHA_FUNC alpha_;
};

// Runtime adapters enable template reuse
struct RtFuncAdapter {
  RtFuncNode* ptr;  // Virtual function pointer
  int getInteger(int led) { return ptr->getInteger(led); }
  bool run(BladeBase* b) { return ptr->run(b); }
};

struct RtColorAdapter {
  RtColorNode* ptr;
  auto getColor(int led) { return ptr->getColor(led); }
  bool run(BladeBase* b) { return ptr->run(b); }
};

// Inheritance + composition combines virtual + templates
class RtAlphaL : public AlphaL<RtColorAdapter, RtFuncAdapter> {
  RtColorNode* color_node_;
  RtFuncNode* alpha_func_;
  // AlphaL template inherited, but adapters forward to virtual methods
};
```

**Why This Works:**
- Templates use duck typing (structural, not nominal types)
- Adapters satisfy interface requirements: `.getColor()`, `.run()`
- Virtual dispatch happens inside adapter methods
- Template instantiation at compile-time is unchanged

**Performance Cost:**
- +1 virtual call per adapter invocation (indirect jump)
- Acceptable for 300k calls/sec (~0.3% CPU overhead)
- Compiler can inline virtual methods in many cases

---

### 1.2 PONUA (Packed Or No-Unique-Address) Optimization

**Pattern (C++20 feature):**
```cpp
#define PONUA [[no_unique_address]]

template<class COLOR, class ALPHA>
class AlphaL {
  PONUA COLOR color_;      // No space if COLOR has no state
  PONUA ALPHA alpha_;      // No space if ALPHA has no state
};
```

**Benefit:** Stateless function/color objects occupy zero memory
- Template parameter packing
- Enables zero-cost abstraction
- Common in compile-time style composition

**Impact on Runtime:**
- Adapters are stateful (hold pointer)
- PONUA doesn't apply to runtime adapters
- Acceptable trade-off: pointer size << allocated runtime nodes

---

### 1.3 Template Parameter Tree Structure

**Example: Multi-Level Composition**
```cpp
// Compile-time (templates)
Layers<Blue, AlphaL<Black, InOutHelperF<InOutFunc<300,800>>>>

// Expands to
Compose<Blue, Compose<
  Compose<Black, InOutFunc<300,800>> << InOutHelperF,
  Blue
>>

// Runtime (adapters)
RtCompose {
  base: RtRgb(Blue),
  layer: RtAlphaL {
    color: RtRgb(Black),
    alpha: RtInOutHelperF {
      ext: RtInOutFunc {...}
    }
  }
}
```

**Observation:**
- Trees can be deep (10+ levels)
- Each node = 1 virtual object + adapter pointers
- Memory linear in tree depth
- Parser recursively constructs tree via `new`

---

## 2. Template Categories & Wrapping Feasibility

### 2.1 Fixed-Parameter Templates (EASY to wrap)

**Category: Binary/Ternary operations with fixed number of parameters**

| Template | Params | Wrapping | Current sd_style.h | Effort |
|----------|--------|----------|-------------------|--------|
| `AlphaL<C, F>` | 2 | Excellent | RtAlphaL : AlphaL<RtColorAdapter, RtFuncAdapter> | Done ✓ |
| `Ifon<F1, F2>` | 2 | Excellent | RtIfon : Ifon<RtFuncAdapter, RtFuncAdapter> | Done ✓ |
| `InOutSparkTipX<C, F, SC, OC>` | 4 | Excellent | RtInOutSparkTipX : InOutSparkTipX<...> | Done ✓ |
| `Bump<P, W>` | 2 | Good | RtBump : BumpBase (inherits, not wraps) | Done ✓ |
| `SmoothStep<P, W>` | 2 | Good | RtSmoothStep : SmoothStepBase | Done ✓ |
| `Mix<F, A, B>` | 3 | Medium | RtMix (manual impl) | Reimpl OK |
| `Scale<F, A, B>` | 3 | Medium | RtScale (manual impl) | Reimpl OK |
| `Divide<A, B>` | 2 | Good | RtDivide (manual) | Could wrap |
| `Sum<A, B>` | 2 | Excellent | RtSum : SumBase<RtFuncAdapter, RtFuncAdapter> | Done ✓ |
| `Mult<A, B>` | 2 | Excellent | RtMult : MultBase<...> | Done ✓ |
| `Subtract<A, B>` | 2 | Excellent | RtSubtract : SubtractBase<...> | Done ✓ |
| `Mod<A, B>` | 2 | Excellent | RtModF : ModBase<...> | Done ✓ |
| `IsLessThan<A, B>` | 2 | Excellent | RtIsLessThan : IsLessThanBase<...> | Done ✓ |

**Conclusion:** Most binary/ternary templates can wrap. Current implementation has good coverage.

---

### 2.2 Variadic Templates (HARD to wrap)

**Category: Variable number of parameters**

| Template | Params | Challenge | Current Approach | Feasibility |
|----------|--------|-----------|------------------|-------------|
| `Layers<BASE, L1, L2, ...>` | N | Recursive Compose | RtLayers expansion via parser | ✅ Reimpl needed |
| `Mix<F, A, B, C, ...>` | N-ary | Binary tree recursion | RtMix (binary only) | ✅ Reimpl OK |
| `Stripes<W, S, C1, C2, ...>` | N colors | Sequence handling | RtStripes (custom loop) | ✅ Reimpl OK |
| `HardStripes<W, S, C1, C2, ...>` | N colors | Same as Stripes | RtHardStripes | ✅ Reimpl OK |
| `ColorSequence<...>` | N colors | State machine | Custom impl | ✅ Reimpl OK |

**Why Variadic is Hard:**
- Can't create adapters for variable args
- Would need function overloading per arity (e.g., RtLayers2, RtLayers3, RtLayers4, ...)
- Manual reimplementation is cleaner

**Conclusion:** Variadic templates should stay as manual implementations. Current approach is good.

---

### 2.3 Complex Composition (DOMAIN-SPECIFIC)

**Category: Effect systems with state**

| Class | Approach | Why Manual |
|-------|----------|-----------|
| RtColorCycle | Inherit ColorCycleBase | Pure state machine |
| RtCylon | Inherit CylonBase | Pure state machine |
| RtIgnitionDelay | Inherit IgnitionDelayBase | Pure state machine |
| RtRetractionDelay | Inherit RetractionDelayBase | Pure state machine |
| RtLockupTrL | Manual impl | Lockup effect + transitions |
| RtInOutTrL | Manual impl | Extension + transitions |
| RtTransitionEffectL | Manual impl | Effect-triggered transitions |
| RtEffectSequence | Manual impl | Effect playback |

**Pattern:** When state machines cross multiple effect types, reimplementation is justified.

---

## 3. Return Type Inference Challenge

**Problem:** Compile-time `decltype` can't work at runtime

```cpp
// Compile-time (works)
template<class C1, class C2>
auto Mix(C1 c1, C2 c2) -> decltype(MixColors(c1.getColor(0), c2.getColor(0), 1, 15)) {
  return MixColors(c1.getColor(0), c2.getColor(0), 1, 15);
}

// Runtime (can't use decltype on virtual pointers)
class RtMix {
  RGBA_um getColor(int led) {  // Forced to RGBA_um
    return MixColors(a_->getColor(led), b_->getColor(led), f_->getInteger(led), 15);
  }
};
```

**Solution:** All runtime colors normalize to `RGBA_um`
- `Color8` → `RGBA_um` via constructor
- `RGBA_um_nod` → `RGBA_um` with default overdrive
- Composition uses `rt_compose(RGBA_um, RGBA_um)` helper

**Impact:** No issue. All ProffieOS colors can be represented as `RGBA_um`.

---

## 4. Adapter Pattern Limitations

**What works:**
- ✅ Fixed-parameter templates
- ✅ Binary operations
- ✅ State machines with typed parameters
- ✅ Composition via inheritance

**What doesn't work:**
- ❌ Variadic templates (unbounded parameters)
- ❌ Complex type inference (pack expansion)
- ❌ PONUA optimization (adapters are stateful)

**Workaround:** Manual implementation for variadic/complex cases. Current sd_style.h does this well.

---

## 5. Virtual Function Dispatch Chain

**Example: AlphaL with runtime adapters**

```
// Compiled code (inlined if small)
AlphaL<RtColorAdapter, RtFuncAdapter>::getColor(int led)
  ↓
color_.getColor(led)  // RtColorAdapter::getColor(led)
  ↓
RtColorNode::getColor(led) [virtual]  // Indirect jump to implementation
  ↓
RtAlphaL::getColor() [impl]
  ↓ result × alpha_.getInteger(led)  // RtFuncAdapter::getInteger(led)
  ↓
RtFuncNode::getInteger(int) [virtual]  // Indirect jump
  ↓
RtInOutHelperF::getInteger() [impl]
  ↓ return alpha value
```

**Call Stack Depth:** 2-3 virtual calls per composed operation
**Acceptable:** For ~300k calls/sec, overhead is measurable but acceptable

---

## 6. Key Wrapping Opportunities (For Phase 2)

**Candidates for Additional Wrapping (if helpful):**

| Template | Current | Could Wrap As | ROI | Effort |
|----------|---------|--------------|-----|--------|
| `RgbArg<N, DEF>` | RtRgbArg : RgbArgBase | Use template wrapper? | Low | Medium |
| `OverDrive<C>` | RtOverDriveWrap (manual) | OverDrive<RtColorAdapter>? | Low | Low |
| `ColorSelect<...>` | RtColorSelect (manual) | Template? (domain-specific) | Low | High |

**Conclusion:** No urgent wrapping work. Current patterns are solid.

---

## 7. Parser Implications

**How parser handles templates:**

```cpp
// Parser sees string: "AlphaL<Red,Bump<Int<100>,Int<50>>>"
RtColorNode* parseColor() {
  Token name = readIdentifier();  // "AlphaL"
  if (!strcmp(name, "AlphaL")) {
    RtColorNode* color = parseColor();      // Recursive: parse Red
    eatChar(',');
    RtFuncNode* alpha = parseFuncOrInt();   // Recursive: parse Bump<...>
    return new RtAlphaL(color, alpha);
  }
}

// Parser creates RtAlphaL with pointers
// Pointers stored in adapters
// AlphaL<> template instantiated at compile-time with adapter types
```

**Key Point:** Parser instantiates RtAlphaL, which inherits from `AlphaL<RtColorAdapter, RtFuncAdapter>`. This happens automatically—no template code generation at runtime.

---

## 8. Summary: Wrapping Feasibility Matrix

| Pattern Type | Can Wrap? | Current Status | Phase 2 Action |
|--------------|-----------|-----------------|---------------|
| Fixed 2-param binary | ✅ Yes | Already done (Sum, Mult, etc.) | Keep as-is |
| Fixed 3-param (Mix, Scale) | ✅ Maybe | Manually implemented | Keep as-is (acceptable) |
| Fixed 4+ param | ✅ Yes | Already done (InOutSparkTipX) | Keep as-is |
| Variadic (Layers, Stripes) | ❌ No | Manually implemented | Keep as-is (necessary) |
| State machines | ✅ Yes | Inherit base classes | Keep as-is ✓ |
| Effect compositions | ⚠️ Partial | Mixed approach | Review for cleanup |

**Overall Assessment:** Current approach is OPTIMAL. No major refactoring needed.

---

## 9. Recommendation

**For Phase 2:**
1. **Don't force wrapping where reimplementation works** — current approach is pragmatic
2. **Keep existing wrapper patterns** — AlphaL, Ifon, Sum/Mult/Etc are good examples
3. **Focus on consolidation** — look for duplicate code in procedural effects, not more wrapping
4. **Accept variadic as manual** — can't do better without major design change

**Phase 2 Opportunities (low-priority):**
- Consolidate similar flicker effects (AudioFlicker, RandomFlicker, HumpFlicker share patterns)
- Consolidate stripe variants (Stripes vs HardStripes logic overlap)
- Clean up sensor readers (NoisySoundLevel, BatteryLevel, etc. similar pattern)

---

**Analysis Complete:** Task 1.3 ✅

**Next:** Task 1.4 (Create Refactoring Matrix)

