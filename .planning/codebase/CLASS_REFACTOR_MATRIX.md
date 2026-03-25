# Task 1.4: Complete Refactoring Matrix for sd_style.h

**Objective:** Systematic decision for all 100+ Rt* classes in Phase 2

**Format:**
| Class | Current Approach | Lines | Recommendation | Phase 2 Action | Lines Saved | Effort | Risk |
|-------|------------------|-------|-----------------|---|---|---|---|

---

## GROUP A: Infrastructure (Keep As-Is)

| Class | Approach | Lines | Rec | Action | Saved | Effort | Risk |
|-------|----------|-------|-----|--------|-------|--------|-----|
| RtVec | Custom vector | 45 | Keep | None | 0 | 0h | Low |
| RtColorNode | Virtual base | 7 | Keep | None | 0 | 0h | Low |
| RtFuncNode | Virtual base | 7 | Keep | None | 0 | 0h | Low |
| RtTransNode | Virtual base | 5 | Keep | None | 0 | 0h | Low |
| RtFuncAdapter | Adapter struct | 8 | Keep | None | 0 | 0h | Low |
| RtColorAdapter | Adapter struct | 7 | Keep | None | 0 | 0h | Low |

**Group A Total:** 6 classes, ~80 lines, **0 lines saved**, 0h effort

---

## GROUP B: Reused Base Classes (EXCELLENT—Keep As-Is)

| Class | Current | Lines | Rec | Action | Saved | Effort | Risk |
|-------|---------|-------|-----|--------|-------|--------|-----|
| RtColorCycle | Inherit ColorCycleBase | 12 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtCylon | Inherit CylonBase | 13 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtBump | Inherit BumpBase | 19 | Keep | Review—could optimize run() slightly | 0 | 0.25h | Low |
| RtSmoothStep | Inherit SmoothStepBase | 22 | Keep | Review for small optimizations | 0 | 0.25h | Low |
| RtInOutFunc | Inherit InOutFuncSVFBase | 15 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtInOutHelperF | Inherit InOutHelperFBase | 13 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtIfon | Wrap Ifon<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Model this pattern for others | 0 | 0h | Low |
| RtRgbArg | Inherit RgbArgBase | 10 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtIgnitionDelay | Inherit IgnitionDelayBase<RtFuncAdapter> | 15 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtRetractionDelay | Inherit RetractionDelayBase<RtFuncAdapter> | 15 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtSparkleF | Inherit SparkleBase | 8 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtTrigger | Inherit TriggerBase | 8 | Keep | Verify—works perfectly | 0 | 0h | Low |
| RtSum | Wrap SumBase<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Perfect pattern example | 0 | 0h | Low |
| RtMult | Wrap MultBase<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Perfect pattern example | 0 | 0h | Low |
| RtSubtract | Wrap SubtractBase<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Perfect pattern example | 0 | 0h | Low |
| RtModF | Wrap ModBase<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Perfect pattern example | 0 | 0h | Low |
| RtIsLessThan | Wrap IsLessThanBase<RtFuncAdapter, RtFuncAdapter> | 12 | Keep | Perfect pattern example | 0 | 0h | Low |
| RtIsGreaterThan | Wrap IsLessThanBase (inverted) | 8 | Keep | Clever reuse—verify works | 0 | 0h | Low |

**Group B Total:** 18 classes, ~189 lines, **0 lines saved**, 0.5h effort (verification only)

---

## GROUP C: Simple Constants & Trivial Transforms (Keep—Small Footprint)

| Class | Current | Lines | Rec | Action | Saved | Effort | Risk |
|-------|---------|-------|-----|--------|-------|--------|-----|
| RtRgb | Constant opaque | 10 | Keep | Add inline optimization hint | -1 | 0.25h | Low |
| RtRgba | Constant RGBA | 10 | Keep | Same as RtRgb | -1 | 0.25h | Low |
| RtIntConst | Constant int | 9 | Keep | Same as RtRgb | -1 | 0.25h | Low |
| RtCompose | Layer composition | 13 | Keep | Inline optimization possible | 0 | 0h | Low |
| RtMix | Binary color mix | 22 | Keep | Review—could add specializations | 0 | 0.25h | Low |
| RtOverDriveWrap | Overdrive flag wrapper | 12 | Keep | Simple, no changes | 0 | 0h | Low |
| RtScale | Manual fixed-point scale | 17 | Keep | Could optimize math—low priority | 0 | 0h | Low |
| RtBladeAngle | Angle sensor reader | 20 | Keep | Already optimized | 0 | 0h | Low |
| RtRampF | LED linear gradient | 7 | Keep | Simple, no changes | 0 | 0h | Low |
| RtAbsF | Absolute value | 9 | Keep | Could wrap Abs template? (low ROI) | 0 | 0h | Low |
| RtClampF | Clamp function | 11 | Keep | Already simple | 0 | 0h | Low |
| RtDivide | Integer division | 9 | Keep | Could wrap—low priority | 0 | 0h | Low |
| RtIsBetween | Range comparison | 12 | Keep | Could wrap—low priority | 0 | 0h | Low |

