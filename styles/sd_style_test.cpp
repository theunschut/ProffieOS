#include <vector>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include <string.h>

// cruft
#define interrupts() do {} while(0)
#define noInterrupts() do {} while(0)
#define NELEM(X) (sizeof(X)/sizeof((X)[0]))
#define SCOPED_PROFILER() do { } while(0)
#define NUM_BLADES 1
const int maxLedsPerStrip = 144;

struct CONFIG { struct Preset* presets; size_t num_presets;};
extern CONFIG* current_config;

#define PROFFIE_TEST

#define COMMON_FUSE_H

struct V3 {
  V3(float v) { x=y=z=v; }
  float x, y, z;
};

struct MockFuse {
  float angle1_ = 0.0;
  float angle1() { return angle1_; }
  float angle2_ = 0.0;
  float angle2() { return angle2_; }
  float swing_speed_ = 0.0;
  float swing_speed() { return swing_speed_; }
  float swing_accel() { return 0.0; }
  float twist_accel() { return 0.0; }
  V3 gyro() { return V3(0.0); }
};

MockFuse fusor;

template<class A, class B>
constexpr auto min(A&& a, B&& b) -> decltype(a < b ? std::forward<A>(a) : std::forward<B>(b)) {
  return a < b ? std::forward<A>(a) : std::forward<B>(b);
}
template<class A, class B>
constexpr auto max(A&& a, B&& b) -> decltype(a < b ? std::forward<A>(a) : std::forward<B>(b)) {
  return a >= b ? std::forward<A>(a) : std::forward<B>(b);
}

char* itoa(int value, char* str, int radix) {
  if (radix != 10) {
    fprintf(stderr, "Unexpected radix!\n");
    exit(1);
  }
  sprintf(str, "%d", value);
  return str;
}

uint32_t micros_ = 0;
uint32_t micros() { return micros_; }
uint32_t millis() { return micros_ / 1000; }

int random(int x) { return (rand() & 0x7fffff) % x; }
class BladeBase;
int GetBladeNumber(BladeBase* blad) { return 0; }

class Looper {
public:
  static void DoHFLoop() {}
};

template<class T, class U>
struct is_same_type { static const bool value = false; };

template<class T>
struct is_same_type<T, T> { static const bool value = true; };

// This really ought to be a typedef, but it causes problems I don't understand.
#define StyleAllocator class StyleFactory*

#define HEX 16

#define ENABLE_AUDIO

struct MockDynamicMixer {
  int32_t last_sample() const { return 4093; }
  int32_t last_sum() const { return 16384; }
  int32_t audio_volume() const { return 100000; }
};

MockDynamicMixer dynamic_mixer;

#include "../common/common.h"
#include "../common/math.h"
#include "../common/stdout.h"
Print default_printer;
Print* default_output = &default_printer;
Print* stdout_output = &default_printer;
ConsoleHelper STDOUT;

Monitoring monitor;

#include "../common/stdout.h"
#include "../common/color.h"
#include "../blades/blade_base.h"
#include "cylon.h"
#include "../common/arg_parser.h"
#include "style_ptr.h"
#include "colors.h"
#include "inout_helper.h"
#include "blast.h"
#include "transition_effect.h"
#include "audio_flicker.h"
#include "pulsing.h"
#include "../functions/bump.h"
#include "lockup.h"
#include "blinking.h"
#include "clash.h"
#include "color_cycle.h"
#include "edit_mode.h"
#include "remap.h"
#include "stripes.h"
#include "transition_loop.h"
#include "sequence.h"
#include "../transitions/base.h"
#include "../transitions/join.h"
#include "../transitions/boing.h"
#include "../transitions/wipe.h"
#include "../transitions/delay.h"
#include "../transitions/concat.h"
#include "../transitions/fade.h"
#include "../transitions/instant.h"
#include "../transitions/random.h"
#include "../transitions/loop.h"
#include "../functions/blade_angle.h"
#include "../functions/twist_angle.h"
#include "../functions/swing_speed.h"
#include "../functions/wavlen.h"
#include "../functions/center_dist.h"
#include "../functions/effect_position.h"
#include "../functions/random.h"
#include "../functions/mult.h"
#include "../functions/hold_peak.h"
#include "mix.h"
#include "strobe.h"
#include "hump_flicker.h"
#include "brown_noise_flicker.h"
#include "responsive_styles.h"
#include "rainbow.h"
#include "legacy_styles.h"
#include "rgb_arg.h"
#include "inout_sparktip.h"
#include "on_spark.h"
#include "gradient.h"
#include "fire.h"
#include "sparkle.h"
#include "../common/command_parser.h"
#include "../common/preset.h"
#include "random_per_led_flicker.h"
#include "../functions/clash_impact.h"
#include "../functions/sum.h"
#include "../functions/ramp.h"
#include "rotate_color.h"
#include "random_blink.h"
#include "../functions/effect_increment.h"
#include "../transitions/extend.h"

Preset presets[] = {
  { "one", "t1",
    StylePtr<Red>(),
    "uno" }
};
CONFIG preset_cfg = { presets, 1 };
CONFIG* current_config = &preset_cfg;

CommandParser* parsers = NULL;
ArgParserInterface* CurrentArgParser;

class StyleCharging : public BladeStyle {
public:
  void activate() override {}
  void run(BladeBase *blade) override {};
  bool NoOnOff() override { return true; }
  bool Charging() override { return true; }
  bool IsHandled(HandledFeature effect) override { return false; }
};
StyleFactoryImpl<StyleCharging> style_charging;

#include "style_parser.h"

SaberBase* saberbases = NULL;
SaberBase::LockupType SaberBase::lockup_ = SaberBase::LOCKUP_NONE;
SaberBase::ColorChangeMode SaberBase::color_change_mode_ =
  SaberBase::COLOR_CHANGE_MODE_NONE;
uint32_t SaberBase::last_motion_request_ = 0;
uint32_t SaberBase::current_variation_ = 0;
float SaberBase::sound_length = 0.0;
int SaberBase::sound_number = -1;

bool on_ = false;
bool allow_disable_ = false;

// Additional includes needed by sd_style.h
#include "../common/lsfs.h"
#include "../common/errors.h"
#include <unistd.h>  // for unlink()

// Stub ProffieOSErrors functions (their implementations require hardware)
void ProffieOSErrors::sd_card_not_found() {}
void ProffieOSErrors::font_directory_not_found() {}
void ProffieOSErrors::voice_pack_not_found() {}
void ProffieOSErrors::error_in_blade_array() {}
void ProffieOSErrors::error_in_font_directory() {}
void ProffieOSErrors::error_in_voice_pack_version() {}
void ProffieOSErrors::low_battery() {}

// Missing SaberBase static member definition
float SaberBase::clash_strength_ = 0.0;

// Stub battery_monitor for BatteryLevel function (not used in test styles)
struct MockBatteryMonitor {
  int battery_percent() const { return 75; }
} battery_monitor;

#include "../functions/battery_level.h"

// Include the file under test after all stubs and dependencies
#include "sd_style.h"

// ============================================================
// CHECK Macros
// ============================================================

