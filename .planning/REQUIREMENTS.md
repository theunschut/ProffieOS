# ProffieOS Style Parser: Requirements & UAT

## Functional Requirements

### FR1: Runtime Style Parsing
- **REQ 1.1**: Parser accepts ProffieOS style template syntax from plain text
  - Format: `ClassName<Arg1,Arg2,...>` with nested support
  - Examples: `Layers<Blue,AlphaL<Black,InOutHelperF<...>>>`
  - Whitespace tolerance (tabs, newlines, spaces)

- **REQ 1.2**: Parser supports all standard ProffieOS primitives currently in use
  - Color classes: `Black`, `Blue`, `White`, `Green`, `LemonChiffon`, `RotateColorsX`, `Rgb<R,G,B>`, `Rgb16<...>`
  - Style wrappers: `Layers`, `AlphaL`, `HumpFlicker`, `Stripes`, `LockupTrL`, `ResponsiveLightningBlockL`, `ResponsiveStabL`, `ResponsiveClashL`, etc.
  - Function wrappers: `InOutHelperF`, `InOutFunc`, `Bump`, `SmoothStep`, `Scale`, `BladeAngle`, `SwingSpeed`, `SlowNoise`, `BlastF`, etc.
  - Integer/numeric literals: `Int<N>`, `Variation`, constants

- **REQ 1.3**: Unknown tokens gracefully degrade
  - Log unknown token to serial output
  - Substitute sensible default (typically `Black` for styles, `Int<0>` for functions)
  - Do not crash or hang

- **REQ 1.4**: Return values correctly semantically
  - Style `getColor()` methods return correct color/alpha
  - Function `run()` methods return bool indicating blade can be powered down
  - No undefined/garbage return values

### FR2: Style Loading Sources
- **REQ 2.1**: Load from SD card file
  - API: `StyleFromSD("font/style1.style")`
  - File format: plain text, one style expression per file
  - Support both relative and absolute paths on SD

- **REQ 2.2**: Load from presets.ini (ProffieOS standard)
  - Integrate into existing ProffieOS preset loading flow
  - Replace "builtin" prefix with SD style references
  - Example: `preset.style = StyleFromSD("assassin.style")`

- **REQ 2.3**: Coexist with compiled styles (hybrid mode)
  - Users can mix compiled StylePtr<...> and StyleFromSD(...) in config
  - No conflicts or naming collisions
  - Fallback to compiled if SD file missing

### FR3: Code Reuse & Architecture
- **REQ 3.1**: Use template wrappers, not reimplementation
  - Wrap existing ProffieOS templates (ColorCycleBase, CylonBase, etc.)
  - Per-signature templates: one template per unique input type combination
  - Leverage MixColors, virtual functions, existing optimizations

- **REQ 3.2**: Minimize runtime adapter overhead
  - Runtime color/function nodes inherit from base classes
  - Dispatch to template implementations where possible
  - Avoid redundant parsing loops

- **REQ 3.3**: Memory efficiency
  - Parse once, store in minimal representation (RtVec already used)
  - No deep copies of entire template trees
  - Reuse allocator where possible

### FR4: Correctness & Robustness
- **REQ 4.1**: Fast ignition animation speed
  - Ignition timing within 10% of compiled equivalent
  - No pre-animation delays or stutters
  - Profile with "top" command if regression detected

- **REQ 4.2**: Correct color initialization
  - No pre-ignited colors on style load
  - Greyscale fonts initialize correctly
  - Color state resets between presets

- **REQ 4.3**: All documented effects/functions parsed
  - Audio flicker, lockup, clash, blast, stab effects working
  - Transitions and modulation functions complete
  - Missing functions logged, not silently broken

- **REQ 4.4**: Blade power control
  - run() return values determine when blade can power down
  - Blade doesn't stay powered when effect ends
  - Consistent with compiled style behavior

### FR5: Performance & Monitoring
- **REQ 5.1**: Frame rate within acceptable range
  - Target: within 5% of compiled equivalent
  - Monitor with "top" command during testing
  - Document baseline for typical setups (100-LED, 60fps)

- **REQ 5.2**: Memory usage profiling
  - Document RAM usage for typical styles
  - Compare with compiled equivalents
  - Identify pathological cases (deeply nested styles)

## Non-Functional Requirements

### NFR1: Code Quality
- **NFR 1.1**: Code reviews by profezzorn (optional but recommended)
- **NFR 1.2**: No undefined behavior (valgrind/ASAN clean on supported platforms)
- **NFR 1.3**: No memory leaks (RAII, no malloc without matching free)
- **NFR 1.4**: Consistent style (follow existing ProffieOS conventions)