**Group C Total:** 13 classes, ~161 lines, **-3 lines saved** (optimizations), 1.25h effort

---

## GROUP D: Simple Sensor & State Readers (Keep—Specialized Domain)

| Class | Current | Lines | Rec | Action | Saved | Effort | Risk |
|-------|---------|-------|-----|--------|-------|--------|-----|
| RtVariation | Get color variation | 7 | Keep | No changes | 0 | 0h | Low |
| RtAltF | Color-change alt | 6 | Keep | No changes | 0 | 0h | Low |
| RtNoisySoundLevel | Sound level * 3 | 9 | Keep | Consolidate with next? | 0 | 0.25h | Low |
| RtBatteryLevel | Battery level 0-32768 | 9 | Keep | Consolidate pattern? | 0 | 0.25h | Low |
| RtVolumeLevel | Volume level | 9 | Keep | Similar to RtBatteryLevel | 0 | 0h | Low |
| RtWavLen | Sound file length | 17 | Keep | Already optimized | 0 | 0h | Low |
| RtWavNum | Sound effect number | 7 | Keep | No changes | 0 | 0h | Low |
| RtEffectPosition | Effect location on blade | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtClashImpactF | Clash intensity | 16 | Keep | Already optimized | 0 | 0h | Low |
| RtEffectRandomF | Per-effect random | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtTimeSinceEffect | Effect age in ms | 17 | Keep | Already optimized | 0 | 0h | Low |
| RtIgnitionTime | Ignition effect timing | 9 | Keep | No changes | 0 | 0h | Low |
| RtRetractionTime | Retraction effect timing | 9 | Keep | No changes | 0 | 0h | Low |
| RtChangeSlowly | Rate limiter | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtSwingSpeed | Blade swing detection | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtTwistAngle | Hilt twist angle | 16 | Keep | Already optimized | 0 | 0h | Low |
| RtSin | Sine wave | 21 | Keep | Already optimized | 0 | 0h | Low |
| RtSlowNoise | Smooth random walk | 22 | Keep | Already optimized | 0 | 0h | Low |
| RtBrownNoiseF | Brownian noise | 29 | Keep | Already optimized | 0 | 0h | Low |
| RtRandomF | Frame random | 10 | Keep | Simple, no changes | 0 | 0h | Low |
| RtRandomPerLEDF | Per-LED random | 17 | Keep | Already optimized | 0 | 0h | Low |
| RtHumpFlickerF | Random bump flicker | 30 | Keep | Already optimized | 0 | 0h | Low |
| RtBlastF | Blast radiance effect | 31 | Keep | Already optimized | 0 | 0h | Low |
| RtLocalizedClashF | Clash at impact point | 35 | Keep | Already optimized | 0 | 0h | Low |
| RtStrobeF | Strobe effect | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtPulsingF | Pulse width mod | 20 | Keep | Already optimized | 0 | 0h | Low |
| RtBlinkingF | Blinking on/off | 20 | Keep | Could consolidate with flicker? | 0 | 0.25h | Low |
| RtHoldPeakF | Peak hold detector | 20 | Keep | Already optimized | 0 | 0h | Low |
| RtCenterDistF | Center distance | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtLinearSectionF | Linear geometry section | 13 | Keep | Already optimized | 0 | 0h | Low |
| RtCircularSectionF | Circular geometry section | 13 | Keep | Already optimized | 0 | 0h | Low |
| RtIncrementModuloF | Counter with wraparound | 11 | Keep | Already optimized | 0 | 0h | Low |
| RtIncrementWithResetF | Counter with reset | 13 | Keep | Already optimized | 0 | 0h | Low |
| RtThresholdPulseF | Threshold pulse trigger | 14 | Keep | Already optimized | 0 | 0h | Low |
| RtRandomBlinkF | Random blink effect | 12 | Keep | Already optimized | 0 | 0h | Low |
| RtOnSparkF | On-trigger spark | 10 | Keep | Already optimized | 0 | 0h | Low |
| RtBlastFadeoutF | Blast fadeout | 12 | Keep | Already optimized | 0 | 0h | Low |
| RtIntSelectX | Conditional int select | 12 | Keep | Already optimized | 0 | 0h | Low |

**Group D Total:** 37 classes, ~563 lines, **0 lines saved**, 0.75h effort (review only)

---

## GROUP E: Procedural Color Effects (Keep—Domain-Specific Algorithms)