#define CHECK(X) do { if (!(X)) { fprintf(stderr, "%s:%d: CHECK(" #X ") failed!\n", __FILE__, __LINE__); exit(1); } } while(0)
#define CHECK_EQ(X,Y) do { auto x_=(X); auto y_=(Y); if (x_ != y_) { fprintf(stderr, "%s:%d: CHECK_EQ(" #X ", " #Y ") failed: %d != %d\n", __FILE__, __LINE__, (int)x_, (int)y_); exit(1); } } while(0)
#define CHECK_NE(X,Y) do { auto x_=(X); auto y_=(Y); if (x_ == y_) { fprintf(stderr, "%s:%d: CHECK_NE(" #X ", " #Y ") failed: both %d\n", __FILE__, __LINE__, (int)x_); exit(1); } } while(0)
#define CHECK_STREQ(X,Y) do { const char* x_=(X); const char* y_=(Y); if (strcmp(x_,y_) != 0) { fprintf(stderr, "%s:%d: CHECK_STREQ failed: '%s' != '%s'\n", __FILE__, __LINE__, x_, y_); exit(1); } } while(0)

// ============================================================
// MockBlade for sd_style tests
// ============================================================

class MockBlade : public BladeBase {
public:
  std::vector<Color16> colors;

  int num_leds() const override { return colors.size(); }
  bool is_on() const override { return on_; }
  bool is_powered() const override { return true; }
  void set(int led, Color16 c) override {
    colors[led] = c;
  }
  void set_overdrive(int led, Color16 c) override {
    // Treat overdrive as normal set for testing purposes
    colors[led] = c;
  }
  void allow_disable() override {
    allow_disable_ = true;
  }
  void Activate(int blade_number) override {
    fprintf(stderr, "NOT IMPLEMENTED\n");
    exit(1);
  }
  void Deactivate() override {
    fprintf(stderr, "NOT IMPLEMENTED\n");
    exit(1);
  }
  Color8::Byteorder get_byteorder() const {
    return Color8::RGB;
  }
  int GetBladeNumber() const override { return 1; }
  void SetStyle(BladeStyle* style) override {
    current_style_ = style;
    if (current_style_) {
      current_style_->activate();
    }
  }
  BladeStyle* UnSetStyle() override {
    BladeStyle *ret = current_style_;
    if (ret) {
      ret->deactivate();
    }
    current_style_ = nullptr;
    return ret;
  }
  BladeStyle* current_style() const override {
    return current_style_;
  }
protected:
  BladeStyle *current_style_ = nullptr;
};

// ============================================================
// Tokenizer Tests
// ============================================================

void test_tokenizer_simple_ident() {
  // Test: "Red" -> TOK_IDENT("Red"), TOK_EOF
  Tokenizer tok("Red");
  tok.next();
  CHECK_EQ(tok.current(), TOK_IDENT);
  CHECK_STREQ(tok.identifier(), "Red");
  tok.next();
  CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_simple_ident PASSED\n");
}

void test_tokenizer_integer() {
  // Test: "300" -> TOK_INT(300), TOK_EOF
  Tokenizer tok("300");
  tok.next();
  CHECK_EQ(tok.current(), TOK_INT);
  CHECK_EQ(tok.intValue(), 300);
  tok.next();
  CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_integer PASSED\n");
}

void test_tokenizer_negative_integer() {
  // Test: "-1" -> TOK_INT(-1)
  Tokenizer tok("-1");
  tok.next();
  CHECK_EQ(tok.current(), TOK_INT);
  CHECK_EQ(tok.intValue(), -1);
  fprintf(stderr, "  test_tokenizer_negative_integer PASSED\n");
}

void test_tokenizer_hex_color() {
  // Test: "#FF0000" -> TOK_HEX(0xFF0000)
  Tokenizer tok("#FF0000");
  tok.next();
  CHECK_EQ(tok.current(), TOK_HEX);
  CHECK_EQ(tok.hexValue(), (uint32_t)0xFF0000);
  fprintf(stderr, "  test_tokenizer_hex_color PASSED\n");
}

void test_tokenizer_delimiters() {
  // Test: "<,>" -> TOK_OPEN, TOK_COMMA, TOK_CLOSE, TOK_EOF
  Tokenizer tok("<,>");
  tok.next(); CHECK_EQ(tok.current(), TOK_OPEN);
  tok.next(); CHECK_EQ(tok.current(), TOK_COMMA);
  tok.next(); CHECK_EQ(tok.current(), TOK_CLOSE);
  tok.next(); CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_delimiters PASSED\n");
}

void test_tokenizer_whitespace() {
  // Test: "  Red < Blue , 300 >" with whitespace
  Tokenizer tok("  Red < Blue , 300 >");
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "Red");
  tok.next(); CHECK_EQ(tok.current(), TOK_OPEN);
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "Blue");
  tok.next(); CHECK_EQ(tok.current(), TOK_COMMA);
  tok.next(); CHECK_EQ(tok.current(), TOK_INT); CHECK_EQ(tok.intValue(), 300);
  tok.next(); CHECK_EQ(tok.current(), TOK_CLOSE);
  tok.next(); CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_whitespace PASSED\n");
}

void test_tokenizer_nested_style() {
  // Test: "InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>"
  Tokenizer tok("InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>");
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "InOutTrL");
  tok.next(); CHECK_EQ(tok.current(), TOK_OPEN);
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "TrWipe");
  tok.next(); CHECK_EQ(tok.current(), TOK_OPEN);
  tok.next(); CHECK_EQ(tok.current(), TOK_INT); CHECK_EQ(tok.intValue(), 300);
  tok.next(); CHECK_EQ(tok.current(), TOK_CLOSE);
  tok.next(); CHECK_EQ(tok.current(), TOK_COMMA);
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "TrWipeIn");
  tok.next(); CHECK_EQ(tok.current(), TOK_OPEN);
  tok.next(); CHECK_EQ(tok.current(), TOK_INT); CHECK_EQ(tok.intValue(), 500);
  tok.next(); CHECK_EQ(tok.current(), TOK_CLOSE);
  tok.next(); CHECK_EQ(tok.current(), TOK_COMMA);
  tok.next(); CHECK_EQ(tok.current(), TOK_IDENT); CHECK_STREQ(tok.identifier(), "Black");
  tok.next(); CHECK_EQ(tok.current(), TOK_CLOSE);
  tok.next(); CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_nested_style PASSED\n");
}

void test_tokenizer_scope_operator() {
  // Test: "SaberBase::LOCKUP_NORMAL"
  // The tokenizer treats the full scoped name as a single TOK_IDENT
  // with identifier() == "SaberBase::LOCKUP_NORMAL".
  // (It does NOT produce a separate TOK_SCOPE token.)
  Tokenizer tok("SaberBase::LOCKUP_NORMAL");
  tok.next();
  CHECK_EQ(tok.current(), TOK_IDENT);
  CHECK_STREQ(tok.identifier(), "SaberBase::LOCKUP_NORMAL");
  tok.next();
  CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_scope_operator PASSED\n");
}

void test_tokenizer_empty_input() {
  // Test: "" -> TOK_EOF
  Tokenizer tok("");
  tok.next();
  CHECK_EQ(tok.current(), TOK_EOF);
  fprintf(stderr, "  test_tokenizer_empty_input PASSED\n");
}

void test_tokenizer_error_input() {
  // Test: "@" (invalid char) -> TOK_ERROR
  Tokenizer tok("@");
  tok.next();
  CHECK_EQ(tok.current(), TOK_ERROR);
  fprintf(stderr, "  test_tokenizer_error_input PASSED\n");
}

// ============================================================
// Basic Parse Integration Smoke Tests
// ============================================================

void test_parse_simple_color() {
  // Parse "Black" -> should produce a valid RtColorNode
  Tokenizer tok("Black");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_simple_color PASSED\n");
}

void test_parse_simple_style() {
  // Parse "InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>"
  // Should produce a valid RtColorNode tree
  Tokenizer tok("InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_simple_style PASSED\n");
}

void test_parse_unknown_type() {
  // Parse "UnknownFoo<>" -> should return nullptr (unknown type)
  Tokenizer tok("UnknownFoo<>");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);
  fprintf(stderr, "  test_parse_unknown_type PASSED\n");
}

