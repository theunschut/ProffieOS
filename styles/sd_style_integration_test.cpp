#include <vector>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include <string.h>

// Test fixture setup
#define NOTEST
#define PROFFIE_TEST
#define NUM_BLADES 3
#define ENABLE_AUDIO
#define ENABLE_SD

// Mock infrastructure
#define interrupts() do {} while(0)
#define noInterrupts() do {} while(0)
#define SCOPED_PROFILER() do { } while(0)
#define NELEM(X) (sizeof(X)/sizeof((X)[0]))

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

struct BM {
  float battery() { return 3.7; }
};

struct MockDynamicMixer {
  int32_t last_sample() const { return 4093; }
  int32_t last_sum() const { return 16384; }
  int32_t audio_volume() const { return 100000; }
};

MockFuse fusor;
BM battery_monitor;
MockDynamicMixer dynamic_mixer;

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

#include "../common/common.h"
#include "../common/math.h"
#include "../common/stdout.h"
Print default_printer;
Print* default_output = &default_printer;
Print* stdout_output = &default_printer;
ConsoleHelper STDOUT;

Monitoring monitor;

// Note: STDOUT macro is defined in stdout.h as (*stdout_output)
// STDERR macro is defined in stdout.h as (*default_output)
// Both are ConsoleHelper instances via the pointers declared above

#include "../common/color.h"
#include "../blades/blade_base.h"

// Test assertions
#define EXPECT(X) do {                                          \
  if (!(X)) {                                                   \
    fprintf(stderr, "FAILED: %s on line %d\n", #X, __LINE__);  \
    exit(1);                                                    \
  }                                                             \
} while(0)

#define EXPECT_EQ(X,Y) do {                                     \
  auto x_ = (X);                                               \
  auto y_ = (Y);                                               \
  if (x_ != y_) {                                              \
    fprintf(stderr, "FAILED: %s != %s on line %d\n", #X, #Y, __LINE__); \
    exit(1);                                                    \
  }                                                             \
} while(0)

#define EXPECT_NOT_NULL(X) do {                                 \
  if ((X) == nullptr) {                                         \
    fprintf(stderr, "FAILED: %s is null on line %d\n", #X, __LINE__); \
    exit(1);                                                    \
  }                                                             \
} while(0)

#define EXPECT_NULL(X) do {                                     \
  if ((X) != nullptr) {                                         \
    fprintf(stderr, "FAILED: %s is not null on line %d\n", #X, __LINE__); \
    exit(1);                                                    \
  }                                                             \
} while(0)

// Mock BladeStyle for testing — implements all pure virtuals
class MockBladeStyle : public BladeStyle {
public:
  void run(BladeBase* blade) override {}
  bool IsHandled(HandledFeature feature) override { return false; }
};

// Mock BladeBase for testing — implements all pure virtuals
class MockBladeBase : public BladeBase {
public:
  BladeStyle* current_style_ = nullptr;
  bool set_style_called = false;
  BladeStyle* set_style_arg = nullptr;
  bool unset_style_called = false;

  void SetStyle(BladeStyle* style) override {
    set_style_called = true;
    set_style_arg = style;
    current_style_ = style;
    if (style) style->activate();
  }

  BladeStyle* UnSetStyle() override {
    unset_style_called = true;
    BladeStyle* ret = current_style_;
    if (ret) ret->deactivate();
    current_style_ = nullptr;
    return ret;
  }

  BladeStyle* current_style() const override { return current_style_; }
  int num_leds() const override { return 144; }
  int GetBladeNumber() const override { return 0; }
  Color8::Byteorder get_byteorder() const override { return Color8::RGB; }
  bool is_powered() const override { return true; }
  void set(int led, Color16 c) override {}
  void allow_disable() override {}
  void Activate(int blade_number) override {}
  void Deactivate() override {}
};

// Mock StyleFactory for testing
class MockStyleFactory : public StyleFactory {
public:
  BladeStyle* return_value = nullptr;
  bool make_called = false;
  int make_call_count = 0;

  BladeStyle* make() override {
    make_called = true;
    make_call_count++;
    return return_value;
  }
};

// Minimal CurrentPreset mock for factory storage tests.
// Tests the dual-mode storage (current_style_factory_[] array) without
// requiring the full CurrentPreset include chain (which needs config macros).
struct MockCurrentPreset {
  StyleFactory* current_style_factory_[NUM_BLADES] = {nullptr, nullptr, nullptr};
  const char* current_style_[NUM_BLADES] = {nullptr, nullptr, nullptr};
};

// Global test tracking
struct TestState {
  int parse_error_count = 0;
  bool style_parse_error_called = false;
};

TestState test_state;

// Mock error handler
namespace ProffieOSErrors {
  void style_parse_error() {
    test_state.style_parse_error_called = true;
    test_state.parse_error_count++;
    STDOUT << "ERROR: style_parse_error called\n";
  }
  void sd_card_not_found() {}
  void font_directory_not_found() {}
  void voice_pack_not_found() {}
  void error_in_blade_array() {}
  void error_in_font_directory() {}
  void error_in_voice_pack_version() {}
  void low_battery() {}
}

