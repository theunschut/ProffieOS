# Phase 4 Plan Overview: Integration & Format Support

**Status:** ✅ PLAN CREATED
**Date:** 2026-03-25
**Phase Goal:** Add presets.ini support, validate hybrid mode (compiled + SD styles coexist), ensure both .style file and preset formats work
**Effort Estimate:** 3-4 hours (as per ROADMAP)

---

## Executive Summary

Phase 4 delivers the final integration piece for the SD-card style loader. After Phase 3 fixed critical issues (pre-ignition, power control, effects parsing), Phase 4 enables users to deploy styles through ProffieOS's standard presets.ini configuration without any code recompilation.

**Key Deliverables:**
- Extended `CurrentPreset::Read()` parser to handle `preset.style = StyleFromSD(...)` entries
- Implemented path resolution strategy (follows ProffieOS conventions)
- Validated hybrid mode (compiled and SD styles coexist safely)
- Comprehensive user documentation with real-world examples
- Test configuration demonstrating mixed style deployment

---

## Phase 4 Plan Structure

**File:** `.planning/4.PLAN.md`
**Format:** Executable plan with 6 tasks in Wave 1 (all can run in parallel)
**Tasks:** 805 lines of detailed implementation guidance

### Task Breakdown

| # | Task | Owner | Est. Duration | Deliverable |
|---|------|-------|---------------|-------------|
| 4.1 | Extend presets.ini Parser | Claude | 45-60 min | Extended `CurrentPreset::Read()` with function-call parsing |
| 4.2 | Path Resolution & Lazy Init | Claude | 45-60 min | `StyleFromSD()` calls resolve files in conventional locations |
| 4.3 | Hybrid Mode Test Config | Claude | 30-45 min | Test configuration + hybrid validation guide |
| 4.4 | Memory Management Validation | Claude | 20-30 min | Code inspection + memory safety documentation |
| 4.5 | Example presets.ini | Claude | 60-90 min | Comprehensive user guide with 5+ real examples |
| 4.6 | Git Commit | Claude | 10-15 min | Atomic commit documenting Phase 4 integration |

**Total Estimated Effort:** 3-4 hours (matches ROADMAP estimate)

---

## Requirements Coverage

**All Phase 4 requirements fully addressed:**

| Requirement | Task | Coverage | Status |
|-------------|------|----------|--------|
| REQ 2.2: Load from presets.ini | 4.1, 4.2, 4.5 | Parser extension + path resolution + docs | ✅ |
| REQ 2.3: Hybrid mode coexistence | 4.3, 4.4 | Validation + memory safety review | ✅ |
| UAT3: Hybrid mode switching | 4.3 | Test matrix with 5+ scenarios | ✅ |
| UAT6: presets.ini integration | 4.1, 4.2, 4.5 | End-to-end flow documented | ✅ |

---

## Technical Approach

### Architecture: Three Layers

1. **Parsing Layer** (Task 4.1)
   - Extend `CurrentPreset::Read()` to handle dotted variable names (`preset.style`, `preset.style1`)
   - Parse function-call syntax: `StyleFromSD("path")`
   - Store paths in `current_style_[blade_num]` array

2. **Resolution Layer** (Task 4.2)
   - Implement path resolution strategy:
     1. `/config/styles/{filename}` (primary, ProffieOS convention)
     2. `/styles/{filename}` (secondary)
     3. Raw path provided (absolute/relative)
   - Use `LSFS::Exists()` to verify files before instantiation
   - Log resolution attempts to `STDOUT` for debugging

3. **Integration Layer** (Tasks 4.3, 4.4, 4.5)
   - Call `StyleFromSD(resolved_path)` from `CurrentPreset::Read()`
   - Returns `StyleFactory*` compatible with existing `style_allocator`
   - At preset activation, `make()` defers file loading and parsing
   - Memory lifecycle: `new` by factory, `delete` by preset system

### Key Integration Points

**File: `common/current_preset.h`**
- `CurrentPreset::Read()`: Parse preset entries from presets.ini
- Line 127: `f->readVariable(variable)` → extend to support `preset.styleN`
- Line 133: Add handler for `"preset.style*"` entries
- Extract paths and call `StyleFromSD()`