void test_parse_depth_limit() {
  // Build a deeply nested string > MAX_PARSE_DEPTH levels
  // "Layers<Layers<Layers<...Black...>>>" with 35 levels
  char buf[4096];
  int pos = 0;
  for (int i = 0; i < 35; i++) pos += sprintf(buf + pos, "Layers<");
  pos += sprintf(buf + pos, "Black");
  for (int i = 0; i < 35; i++) pos += sprintf(buf + pos, ">");
  Tokenizer tok(buf);
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);  // Should fail due to depth limit
  fprintf(stderr, "  test_parse_depth_limit PASSED\n");
}

// ============================================================
// File-Based Parse Tests (StyleFromSD path)
// ============================================================

void test_style_from_sd_file() {
  // Write a .style file to disk
  FILE* f = fopen("test_basic.style", "w");
  CHECK(f != nullptr);
  fprintf(f, "InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>");
  fclose(f);

  // Create factory and call make()
  StyleAllocator factory = StyleFromSD("test_basic.style");
  CHECK(factory != nullptr);
  BladeStyle* style = factory->make();
  CHECK(style != nullptr);

  // Run on MockBlade
  MockBlade mock_blade;
  mock_blade.colors.resize(144);
  on_ = true;
  micros_ = 1000000;  // 1 second — past ignition
  style->run(&mock_blade);
  // Style should set some colors (not all zero)
  // Just verify it doesn't crash — detailed color checks in Plan 04

  delete style;
  delete factory;
  unlink("test_basic.style");
  fprintf(stderr, "  test_style_from_sd_file PASSED\n");
}

void test_style_from_sd_missing_file() {
  // File that doesn't exist
  StyleAllocator factory = StyleFromSD("nonexistent_xyz.style");
  CHECK(factory != nullptr);  // Factory itself is always created
  BladeStyle* style = factory->make();
  CHECK(style == nullptr);    // make() returns nullptr for missing file
  delete factory;
  fprintf(stderr, "  test_style_from_sd_missing_file PASSED\n");
}

// ============================================================
// Plan 04: Comprehensive Factory Builder Tests
// ============================================================

// Helper: write style to temp file, parse, return BladeStyle
// Returns nullptr on parse failure. Caller must delete result.
static BladeStyle* parseStyleFromFile(const char* path, const char* content) {
  FILE* f = fopen(path, "w");
  if (!f) return nullptr;
  fprintf(f, "%s", content);
  fclose(f);
  StyleAllocator factory = StyleFromSD(path);
  if (!factory) { unlink(path); return nullptr; }
  BladeStyle* bs = factory->make();
  delete factory;
  unlink(path);
  return bs;
}

// Helper: parse color node inline from string
static RtColorNode* parseInline(const char* s) {
  Tokenizer tok(s);
  tok.next();
  return parseColorNode(tok, 0);
}

// ============================================================
// Color Type Tests
// ============================================================

void test_parse_rgb() {
  RtColorNode* node = parseInline("Rgb<255,0,0>");
  CHECK(node != nullptr);
  MockBlade mb;
  mb.colors.resize(1);
  on_ = true;
  node->run(&mb);
  RGBA_um c = node->getColor(0);
  CHECK(c.c.r > 0);
  CHECK_EQ(c.c.g, 0);
  CHECK_EQ(c.c.b, 0);
  delete node;
  fprintf(stderr, "  test_parse_rgb PASSED\n");
}

void test_parse_hex_color_blue() {
  // Use Rgb<0,0,255> as a stand-in for blue hex color test
  RtColorNode* node = parseInline("Blue");
  CHECK(node != nullptr);
  MockBlade mb;
  mb.colors.resize(1);
  node->run(&mb);
  RGBA_um c = node->getColor(0);
  CHECK_EQ(c.c.r, 0);
  CHECK(c.c.b > 0);
  delete node;
  fprintf(stderr, "  test_parse_hex_color_blue PASSED\n");
}

void test_parse_hex_color_literal() {
  // Test: #FF0000 parses to a color node that returns red
  RtColorNode* node = parseInline("#FF0000");
  CHECK(node != nullptr);
  MockBlade mb;
  mb.colors.resize(1);
  node->run(&mb);
  RGBA_um c = node->getColor(0);
  // Color8(255,0,0) -> Color16 scales to 16-bit: r should be ~65535, g=0, b=0
  CHECK(c.c.r > 60000);
  CHECK_EQ(c.c.g, 0);
  CHECK_EQ(c.c.b, 0);
  delete node;

  // Test: #00FF00 parses to green
  node = parseInline("#00FF00");
  CHECK(node != nullptr);
  node->run(&mb);
  c = node->getColor(0);
  CHECK_EQ(c.c.r, 0);
  CHECK(c.c.g > 60000);
  CHECK_EQ(c.c.b, 0);
  delete node;

  // Test: #0000FF parses to blue
  node = parseInline("#0000FF");
  CHECK(node != nullptr);
  node->run(&mb);
  c = node->getColor(0);
  CHECK_EQ(c.c.r, 0);
  CHECK_EQ(c.c.g, 0);
  CHECK(c.c.b > 60000);
  delete node;

  fprintf(stderr, "  test_parse_hex_color_literal PASSED\n");
}

void test_parse_named_colors() {
  const char* colors[] = {
    "Red", "Blue", "Green", "Black", "White", "Yellow",
    "Orange", "DarkOrange", "OrangeRed", "DeepPink",
    "LemonChiffon", "NavajoWhite", "Ivory",
    "HotPink", "LightPink", "DodgerBlue", "DeepSkyBlue",
    nullptr
  };
  for (int i = 0; colors[i]; i++) {
    RtColorNode* node = parseInline(colors[i]);
    if (!node) {
      fprintf(stderr, "  FAIL: named color '%s' returned nullptr\n", colors[i]);
      exit(1);
    }
    delete node;
  }
  fprintf(stderr, "  test_parse_named_colors PASSED\n");
}

void test_parse_rgb16() {
  RtColorNode* node = parseInline("Rgb16<65535,0,0>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_rgb16 PASSED\n");
}

// ============================================================
// Layer/Effect Type Tests
// ============================================================

void test_parse_layers() {
  RtColorNode* node = parseInline("Layers<Red,AlphaL<Blue,Int<16384>>>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_layers PASSED\n");
}

void test_parse_audio_flicker() {
  RtColorNode* node = parseInline("AudioFlicker<Red,Blue>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_audio_flicker PASSED\n");
}

void test_parse_hump_flicker() {
  RtColorNode* node = parseInline("HumpFlicker<Red,White,40>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_hump_flicker PASSED\n");
}

void test_parse_lockup_trl() {
  // Representative LockupTrL pattern from torro configs
  RtColorNode* node = parseInline(
    "LockupTrL<AlphaL<White,Int<32768>>,TrFade<200>,TrFade<400>,SaberBase::LOCKUP_NORMAL>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_lockup_trl PASSED\n");
}

void test_parse_blinking_l() {
  // BlinkingL<COLOR, MILLIS_FUNC, PROMILLE_FUNC>
  RtColorNode* node = parseInline("BlinkingL<Blue,Int<300>,Int<500>>");
  CHECK(node != nullptr);
  MockBlade mb;
  mb.colors.resize(1);
  on_ = true;
  node->run(&mb);
  delete node;
  fprintf(stderr, "  test_parse_blinking_l PASSED\n");
}

// ============================================================
// Transition Type Tests
// ============================================================