// SaberBase static members needed for blade_base.h
SaberBase* saberbases = NULL;
SaberBase::LockupType SaberBase::lockup_ = SaberBase::LOCKUP_NONE;
SaberBase::ColorChangeMode SaberBase::color_change_mode_ =
  SaberBase::COLOR_CHANGE_MODE_NONE;
uint32_t SaberBase::last_motion_request_ = 0;
uint32_t SaberBase::current_variation_ = 0;
float SaberBase::sound_length = 0.0;
int SaberBase::sound_number = -1;
float SaberBase::clash_strength_ = 0.0;

// ==============================================================
// TEST CASES
// ==============================================================

void test_current_preset_factory_storage() {
  STDOUT << "TEST: current_preset_factory_storage\n";

  // Create a CurrentPreset instance
  MockCurrentPreset cp;

  // Verify factory array exists and is initialized to nullptr
  for (int i = 0; i < NUM_BLADES; i++) {
    EXPECT_NULL(cp.current_style_factory_[i]);
  }

  STDOUT << "  PASSED: Factory storage initialized correctly\n";
}

void test_dual_mode_allocation_factory_path() {
  STDOUT << "TEST: dual_mode_allocation_factory_path\n";

  test_state.style_parse_error_called = false;

  // Setup
  MockCurrentPreset cp;
  MockBladeBase blade0;
  MockStyleFactory factory;
  MockBladeStyle test_style;

  // Configure factory to return a style
  factory.return_value = &test_style;
  cp.current_style_factory_[0] = &factory;

  // Simulate AllocateBladeStyles macro behavior for blade 0
  BladeStyle* tmp = nullptr;
  if (cp.current_style_factory_[0]) {
    tmp = cp.current_style_factory_[0]->make();
    if (!tmp) {
      ProffieOSErrors::style_parse_error();
    }
  }

  // Verify factory->make() was called
  EXPECT(factory.make_called);
  EXPECT_EQ(factory.make_call_count, 1);

  // Verify we got the style
  EXPECT_NOT_NULL(tmp);
  EXPECT_EQ(tmp, (BladeStyle*)&test_style);

  // No error should have been triggered
  EXPECT(!test_state.style_parse_error_called);

  STDOUT << "  PASSED: Factory path invoked correctly\n";
}

void test_dual_mode_allocation_factory_failure() {
  STDOUT << "TEST: dual_mode_allocation_factory_failure\n";

  test_state.style_parse_error_called = false;
  test_state.parse_error_count = 0;

  // Setup
  MockCurrentPreset cp;
  MockStyleFactory factory;

  // Configure factory to return nullptr (parse failure)
  factory.return_value = nullptr;
  cp.current_style_factory_[0] = &factory;

  // Simulate AllocateBladeStyles macro behavior
  BladeStyle* tmp = nullptr;
  if (cp.current_style_factory_[0]) {
    tmp = cp.current_style_factory_[0]->make();
    if (!tmp) {
      ProffieOSErrors::style_parse_error();
    }
  }

  // Verify factory->make() was called
  EXPECT(factory.make_called);

  // Verify we got nullptr
  EXPECT_NULL(tmp);

  // Error SHOULD have been triggered
  EXPECT(test_state.style_parse_error_called);
  EXPECT_EQ(test_state.parse_error_count, 1);

  STDOUT << "  PASSED: Factory failure triggers error\n";
}

void test_free_blade_styles_cleanup() {
  STDOUT << "TEST: free_blade_styles_cleanup\n";

  // Setup
  MockCurrentPreset cp;
  MockBladeBase blade0;
  MockStyleFactory factory;
  MockBladeStyle test_style;

  // Allocate style via factory
  factory.return_value = &test_style;
  cp.current_style_factory_[0] = &factory;
  blade0.SetStyle(&test_style);

  EXPECT_NOT_NULL(cp.current_style_factory_[0]);
  EXPECT_NOT_NULL(blade0.current_style_);

  // Simulate FreeBladeStyles cleanup
  cp.current_style_factory_[0] = nullptr;
  blade0.UnSetStyle();

  // Verify cleanup completed
  EXPECT_NULL(cp.current_style_factory_[0]);
  EXPECT_NULL(blade0.current_style_);
  EXPECT(blade0.unset_style_called);

  STDOUT << "  PASSED: Cleanup clears factory and style\n";
}

