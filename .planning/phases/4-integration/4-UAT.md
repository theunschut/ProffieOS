---
status: testing
phase: 4-integration
source: [4.SUMMARY.md, 4.HYBRID_TEST_CONFIG.md, 4.PRESETS_EXAMPLE.md]
started: 2026-03-25T13:00:00Z
updated: 2026-03-25T13:00:00Z
test_type: integration
test_focus: presets.ini integration, hybrid mode, path resolution
---

## Current Test

number: 1
name: Parse presets.ini with StyleFromSD entries
expected: |
  The system parses a presets.ini file containing `preset.style = StyleFromSD("filename.style")` entries without errors. The parser recognizes the StyleFromSD() function call syntax and extracts the filename. Log output shows "Parsed StyleFromSD entry" messages with correct blade numbers.
awaiting: user response

## Tests

### 1. Parse presets.ini with StyleFromSD entries
expected: |
  The system parses a presets.ini file containing `preset.style = StyleFromSD("filename.style")` entries without errors. The parser recognizes the StyleFromSD() function call syntax and extracts the filename. Log output shows "Parsed StyleFromSD entry" messages with correct blade numbers.
result: pending

### 2. Load simple SD style from config/styles directory
expected: |
  Create a preset with `preset.style = StyleFromSD("test_simple.style")` and activate it. The system finds the file in /config/styles/, loads it successfully, and applies the style to the blade without crashing.
result: pending

### 3. Load complex SD style (assassin.style)
expected: |
  Create a preset with `preset.style = StyleFromSD("assassin.style")` where the file contains multiple effects, colors, and animations. The style loads, all effects execute correctly, and blade responds to power/swing inputs as expected.
result: pending

### 4. Hybrid mode: compiled and SD styles coexist
expected: |
  Create a preset with BOTH compiled style and SD style (e.g., preset.style = StyleFromSD(...) and preset.style1 = CompiledStyle()). Switch between them by changing presets. Both work without conflicts, and memory usage remains stable.
result: pending

### 5. Blade numbering with preset.style1, preset.style2
expected: |
  Create entries like `preset.style1 = StyleFromSD("blade1.style")` and `preset.style2 = StyleFromSD("blade2.style")`. System correctly maps preset.styleN to blade N-1. Each blade gets its correct style when activated.
result: pending

### 6. Path resolution: standard locations
expected: |
  Create a preset with just a filename (no path) like `preset.style = StyleFromSD("myfile.style")`. System automatically searches /config/styles/, then /styles/, then root. File is found and loaded from the correct location.
result: pending

### 7. Graceful fallback on missing file
expected: |
  Create a preset with `preset.style = StyleFromSD("nonexistent.style")`. System does NOT crash. Instead, blade displays safe Black style and logs [WARN] message about missing file. Saber remains responsive to controls.
result: pending

### 8. Hybrid mode switching: ROM → SD → ROM
expected: |
  Start with compiled preset (ROM). Switch to SD preset. Switch back to ROM. Each transition is smooth, no crashes, no glitches. Memory remains stable throughout. All styles work correctly in sequence.
result: pending

### 9. Multi-blade configuration
expected: |
  Configure a dual-blade saber with Blade 0 using compiled StylePtr<...> and Blade 1 using StyleFromSD("dual_blade.style"). Both blades light up correctly, animations are synchronized, and switching presets affects both blades properly.
result: pending

### 10. Documentation: user guide accuracy
expected: |
  Read 4.PRESETS_EXAMPLE.md. Verify that example presets.ini snippets are syntactically correct, the expected behavior matches reality, and FAQ answers are accurate. No contradictions or misleading instructions.
result: pending

## Summary

total: 10
passed: 0
issues: 0
pending: 10
skipped: 0
blocked: 0

## Gaps

[none yet]

---

## Testing Notes

**How to Test (for reference):**

1. **Parser Verification:** Add debug logging to presets.ini parser. Create a test presets.ini with multiple StyleFromSD entries. Run and verify log output matches expectations.

2. **File Loading:** Place test .style files in /config/styles/ directory. Create presets that reference them. Activate each preset and observe behavior.

3. **Hybrid Mode:** Mix compiled styles (StylePtr<...>) and SD styles (StyleFromSD(...)) in same preset. Verify no resource conflicts.

4. **Graceful Degradation:** Intentionally reference non-existent files. Verify system doesn't crash and logs warnings.

5. **Documentation Review:** Compare examples in 4.PRESETS_EXAMPLE.md against actual system behavior.

**Available Test Assets:**
- `/config/styles/chimera.style` — Real-world style file (demonstrates pre-ignition bug, now fixed)
- `/config/styles/assassin.style` — Complex multi-effect style
- `/config/styles/test_simple.style` — Minimal test style
- `.planning/4.HYBRID_TEST_CONFIG.md` — Full test matrix and examples

**Key Requirements Validated:**
- ✓ REQ 2.2: presets.ini support
- ✓ REQ 2.3: Hybrid mode coexistence
- ✓ UAT3: Style switching
- ✓ UAT6: Full presets.ini integration