void test_parse_tr_concat() {
  RtColorNode* node = parseInline(
    "InOutTrL<TrConcat<TrFade<100>,Red,TrFade<200>>,TrWipeIn<500>,Black>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_tr_concat PASSED\n");
}

void test_parse_tr_wipe_spark_tip() {
  RtColorNode* node = parseInline(
    "InOutTrL<TrWipeSparkTip<White,300>,TrWipeIn<500>,Black>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_tr_wipe_spark_tip PASSED\n");
}

// ============================================================
// Function Node Tests
// ============================================================

void test_parse_scale() {
  RtColorNode* node = parseInline(
    "AlphaL<White,Scale<Int<100>,Int<0>,Int<32768>>>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_scale PASSED\n");
}

void test_parse_trigger() {
  RtColorNode* node = parseInline(
    "AlphaL<Red,Trigger<EFFECT_IGNITION,Int<0>,Int<1000>,Int<500>>>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_trigger PASSED\n");
}

void test_parse_rotate_colors_variation() {
  // RotateColorsX<Variation, Color> — used extensively in torro configs
  RtColorNode* node = parseInline("RotateColorsX<Variation,Red>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_rotate_colors_variation PASSED\n");
}

void test_parse_rgbarg() {
  // RgbArg<ARG_CONSTANT, DEFAULT_COLOR>
  RtColorNode* node = parseInline("RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_rgbarg PASSED\n");
}

void test_parse_intarg() {
  // IntArg<ARG_CONSTANT, DEFAULT_INT> used as function node
  RtColorNode* node = parseInline(
    "AlphaL<White,Scale<IntArg<LOCKUP_POSITION_ARG,16000>,Int<0>,Int<32768>>>");
  CHECK(node != nullptr);
  delete node;
  fprintf(stderr, "  test_parse_intarg PASSED\n");
}

// ============================================================
// Torro Config Style Tests — all 7 presets
// ============================================================

// Helper macro for torro-style tests: write style to file, parse, run on MockBlade
#define TORRO_STYLE_TEST(name, style_str)                            \
void test_parse_torro_##name() {                                     \
  BladeStyle* bs = parseStyleFromFile("test_" #name ".style",       \
                                      style_str);                    \
  CHECK(bs != nullptr);                                              \
  MockBlade mb;                                                      \
  mb.colors.resize(144);                                             \
  on_ = true;                                                        \
  micros_ = 1000000;                                                 \
  mb.SetStyle(bs);                                                   \
  bs->run(&mb);                                                      \
  mb.UnSetStyle();                                                   \
  delete bs;                                                         \
  fprintf(stderr, "  test_parse_torro_" #name " PASSED\n");        \
}

// Calkestis (blade 1 from torro_config.h) — Multi-phase color select with
// StripesX, HoldPeakF, EffectPulseF, LockupTrL, ResponsiveLightningBlockL,
// TransitionEffectL, TrConcat, TrWipe, InOutTrL, SyncAltToVarianceL
TORRO_STYLE_TEST(calkestis,
  "Layers<Black,"
  "ColorSelect<AltF,TrInstant,Rgb<0,42,255>,Rgb<7,255,5>,Rgb<174,0,255>>,"
  "TransitionEffectL<TrConcat<TrJoin<TrDelayX<WavLen<>>,TrWipeIn<200>>,AlphaL<RandomPerLEDFlickerL<Rgb<255,68,0>>,SmoothStep<Int<28000>,Int<2000>>>,TrWipe<200>>,EFFECT_STAB>,"
  "TransitionEffectL<TrWaveX<Rgb<255,255,255>,Int<400>,Int<100>,Int<400>,Int<28000>>,EFFECT_BLAST>,"
  "LockupTrL<AlphaL<AudioFlicker<Rgb<255,255,255>,Mix<Int<12000>,Black,Rgb<255,255,255>>>,Int<32768>>,TrConcat<TrInstant,Rgb<255,255,255>,TrFade<200>>,TrConcat<TrInstant,Rgb<255,255,255>,TrFade<400>>,SaberBase::LOCKUP_NORMAL,Int<1>>,"
  "ResponsiveLightningBlockL<Strobe<Rgb<255,255,255>,AudioFlicker<Rgb<255,255,255>,Blue>,50,1>,TrConcat<TrInstant,AlphaL<Rgb<255,255,255>,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,Rgb<255,255,255>,TrFade<400>>,Int<1>>,"
  "LockupTrL<AlphaL<BrownNoiseFlickerL<Rgb<255,255,255>,Int<300>>,SmoothStep<Int<28000>,Int<3000>>>,TrWipeIn<200>,TrWipe<200>,SaberBase::LOCKUP_DRAG,Int<1>>,"
  "LockupTrL<AlphaL<Stripes<2000,4000,Mix<TwistAngle<>,Rgb<255,68,0>,RotateColorsX<Int<3000>,Rgb<255,68,0>>>,Mix<Sin<Int<50>>,Black,Rgb<255,68,0>>,Mix<Int<4096>,Black,Rgb<255,68,0>>>,SmoothStep<Scale<TwistAngle<>,Int<28000>,Int<30000>>,Int<3000>>>,TrConcat<TrExtend<4000,TrWipeIn<200>>,AlphaL<HumpFlicker<Rgb<255,68,0>,RotateColorsX<Int<3000>,Rgb<255,68,0>>,100>,SmoothStep<Scale<TwistAngle<>,Int<28000>,Int<30000>>,Int<3000>>>,TrFade<4000>>,TrWipe<200>,SaberBase::LOCKUP_MELT,Int<1>>,"
  "SyncAltToVarianceL,"
  "InOutTrL<TrWipeSparkTip<Rgb<255,255,255>,300>,TrWipeInSparkTip<Rgb<255,255,255>,300>,Black>,"
  "EffectSequence<EFFECT_POWERSAVE,AlphaL<Black,Int<8192>>,AlphaL<Black,Int<16384>>,AlphaL<Black,Int<24576>>,AlphaL<Black,Int<0>>>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrFade<300>>,EFFECT_BATTERY_LEVEL>>"
)