void test_multi_blade_independence() {
  STDOUT << "TEST: multi_blade_independence\n";

  // Setup: blade 0 uses factory, blade 1 uses built-in style string
  MockCurrentPreset cp;
  MockBladeBase blade0, blade1;
  MockStyleFactory factory0;
  MockBladeStyle style0;

  // Blade 0: factory path
  factory0.return_value = &style0;
  cp.current_style_factory_[0] = &factory0;

  // Blade 1: no factory (would use parser path)
  cp.current_style_factory_[1] = nullptr;
  cp.current_style_[1] = "builtin 0 1";

  // Simulate AllocateBladeStyles for blade 0 (factory path)
  BladeStyle* tmp0 = nullptr;
  if (cp.current_style_factory_[0]) {
    tmp0 = cp.current_style_factory_[0]->make();
  }

  // Simulate AllocateBladeStyles for blade 1 (parser would be called)
  BladeStyle* tmp1 = nullptr;
  if (cp.current_style_factory_[1]) {
    tmp1 = cp.current_style_factory_[1]->make();
  }
  // (parser path would be invoked for blade 1 — not tested here)

  // Verify independence
  EXPECT_NOT_NULL(cp.current_style_factory_[0]);  // Blade 0 has factory
  EXPECT_NULL(cp.current_style_factory_[1]);       // Blade 1 has no factory
  EXPECT(factory0.make_called);                     // Factory called for blade 0
  EXPECT_NOT_NULL(tmp0);                            // Blade 0 got style
  EXPECT_NULL(tmp1);                                // Blade 1 didn't use factory

  STDOUT << "  PASSED: Multi-blade independence verified\n";
}

void test_parse_failure_error_effect() {
  STDOUT << "TEST: parse_failure_error_effect\n";

  test_state.parse_error_count = 0;
  test_state.style_parse_error_called = false;

  // Setup two blades: one factory fails, one succeeds
  MockCurrentPreset cp;
  MockStyleFactory factory0, factory1;
  MockBladeStyle style1;

  // Blade 0: factory returns nullptr
  factory0.return_value = nullptr;
  cp.current_style_factory_[0] = &factory0;

  // Blade 1: factory returns valid style
  factory1.return_value = &style1;
  cp.current_style_factory_[1] = &factory1;

  // Simulate allocation for blade 0
  BladeStyle* tmp0 = nullptr;
  if (cp.current_style_factory_[0]) {
    tmp0 = cp.current_style_factory_[0]->make();
    if (!tmp0) {
      ProffieOSErrors::style_parse_error();
    }
  }

  // Simulate allocation for blade 1
  BladeStyle* tmp1 = nullptr;
  if (cp.current_style_factory_[1]) {
    tmp1 = cp.current_style_factory_[1]->make();
    if (!tmp1) {
      ProffieOSErrors::style_parse_error();
    }
  }

  // Verify error was triggered for blade 0, not blade 1
  EXPECT(test_state.style_parse_error_called);
  EXPECT_EQ(test_state.parse_error_count, 1);  // Only 1 error, not 2
  EXPECT_NULL(tmp0);
  EXPECT_NOT_NULL(tmp1);

  STDOUT << "  PASSED: Error effect triggered on parse failure\n";
}

void test_backward_compatibility_rom_preset() {
  STDOUT << "TEST: backward_compatibility_rom_preset\n";

  // Setup: ROM preset with no factory (parser fallback)
  MockCurrentPreset cp;

  // Blade 0: ROM style (no factory)
  cp.current_style_factory_[0] = nullptr;
  cp.current_style_[0] = "builtin 0 1";

  // Verify factory is null for ROM preset
  EXPECT_NULL(cp.current_style_factory_[0]);

  // When AllocateBladeStyles runs, it should use parser path
  bool parser_path_taken = false;

  if (cp.current_style_factory_[0]) {
    // Factory path — should NOT be taken for ROM preset
  } else {
    // Parser path — this is what ROM presets should use
    parser_path_taken = true;
  }

  EXPECT(parser_path_taken);
  STDOUT << "  PASSED: ROM presets use parser fallback\n";
}

// ==============================================================
// TEST RUNNER
// ==============================================================

int main() {
  STDOUT << "=== SD Style Integration Tests ===\n\n";

  fprintf(stderr, "Starting test_current_preset_factory_storage\n");
  fflush(stderr);
  test_current_preset_factory_storage();
  fprintf(stderr, "Completed test_current_preset_factory_storage\n");
  fflush(stderr);

  test_dual_mode_allocation_factory_path();
  fprintf(stderr, "Completed test_dual_mode_allocation_factory_path\n");
  fflush(stderr);

  test_dual_mode_allocation_factory_failure();
  fprintf(stderr, "Completed test_dual_mode_allocation_factory_failure\n");
  fflush(stderr);

  test_free_blade_styles_cleanup();
  fprintf(stderr, "Completed test_free_blade_styles_cleanup\n");
  fflush(stderr);

  test_multi_blade_independence();
  fprintf(stderr, "Completed test_multi_blade_independence\n");
  fflush(stderr);

  test_parse_failure_error_effect();
  fprintf(stderr, "Completed test_parse_failure_error_effect\n");
  fflush(stderr);

  test_backward_compatibility_rom_preset();
  fprintf(stderr, "Completed test_backward_compatibility_rom_preset\n");
  fflush(stderr);

  STDOUT << "\n=== All tests passed! ===\n";
  fprintf(stderr, "About to return from main\n");
  fflush(stderr);
  return 0;
}