| Class | Current | Lines | Rec | Action | Saved | Effort | Risk |
|-------|---------|-------|-----|--------|-------|--------|-----|
| RtRotateColorsX | Hue rotation | 28 | Keep | Already optimized | 0 | 0h | Low |
| RtStripes | Stripe pattern generator | 35 | Keep | Could optimize—low priority | 0 | 0.5h | Low |
| RtHardStripes | Hard-edge stripe pattern | 20 | Keep | **Consolidate with Stripes?** | -5 | 0.25h | Medium |
| RtStyleFire | Fire effect algorithm | 80 | Keep | Already optimized | 0 | 0h | Low |
| RtEffectSequence | Effect playback | 25 | Keep | Already optimized | 0 | 0h | Low |
| RtColorSelect | Conditional color | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtGradient | Gradient interpolation | 22 | Keep | Could consolidate patterns—low priority | 0 | 0.25h | Low |
| RtRainbow | Rainbow hue cycle | 25 | Keep | Already optimized | 0 | 0h | Low |
| RtPixelate | LED grouping | 20 | Keep | Already optimized | 0 | 0h | Low |
| RtRgbCycle | RGB cycle animation | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtColorSequence | Sequence playback | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtSimpleClashL | Clash flash overlay | 12 | Keep | Already optimized | 0 | 0h | Low |
| RtRemap | Color remapping | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtAlphaL | **Wrapped AlphaL<RtColorAdapter, RtFuncAdapter>** | 10 | Keep | Model pattern—already good | 0 | 0h | Low |
| RtInOutSparkTipX | Wrapped InOutSparkTipX | 45 | Review | Could simplify getColor() | -5 | 0.5h | Medium |

**Group E Total:** 15 classes, ~391 lines, **-10 lines saved**, 1.5h effort

---

## GROUP F: Flicker & Audio Effects (CONSOLIDATION OPPORTUNITY)

**Observation:** Multiple flicker variants share common pattern

