# Styles

Style templates go in this directory. Styles are used to calculate the color for each pixel on the blade.
Note that most styles takes other styles as arguments, and even basic colors like "RED" are really
style templates. This means that colors can easily be replaced with much more complicated and dynamic
effects. Each style template needs at least two functions:

# void run(BladeBase* blade);

Called to start a frame.
It's ok to put semi-complicated calculations here, but not in getColor.

# OverDriveColor getColor(int led)

Called once per LED on the blade to calculate the color for that led.
Generally it's best to avoid complicated calculations in getColor since
it will be called thousands of times per second.

## SD Card Style Files

StyleFromSD() enables loading lightsaber blade styles from `.style` files on the SD card at runtime,
instead of compiling them into firmware. Edit a style, restart the saber, and see the changes — no recompile needed.
All StyleFromSD() styles are fully compatible with compiled StylePtr<>() styles and can be mixed freely within a preset.

### .Style File Format

A `.style` file is a plain text file containing a single style expression. The format is simple:

```
ColorType<arg1, arg2, ...>
```

or for composite styles:

```
Layers<Color1, Color2, ..., Effect>
```

Whitespace (spaces, tabs, newlines) is ignored. Nesting is allowed: styles can contain other styles as arguments.
A style file contains only the style definition itself — no C++ syntax, no classes, just the template expression.
Comments are not supported in .style files, but the syntax is simple enough that comments are rarely needed.

### StyleFromSD() API

Use `StyleFromSD()` in your preset configuration to load a style from the SD card:

```cpp
StyleFromSD("path/to/mystyle.style")
```

The function returns a style factory compatible with StylePtr<>. It can be used anywhere StylePtr<>() is used:

```cpp
// In config file (compiled presets):
{ "My Font", "track.wav",
  StyleFromSD("styles/mystyle.style"),  // Blade 0
  StyleFromSD("styles/accent.style")    // Blade 1
}

// In presets.ini (dynamic presets):
styledef=styles/mystyle.style
```

**Lazy Loading:** The SD card is not read during firmware boot. Styles are parsed when the preset is first selected,
adding a small delay (~50ms typical) on first selection. Subsequent selections are instant. This ensures that
boot time is not affected by style file loading, allowing many styles to coexist on the SD card.

### Path Resolution

Paths in StyleFromSD() are resolved as follows:

**Absolute paths** (starting with `/`):
```
StyleFromSD("/styles/mercenary.style")  → Opens /styles/mercenary.style directly from SD card root
```

**Relative paths** (no leading `/`):
```
StyleFromSD("mercenary.style")  → Searches backward from the current font directory
```

The backward search means: if your font is in `/fonts/MyFont/`, the loader searches:
1. `/fonts/MyFont/mercenary.style`
2. `/fonts/mercenary.style`
3. `/mercenary.style`

This allows you to distribute styles with your font (place in font directory) or override them in parent directories.
For production distributions, relative paths are preferred — they allow users to place styles in the font folder and move
everything together without changing preset configuration.

### Error Handling & Troubleshooting

**What happens when a style fails to parse:**

If a .style file contains syntax errors or is malformed, the blade will go dark when the preset is selected.
An error effect (beep or talkie) will play to alert you. In builds with ENABLE_DEBUG enabled, detailed parse
error messages are sent to the serial console.

**Common issues and fixes:**

| Issue | Cause | Fix |
|-------|-------|-----|
| Blade dark, no effect | File not found | Check the path and spelling; use absolute paths (/styles/...) to verify |
| Blade dark, error sound | Syntax error | Check for missing `>`, `,`, or mismatched brackets |
| Blade dark, error sound | Unknown style type | Check spelling of color, function, or transition name against examples |
| Slow first selection | Normal behavior | Lazy loading parses the .style file on first use (~50ms typical) |

**Debugging:**

1. **Enable ENABLE_DEBUG:** Recompile firmware with `#define ENABLE_DEBUG` in config. Parse errors will print to serial:
   ```
   StyleFromSD: expected '<', got token 5
   ```

2. **Test with simple styles first:** Start with `Rgb<255,0,0>` before complex nesting.

3. **Use absolute paths:** Easier to verify path exists:
   ```
   StyleFromSD("/styles/test.style")
   ```

4. **Check SD card:** Ensure SD card is not corrupted. Other files on the card should load normally.

**Production builds:** For production (ENABLE_DEBUG disabled), parse errors are silent. The error effect provides user feedback
without console output, keeping the saber responsive.

### Examples

The following examples progress from simple to moderately complex, showing how to build up style definitions.

#### Example 1: Solid Color

The simplest style: a single solid color.

File: `styles/red.style`
```
Rgb<255,0,0>
```

Use in preset:
```cpp
StyleFromSD("styles/red.style")
```

This creates a solid red blade. `Rgb<R,G,B>` takes three values (0-255) for red, green, and blue channels.

#### Example 2: Layered Colors

A base color with an accent layer on top.

File: `styles/red_with_accent.style`
```
Layers<
  Rgb<255,0,0>,
  Rgb<0,0,255>
>
```

This creates a red blade with a blue accent layer. Layers combine multiple styles, allowing base colors and effects to be mixed.

#### Example 3: Color with Transition

A color that fades in when the blade is turned on and fades out when turned off.

File: `styles/red_with_fade.style`
```
InOutTrL<TrFade<200>,
  Rgb<255,0,0>
>
```

- `InOutTrL<>` handles on/off transitions
- `TrFade<200>` specifies a 200ms fade (both in and out)
- `Rgb<255,0,0>` is the target color (red)

This pattern is useful for blade ignition and retraction animations.

#### Example 4: Production Style

Here's a real style from production configuration (crispity). It demonstrates layers, colors, and effects:

File: `styles/crispity.style`
```
Layers<HumpFlicker<Red,Orange,50>,
  TransitionEffectL<TrConcat<TrFade<200>,Yellow,TrDelay<1000>,
    Yellow,TrFade<800>>,EFFECT_FORCE>,
  AlphaL<AudioFlickerL<DeepPink>,Int<10000>>,
  LockupTrL<Layers<AlphaL<AudioFlickerL<Yellow>,Int<10000>>,
    AlphaL<NavajoWhite,Int<10000>>>,
    TrConcat<TrInstant,White,TrFade<400>>,
    TrConcat<TrInstant,White,TrFade<400>>,
    SaberBase::LOCKUP_NORMAL>,
  ResponsiveLightningBlockL<Strobe<White,Blue,50,1>,
    TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,
    TrFade<200>>,
    TrConcat<TrInstant,White,TrFade<400>>>,
  InOutTrL<TrWipeSparkTip<White,100>,
    TrWipeInSparkTip<White,100>,Black>>
```

This style uses:
- **HumpFlicker:** Creates a flickering red/orange base
- **TransitionEffectL:** Adds a yellow flash effect when Force is activated
- **AudioFlickerL:** Creates audio-reactive deep pink accents
- **LockupTrL:** Handles lockup (collision) effects with white flashes
- **ResponsiveLightningBlockL:** Adds responsive lightning effects
- **InOutTrL:** Smooth wipe transitions on ignition and retraction

You can explore other styles in `config/styles/` or examine the source code for more advanced patterns. The key is to combine
simple building blocks (colors, flickers, transitions) into complex effects through layering and nesting.