**File: `styles/sd_style.h`**
- `StyleFromSD()` function (line 4354): Already exists, no changes needed
- `SDStyleFactory` class (lines 4287-4344): Already implements lazy loading
- Returns `StyleFactory*` compatible with `StyleAllocator` typedef

**File: `common/preset.h`**
- `Preset` struct: No changes (ROM presets unaffected)
- Hybrid mode: Disk presets use `current_style_[N]`, ROM use `style_allocatorN`

---

## Hybrid Mode Validation

**Test Scenarios (Task 4.3):**

| Scenario | Blade 0 | Blade 1 | Result |
|----------|---------|---------|--------|
| ROM only | Compiled | N/A | Both work independently |
| Disk SD only | StyleFromSD | N/A | File loads correctly |
| Mixed (Disk) | Compiled | StyleFromSD | No conflicts, smooth switching |
| Transition ROM→Disk | Compiled→SD | - | Colors change correctly |
| Edge case: override | Compiled (overwritten) | StyleFromSD | Second value wins |

**Success Criteria:**
- Both preset types activate without errors
- Switching presets is smooth (no glitches, no memory leaks)
- Compiled and SD styles render equivalent appearances
- Hybrid config handles both types simultaneously

---

## User-Facing Changes

### New Syntax: presets.ini

**Before (ROM presets only):**
```
// Limited to compiled styles in firmware
```

**After (with Phase 4):**
```ini
[new_preset]
name = "Assassin Style"
font = "fonts/OldRepublic"
track = "tracks/saber_init.wav"
preset.style = StyleFromSD("assassin.style")
[end]

[new_preset]
name = "Hybrid Mode"
font = "fonts/OldRepublic"
preset.style1 = StylePtr<Layers<Blue,Black>>()      # Blade 0: Compiled
preset.style2 = StyleFromSD("test_simple.style")    # Blade 1: SD-loaded
[end]
```

### Documentation: User Guide

**File:** `.planning/4.PRESETS_EXAMPLE.md` (~400-500 lines)

Includes:
- 5+ real-world preset examples
- Path resolution rules
- Feature support matrix
- Common mistakes & fixes
- FAQ: "Why use StyleFromSD?", "Do I need to recompile?", etc.
- Migration guide: Convert compiled styles to SD format

---

## Risk Mitigation

**Identified Risks & Mitigations:**

| Risk | Likelihood | Impact | Mitigation (Task) |
|------|------------|--------|-------------------|
| Parser breaks on `StyleFromSD(...)` | Medium | Critical | 4.1: Test cases for syntax handling |
| File path resolution fails | Medium | High | 4.2: Try multiple locations + logging |
| Memory leaks in cycle | Low | High | 4.4: Code inspection + lifecycle test |
| Compiled/SD style conflicts | Low | Medium | 4.3: Hybrid validation matrix |
| User syntax errors in .ini | High | Low | 4.5: FAQ + error handling docs |

**Contingencies:**
- If parser fails: Simple fallback (skip unrecognized entries, log warning)
- If path resolution fails: Return black (safe default style)
- If memory leak detected: Review `SDStyleFactory::make()` allocation pattern

---

## Testing Strategy

### Phase 4 Testing (Manual, no hardware needed)

1. **Code Review** (Tasks 4.1, 4.2, 4.4):
   - Verify parser extension handles dotted names
   - Inspect path resolution logic
   - Check memory safety

2. **Configuration Testing** (Task 4.3):
   - Create test presets.ini with mixed entries
   - Document expected behavior for each scenario
   - Outline verification steps for UAT

3. **Documentation Testing** (Task 4.5):
   - Verify syntax examples are valid
   - Check migration guide is complete
   - Ensure FAQ addresses common questions

### Phase 5 Testing (Hardware-dependent, deferred)

- **UAT1-UAT8:** Execute scenarios with actual hardware
- **Performance:** Measure frame rate, memory usage
- **Stress tests:** Multiple styles, rapid switching, etc.

---

## Files Modified & Created

### Code Changes

| File | Change | Lines | Risk |
|------|--------|-------|------|
| `common/current_preset.h` | Extend `Read()` parser | +30-50 | Low (isolated to Read method) |
| `styles/sd_style.h` | Add documentation comments | +5-10 | None (comments only) |

### Documentation Created