// Chimera — deepest nesting, 21+ levels, BrownNoiseFlicker, Rgb16,
// RotateColorsX<Variation>, HoldPeakF<SwingSpeed>, ResponsiveLightningBlockL,
// EffectSequence<EFFECT_BLAST>, TransitionEffect, InOutTrL, TransitionEffectL<EFFECT_PREON>
TORRO_STYLE_TEST(chimera,
  "Layers<"
  "Mix<SmoothStep<Scale<HoldPeakF<SwingSpeed<1150>,Int<750>,Int<17500>>,HoldPeakF<IsGreaterThan<SwingSpeed<1150>,Int<30000>>,Int<31000>,Int<8000>>,Int<32768>>,Int<-15000>>,HumpFlicker<RotateColorsX<Variation,Rgb<135,35,210>>,RotateColorsX<Variation,Rgb<57,20,125>>,35>,BrownNoiseFlicker<Red,Rgb16<18927,0,0>,50>>,"
  "AlphaL<BrownNoiseFlicker<Red,Rgb16<18927,0,0>,50>,SmoothStep<Scale<SwingSpeed<7000>,HoldPeakF<IsGreaterThan<SwingSpeed<1150>,Int<30000>>,Int<30000>,Int<8000>>,Int<32768>>,Int<-10>>>,"
  "LockupTrL<AlphaMixL<Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>,BrownNoiseFlickerL<RgbArg<LOCKUP_COLOR_ARG,White>,Int<200>>,StripesX<Int<1800>,Scale<NoisySoundLevel,Int<-3500>,Int<-5000>>,Mix<Int<6425>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>,RgbArg<LOCKUP_COLOR_ARG,White>,Mix<Int<12850>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>>>,TrConcat<TrExtend<50,TrInstant>,Mix<IsLessThan<ClashImpactF<>,Int<26000>>,RgbArg<LOCKUP_COLOR_ARG,White>,AlphaL<RgbArg<LOCKUP_COLOR_ARG,White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<ClashImpactF<>,Int<20000>,Int<60000>>>>>,TrExtend<3000,TrFade<300>>,AlphaL<AudioFlicker<RgbArg<LOCKUP_COLOR_ARG,White>,Mix<Int<10280>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Int<13000>>>,TrFade<3000>>,TrConcat<TrInstant,RgbArg<LOCKUP_COLOR_ARG,White>,TrFadeX<Percentage<WavLen<EFFECT_LOCKUP_END>,33>>>,SaberBase::LOCKUP_NORMAL>,"
  "ResponsiveLightningBlockL<Strobe<RgbArg<LB_COLOR_ARG,White>,AudioFlicker<RgbArg<LB_COLOR_ARG,White>,Blue>,50,1>,TrConcat<TrInstant,AlphaL<RgbArg<LB_COLOR_ARG,White>,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<RgbArg<LB_COLOR_ARG,White>,Int<16000>>,30>,TrSmoothFade<600>>>,"
  "ResponsiveStabL<AudioFlickerL<RgbArg<STAB_COLOR_ARG,Yellow>>,TrWipeInX<Percentage<WavLen<EFFECT_STAB>,50>>,TrFadeX<Percentage<WavLen<EFFECT_STAB>,50>>>,"
  "EffectSequence<EFFECT_BLAST,ResponsiveBlastL<RgbArg<BLAST_COLOR_ARG,White>,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,LocalizedClashL<RgbArg<BLAST_COLOR_ARG,White>,80,30,EFFECT_BLAST>,ResponsiveBlastWaveL<RgbArg<BLAST_COLOR_ARG,White>,Scale<SwingSpeed<400>,Int<500>,Int<200>>,Scale<SwingSpeed<400>,Int<100>,Int<400>>>>,"
  "Mix<IsLessThan<ClashImpactF<>,Int<26000>>,TransitionEffectL<TrConcat<TrInstant,AlphaL<RgbArg<CLASH_COLOR_ARG,White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<ClashImpactF<>,Int<12000>,Int<60000>>>>,TrFadeX<Scale<ClashImpactF<>,Int<200>,Int<400>>>>,EFFECT_CLASH>,TransitionEffectL<TrWaveX<RgbArg<CLASH_COLOR_ARG,White>,Scale<ClashImpactF<>,Int<100>,Int<400>>,Int<100>,Scale<ClashImpactF<>,Int<100>,Int<400>>,Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>>,EFFECT_CLASH>>,"
  "LockupTrL<AlphaL<TransitionEffect<RandomPerLEDFlickerL<RgbArg<DRAG_COLOR_ARG,White>>,BrownNoiseFlickerL<RgbArg<DRAG_COLOR_ARG,White>,Int<300>>,TrExtend<4000,TrInstant>,TrFade<4000>,EFFECT_DRAG_BEGIN>,SmoothStep<Scale<TwistAngle<>,IntArg<DRAG_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrWipeIn<200>,TrWipe<200>,SaberBase::LOCKUP_DRAG,Int<1>>,"
  "LockupTrL<AlphaL<Stripes<2000,4000,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>,Mix<Sin<Int<50>>,Black,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>,Mix<Int<4096>,Black,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>>,SmoothStep<Scale<TwistAngle<>,IntArg<MELT_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrConcat<TrExtend<4000,TrWipeIn<200>>,AlphaL<HumpFlicker<Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>,RotateColorsX<Int<3000>,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>,100>,SmoothStep<Scale<TwistAngle<>,IntArg<MELT_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrFade<4000>>,TrWipe<200>,SaberBase::LOCKUP_MELT,Int<1>>,"
  "InOutTrL<TrWipeSparkTip<White,300>,TrWipeInSparkTip<White,300>,Black>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<BrownNoiseFlicker<Black,RotateColorsX<Variation,Rgb16<65535,58942,40982>>,150>,SmoothStep<Scale<NoisySoundLevel,Int<300>,Int<1700>>,Int<-11000>>>,TrDelayX<WavLen<EFFECT_PREON>>>,EFFECT_PREON>>"
)

// Kyber Radiance — Stripes, RotateColorsX<Variation>, TransitionLoopL, BlinkingL,
// AudioFlickerL, SwingSpeed, TrBoing, LockupTrL<Layers<...>>, InOutTrL<TrWipeSparkTip>
TORRO_STYLE_TEST(kyberradiance,
  "Layers<"
  "Stripes<2000,-2500,RotateColorsX<Variation,Red>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb<60,0,0>>,Black>,BrownNoiseFlicker<RotateColorsX<Variation,Red>,RotateColorsX<Variation,Rgb<30,0,0>>,200>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb<80,0,0>>,RotateColorsX<Variation,Rgb<30,0,0>>>>,"
  "TransitionLoopL<TrConcat<TrWaveX<RandomFlicker<RotateColorsX<Variation,Red>,BrownNoiseFlicker<RotateColorsX<Variation,Rgb<80,0,0>>,Black,300>>,Int<400>,Int<100>,Int<200>,Int<0>>,AlphaL<Red,Int<0>>,TrDelayX<Scale<SlowNoise<Int<1500>>,Int<200>,Int<1200>>>>>,"
  "TransitionEffectL<TrConcat<TrFade<400>,Mix<SwingSpeed<400>,AudioFlickerL<Rgb<150,0,0>>,Red>,TrDelay<10000>,Mix<SwingSpeed<400>,AudioFlickerL<Rgb<150,0,0>>,Red>,TrFade<800>>,EFFECT_FORCE>,"
  "LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,"
  "ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,"
  "ResponsiveStabL<Orange>,"
  "LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,"
  "LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,"
  "InOutTrL<TrWipeSparkTip<White,300>,TrWipeInSparkTip<White,1000>>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<BlinkingL<Blue,Int<300>,Int<500>>,Bump<Int<0>,Int<4000>>>,TrBoing<200,3>,AlphaL<BlinkingL<DodgerBlue,Int<200>,Int<500>>,Bump<Int<0>,Int<4000>>>,TrBoing<100,3>,AlphaL<BlinkingL<DeepSkyBlue,Int<100>,Int<500>>,Bump<Int<0>,Int<10000>>>,TrDelay<100>>,EFFECT_PREON>>"
)

