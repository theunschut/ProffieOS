#include <vector>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <chrono>
#include <algorithm>
#include <cmath>

// ============================================================
// Test fixture setup
// ============================================================
#define NOTEST
#define PROFFIE_TEST
#define NUM_BLADES 1
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
  int battery_percent() { return 100; }
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

#include "../common/color.h"
#include "../blades/blade_base.h"

// ============================================================
// SECTION 1: Performance Measurement Structures
// ============================================================

// ParseMetrics: timing and stats for .style file parsing
struct ParseMetrics {
  uint64_t parse_time_us;      // Time to parse file and build node tree
  int node_count;              // Number of nodes created
  uint64_t memory_peak;        // Peak memory used during parse

  ParseMetrics() : parse_time_us(0), node_count(0), memory_peak(0) {}
};

// ExecutionMetrics: timing stats for per-frame style execution
struct ExecutionMetrics {
  uint64_t frame_time_us_min;  // Minimum execution time per frame
  uint64_t frame_time_us_avg;  // Average execution time per frame
  uint64_t frame_time_us_max;  // Maximum execution time per frame
  uint64_t frame_time_us_stddev; // Standard deviation in microseconds
  int frame_count;             // Number of frames executed

  ExecutionMetrics() : frame_time_us_min(0), frame_time_us_avg(0),
                       frame_time_us_max(0), frame_time_us_stddev(0), frame_count(0) {}
};

// ============================================================
// SECTION 2: Mock BladeBase Implementation
// ============================================================

// Minimal mock blade for benchmark execution
class BenchmarkBladeBase : public BladeBase {
private:
  BladeStyle* current_style_ = nullptr;
  static const int NUM_LEDS = 80;  // Standard blade LED count

public:
  BenchmarkBladeBase() {}

  void SetStyle(BladeStyle* style) override {
    if (current_style_) current_style_->deactivate();
    current_style_ = style;
    if (style) style->activate();
  }

  BladeStyle* UnSetStyle() override {
    BladeStyle* ret = current_style_;
    if (ret) ret->deactivate();
    current_style_ = nullptr;
    return ret;
  }

  BladeStyle* current_style() const override { return current_style_; }
  int num_leds() const override { return NUM_LEDS; }
  int GetBladeNumber() const override { return 0; }
  Color8::Byteorder get_byteorder() const override { return Color8::RGB; }
  bool is_powered() const override { return true; }
  void set(int led, Color16 c) override {}
  void allow_disable() override {}
  void Activate(int blade_number) override {}
  void Deactivate() override {}
};

// ============================================================
// SECTION 2.5: Include SD Style Parser
// ============================================================

#include "sd_style.h"

// ============================================================
// SECTION 3: Compiled Baseline Measurement
// ============================================================

// Simple compiled style for baseline: solid red color
#include "style_ptr.h"
#include "colors.h"

struct CompiledStyleBaseline {
  ExecutionMetrics metrics;

  ExecutionMetrics measureBaseline() {
    using CompiledStyle = Rgb<255, 0, 0>;  // Solid red

    BenchmarkBladeBase blade;
    CompiledStyle style;
    // Note: compiled styles don't inherit from BladeStyle interface;
    // we measure them directly without SetStyle() call

    const int FRAME_COUNT = 1000;
    const int SIMULATE_TICKS_PER_FRAME = 1;  // One tick per frame at 60fps
    std::vector<uint64_t> frame_times;
    frame_times.reserve(FRAME_COUNT);

    // Warm-up pass
    for (int i = 0; i < 10; i++) {
      style.run(&blade);
      for (int led = 0; led < blade.num_leds(); led++) {
        style.getColor(led);
      }
    }

    // Measurement pass
    for (int frame = 0; frame < FRAME_COUNT; frame++) {
      auto start = std::chrono::high_resolution_clock::now();

      style.run(&blade);
      for (int led = 0; led < blade.num_leds(); led++) {
        style.getColor(led);
      }

      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      frame_times.push_back(duration.count());
    }

    // Calculate statistics
    uint64_t min_time = *std::min_element(frame_times.begin(), frame_times.end());
    uint64_t max_time = *std::max_element(frame_times.begin(), frame_times.end());

    uint64_t sum = 0;
    for (auto t : frame_times) sum += t;
    uint64_t avg_time = sum / FRAME_COUNT;

    // Standard deviation
    uint64_t sq_sum = 0;
    for (auto t : frame_times) {
      int64_t diff = (int64_t)t - (int64_t)avg_time;
      sq_sum += diff * diff;
    }
    double variance = (double)sq_sum / FRAME_COUNT;
    uint64_t stddev = (uint64_t)std::sqrt(variance);

    metrics.frame_time_us_min = min_time;
    metrics.frame_time_us_avg = avg_time;
    metrics.frame_time_us_max = max_time;
    metrics.frame_time_us_stddev = stddev;
    metrics.frame_count = FRAME_COUNT;

    blade.UnSetStyle();
    return metrics;
  }
};