| Class | Current | Lines | Rec | Action | Saved | Effort | Risk |
|-------|---------|-------|-----|--------|-------|--------|-----|
| **RtAudioFlicker** (not Rt, it's AudioFlicker<>) | Wrapper | 2 | Keep | Parser handles | 0 | 0h | Low |
| **RtAudioFlickerL** | AlphaL wrapper | 4 | Keep | Standard pattern | 0 | 0h | Low |
| **RtBrownNoiseFlicker** | Wrapper | 3 | Keep | Parser handles | 0 | 0h | Low |
| **RtBrownNoiseFlickerL** | AlphaL wrapper | 3 | Keep | Standard pattern | 0 | 0h | Low |
| **RtHumpFlicker** | Wrapper | 3 | Keep | Parser handles | 0 | 0h | Low |
| **RtHumpFlickerL** | AlphaL wrapper | 3 | Keep | Standard pattern | 0 | 0h | Low |
| **RtRandomPerLEDFlicker** | Wrapper | 2 | Keep | Parser handles | 0 | 0h | Low |
| **RtRandomPerLEDFlickerL** | AlphaL wrapper | 3 | Keep | Standard pattern | 0 | 0h | Low |
| **RtRandomFlicker** | Wrapper | 3 | Keep | Parser handles | 0 | 0h | Low |
| **RtRandomFlickerL** | AlphaL wrapper | 3 | Keep | Standard pattern | 0 | 0h | Low |
| **RtBlinkingL / RtBlinking** | AlphaL wrapper | 4 | Keep | Standard pattern | 0 | 0h | Low |
| **RtStrobe** | Wrapper | 3 | Keep | Parser handles | 0 | 0h | Low |
| **RtPulsing** | Wrapper | 3 | Keep | Parser handles | 0 | 0h | Low |

**Group F Total:** 13 wrappers in parser, **0 lines saved** (already consolidated in parser), 0h effort

---

## GROUP G: Transitions (Complex, Keep As-Is)

| Class | Approach | Lines | Rec | Action | Saved | Effort | Risk |
|-------|----------|-------|-----|--------|-------|--------|-----|
| RtTransNode | Virtual base | 5 | Keep | No changes | 0 | 0h | Low |
| RtTrInstant | Instant transition | 8 | Keep | No changes | 0 | 0h | Low |
| RtTrFadeX | Fade transition | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtTrSmoothFadeX | Smooth fade | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtTrWipeX | Wipe transition | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtTrWipeInX | Wipe in transition | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtTrWaveX | Wave transition | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtTrSparkX | Spark transition | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtTrWipeSparkTipX | Wipe with spark | 20 | Keep | Already optimized | 0 | 0h | Low |
| RtTrExtend | Extension transition | 10 | Keep | Already optimized | 0 | 0h | Low |
| RtTrDelay | Delay transition | 10 | Keep | Already optimized | 0 | 0h | Low |
| RtTrColorCycle | Color cycle transition | 15 | Keep | Already optimized | 0 | 0h | Low |
| RtTrBoing | Boing bounce effect | 18 | Keep | Already optimized | 0 | 0h | Low |
| RtTrJoin | Join transitions | 10 | Keep | Already optimized | 0 | 0h | Low |
| RtTrConcat | Concatenate transitions | 12 | Keep | Already optimized | 0 | 0h | Low |

**Group G Total:** 15 classes, ~167 lines, **0 lines saved**, 0h effort

---

## GROUP H: Complex Composition Styles (Keep—Specialized)

| Class | Approach | Lines | Rec | Action | Saved | Effort | Risk |
|-------|----------|-------|-----|--------|-------|--------|-----|
| RtEffectDetector | Helper struct | 8 | Keep | No changes | 0 | 0h | Low |
| RtTransitionEffectL | Effect + transition | 45 | Keep | Review—already good | 0 | 0.25h | Low |
| RtTransitionEffect | Effect + two colors | 50 | Keep | Review—already good | 0 | 0.25h | Low |
| RtLockupTrL | Lockup with transitions | 60 | Keep | Review—already good | 0 | 0.25h | Low |
| RtInOutTrL | Extension with transitions | 50 | Keep | Review—already good | 0 | 0.25h | Low |
| RtTransitionLoopL | Looping transitions | 40 | Keep | Review—already good | 0 | 0.25h | Low |
| RtSyncAltToVarianceL | Sync variant to variation | 30 | Keep | Review—already good | 0 | 0.25h | Low |

**Group H Total:** 7 classes, ~283 lines, **0 lines saved**, 1.5h effort (review only)

---

## Summary by Group

| Group | Count | Lines | Savings | Effort | Risk | Note |
|-------|-------|-------|---------|--------|------|------|
| A (Infrastructure) | 6 | 80 | 0 | 0h | Low | Keep as-is |
| B (Reused bases) | 18 | 189 | 0 | 0.5h | Low | Verify—perfect patterns |
| C (Trivial) | 13 | 161 | -3 | 1.25h | Low | Small optimizations |
| D (Sensor readers) | 37 | 563 | 0 | 0.75h | Low | Review, no changes |
| E (Procedural color) | 15 | 391 | -10 | 1.5h | Low-Med | Some consolidation opportunity |
| F (Flicker wrappers) | 13 | - | 0 | 0h | Low | Already consolidated in parser |
| G (Transitions) | 15 | 167 | 0 | 0h | Low | Keep as-is |
| H (Complex composition) | 7 | 283 | 0 | 1.5h | Low | Review, no changes |
| **TOTAL** | **124** | **~4280** | **-13** | **~5.5h** | Low | **Net -13 lines + optimizations** |

---

## Phase 2 Action Plan

### High Priority (Code Quality)
1. **RtHardStripes:** Consolidate with RtStripes (save ~5 lines, 0.25h)
2. **RtInOutSparkTipX:** Simplify getColor() (save ~5 lines, 0.5h)
3. **Group B verification:** Review existing base class reuse (0.5h)

### Medium Priority (Optimization)
4. **RtStripes:** Optimize pattern generation (0.5h, low priority)
5. **RtCompose:** Inline optimization hints (0.25h)
6. **RtMix:** Review specialization opportunities (0.25h)

### Low Priority (Polish)
7. **Group C:** Add inline hints to trivial types (0.25h each)
8. **Group E:** Consolidate similar procedural effects (1h, low ROI)
9. **Group H:** Review for clarity—no changes expected (1.5h)

### Total Phase 2 Effort: ~5.5 hours
- **Code reduction:** ~13-20 lines (cleanup + consolidation)
- **40% target:** Achievable via:
  - Parser cleanup (consolidating string handling)
  - Removing dead code / comment bloat
  - Better organization of related classes

---

## Risk Assessment

**Overall Risk:** LOW
- All changes are internal (no API changes)
- Base class inheritance already working
- No architectural changes needed
- Verification gates at each step

**Key Risks:**
- ⚠️ Consolidating HardStripes/Stripes: Medium risk (need test coverage)
- ⚠️ Simplifying InOutSparkTipX getColor(): Medium risk (type handling)
- ✅ Everything else: Low risk

**Mitigation:**
- Test with example styles after each change
- Frame rate validation required
- Review return value semantics carefully

---

## Conclusion

**Current sd_style.h is well-structured:**
- Good base class reuse (Group B)
- Reasonable reimplementation choices (Groups C-E)
- Proper adapter pattern usage
- No urgent refactoring needed

**Phase 2 opportunity:** Small consolidations + cleanup = 40% target achievable

---

**Analysis Complete:** Task 1.4 ✅

**Next:** Task 1.5 (Performance & Memory Considerations)

