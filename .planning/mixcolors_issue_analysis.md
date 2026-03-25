# MixColors Overload Conflict Analysis

**Issue:** Line 557 in `common/color.h` uses `DISAMBIGUATE_MIXCOLORS2` instead of `DISAMBIGUATE_MIXCOLORS3`

**Root Cause:** Conflict between:
1. Explicit `MixColors(RGBA_um, RGBA_um)` override in `styles/sd_style.h` (line 40-50)
2. `DISAMBIGUATE_MIXCOLORS3(RGBA_um, RGBA_um, RGBA)` attempting to create same signature in `color.h` (line 557)

---

## The Problem Explained

### What DISAMBIGUATE_MIXCOLORS macros do:

```cpp
#define DISAMBIGUATE_MIXCOLORS1(A, B, C) \
  inline auto MixColors(A a, B b, int x, int shift) -> decltype(MixColors(C(a), C(b), x, shift)) \
  { return MixColors(C(a), C(b), x, shift); }

#define DISAMBIGUATE_MIXCOLORS2(A, B, C) \
  DISAMBIGUATE_MIXCOLORS1(A##_nod, B, C)   // MixColors(A_nod, B, ...)
  DISAMBIGUATE_MIXCOLORS1(A, B##_nod, C)   // MixColors(A, B_nod, ...)

#define DISAMBIGUATE_MIXCOLORS3(A, B, C) \
  DISAMBIGUATE_MIXCOLORS1(A, B, C)        // MixColors(A, B, ...)
  DISAMBIGUATE_MIXCOLORS2(A, B, C)        // + the two _nod variants above
```

### Current situation (with DISAMBIGUATE_MIXCOLORS2):

**sd_style.h line 40-50:**
```cpp
inline RGBA_um MixColors(RGBA_um a, RGBA_um b, int x, int shift) {
  // Custom RGBA_um mixing logic (handles alpha blending, overdrive)
  ...
}
```

**color.h line 557 (CURRENT):**
```cpp
DISAMBIGUATE_MIXCOLORS2(RGBA_um, RGBA_um, RGBA)
```
Creates only:
- `MixColors(RGBA_um_nod a, RGBA_um b, ...)` → dispatches to `MixColors(RGBA(a), RGBA(b), ...)`
- `MixColors(RGBA_um a, RGBA_um_nod b, ...)` → dispatches to `MixColors(RGBA(a), RGBA(b), ...)`

**DOES NOT create:**
- `MixColors(RGBA_um, RGBA_um, ...)` (already defined in sd_style.h)

### What if we changed it back to DISAMBIGUATE_MIXCOLORS3?

```cpp
DISAMBIGUATE_MIXCOLORS3(RGBA_um, RGBA_um, RGBA)
```
Would try to create:
- `MixColors(RGBA_um, RGBA_um, ...)` ← **CONFLICT!** Already defined in sd_style.h
- `MixColors(RGBA_um_nod, RGBA_um, ...)`
- `MixColors(RGBA_um, RGBA_um_nod, ...)`

**Result:** `error: multiple definitions of 'MixColors'` → **Build fails**

---

## Is the DISAMBIGUATE_MIXCOLORS2 approach correct?

**No.** Here's why:

### Problem 1: Semantics are wrong

The _nod variants created by DISAMBIGUATE_MIXCOLORS2 dispatch to `MixColors(RGBA(a), RGBA(b), ...)`, which:
- Loses the alpha channel entirely
- Uses basic RGB mixing, not RGBA_um's sophisticated alpha blending
- Ignores overdrive handling

When code calls `MixColors(RGBA_um_nod, RGBA_um, ...)`, it should use the same logic as `MixColors(RGBA_um, RGBA_um, ...)`, but instead it converts to RGB and loses information.

### Problem 2: Inconsistent with pattern

Lines 554-560 all use DISAMBIGUATE_MIXCOLORS3:
```cpp
DISAMBIGUATE_MIXCOLORS3(OverDriveColor, RGBA_um, RGBA_um)
DISAMBIGUATE_MIXCOLORS3(OverDriveColor, RGBA, RGBA)
DISAMBIGUATE_MIXCOLORS3(RGBA_um, OverDriveColor, RGBA)
DISAMBIGUATE_MIXCOLORS2(RGBA_um, RGBA_um, RGBA)    ← DIFFERENT!
DISAMBIGUATE_MIXCOLORS3(RGBA_um, RGBA, RGBA)
```

Line 561 also uses DISAMBIGUATE_MIXCOLORS2:
```cpp
DISAMBIGUATE_MIXCOLORS2(RGBA, RGBA, RGBA)    ← DIFFERENT!
```

Why are two lines different from all the others?

---

## The Correct Solution

### Option A: Move the override to color.h (RECOMMENDED)