// Mercenary — StripesX with nested StripesX, StyleFire, RotateColorsX<Variation,Yellow>,
// LockupTrL Responsive Intensity, ResponsiveLightningBlockL, EFFECT_ALT_SOUND
TORRO_STYLE_TEST(mercenary,
  "Layers<"
  "StripesX<Sin<Int<12>,Int<3000>,Int<7000>>,Scale<SwingSpeed<100>,Int<75>,Int<125>>,StripesX<Sin<Int<10>,Int<1000>,Int<3000>>,Scale<SwingSpeed<100>,Int<75>,Int<100>>,Pulsing<RotateColorsX<Variation,Yellow>,RotateColorsX<Variation,Rgb<15,14,0>>,1200>,Mix<SwingSpeed<200>,RotateColorsX<Variation,Rgb<90,87,0>>,Black>>,RotateColorsX<Variation,Rgb<40,40,0>>,Pulsing<RotateColorsX<Variation,Rgb<36,33,0>>,StripesX<Sin<Int<10>,Int<2000>,Int<3000>>,Sin<Int<10>,Int<75>,Int<100>>,RotateColorsX<Variation,Yellow>,RotateColorsX<Variation,Rgb<60,58,0>>>,2000>,Pulsing<RotateColorsX<Variation,Rgb<90,88,0>>,RotateColorsX<Variation,Rgb<5,5,0>>,3000>>,"
  "AlphaL<StyleFire<RotateColorsX<Variation,Yellow>,RotateColorsX<Variation,Rgb<2,2,0>>,0,1,FireConfig<10,2000,2>,FireConfig<10,2000,2>,FireConfig<10,2000,2>,FireConfig<0,0,25>>,Int<10000>>,"
  "AlphaL<Stripes<2500,-3000,RotateColorsX<Variation,Yellow>,RotateColorsX<Variation,Rgb<44,42,0>>,Pulsing<RotateColorsX<Variation,Rgb<22,20,0>>,Black,800>>,SwingSpeed<375>>,"
  "LockupTrL<AlphaMixL<Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>,BrownNoiseFlickerL<RgbArg<LOCKUP_COLOR_ARG,White>,Int<200>>,StripesX<Int<1800>,Scale<NoisySoundLevel,Int<-3500>,Int<-5000>>,Mix<Int<6425>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>,RgbArg<LOCKUP_COLOR_ARG,White>,Mix<Int<12850>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>>>,TrConcat<TrInstant,RgbArg<LOCKUP_COLOR_ARG,White>,TrFade<400>>,TrConcat<TrInstant,AlphaL<RgbArg<LOCKUP_COLOR_ARG,White>,Int<0>>,TrWaveX<RgbArg<LOCKUP_COLOR_ARG,White>,Int<300>,Int<100>,Int<400>,Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>>,SaberBase::LOCKUP_NORMAL>,"
  "ResponsiveLightningBlockL<Strobe<RgbArg<LB_COLOR_ARG,White>,AudioFlicker<RgbArg<LB_COLOR_ARG,White>,Blue>,50,1>,TrConcat<TrInstant,AlphaL<RgbArg<LB_COLOR_ARG,White>,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<RgbArg<LB_COLOR_ARG,White>,Int<16000>>,30>,TrSmoothFade<600>>>,"
  "EffectSequence<EFFECT_POWERSAVE,AlphaL<Black,Int<8192>>,AlphaL<Black,Int<16384>>,AlphaL<Black,Int<24576>>,AlphaL<Black,Int<0>>>,"
  "InOutTrL<TrJoin<TrWipeX<Percentage<WavLen<EFFECT_IGNITION>,8>>,TrWaveX<White,Percentage<WavLen<EFFECT_IGNITION>,25>,Int<300>,Percentage<WavLen<EFFECT_IGNITION>,8>,Int<0>>>,TrColorCycle<950,7500>>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrDelay<2000>,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrFade<1000>>,EFFECT_BATTERY_LEVEL>>"
)

// Hati — AudioFlicker<Stripes<...>>, RgbArg<BASE_COLOR_ARG>, full lockup chain,
// BendTimePowInvX, BendTimePowX, IntArg<IGNITION_OPTION2_ARG>
TORRO_STYLE_TEST(hati,
  "Layers<"
  "AudioFlicker<Stripes<25000,-1400,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>,Mix<Int<12600>,Black,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>>,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>,Mix<Int<18600>,Black,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>>>,RgbArg<BASE_COLOR_ARG,Rgb<255,14,0>>>,"
  "TransitionEffectL<TrWaveX<RgbArg<BLAST_COLOR_ARG,Rgb<255,255,255>>,Scale<EffectRandomF<EFFECT_BLAST>,Int<100>,Int<400>>,Int<100>,Scale<EffectPosition<EFFECT_BLAST>,Int<100>,Int<400>>,Scale<EffectPosition<EFFECT_BLAST>,Int<28000>,Int<8000>>>,EFFECT_BLAST>,"
  "LockupTrL<TransitionEffect<AlphaL<AlphaMixL<Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<22000>>>,AudioFlicker<RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,Mix<Int<12000>,Black,RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>>>,BrownNoiseFlicker<RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,Mix<Int<12000>,Black,RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>>,300>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<22000>>>>,AlphaL<AudioFlicker<RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,Mix<Int<20000>,Black,RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,TrExtend<5000,TrInstant>,TrFade<5000>,EFFECT_LOCKUP_BEGIN>,TrConcat<TrJoin<TrDelay<50>,TrInstant>,Mix<IsLessThan<ClashImpactF<>,Int<26000>>,RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,AlphaL<RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<ClashImpactF<>,Int<20000>,Int<60000>>>>>,TrFade<300>>,TrConcat<TrInstant,RgbArg<LOCKUP_COLOR_ARG,Rgb<255,255,255>>,TrFade<400>>,SaberBase::LOCKUP_NORMAL,Int<1>>,"
  "ResponsiveLightningBlockL<Strobe<RgbArg<LB_COLOR_ARG,Rgb<255,255,255>>,AudioFlicker<RgbArg<LB_COLOR_ARG,Rgb<255,255,255>>,Blue>,50,1>,TrConcat<TrExtend<200,TrInstant>,AlphaL<RgbArg<LB_COLOR_ARG,Rgb<255,255,255>>,Bump<Scale<BladeAngle<>,Int<10000>,Int<21000>>,Int<10000>>>,TrFade<200>>,TrConcat<TrInstant,RgbArg<LB_COLOR_ARG,Rgb<255,255,255>>,TrFade<400>>,Int<1>>,"
  "LockupTrL<AlphaL<TransitionEffect<RandomPerLEDFlickerL<RgbArg<DRAG_COLOR_ARG,Rgb<255,255,255>>>,BrownNoiseFlickerL<RgbArg<DRAG_COLOR_ARG,Rgb<255,255,255>>,Int<300>>,TrExtend<4000,TrInstant>,TrFade<4000>,EFFECT_DRAG_BEGIN>,SmoothStep<Scale<TwistAngle<>,IntArg<DRAG_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrWipeIn<200>,TrWipe<200>,SaberBase::LOCKUP_DRAG,Int<1>>,"
  "InOutTrL<TrWipeX<BendTimePowInvX<IgnitionTime<300>,Mult<IntArg<IGNITION_OPTION2_ARG,10992>,Int<98304>>>>,TrWipeInX<BendTimePowX<RetractionTime<0>,Mult<IntArg<RETRACTION_OPTION2_ARG,10992>,Int<98304>>>>,Black>>"
)

// Crispity — StyleFire with Rgb16, TransitionEffectL<EFFECT_FORCE>,
// ResponsiveClashL, LockupTrL<Layers<LemonChiffon>>, TrColorCycle,
// EFFECT_POWERSAVE, EFFECT_BATTERY_LEVEL, EFFECT_PREON
TORRO_STYLE_TEST(crispity,
  "Layers<"
  "StyleFire<BrownNoiseFlicker<RotateColorsX<Variation,Red>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb16<11805,0,1587>>,RotateColorsX<Variation,Rgb<60,0,0>>>,300>,RotateColorsX<Variation,Rgb<80,0,0>>,0,6,FireConfig<10,1000,2>,FireConfig<10,1000,2>,FireConfig<10,1000,2>,FireConfig<10,1000,2>>,"
  "TransitionEffectL<TrConcat<TrFade<200>,AlphaL<RotateColorsX<Variation,DeepPink>,SwingSpeed<400>>,TrDelay<30000>,AlphaL<RotateColorsX<Variation,DeepPink>,SwingSpeed<400>>,TrFade<800>>,EFFECT_FORCE>,"
  "AlphaL<AudioFlickerL<RotateColorsX<Variation,DeepPink>>,SwingSpeed<400>>,"
  "LockupTrL<Layers<AlphaL<AudioFlickerL<Rgb<255,225,0>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<NavajoWhite,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,"
  "ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,"
  "ResponsiveStabL<Orange>,"
  "ResponsiveClashL<TransitionEffect<Rgb<255,240,80>,LemonChiffon,TrInstant,TrFade<100>,EFFECT_CLASH>,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,"
  "LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,"
  "LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,"
  "EffectSequence<EFFECT_POWERSAVE,AlphaL<Black,Int<8192>>,AlphaL<Black,Int<16384>>,AlphaL<Black,Int<24576>>,AlphaL<Black,Int<0>>>,"
  "InOutTrL<TrWipeSparkTip<White,100>,TrColorCycle<1065>>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrDelay<2000>,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrFade<1000>>,EFFECT_BATTERY_LEVEL>>"
)

