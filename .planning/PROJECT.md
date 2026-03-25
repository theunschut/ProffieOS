# ProffieOS Style Parser: SD Card Runtime Loading

**Project Vision:** Enable ProffieOS users to load blade styles from SD card at runtime, eliminating board flash memory limitations while maintaining performance and code reusability.

## Problem Statement

**Current State:**
- Blade styles are compiled into board firmware, consuming significant flash memory
- Users with full storage cannot add new styles without rebuilding firmware
- Previous PoC implementation (sd_style.h) achieved basic functionality but has issues:
  - Reimplemented full style classes instead of reusing templates
  - Performance regression on ignition animations
  - Pre-ignited color issues with greyscale fonts
  - Incomplete parsing of some effects
  - Incorrect run() return value handling breaks blade power control

**Constraints:**
- ProffieOS optimizes templates at compile-time for minimal overhead
- Virtual function calls incur performance cost (300k calls/sec on typical blade)
- Frame rate critical: 100-LED blade at 60fps with 50-style composition = 300k calls/sec
- Memory-constrained embedded systems (current: limited RAM/flash; next-gen: v2/v3 will have more)

## Goals

1. **Refactor for Code Reuse** ✓ Primary goal
   - Replace reimplemented classes with template wrappers
   - Wrap existing ProffieOS primitives instead of duplicating
   - Reduce maintainability burden long-term

2. **Fix Known Issues** ✓ Critical
   - Debug and resolve fast ignition regression
   - Fix pre-ignited color initialization
   - Complete effect/function parsing
   - Correct run() return value semantics

3. **Balanced Optimization** ✓ No single priority dominates
   - Memory savings: allow SD-based styles to free flash
   - Performance: monitor frame rate, keep within ~5% of compiled
   - Maintainability: use standard C++ templates, avoid custom implementations

4. **Flexible Deployment** ✓ Support both approaches
   - Integrate into presets.ini (ProffieOS standard)
   - Support standalone .style files for modularity
   - Maintain backward compatibility with compiled styles (hybrid mode)

## Scope

### In Scope
- Redesign sd_style.h runtime adapter layer using template wrappers
- Implement parser for all current ProffieOS style/function primitives
- Fix critical issues (ignition, colors, return values, effects)
- Support both presets.ini and .style file formats
- Maintain hybrid mode (compiled + SD-loaded styles coexist)
- Frame rate monitoring/validation
- Memory usage documentation

### Out of Scope (for this phase)
- New style/function primitives
- Streaming styles (only in-memory after load)
- Web UI or preset management tools
- Audio effects beyond current ProffieOS
- Full documentation (basic README ok)

## Stakeholders & Constraints

**Creator Input (profezzorn):**
- Don't reimplement classes—wrap existing templates
- Template approach: `Base` → template parameter per unique input signature
- Performance will be slightly slower (virtual function cost)
- Return values critical for blade power control
- Consider presets.ini integration (standard ProffieOS flow)

**Known Limitations:**
- Dynamic loading inherently slower than compiled (acceptable trade-off)
- Smaller allocations may use RAM less efficiently long-term
- Flash savings offset by parsing overhead
- Performance acceptable on v1 boards; critical on memory-constrained variants

## Success Criteria

- [ ] All style primitives parse correctly (full parity with assassin.style, calkestis.style, etc.)
- [ ] Frame rate within 5% of compiled equivalent on test blade
- [ ] No pre-ignited colors; ignition speed within 10% of compiled
- [ ] Blade power control (run() return values) works correctly
- [ ] Hybrid mode: compiled and SD styles coexist without issues
- [ ] Both presets.ini and .style file formats supported
- [ ] Code reviewed by profezzorn (optional but recommended)
- [ ] 0 undefined behavior / memory leaks

## Timeline & Phases

**Phase 1: Refactor (Code Quality)**
- Analyze existing template structure
- Design wrapper layer for reuse
- Replace reimplemented classes with templates
- Commit checkpoint

**Phase 2: Fix & Complete**
- Debug and fix ignition regression
- Fix color initialization
- Complete missing effects parsing
- Validate run() semantics
- Frame rate validation

**Phase 3: Integration & Testing**
- Support presets.ini loading
- Hybrid mode testing (compiled + SD)
- UAT with example styles (assassin, calkestis, etc.)
- Performance characterization

**Phase 4: Polish & Documentation**
- Code cleanup
- Basic README
- Example presets.ini snippet
- Optional: profezzorn review

## Key Decisions

1. **Template-First Approach**: Reuse over reimplementation (per profezzorn recommendation)
2. **Balanced Priorities**: No aggressive optimization trade-offs (v2/v3 will have more resources)
3. **Hybrid Compatibility**: Compiled styles stay, SD adds flexibility
4. **Format Support**: Support both presets.ini and .style files for deployment flexibility
5. **Frame Rate Monitoring**: Track performance vs compiled baseline

## Team & Resources

**You**: Backend developer, learning embedded/ProffieOS internals via code analysis
**profezzorn**: ProffieOS creator (optional review/guidance)
**Tools**: ProffieOS codebase, git branch feature/StyleFromSD, AI assistance

## Document History

- **2026-03-25**: Project initialization, user confirmed goals and priorities