Move the explicit `MixColors(RGBA_um, RGBA_um)` override from `sd_style.h` to `color.h`, placing it **before** the DISAMBIGUATE lines, alongside other MixColors definitions.

**Steps:**
1. Remove from `styles/sd_style.h` (line 40-50):
   ```cpp
   inline RGBA_um MixColors(RGBA_um a, RGBA_um b, int x, int shift) { ... }
   ```

2. Add to `common/color.h` before line 553 (before DISAMBIGUATE_MIXCOLORS calls):
   ```cpp
   // MixColors overload for RGBA_um — custom alpha blending with overdrive handling
   inline RGBA_um MixColors(RGBA_um a, RGBA_um b, int x, int shift) {
     int ax = (1 << shift) - x;
     return RGBA_um(
       ((a.c * (uint16_t)ax) + (b.c * (uint16_t)x)) >> shift,
       x > (1 << (shift - 1)) ? b.overdrive : a.overdrive,
       (uint16_t)(((uint32_t)a.alpha * (uint16_t)ax + (uint32_t)b.alpha * (uint16_t)x
                   + ((1 << shift) - 1)) >> shift)
     );
   }
   ```

3. Change line 557 back to:
   ```cpp
   DISAMBIGUATE_MIXCOLORS3(RGBA_um, RGBA_um, RGBA)
   ```

4. Now DISAMBIGUATE_MIXCOLORS3 creates:
   - `MixColors(RGBA_um, RGBA_um, ...)` → Our explicit override (perfect!)
   - `MixColors(RGBA_um_nod, RGBA_um, ...)` → Dispatches to our override
   - `MixColors(RGBA_um, RGBA_um_nod, ...)` → Dispatches to our override

**Why this works:**
- ✅ No duplicate definition (override is the implementation, DISAMBIGUATE creates dispatch variants)
- ✅ Consistent with ProffieOS pattern (all MixColors in color.h)
- ✅ Semantically correct (_nod variants dispatch to our alpha-aware override)
- ✅ Follows existing architecture (like how other MixColors are organized)

---

### Option B: Keep DISAMBIGUATE_MIXCOLORS2 but add comment

If moving is not feasible, at least document why:

```cpp
// NOTE: Uses DISAMBIGUATE_MIXCOLORS2 (not 3) because sd_style.h provides explicit MixColors(RGBA_um, RGBA_um)
// override for custom alpha blending. The _nod variants still dispatch to RGBA version, which is acceptable
// because RGBA_um_nod should be used rarely in practice.
DISAMBIGUATE_MIXCOLORS2(RGBA_um, RGBA_um, RGBA)
```

**Downside:** Semantically incorrect behavior for _nod variants, but may be acceptable if rarely used.

---

### Option C: Add a DISAMBIGUATE_MIXCOLORS3 after the sd_style.h override

Keep the explicit override in sd_style.h and add a _nod dispatch after it:

```cpp
// In sd_style.h
inline RGBA_um MixColors(RGBA_um a, RGBA_um b, int x, int shift) { ... }

// Also dispatch from _nod variants
inline RGBA_um MixColors(RGBA_um_nod a, RGBA_um b, int x, int shift) {
  return MixColors(RGBA_um(a), b, x, shift);
}
inline RGBA_um MixColors(RGBA_um a, RGBA_um_nod b, int x, int shift) {
  return MixColors(a, RGBA_um(b), x, shift);
}
```

**Downside:** Duplicates dispatch logic that DISAMBIGUATE is designed to handle.

---

## Recommendation

### ✅ **Use Option A (Move to color.h)**

This is the most correct approach because it:
1. Follows ProffieOS architecture (all MixColors definitions in color.h)
2. Uses DISAMBIGUATE correctly (explicit override + dispatch variants)
3. Is semantically sound (no information loss)
4. Makes line 557 consistent with lines 554-556, 558-560
5. Explains why RGBA/RGBA on line 561 also uses DISAMBIGUATE_MIXCOLORS2 (investigate if needed)

---

## Implementation Checklist

- [ ] Review and approve approach (Option A vs B vs C)
- [ ] If Option A:
  - [ ] Move MixColors(RGBA_um, RGBA_um) from sd_style.h to color.h (lines 40-50 → before line 553)
  - [ ] Change line 557 from DISAMBIGUATE_MIXCOLORS2 back to DISAMBIGUATE_MIXCOLORS3
  - [ ] Update comment in sd_style.h to reference color.h location
  - [ ] Compile and verify no errors
  - [ ] Verify InOutSparkTipX behavior unchanged
  - [ ] Create atomic commit explaining the fix
- [ ] Investigate line 561 (RGBA/RGBA using DISAMBIGUATE_MIXCOLORS2) — is it also a workaround?
- [ ] Code review to ensure pattern consistency

---

**Status:** Analysis complete. Awaiting user decision on approach.

**Created:** 2026-03-25
**Reviewer:** Claude Code Assistant