// Assassin — HumpFlicker<RotateColorsX<Variation,Green>>, EffectSequence<EFFECT_BLAST>,
// ResponsiveClashL, Rgb16<> colors, EFFECT_POWERSAVE, EFFECT_BATTERY_LEVEL
TORRO_STYLE_TEST(assassin,
  "Layers<"
  "HumpFlicker<RotateColorsX<Variation,Green>,RotateColorsX<Variation,Rgb<0,128,0>>,50>,"
  "AlphaL<Stripes<2500,-2750,RotateColorsX<Variation,Green>,RotateColorsX<Variation,Rgb<25,60,0>>,Pulsing<RotateColorsX<Variation,Rgb<0,30,0>>,Black,800>>,SwingSpeed<375>>,"
  "LockupTrL<Layers<AlphaL<AudioFlickerL<Rgb<255,240,80>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<LemonChiffon,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,"
  "ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,"
  "AlphaL<RotateColorsX<Variation,Rgb16<21301,65535,0>>,SmoothStep<Scale<SlowNoise<Int<2500>>,Int<1000>,Int<3000>>,Int<-4000>>>,"
  "ResponsiveStabL<Red>,"
  "EffectSequence<EFFECT_BLAST,TransitionEffectL<TrConcat<TrInstant,AlphaL<White,BlastF<200,200>>,TrFade<300>>,EFFECT_BLAST>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveBlastWaveL<White,Scale<SwingSpeed<400>,Int<500>,Int<200>>,Scale<SwingSpeed<400>,Int<100>,Int<400>>>,ResponsiveBlastFadeL<White,Scale<SwingSpeed<400>,Int<6000>,Int<12000>>,Scale<SwingSpeed<400>,Int<400>,Int<100>>>,ResponsiveBlastL<White,Scale<SwingSpeed<400>,Int<400>,Int<100>>,Scale<SwingSpeed<400>,Int<200>,Int<100>>,Scale<SwingSpeed<400>,Int<400>,Int<200>>>>,"
  "ResponsiveClashL<TransitionEffect<Rgb<255,240,80>,LemonChiffon,TrInstant,TrFade<100>,EFFECT_CLASH>,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,"
  "TransitionEffectL<TrConcat<TrInstant,Stripes<3000,-3500,RotateColorsX<Variation,Rgb16<38402,65535,3934>>,RandomPerLEDFlicker<Rgb<60,60,60>,Black>,BrownNoiseFlicker<RotateColorsX<Variation,Rgb16<38402,65535,3934>>,Rgb<30,30,30>,200>,RandomPerLEDFlicker<Rgb<80,80,80>,Rgb<30,30,30>>>,TrFade<500>>,EFFECT_IGNITION>,"
  "LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,"
  "LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,"
  "EffectSequence<EFFECT_POWERSAVE,AlphaL<Black,Int<8192>>,AlphaL<Black,Int<16384>>,AlphaL<Black,Int<24576>>,AlphaL<Black,Int<0>>>,"
  "InOutTrL<TrWipeSparkTip<White,250>,TrWipeInSparkTip<White,656>>,"
  "TransitionEffectL<TrConcat<TrInstant,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrDelay<2000>,AlphaL<Mix<BatteryLevel,Red,Green>,Bump<BatteryLevel,Int<10000>>>,TrFade<1000>>,EFFECT_BATTERY_LEVEL>>"
)

// ============================================================
// Error Path Tests
// ============================================================

void test_parse_empty_string() {
  Tokenizer tok("");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);
  fprintf(stderr, "  test_parse_empty_string PASSED\n");
}

void test_parse_truncated_style() {
  // Missing closing >
  Tokenizer tok("InOutTrL<TrWipe<300>,TrWipeIn<500>");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);
  fprintf(stderr, "  test_parse_truncated_style PASSED\n");
}

void test_parse_wrong_arg_type() {
  // Int where a color is expected (inside Layers)
  // "Layers<300,Red>" — 300 is not a valid color, should fail
  Tokenizer tok("Layers<300,Red>");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);
  fprintf(stderr, "  test_parse_wrong_arg_type PASSED\n");
}

void test_parse_extra_comma() {
  // Double comma in Rgb<>
  Tokenizer tok("Rgb<255,,0,0>");
  tok.next();
  RtColorNode* node = parseColorNode(tok, 0);
  CHECK(node == nullptr);
  fprintf(stderr, "  test_parse_extra_comma PASSED\n");
}

// ============================================================
// Chimera Layer Isolation Tests
// ============================================================

