# ProffieOS Style Parser: Execution Roadmap

## Phase Breakdown

### Phase 1: Architecture Analysis & Design (Planning)
**Goal:** Understand existing code structure and design refactored template wrapper approach

**Key Tasks:**
- Analyze current sd_style.h implementation (4249 lines)
- Study ProffieOS template architecture (styles/, functions/)
- Identify which classes are reimplemented vs needed adapters
- Design wrapper strategy: base classes vs template wrappers
- Document reimplementation debt
- Create architecture decision record

**Deliverables:**
- Architecture analysis document
- Wrapper design specification
- List of templates to reuse vs adapt
- Codebase map (.planning/codebase/)

**Success Criteria:**
- Clear understanding of how to wrap existing templates
- Design approved by team
- No uncertainty about feasibility

**Effort:** 1-2 hours research + analysis

---

### Phase 2: Refactor Runtime Adapter Layer (Implementation)
**Goal:** Replace reimplemented classes with template wrappers, reduce sd_style.h size and complexity

**Key Tasks:**
- Refactor RtColorNode adapter to use base class inheritance
- Replace reimplemented color classes with template wrappers
  - RtAlphaL → wrap AlphaL<Base, ...>
  - RtHumpFlicker → wrap HumpFlicker<Base, ...>
  - RtStripes → wrap Stripes<Base, ...>
  - etc.
- Replace reimplemented function classes with templates
  - RtBump → template wrapper
  - RtSmoothStep → template wrapper
  - etc.
- Update parser to instantiate wrapped templates instead of custom classes
- Ensure run() return values propagate correctly
- No behavior change, only refactoring

**Deliverables:**
- Refactored sd_style.h (target: < 2500 lines)
- Compilation passes
- All parsing tests still work
- Git commit with clear message

**Success Criteria:**
- Code size reduced by ~40%
- Zero reimplemented style/function classes
- Build clean with no warnings
- Frame rate unchanged (no performance regression)

**Effort:** 4-6 hours (depends on coupling complexity)

---

### Phase 3: Debug & Fix Known Issues (Debugging)
**Goal:** Resolve ignition timing, color initialization, missing effects, return values

**Key Tasks:**
1. **Ignition Regression Investigation**
   - Profile ignition animation timing (compiled vs SD)
   - Identify bottleneck (parsing? virtual call? state init?)
   - Optimize: inline hot paths, cache parsed trees, etc.
   - Validate: within 10% of compiled

2. **Color Initialization Fix**
   - Debug greyscale font pre-ignition
   - Trace color state initialization in getColor()
   - Fix color state reset between presets
   - Test with problematic styles

3. **Missing Effects Parsing**
   - Audit parser against all styles in /config/styles/
   - Identify unparsed primitives
   - Add parser support for missing functions
   - Validate with complex styles (assassin, calkestis)

4. **Return Value Semantics**
   - Fix run() implementations to return correct bool
   - Ensure blade power control works
   - Test blade on/off transitions

**Deliverables:**
- Fixed ignition timing
- Correct color initialization
- Complete parser (no missing effects)
- Correct run() return values
- Performance profile document
- Git commit per fix

**Success Criteria:**
- Ignition within 10% of compiled
- No pre-ignited colors
- All effects from test styles parse correctly
- Blade power control works as expected
- Frame rate stable (within 5% of compiled)

**Effort:** 6-8 hours (depends on root causes)

---

### Phase 4: Integration & Format Support (Implementation)
**Goal:** Add presets.ini support, validate hybrid mode, ensure both .style and preset formats work

**Key Tasks:**
1. **presets.ini Integration**
   - Study existing ProffieOS preset loading code
   - Add parser for `StyleFromSD("...")` references in presets.ini
   - Implement loading from preset path (not just SD root)
   - Handle missing file gracefully

2. **Hybrid Mode Testing**
   - Create test config with mix of compiled StylePtr and StyleFromSD
   - Test switching between compiled and SD styles
   - Verify no conflicts or state leakage
   - Test preset selection workflow