### NFR2: Compatibility
- **NFR 2.1**: Supports ProffieOS master branch (as of 2026-03-25)
- **NFR 2.2**: Backward compatible with existing compiled styles
- **NFR 2.3**: No modifications to core ProffieOS API outside sd_style.h

### NFR3: Documentation
- **NFR 3.1**: README with basic usage (StyleFromSD API, file format)
- **NFR 3.2**: Example presets.ini snippet
- **NFR 3.3**: Known limitations documented
- **NFR 3.4**: Troubleshooting guide for common issues

## UAT Scenarios

### UAT1: Basic Style Parsing
**Given:** style file `test_simple.style` with `Layers<Blue,AlphaL<Black,InOutHelperF<InOutFunc<300,800>>>>`
**When:** Loaded via StyleFromSD("test_simple.style")
**Then:** Blade displays blue with fade-in/out animation at specified timings

**Pass Criteria:**
- [ ] Style renders without errors
- [ ] Color is blue (not black, not undefined)
- [ ] Fade timing matches 300ms in, 800ms out

### UAT2: Complex Style (assassin.style)
**Given:** Full assassin.style from /config/styles/
**When:** Loaded via StyleFromSD or preset
**Then:** All effects work: flicker, swing ripple, lockup, clash, blast, stab effects

**Pass Criteria:**
- [ ] No parsing errors
- [ ] Frame rate within 5% of compiled version
- [ ] All effects trigger correctly
- [ ] No pre-rendered issues or artifacts

### UAT3: Hybrid Mode
**Given:** Config with mix of compiled `StylePtr<...>` and `StyleFromSD(...)`
**When:** Loaded and switching between presets
**Then:** Both styles coexist, switching seamless, no conflicts

**Pass Criteria:**
- [ ] Compiled styles unchanged
- [ ] SD styles load without affecting compiled
- [ ] Switching presets smooth (no glitches)

### UAT4: Greyscale Font Colors
**Given:** greyscale font with pre-ignited and non-ignited styles
**When:** Loaded and activated
**Then:** Correct colors, no unexpected pre-ignition

**Pass Criteria:**
- [ ] Non-ignited styles start at black
- [ ] No color shown until ignition triggered
- [ ] Color matches reference compiled version

### UAT5: Ignition Timing
**Given:** Style with `InOutDelayX` or `IgnitionDelayBase` wrapper
**When:** Ignition triggered (sound + color)
**Then:** Animation speed within 10% of compiled equivalent

**Pass Criteria:**
- [ ] Ignition smoothness visually similar to compiled
- [ ] No stutter or delayed startup
- [ ] Profile shows frame rate stable during animation

### UAT6: presets.ini Integration
**Given:** presets.ini with entry `preset.style = StyleFromSD("myStyle.style")`
**When:** Preset selected in standard ProffieOS flow
**Then:** Style loads and renders correctly

**Pass Criteria:**
- [ ] Preset loads without code changes
- [ ] UI/preset selection works as expected
- [ ] Fallback if file missing (graceful)

### UAT7: Error Handling
**Given:** Style file with unknown/malformed tokens
**When:** Loaded
**Then:** Graceful degradation, logging, no crash

**Pass Criteria:**
- [ ] Unknown token logged to serial
- [ ] Blade still functional (renders something)
- [ ] No undefined behavior
- [ ] System remains responsive

### UAT8: Blade Power Control
**Given:** Style that should turn off blade (run() returns false)
**When:** Effect ends
**Then:** Blade power drops (not lingering)

**Pass Criteria:**
- [ ] Blade powers off when expected
- [ ] Matches compiled style behavior
- [ ] No stuck-on blade

## Acceptance Criteria (Definition of Done)

For this feature to be considered complete:

1. **Code Quality**
   - [ ] sd_style.h refactored to use template wrappers (< 50% of current size target)
   - [ ] No reimplemented style/function classes
   - [ ] Code reviewed for embedded best practices

2. **Functional**
   - [ ] All FR1-FR5 functional requirements met
   - [ ] UAT1-UAT8 pass without modifications
   - [ ] Known issues fixed: ignition, colors, effects, return values

3. **Performance**
   - [ ] Frame rate measured and within spec
   - [ ] Memory usage profiled and documented
   - [ ] No performance regression on compiled styles

4. **Integration**
   - [ ] feature/StyleFromSD branch ready for merge to master
   - [ ] Tested on actual hardware or realistic emulation
   - [ ] Hybrid mode validated

5. **Documentation**
   - [ ] README with API and examples
   - [ ] Known limitations documented
   - [ ] Example presets.ini snippet provided

---

**Version:** 1.0
**Last Updated:** 2026-03-25
**Status:** Active