| File | Size | Content |
|------|------|---------|
| `.planning/4.HYBRID_TEST_CONFIG.md` | ~200 lines | Test matrix, preset examples, verification steps |
| `.planning/4.PRESETS_EXAMPLE.md` | ~400 lines | User guide, real examples, FAQ, migration guide |
| `config/styles/test_hybrid.style` | ~5 lines | Complex test style for hybrid validation |

---

## Quality Assurance

**Code Quality:**
- No breaking changes (backward compatible)
- No new compiler warnings expected
- Memory safe (RAII, proper lifecycle)
- Following ProffieOS conventions

**Documentation Quality:**
- Beginner-friendly (target: any user, not just developers)
- Comprehensive (covers happy path + error cases)
- Well-organized (clear sections, real examples)
- Actionable (step-by-step instructions)

**Test Coverage:**
- Manual validation provided (test matrix)
- Hardware testing deferred to Phase 5 (non-blocking)
- Integration points documented for UAT

---

## Success Criteria

✅ **Functional:**
- Users can write `preset.style = StyleFromSD("file.style")` in presets.ini
- Styles load automatically at preset selection (no code changes)
- Compiled and SD styles work together (hybrid mode)
- Missing files don't crash (graceful fallback)

✅ **Documentation:**
- Example presets.ini provided
- User guide covers all scenarios
- Troubleshooting FAQ available
- Migration path documented

✅ **Engineering:**
- Parser handles function-call syntax
- Path resolution follows conventions
- Memory lifecycle reviewed for safety
- No performance regression expected

✅ **Integration:**
- All changes additive (no refactoring)
- ROM presets unaffected
- Feature/StyleFromSD branch clean
- Atomic commit with clear message

---

## Next Steps

### Immediate (After Phase 4 Plan)

1. Execute Phase 4 tasks (use `4.PLAN.md` for detailed implementation)
2. Create atomic git commit with all Phase 4 changes
3. Verify no breaking changes to existing functionality

### Phase 5: Testing & Validation

1. Execute UAT1-UAT8 scenarios with actual presets.ini
2. Measure performance (frame rate, memory usage)
3. Test on hardware (actual Proffieboard)
4. Document test results

### Phase 6: Polish & Merge

1. Code review by maintainer
2. Final documentation polish
3. Merge feature/StyleFromSD to master
4. Create release notes

---

## Decision Log

**Phase 4 Planning Decisions:**

| Decision | Rationale | Trade-off |
|----------|-----------|-----------|
| Extend `CurrentPreset::Read()` rather than create new parser | Reuses existing parsing infrastructure | Limited to presets.ini syntax (acceptable) |
| Path resolution tries `/config/styles/` first | Follows ProffieOS conventions | May surprise users unfamiliar with structure |
| Lazy loading deferred to `SDStyleFactory::make()` | Reuses existing `StyleFactory` pattern | Cannot pre-validate styles at preset load time |
| Documentation in separate files (not code comments) | Readable for end-users | Additional files to maintain |
| Hybrid mode testing in Phase 4 (no hardware) | De-risk integration early | Full UAT deferred to Phase 5 |

---

## Appendix: File References

**Key source files referenced in plan:**

- `/c/Repos/ProffieOS/styles/sd_style.h`: SD style parser & loader (4358 lines)
- `/c/Repos/ProffieOS/common/current_preset.h`: Preset loading logic
- `/c/Repos/ProffieOS/common/preset.h`: Preset data structure
- `/c/Repos/ProffieOS/styles/blade_style.h`: StyleFactory base class
- `/c/Repos/ProffieOS/config/styles/`: Test style files

**Documentation references:**

- `/c/Repos/ProffieOS/.planning/REQUIREMENTS.md`: Phase 4 requirements (REQ 2.2, 2.3)
- `/c/Repos/ProffieOS/.planning/ROADMAP.md`: Phase 4 overview & timeline
- `/c/Repos/ProffieOS/.planning/3.SUMMARY.md`: Phase 3 completion status

---

**Phase 4 Plan Status:** ✅ READY FOR EXECUTION

**Estimated Completion:** 3-4 hours (serial execution by single Claude instance)

**Next Action:** Execute `/gsd:execute-phase 4` with `4.PLAN.md`

---

*Plan created: 2026-03-25*
*Plan revision: 1.0*
*Target audience: Claude executor + phase checklist reviewer*
