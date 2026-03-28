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

  fprintf(stderr, "\nAll sd_style tests passed!\n");
  return 0;
}