void test_chimera_layer_isolation() {
  fprintf(stderr, "  [chimera isolation] layer 1 (Mix SmoothStep)...\n");
  {
    RtColorNode* n = parseInline(
      "Mix<SmoothStep<Scale<HoldPeakF<SwingSpeed<1150>,Int<750>,Int<17500>>,HoldPeakF<IsGreaterThan<SwingSpeed<1150>,Int<30000>>,Int<31000>,Int<8000>>,Int<32768>>,Int<-15000>>,HumpFlicker<RotateColorsX<Variation,Rgb<135,35,210>>,RotateColorsX<Variation,Rgb<57,20,125>>,35>,BrownNoiseFlicker<Red,Rgb16<18927,0,0>,50>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 1 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 2 (AlphaL BrownNoise)...\n");
  {
    RtColorNode* n = parseInline(
      "AlphaL<BrownNoiseFlicker<Red,Rgb16<18927,0,0>,50>,SmoothStep<Scale<SwingSpeed<7000>,HoldPeakF<IsGreaterThan<SwingSpeed<1150>,Int<30000>>,Int<30000>,Int<8000>>,Int<32768>>,Int<-10>>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 2 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 3 (LockupTrL NORMAL)...\n");
  {
    RtColorNode* n = parseInline(
      "LockupTrL<AlphaMixL<Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>,BrownNoiseFlickerL<RgbArg<LOCKUP_COLOR_ARG,White>,Int<200>>,StripesX<Int<1800>,Scale<NoisySoundLevel,Int<-3500>,Int<-5000>>,Mix<Int<6425>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>,RgbArg<LOCKUP_COLOR_ARG,White>,Mix<Int<12850>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>>>,TrConcat<TrExtend<50,TrInstant>,Mix<IsLessThan<ClashImpactF<>,Int<26000>>,RgbArg<LOCKUP_COLOR_ARG,White>,AlphaL<RgbArg<LOCKUP_COLOR_ARG,White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<ClashImpactF<>,Int<20000>,Int<60000>>>>>,TrExtend<3000,TrFade<300>>,AlphaL<AudioFlicker<RgbArg<LOCKUP_COLOR_ARG,White>,Mix<Int<10280>,Black,RgbArg<LOCKUP_COLOR_ARG,White>>>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Int<13000>>>,TrFade<3000>>,TrConcat<TrInstant,RgbArg<LOCKUP_COLOR_ARG,White>,TrFadeX<Percentage<WavLen<EFFECT_LOCKUP_END>,33>>>,SaberBase::LOCKUP_NORMAL>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 3 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 4 (ResponsiveLightningBlockL)...\n");
  {
    RtColorNode* n = parseInline(
      "ResponsiveLightningBlockL<Strobe<RgbArg<LB_COLOR_ARG,White>,AudioFlicker<RgbArg<LB_COLOR_ARG,White>,Blue>,50,1>,TrConcat<TrInstant,AlphaL<RgbArg<LB_COLOR_ARG,White>,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<RgbArg<LB_COLOR_ARG,White>,Int<16000>>,30>,TrSmoothFade<600>>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 4 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 5 (ResponsiveStabL)...\n");
  {
    RtColorNode* n = parseInline(
      "ResponsiveStabL<AudioFlickerL<RgbArg<STAB_COLOR_ARG,Yellow>>,TrWipeInX<Percentage<WavLen<EFFECT_STAB>,50>>,TrFadeX<Percentage<WavLen<EFFECT_STAB>,50>>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 5 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 6 (EffectSequence BLAST)...\n");
  {
    RtColorNode* n = parseInline(
      "EffectSequence<EFFECT_BLAST,ResponsiveBlastL<RgbArg<BLAST_COLOR_ARG,White>,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,LocalizedClashL<RgbArg<BLAST_COLOR_ARG,White>,80,30,EFFECT_BLAST>,ResponsiveBlastWaveL<RgbArg<BLAST_COLOR_ARG,White>,Scale<SwingSpeed<400>,Int<500>,Int<200>>,Scale<SwingSpeed<400>,Int<100>,Int<400>>>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 6 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 7 (Mix IsLessThan clash)...\n");
  {
    RtColorNode* n = parseInline(
      "Mix<IsLessThan<ClashImpactF<>,Int<26000>>,TransitionEffectL<TrConcat<TrInstant,AlphaL<RgbArg<CLASH_COLOR_ARG,White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>,Scale<ClashImpactF<>,Int<12000>,Int<60000>>>>,TrFadeX<Scale<ClashImpactF<>,Int<200>,Int<400>>>>,EFFECT_CLASH>,TransitionEffectL<TrWaveX<RgbArg<CLASH_COLOR_ARG,White>,Scale<ClashImpactF<>,Int<100>,Int<400>>,Int<100>,Scale<ClashImpactF<>,Int<100>,Int<400>>,Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-12000>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<10000>>>,Sum<IntArg<LOCKUP_POSITION_ARG,16000>,Int<-10000>>>>,EFFECT_CLASH>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 7 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 8 (LockupTrL DRAG)...\n");
  {
    RtColorNode* n = parseInline(
      "LockupTrL<AlphaL<TransitionEffect<RandomPerLEDFlickerL<RgbArg<DRAG_COLOR_ARG,White>>,BrownNoiseFlickerL<RgbArg<DRAG_COLOR_ARG,White>,Int<300>>,TrExtend<4000,TrInstant>,TrFade<4000>,EFFECT_DRAG_BEGIN>,SmoothStep<Scale<TwistAngle<>,IntArg<DRAG_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrWipeIn<200>,TrWipe<200>,SaberBase::LOCKUP_DRAG,Int<1>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 8 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 9 (LockupTrL MELT)...\n");
  {
    RtColorNode* n = parseInline(
      "LockupTrL<AlphaL<Stripes<2000,4000,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>,Mix<Sin<Int<50>>,Black,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>,Mix<Int<4096>,Black,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>>,SmoothStep<Scale<TwistAngle<>,IntArg<MELT_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrConcat<TrExtend<4000,TrWipeIn<200>>,AlphaL<HumpFlicker<Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>,RotateColorsX<Int<3000>,Mix<TwistAngle<>,RgbArg<STAB_COLOR_ARG,Yellow>,RotateColorsX<Int<3000>,RgbArg<STAB_COLOR_ARG,Yellow>>>>,100>,SmoothStep<Scale<TwistAngle<>,IntArg<MELT_SIZE_ARG,28000>,Int<30000>>,Int<3000>>>,TrFade<4000>>,TrWipe<200>,SaberBase::LOCKUP_MELT,Int<1>>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 9 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 10 (InOutTrL)...\n");
  {
    RtColorNode* n = parseInline(
      "InOutTrL<TrWipeSparkTip<White,300>,TrWipeInSparkTip<White,300>,Black>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 10 PASSED\n");
  }

  fprintf(stderr, "  [chimera isolation] layer 11 (TransitionEffectL PREON)...\n");
  {
    RtColorNode* n = parseInline(
      "TransitionEffectL<TrConcat<TrInstant,AlphaL<BrownNoiseFlicker<Black,RotateColorsX<Variation,Rgb16<65535,58942,40982>>,150>,SmoothStep<Scale<NoisySoundLevel,Int<300>,Int<1700>>,Int<-11000>>>,TrDelayX<WavLen<EFFECT_PREON>>>,EFFECT_PREON>"
    );
    CHECK(n != nullptr);
    delete n;
    fprintf(stderr, "  [chimera isolation] layer 11 PASSED\n");
  }

  fprintf(stderr, "  test_chimera_layer_isolation PASSED\n");
}

// ============================================================
// Main
// ============================================================

int main() {
  fprintf(stderr, "=== SD Style Tokenizer Tests ===\n");
  test_tokenizer_simple_ident();
  test_tokenizer_integer();
  test_tokenizer_negative_integer();
  test_tokenizer_hex_color();
  test_tokenizer_delimiters();
  test_tokenizer_whitespace();
  test_tokenizer_nested_style();
  test_tokenizer_scope_operator();
  test_tokenizer_empty_input();
  test_tokenizer_error_input();

  fprintf(stderr, "\n=== SD Style Parser Basic Tests ===\n");
  test_parse_simple_color();
  test_parse_simple_style();
  test_parse_unknown_type();
  test_parse_depth_limit();

  fprintf(stderr, "\n=== SD Style File Integration Tests ===\n");
  test_style_from_sd_file();
  test_style_from_sd_missing_file();

  fprintf(stderr, "\n=== Color Type Tests ===\n");
  test_parse_rgb();
  test_parse_hex_color_blue();
  test_parse_hex_color_literal();
  test_parse_named_colors();
  test_parse_rgb16();

  fprintf(stderr, "\n=== Layer/Effect Type Tests ===\n");
  test_parse_layers();
  test_parse_audio_flicker();
  test_parse_hump_flicker();
  test_parse_lockup_trl();
  test_parse_blinking_l();

  fprintf(stderr, "\n=== Transition Type Tests ===\n");
  test_parse_tr_concat();
  test_parse_tr_wipe_spark_tip();

  fprintf(stderr, "\n=== Function Node Tests ===\n");
  test_parse_scale();
  test_parse_trigger();
  test_parse_rotate_colors_variation();
  test_parse_rgbarg();
  test_parse_intarg();

  fprintf(stderr, "\n=== Chimera Layer Isolation Tests ===\n");
  test_chimera_layer_isolation();

  fprintf(stderr, "\n=== Torro Config Style Tests ===\n");
  test_parse_torro_calkestis();
  test_parse_torro_chimera();
  test_parse_torro_kyberradiance();
  test_parse_torro_mercenary();
  test_parse_torro_hati();
  test_parse_torro_crispity();
  test_parse_torro_assassin();

  fprintf(stderr, "\n=== Error Path Tests ===\n");
  test_parse_empty_string();
  test_parse_truncated_style();
  test_parse_wrong_arg_type();
  test_parse_extra_comma();

  fprintf(stderr, "\nAll sd_style tests passed!\n");
  return 0;
}