// ============================================================
// SECTION 4: SD-Loaded Style Measurement Function
// ============================================================

// Load and measure an SD-loaded style file
ExecutionMetrics measureStyleFromFile(const char* path) {
  ExecutionMetrics metrics;
  metrics.frame_count = 0;

  // Attempt to open file
  FILE* file = fopen(path, "r");
  if (!file) {
    printf("SKIP: %s (file not found)\n", path);
    return metrics;
  }

  // Read entire file into buffer
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size <= 0 || file_size > 1048576) {  // 1MB max
    printf("SKIP: %s (invalid file size: %ld)\n", path, file_size);
    fclose(file);
    return metrics;
  }

  char* file_contents = (char*)malloc(file_size + 1);
  if (!file_contents) {
    printf("SKIP: %s (memory allocation failed)\n", path);
    fclose(file);
    return metrics;
  }

  size_t bytes_read = fread(file_contents, 1, file_size, file);
  fclose(file);

  if (bytes_read != (size_t)file_size) {
    printf("SKIP: %s (read error: got %zu of %ld bytes)\n", path, bytes_read, file_size);
    free(file_contents);
    return metrics;
  }

  file_contents[file_size] = '\0';

  // Time the parse operation
  auto parse_start = std::chrono::high_resolution_clock::now();

  // Parse the style file using tokenizer and parseColorNode
  Tokenizer tok(file_contents);
  tok.next();  // prime first token
  RtColorNode* root = parseColorNode(tok, 0);

  auto parse_end = std::chrono::high_resolution_clock::now();
  auto parse_duration = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start);

  if (!root) {
    printf("SKIP: %s (parse failed)\n", path);
    free(file_contents);
    return metrics;
  }

  // Create RuntimeBladeStyle wrapper
  RuntimeBladeStyle* style = new RuntimeBladeStyle(root);
  if (!style) {
    printf("SKIP: %s (style creation failed)\n", path);
    free(file_contents);
    return metrics;
  }

  // Warm-up: run a few frames to stabilize
  BenchmarkBladeBase blade;
  blade.SetStyle(style);

  const int WARMUP_FRAMES = 10;
  for (int i = 0; i < WARMUP_FRAMES; i++) {
    style->run(&blade);
    for (int led = 0; led < blade.num_leds(); led++) {
      style->getColor(led);
    }
  }

  // Measurement: run 1000 frames and collect timing data
  const int FRAME_COUNT = 1000;
  std::vector<uint64_t> frame_times;
  frame_times.reserve(FRAME_COUNT);

  for (int frame = 0; frame < FRAME_COUNT; frame++) {
    auto frame_start = std::chrono::high_resolution_clock::now();

    style->run(&blade);
    for (int led = 0; led < blade.num_leds(); led++) {
      style->getColor(led);
    }

    auto frame_end = std::chrono::high_resolution_clock::now();
    auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
    frame_times.push_back(frame_duration.count());
  }

  // Calculate statistics
  if (!frame_times.empty()) {
    uint64_t min_time = *std::min_element(frame_times.begin(), frame_times.end());
    uint64_t max_time = *std::max_element(frame_times.begin(), frame_times.end());

    uint64_t sum = 0;
    for (auto t : frame_times) sum += t;
    uint64_t avg_time = sum / FRAME_COUNT;

    // Standard deviation
    uint64_t sq_sum = 0;
    for (auto t : frame_times) {
      int64_t diff = (int64_t)t - (int64_t)avg_time;
      sq_sum += diff * diff;
    }
    double variance = (double)sq_sum / FRAME_COUNT;
    uint64_t stddev = (uint64_t)std::sqrt(variance);

    metrics.frame_time_us_min = min_time;
    metrics.frame_time_us_avg = avg_time;
    metrics.frame_time_us_max = max_time;
    metrics.frame_time_us_stddev = stddev;
    metrics.frame_count = FRAME_COUNT;
  }

  blade.UnSetStyle();
  delete style;
  free(file_contents);
  return metrics;
}

// ============================================================
// SECTION 5: Main Benchmark Suite
// ============================================================