3. **Format Flexibility**
   - Support both absolute and relative paths
   - Support .style files with one style per file
   - Support inline styles in presets.ini
   - Document both formats

**Deliverables:**
- Presets.ini integration working
- Hybrid mode tested
- Example presets.ini snippet
- Git commit

**Success Criteria:**
- presets.ini loads styles without code changes
- Compiled + SD styles coexist seamlessly
- Switching presets smooth
- Both file formats work

**Effort:** 3-4 hours

---

### Phase 5: Testing & Validation (Quality Assurance)
**Goal:** Comprehensive UAT, performance validation, error handling verification

**Key Tasks:**
1. **UAT Test Suite Execution**
   - Run UAT1-UAT8 scenarios from REQUIREMENTS.md
   - Document pass/fail for each scenario
   - Fix any failures
   - Hardware testing if possible

2. **Performance Characterization**
   - Measure frame rate for simple, medium, complex styles
   - Compare with compiled baselines
   - Document findings
   - Identify pathological cases

3. **Memory Profiling**
   - Measure RAM usage for typical styles
   - Identify peak memory (during parsing vs runtime)
   - Compare with compiled equivalents
   - Document constraints

4. **Error Handling**
   - Test malformed style files
   - Test missing files
   - Test unknown tokens
   - Verify graceful degradation

**Deliverables:**
- UAT test results document
- Performance profile
- Memory usage report
- Error handling verification
- Git commit

**Success Criteria:**
- All UAT scenarios pass
- Performance within spec
- Error handling robust
- Documentation accurate

**Effort:** 4-5 hours

---

### Phase 6: Documentation & Polish (Documentation)
**Goal:** Create user-facing documentation, clean up code, prepare for merging

**Key Tasks:**
- Write README with usage examples
- Document API: StyleFromSD(), presets.ini format
- Provide example styles and presets.ini snippet
- Document known limitations
- Create troubleshooting guide
- Code cleanup: comments, formatting, consistency
- Optional: request profezzorn code review

**Deliverables:**
- README.md with examples
- Example presets.ini
- Known limitations document
- Code comments for complex sections
- Git commit

**Success Criteria:**
- Clear, new-user-friendly documentation
- Code reviewed for style/best practices
- Ready to merge to master

**Effort:** 2-3 hours

---

## Timeline & Dependencies

```
Phase 1 (Analysis)      [1-2h]  ────────────────────────┐
                                                        v
Phase 2 (Refactor)      [4-6h]  ────────────────────────┤
                                                        v
Phase 3 (Debug/Fix)     [6-8h]  ────────────────────────┤
                                                        v
Phase 4 (Integration)   [3-4h]  ────────────────────────┤
                                                        v
Phase 5 (Testing)       [4-5h]  ────────────────────────┤
                                                        v
Phase 6 (Polish)        [2-3h]  ────────────────────────┘

Total Estimated Effort: ~20-28 hours (serial execution)
Actual will vary based on complexity of issues found
```

## Phase Sequencing Logic

1. **Phase 1 first**: Must understand architecture before refactoring
2. **Phase 2 before Phase 3**: Refactoring simplifies debugging
3. **Phases 4-6 parallel**: Integration, testing, and docs can overlap
4. **Validation gates**: Each phase has clear success criteria; don't move forward without passing

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| Template wrapper approach doesn't work | Low | High | Phase 1 design review + PoC if needed |
| Ignition issue requires major redesign | Medium | Medium | Profile early, consider caching optimization |
| Missing many parser functions | Medium | Medium | Phase 3 includes complete audit |
| Performance regression | Low-Medium | Medium | Frame rate monitoring in all phases |
| Hybrid mode has subtle conflicts | Low | Medium | Dedicated test phase 4 |

## Gate Review Points

After each phase, review:
- [ ] All deliverables complete
- [ ] All success criteria met
- [ ] No blockers for next phase
- [ ] Performance/memory within expectations
- [ ] Code quality acceptable

---

**Roadmap Version:** 1.0
**Last Updated:** 2026-03-25
**Target Completion:** 2026-04-01 (estimated, actual ~1-2 weeks depending on issue complexity)