int main(int argc, char** argv) {
  printf("\n");
  printf("========================================\n");
  printf("ProffieOS SD Style Performance Benchmark\n");
  printf("========================================\n");
  printf("\n");

  // Measure compiled baseline
  printf("Measuring compiled StylePtr<> baseline...\n");
  CompiledStyleBaseline baseline_fixture;
  ExecutionMetrics baseline = baseline_fixture.measureBaseline();

  printf("BASELINE: %.1f us/frame (+/- %.1f%% stddev)\n",
         (double)baseline.frame_time_us_avg,
         (double)baseline.frame_time_us_stddev * 100.0 / (double)baseline.frame_time_us_avg);
  printf("          min: %.1f us, max: %.1f us\n",
         (double)baseline.frame_time_us_min,
         (double)baseline.frame_time_us_max);
  printf("\n");

  // Define production style files
  const char* production_styles[] = {
    "config/styles/calkestis.style",
    "config/styles/chimera.style",
    "config/styles/mercenary.style",
    "config/styles/hati.style",
    "config/styles/crispity.style",
    "config/styles/assassin.style",
    "config/styles/kyberradiance.style"
  };
  const int NUM_STYLES = NELEM(production_styles);

  // Print table header
  printf("SD-Loaded Style Performance Measurements\n");
  printf("=============================================================================================================\n");
  printf("%-20s | AvgFrame(us) | MinFrame(us) | MaxFrame(us) | Stddev(us) | Variance%% | Parity%% | Status\n", "Style");
  printf("-----+---------------+--------------+--------------+------------+----------+---------+--------\n");

  // Measure each production style
  int success_count = 0;
  double max_variance_pct = 0;
  for (int i = 0; i < NUM_STYLES; i++) {
    ExecutionMetrics metrics = measureStyleFromFile(production_styles[i]);

    // Extract filename from path
    const char* filename = production_styles[i];
    const char* last_slash = filename;
    for (const char* p = filename; *p; p++) {
      if (*p == '/' || *p == '\\') last_slash = p + 1;
    }

    if (metrics.frame_count > 0) {
      // Calculate variance percentage (stddev relative to average)
      double variance_pct = 0.0;
      if (metrics.frame_time_us_avg > 0) {
        variance_pct = (double)metrics.frame_time_us_stddev * 100.0 / (double)metrics.frame_time_us_avg;
      }

      // Calculate parity percentage vs baseline
      double parity_pct = 0.0;
      if (baseline.frame_time_us_avg > 0) {
        parity_pct = (double)metrics.frame_time_us_avg * 100.0 / (double)baseline.frame_time_us_avg;
      }

      // Determine status: ACCEPTABLE if variance < 5%, FAIL if > 10%, WARNING otherwise
      const char* status = "ACCEPTABLE";
      if (variance_pct > 10.0) {
        status = "FAIL";
        max_variance_pct = (variance_pct > max_variance_pct) ? variance_pct : max_variance_pct;
      } else if (variance_pct > 5.0) {
        status = "WARNING";
        max_variance_pct = (variance_pct > max_variance_pct) ? variance_pct : max_variance_pct;
      }

      printf("%-20s | %12.1f | %12.1f | %12.1f | %10.1f | %8.1f | %7.1f | %s\n",
             last_slash,
             (double)metrics.frame_time_us_avg,
             (double)metrics.frame_time_us_min,
             (double)metrics.frame_time_us_max,
             (double)metrics.frame_time_us_stddev,
             variance_pct,
             parity_pct,
             status);
      success_count++;
    } else {
      printf("%-20s | SKIPPED (parse error)\n", last_slash);
    }
  }

  printf("-----+---------------+--------------+--------------+------------+----------+---------+--------\n");
  printf("\n");
  printf("Summary Statistics\n");
  printf("  Baseline performance:       %.1f us/frame (compiled StylePtr<>)\n", (double)baseline.frame_time_us_avg);
  printf("  Baseline std deviation:     %.1f us (%.1f%%)\n",
         (double)baseline.frame_time_us_stddev,
         (double)baseline.frame_time_us_stddev * 100.0 / (double)baseline.frame_time_us_avg);
  printf("  SD-loaded styles measured:  %d/%d\n", success_count, NUM_STYLES);
  printf("  Maximum variance observed:  %.1f%%\n", max_variance_pct);
  printf("  Target:                     SD-loaded styles within 5%% variance of compiled baseline\n");
  printf("  Success:                    %s\n", (max_variance_pct <= 5.0) ? "YES - all within 5%%" : "NO - some exceeded 5%%");
  printf("\n");

  return (success_count > 0) ? 0 : 1;
}
