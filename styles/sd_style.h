#ifndef STYLES_SD_STYLE_H
#define STYLES_SD_STYLE_H

// SD-card runtime style loader for ProffieOS.
//
// Place a ProffieOS template expression in a .style file on the SD card:
//   Layers<Blue,AlphaL<Black,InOutHelperF<InOutFunc<300,800>>>>
//
// Reference it from your config (instead of StylePtr<...>):
//   StyleFromSD("font/style1.style")
//
// The file is parsed at runtime: no recompilation needed to change styles.
// Supports ~70 primitives. Unknown tokens fall back to black (with STDOUT log).

#include "blade_style.h"
#include "../common/file_reader.h"
#include "../common/color.h"
#include "../blades/blade_base.h"
#include "../common/arg_parser.h"

// ---------------------------------------------------------------------------
// Base node classes
// ---------------------------------------------------------------------------

class RtColorNode {
public:
  virtual ~RtColorNode() = default;
  virtual void run(BladeBase* blade) = 0;
  virtual RGBA_um getColor(int led) = 0;
};

class RtFuncNode {
public:
  virtual ~RtFuncNode() = default;
  virtual void run(BladeBase* blade) = 0;
  virtual int getInteger(int led) = 0;
};

// Paint 'over' on top of 'base': equivalent to RGBA_um << RGBA_um from color.h.
static inline RGBA_um rt_compose(const RGBA_um& base, const RGBA_um& over) {
  if (!over.alpha) return base;
  uint16_t ac = 32768 - over.alpha;
  uint16_t ba = (uint32_t)base.alpha * ac >> 15;
  return RGBA_um(
    ((base.c * ba) + (over.c * over.alpha)) >> 15,
    over.alpha >= 16384 ? over.overdrive : base.overdrive,
    (uint16_t)(ba + over.alpha)
  );
}

static inline int rt_clamp(int x, int lo, int hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

// ---------------------------------------------------------------------------
// Color nodes
// ---------------------------------------------------------------------------

// Constant opaque color: Rgb<R,G,B>, named colors, etc.
class RtRgb : public RtColorNode {
public:
  explicit RtRgb(Color16 c, bool overdrive = false)
    : pixel_(c, overdrive, 32768) {}
  void run(BladeBase*) override {}
  RGBA_um getColor(int) override { return pixel_; }
private:
  RGBA_um pixel_;
};

// AlphaL<COLOR, ALPHA_FUNC>
class RtAlphaL : public RtColorNode {
public:
  RtAlphaL(RtColorNode* color, RtFuncNode* alpha) : color_(color), alpha_(alpha) {}
  ~RtAlphaL() override { delete color_; delete alpha_; }
  void run(BladeBase* blade) override { color_->run(blade); alpha_->run(blade); }
  RGBA_um getColor(int led) override {
    int a = alpha_->getInteger(led);
    if (!a) return RGBA_um::Transparent();
    RGBA_um c = color_->getColor(led);
    c.alpha = (uint32_t)c.alpha * (uint16_t)a >> 15;
    return c;
  }
private:
  RtColorNode* color_;
  RtFuncNode* alpha_;
};

// Compose: paint layer on top of base  (Layers<> expands to nested RtCompose)
class RtCompose : public RtColorNode {
public:
  RtCompose(RtColorNode* base, RtColorNode* layer) : base_(base), layer_(layer) {}
  ~RtCompose() override { delete base_; delete layer_; }
  void run(BladeBase* blade) override { base_->run(blade); layer_->run(blade); }
  RGBA_um getColor(int led) override {
    return rt_compose(base_->getColor(led), layer_->getColor(led));
  }
private:
  RtColorNode* base_;
  RtColorNode* layer_;
};

// Mix<F, A, B>: F=0 -> A, F=32768 -> B
class RtMix : public RtColorNode {
public:
  RtMix(RtFuncNode* f, RtColorNode* a, RtColorNode* b) : f_(f), a_(a), b_(b) {}
  ~RtMix() override { delete f_; delete a_; delete b_; }
  void run(BladeBase* blade) override { f_->run(blade); a_->run(blade); b_->run(blade); }
  RGBA_um getColor(int led) override {
    int mix = f_->getInteger(led);
    RGBA_um a = a_->getColor(led);
    RGBA_um b = b_->getColor(led);
    uint16_t am = 32768 - mix;
    return RGBA_um(
      ((a.c * am) + (b.c * (uint16_t)mix)) >> 15,
      mix >= 16384 ? b.overdrive : a.overdrive,
      (uint16_t)(((uint32_t)a.alpha * am + (uint32_t)b.alpha * (uint16_t)mix + 0x7fff) >> 15)
    );
  }
private:
  RtFuncNode* f_;
  RtColorNode* a_;
  RtColorNode* b_;
};

// Rgba16<R,G,B,A>: constant semi-transparent color
class RtRgba : public RtColorNode {
public:
  explicit RtRgba(RGBA_um p) : p_(p) {}
  void run(BladeBase*) override {}
  RGBA_um getColor(int) override { return p_; }
private:
  RGBA_um p_;
};

// OverDrive<COLOR>: force overdrive flag on a color
class RtOverDriveWrap : public RtColorNode {
public:
  explicit RtOverDriveWrap(RtColorNode* c) : c_(c) {}
  ~RtOverDriveWrap() override { delete c_; }
  void run(BladeBase* b) override { c_->run(b); }
  RGBA_um getColor(int led) override {
    RGBA_um r = c_->getColor(led); r.overdrive = true; return r;
  }
private:
  RtColorNode* c_;
};

// RgbArg<N, DEFAULT>: read color from arg slot N, fall back to DEFAULT
class RtRgbArg : public RtColorNode {
public:
  RtRgbArg(int slot, Color16 def) : slot_(slot), pixel_(def, false, 32768) {
    char def_str[48];
    // Build default string using Color16 values (same format as RgbArgBase::init)
    itoa((int)def.r, def_str, 10);
    strcat(def_str, ",");
    itoa((int)def.g, def_str + strlen(def_str), 10);
    strcat(def_str, ",");
    itoa((int)def.b, def_str + strlen(def_str), 10);
    const char* arg = CurrentArgParser->GetArg(slot_, "COLOR", def_str);
    if (arg) {
      char* tmp;
      int r = strtol(arg, &tmp, 0);
      int g = (tmp && *tmp) ? strtol(tmp + 1, &tmp, 0) : 0;
      int b = (tmp && *tmp) ? strtol(tmp + 1, nullptr, 0) : 0;
      pixel_ = RGBA_um(Color16(r, g, b), false, 32768);
    }
  }
  void run(BladeBase*) override {}
  RGBA_um getColor(int) override { return pixel_; }
private:
  int slot_;
  RGBA_um pixel_;
};

// ---------------------------------------------------------------------------
// Function nodes
// ---------------------------------------------------------------------------

// Int<N>: constant
class RtIntConst : public RtFuncNode {
public:
  explicit RtIntConst(int n) : n_(n) {}
  void run(BladeBase*) override {}
  int getInteger(int) override { return n_; }
private:
  int n_;
};

// InOutFuncX<OUT_MILLIS, IN_MILLIS>: extension timer 0..32768
class RtInOutFunc : public RtFuncNode {
public:
  RtInOutFunc(RtFuncNode* out_ms, RtFuncNode* in_ms)
    : out_ms_(out_ms), in_ms_(in_ms) {}
  ~RtInOutFunc() override { delete out_ms_; delete in_ms_; }
  void run(BladeBase* blade) override {
    out_ms_->run(blade);
    in_ms_->run(blade);
    uint32_t now = micros();
    uint32_t delta = now - last_micros_;
    last_micros_ = now;
    int out_ms = out_ms_->getInteger(0);
    int in_ms  = in_ms_->getInteger(0);
    if (blade->is_on()) {
      extension_ = (extension_ < 0.00001f)
                     ? 0.00001f
                     : std::min(extension_ + delta / (out_ms * 1000.0f), 1.0f);
    } else {
      extension_ = std::max(extension_ - delta / (in_ms * 1000.0f), 0.0f);
    }
    ret_ = (int)(extension_ * 32768.0f);
  }
  int getInteger(int) override { return ret_; }
  float get_extension() const { return extension_; }
private:
  RtFuncNode* out_ms_;
  RtFuncNode* in_ms_;
  float extension_ = 0.0f;
  uint32_t last_micros_ = 0;
  int ret_ = 0;
};

// InOutHelperF<EXTENSION>: per-LED wipe alpha (32768 when off, 0 when on)
// Used inside AlphaL<Black, InOutHelperF<...>> to mask the blade during ignition.
class RtInOutHelperF : public RtFuncNode {
public:
  explicit RtInOutHelperF(RtFuncNode* ext, bool allow_disable = true)
    : ext_(ext), allow_disable_(allow_disable) {}
  ~RtInOutHelperF() override { delete ext_; }
  void run(BladeBase* blade) override {
    ext_->run(blade);
    int ext_val = ext_->getInteger(0);
    // thres = ext_val * num_leds - 32768  (same as compiled InOutHelperF::run)
    thres_ = (int32_t)ext_val * blade->num_leds() - 32768;
    done_off_ = allow_disable_ && (ext_val == 0) && !blade->is_on();
  }
  int getInteger(int led) override {
    int32_t x = (int32_t)led * 32768 - thres_;
    return rt_clamp((int)x, 0, 32768);
  }
  bool done_off() const { return done_off_; }
private:
  RtFuncNode* ext_;
  bool allow_disable_;
  int32_t thres_ = 0;
  bool done_off_ = false;
};

// Ifon<A, B>: return A when blade is on, B when off
class RtIfon : public RtFuncNode {
public:
  RtIfon(RtFuncNode* on_val, RtFuncNode* off_val)
    : on_(on_val), off_(off_val) {}
  ~RtIfon() override { delete on_; delete off_; }
  void run(BladeBase* blade) override {
    on_->run(blade);
    off_->run(blade);
    is_on_ = blade->is_on();
  }
  int getInteger(int led) override {
    return is_on_ ? on_->getInteger(led) : off_->getInteger(led);
  }
private:
  RtFuncNode* on_;
  RtFuncNode* off_;
  bool is_on_ = false;
};

// SmoothStep<POS, WIDTH>: smooth sigmoid by blade position
class RtSmoothStep : public RtFuncNode {
public:
  RtSmoothStep(RtFuncNode* pos, RtFuncNode* width) : pos_(pos), width_(width) {}
  ~RtSmoothStep() override { delete pos_; delete width_; }
  void run(BladeBase* blade) override {
    pos_->run(blade);
    width_->run(blade);
    int w = width_->getInteger(0);
    if (w == 0) {
      mult_     = 32768;
      location_ = blade->num_leds() * pos_->getInteger(0);
    } else {
      mult_     = 32768 * 32768 / w / blade->num_leds();
      location_ = 32768 * pos_->getInteger(0) / w - 16384;
    }
  }
  int getInteger(int led) override {
    int x = led * mult_ - location_;
    if (x < 0) return 0;
    if (x > 32768) return 32768;
    return (((x * x) >> 14) * ((3 << 14) - x)) >> 15;
  }
private:
  RtFuncNode* pos_;
  RtFuncNode* width_;
  int mult_ = 0, location_ = 0;
};

// Scale<F, A, B>: map F in 0..32768 to range A..B
class RtScale : public RtFuncNode {
public:
  RtScale(RtFuncNode* f, RtFuncNode* a, RtFuncNode* b) : f_(f), a_(a), b_(b) {}
  ~RtScale() override { delete f_; delete a_; delete b_; }
  void run(BladeBase* blade) override {
    f_->run(blade); a_->run(blade); b_->run(blade);
    add_ = a_->getInteger(0);
    mul_ = b_->getInteger(0) - add_;
  }
  int getInteger(int led) override {
    return (f_->getInteger(led) * mul_ >> 15) + add_;
  }
private:
  RtFuncNode* f_, *a_, *b_;
  int add_ = 0, mul_ = 0;
};

// Bump<POS, WIDTH>: gaussian bump shape
class RtBump : public RtFuncNode {
public:
  RtBump(RtFuncNode* pos, RtFuncNode* width) : pos_(pos), width_(width) {}
  ~RtBump() override { delete pos_; delete width_; }
  void run(BladeBase* blade) override {
    pos_->run(blade);
    width_->run(blade);
    int fraction = width_->getInteger(0);
    if (fraction == 0) { mult_ = 1; location_ = -10000; return; }
    float m = 32 * 2.0f * 128 * 32768.0f / fraction / blade->num_leds();
    mult_     = (int)m;
    location_ = (int)((int64_t)pos_->getInteger(0) * blade->num_leds() * mult_ / 32768);
  }
  int getInteger(int led) override {
    static const uint8_t shape[33] = {
      255,255,252,247,240,232,222,211,
      199,186,173,159,145,132,119,106,
       94, 82, 72, 62, 53, 45, 38, 32,
       26, 22, 18, 14, 11,  9,  7,  5, 0
    };
    uint32_t dist = (uint32_t)std::abs(led * mult_ - location_);
    uint32_t p = dist >> 7;
    if (p >= 32) return 0;
    int m = dist & 0x3f;
    return shape[p] * (128 - m) + shape[p + 1] * m;
  }
private:
  RtFuncNode* pos_;
  RtFuncNode* width_;
  int mult_ = 1, location_ = -10000;
};

// BladeAngle<MIN, MAX>: 0..32768 based on physical blade angle
class RtBladeAngle : public RtFuncNode {
public:
  RtBladeAngle(RtFuncNode* mn, RtFuncNode* mx) : mn_(mn), mx_(mx) {}
  ~RtBladeAngle() override { delete mn_; delete mx_; }
  void run(BladeBase* blade) override { mn_->run(blade); mx_->run(blade); }
  int getInteger(int) override {
#if defined(FUSE_H) || defined(COMMON_FUSE_H)
    int mn = mn_->getInteger(0);
    int mx = mx_->getInteger(0);
    float v = (fusor.angle1() + (float)M_PI / 2) * 32768.0f / (float)M_PI;
    return rt_clamp((int)((v - mn) * 32768.0f / (mx - mn)), 0, 32768);
#else
    return 16384; // no fusor: return midpoint
#endif
  }
private:
  RtFuncNode* mn_;
  RtFuncNode* mx_;
};

// ---------------------------------------------------------------------------
// RuntimeBladeStyle
// ---------------------------------------------------------------------------

class RuntimeBladeStyle : public BladeStyle {
public:
  explicit RuntimeBladeStyle(RtColorNode* root) : root_(root) {}
  ~RuntimeBladeStyle() override { delete root_; }

  void run(BladeBase* blade) override {
    root_->run(blade);
    int n = blade->num_leds();
    bool blade_off = !blade->is_on();
    bool all_zero = blade_off;
    for (int i = 0; i < n; i++) {
      RGBA_um c = root_->getColor(i);
      Color16 out = (c.alpha >= 32768) ? c.c : (c.c * c.alpha) >> 15;
      if (all_zero && (out.r || out.g || out.b)) all_zero = false;
      if (c.overdrive)
        blade->set_overdrive(i, out);
      else
        blade->set(i, out);
    }
    if (all_zero) blade->allow_disable();
  }

  bool IsHandled(HandledFeature) override { return false; }

private:
  RtColorNode* root_;
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class SDStyleParser {
public:
  SDStyleParser(const char* s, int len) : s_(s), end_(s + len) {}

  RtColorNode* parseColor();
  RtFuncNode*  parseFunc();

private:
  // --- lexer helpers -------------------------------------------------

  void skipWS() {
    while (s_ < end_ && (*s_ == ' ' || *s_ == '\t' || *s_ == '\n' || *s_ == '\r'))
      ++s_;
  }

  bool peekChar(char c) { skipWS(); return s_ < end_ && *s_ == c; }

  bool eatChar(char c) {
    skipWS();
    if (s_ < end_ && *s_ == c) { ++s_; return true; }
    return false;
  }

  bool readIdent(char* buf, int buflen) {
    skipWS();
    if (s_ >= end_ || !(isalpha((uint8_t)*s_) || *s_ == '_')) return false;
    int i = 0;
    while (s_ < end_ && (isalnum((uint8_t)*s_) || *s_ == '_') && i < buflen - 1)
      buf[i++] = *s_++;
    buf[i] = '\0';
    return i > 0;
  }

  int parseInt() {
    skipWS();
    bool neg = (s_ < end_ && *s_ == '-');
    if (neg) ++s_;
    int n = 0;
    while (s_ < end_ && isdigit((uint8_t)*s_))
      n = n * 10 + (*s_++ - '0');
    return neg ? -n : n;
  }

  // Parse a function-or-integer-literal arg (handles Int<N> and raw N).
  RtFuncNode* parseFuncOrInt() {
    skipWS();
    if (s_ < end_ && (isdigit((uint8_t)*s_) || *s_ == '-'))
      return new RtIntConst(parseInt());
    return parseFunc();
  }

  // Skip to closing '>' (called BEFORE consuming '>').
  // Handles nesting; does not consume the final '>'.
  void skipToClose() {
    int depth = 0;
    while (s_ < end_) {
      if      (*s_ == '<') { ++depth; ++s_; }
      else if (*s_ == '>') {
        if (depth == 0) return;
        --depth; ++s_;
      } else { ++s_; }
    }
  }

  // Skip the entire '<...>' block including the opening '<'.
  void skipTemplateArgs() {
    if (!eatChar('<')) return;
    int depth = 1;
    while (s_ < end_ && depth > 0) {
      if      (*s_ == '<') ++depth;
      else if (*s_ == '>') --depth;
      ++s_;
    }
  }

  // --- named color lookup -------------------------------------------

  RtColorNode* namedColor(const char* name);

  // --- state --------------------------------------------------------
  const char* s_;
  const char* end_;
};

// ---------------------------------------------------------------------------
// Named color table (covers colors.h)
// ---------------------------------------------------------------------------

static RtColorNode* sd_named_color(const char* name) {
  struct Entry { const char* n; uint8_t r, g, b; };
  static const Entry t[] = {
    // Short caps forms
    {"RED",     255,  0,  0}, {"GREEN",   0, 255,  0}, {"BLUE",    0,  0, 255},
    {"WHITE",   255,255,255}, {"BLACK",   0,   0,  0}, {"YELLOW", 255,255,  0},
    {"CYAN",      0,255,255}, {"MAGENTA",255,  0,255},
    // CamelCase forms from colors.h
    {"Red",     255,  0,  0}, {"Green",   0, 255,  0}, {"Blue",    0,  0, 255},
    {"White",   255,255,255}, {"Black",   0,   0,  0}, {"Yellow", 255,255,  0},
    {"Cyan",      0,255,255}, {"Magenta",255,  0,255},
    {"Orange",  255, 97,  0}, {"OrangeRed",255,14,  0},
    {"DeepSkyBlue",0,135,255}, {"DodgerBlue",2,72,255}, {"SteelBlue",14,57,118},
    {"HotPink", 255, 36,118}, {"Pink",   255,136,154}, {"DeepPink",255, 0, 75},
    {"LightSalmon",255,91,50}, {"Coral",  255, 55, 19},
    {"Lime",      0,255,  0}, {"SpringGreen",0,255, 55},
    {"Aqua",      0,255,255}, {"Aquamarine",55,255,169},
    {"Amber",   255,135,  0}, {"CyberYellow",255,168,0}, {"CanaryYellow",255,221,0},
    {"ElectricPurple",127,0,255}, {"ElectricViolet",71,0,255},
    {"ElectricLime",156,255,0}, {"PaleGreen",28,255,28},
    {"Flamingo",255,80,154}, {"VividViolet",90,0,255},
    {"PsychedelicPurple",186,0,255}, {"HotMagenta",255,0,156},
    {"BrutalPink",255,0,128}, {"NeonRose",255,0,55}, {"VividRaspberry",255,0,38},
    {"HaltRed", 255,  0, 19},
    {"Fuchsia", 255,  0,255}, {"GreenYellow",108,255,6},
    {"Chartreuse",55,255,0}, {"Tomato",255,31,15},
  };
  for (unsigned i = 0; i < sizeof(t)/sizeof(t[0]); ++i) {
    if (strcmp(name, t[i].n) == 0)
      return new RtRgb(Color16(Color8(t[i].r, t[i].g, t[i].b)));
  }
  return nullptr;
}

RtColorNode* SDStyleParser::namedColor(const char* name) {
  return sd_named_color(name);
}

// ---------------------------------------------------------------------------
// parseColor
// ---------------------------------------------------------------------------

RtColorNode* SDStyleParser::parseColor() {
  char name[64];
  if (!readIdent(name, sizeof(name))) {
    STDOUT.println("SDStyle: expected color identifier");
    return new RtRgb(Color16());
  }

  // --- Layer composition --------------------------------------------
  if (!strcmp(name, "Layers")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* base = parseColor();
    while (eatChar(','))
      base = new RtCompose(base, parseColor());
    eatChar('>');
    return base;
  }

  // --- AlphaL -------------------------------------------------------
  if (!strcmp(name, "AlphaL") || !strcmp(name, "AlphaMixL")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* color = parseColor();
    if (!eatChar(',')) { delete color; return new RtRgb(Color16()); }
    RtFuncNode* alpha = parseFunc();
    skipToClose(); eatChar('>');
    return new RtAlphaL(color, alpha);
  }

  // --- Mix ----------------------------------------------------------
  if (!strcmp(name, "Mix")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtFuncNode* f = parseFunc();
    if (!eatChar(',')) { delete f; return new RtRgb(Color16()); }
    RtColorNode* a = parseColor();
    if (!eatChar(',')) { delete f; delete a; return new RtRgb(Color16()); }
    RtColorNode* b = parseColor();
    skipToClose(); eatChar('>');
    return new RtMix(f, a, b);
  }

  // --- Rgb / Rgb16 --------------------------------------------------
  if (!strcmp(name, "Rgb")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    int r = parseInt(); eatChar(',');
    int g = parseInt(); eatChar(',');
    int b = parseInt(); eatChar('>');
    return new RtRgb(Color16(Color8(r, g, b)));
  }
  if (!strcmp(name, "Rgb16")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    int r = parseInt(); eatChar(',');
    int g = parseInt(); eatChar(',');
    int b = parseInt(); eatChar('>');
    return new RtRgb(Color16(r, g, b));
  }
  if (!strcmp(name, "Rgba16")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    int r = parseInt(); eatChar(',');
    int g = parseInt(); eatChar(',');
    int b = parseInt(); eatChar(',');
    int a = parseInt(); eatChar('>');
    // Rgba16 stores alpha as 0..65535, RGBA_um uses 0..32768
    return new RtRgba(RGBA_um(Color16(r, g, b), false, (uint16_t)(a >> 1)));
  }

  // --- RgbArg / Rgb16Arg -------------------------------------------
  if (!strcmp(name, "RgbArg") || !strcmp(name, "Rgb16Arg")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    int slot = parseInt(); eatChar(',');
    RtColorNode* def = parseColor(); eatChar('>');
    // Extract default color then replace with arg-aware node
    RGBA_um def_rgba = def->getColor(0);
    delete def;
    return new RtRgbArg(slot, def_rgba.c);
  }

  // --- StylePtr wrappers -------------------------------------------
  if (!strcmp(name, "StylePtr")          ||
      !strcmp(name, "StyleNormalPtr")    ||
      !strcmp(name, "StyleNormalPtrX")   ||
      !strcmp(name, "ChargingStylePtr")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* inner = parseColor();
    skipToClose(); eatChar('>');
    return inner;
  }

  // --- InOutHelper family ------------------------------------------
  // InOutHelperX<BASE, EXTENSION_FUNC, OFF_COLOR>
  //   = Layers<BASE, AlphaL<OFF_COLOR, InOutHelperF<EXTENSION_FUNC>>>
  if (!strcmp(name, "InOutHelperX")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* base = parseColor(); eatChar(',');
    RtFuncNode*  ext  = parseFuncOrInt();
    RtColorNode* off  = eatChar(',') ? parseColor() : new RtRgb(Color16());
    skipToClose(); eatChar('>');
    return new RtCompose(base,
             new RtAlphaL(off, new RtInOutHelperF(ext)));
  }

  // InOutHelper<BASE, OUT_MILLIS, IN_MILLIS, OFF_COLOR>
  if (!strcmp(name, "InOutHelper")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* base   = parseColor(); eatChar(',');
    int          out_ms = parseInt();   eatChar(',');
    int          in_ms  = parseInt();
    RtColorNode* off    = eatChar(',') ? parseColor() : new RtRgb(Color16());
    skipToClose(); eatChar('>');
    return new RtCompose(base,
             new RtAlphaL(off,
               new RtInOutHelperF(
                 new RtInOutFunc(new RtIntConst(out_ms),
                                 new RtIntConst(in_ms)))));
  }

  // InOutTr<BASE, OUT_TR, IN_TR, OFF> — simplified: just show BASE
  // Full transition support can be added later.
  if (!strcmp(name, "InOutTr") || !strcmp(name, "InOutTrL")) {
    STDOUT.print("SDStyle: InOutTr not fully supported, using base only: ");
    if (!eatChar('<')) return new RtRgb(Color16());
    // For InOutTr<BASE,...>, parse BASE and skip rest
    // For InOutTrL<...>, no base arg (layer only) — return transparent
    bool is_L = (name[8] == 'L');
    RtColorNode* base = is_L ? (RtColorNode*)new RtRgb(Color16()) : parseColor();
    skipToClose(); eatChar('>');
    return base;
  }

  // --- OverDrive wrapper -------------------------------------------
  if (!strcmp(name, "OverDrive")) {
    if (!eatChar('<')) return new RtRgb(Color16());
    RtColorNode* inner = parseColor(); eatChar('>');
    return new RtOverDriveWrap(inner);
  }

  // --- Named colors (no template args) ----------------------------
  RtColorNode* nc = sd_named_color(name);
  if (nc) return nc;

  // --- Unknown: skip any template args and return black -----------
  STDOUT.print("SDStyle: unknown color '");
  STDOUT.print(name);
  STDOUT.println("' — using Black");
  if (peekChar('<')) skipTemplateArgs();
  return new RtRgb(Color16());
}

// ---------------------------------------------------------------------------
// parseFunc
// ---------------------------------------------------------------------------

RtFuncNode* SDStyleParser::parseFunc() {
  char name[64];
  if (!readIdent(name, sizeof(name))) {
    STDOUT.println("SDStyle: expected function identifier");
    return new RtIntConst(0);
  }

  // Int<N>
  if (!strcmp(name, "Int")) {
    if (!eatChar('<')) return new RtIntConst(0);
    int n = parseInt(); eatChar('>');
    return new RtIntConst(n);
  }

  // IntArg<SLOT, DEFAULT>
  if (!strcmp(name, "IntArg")) {
    if (!eatChar('<')) return new RtIntConst(0);
    int slot = parseInt(); eatChar(',');
    int def  = parseInt(); eatChar('>');
    char def_str[16]; itoa(def, def_str, 10);
    const char* arg = CurrentArgParser->GetArg(slot, "FUNCTION", def_str);
    int val = arg ? atoi(arg) : def;
    return new RtIntConst(val);
  }

  // InOutFunc<OUT_MS, IN_MS>  (integer literal args)
  if (!strcmp(name, "InOutFunc")) {
    if (!eatChar('<')) return new RtIntConst(16384);
    int out_ms = parseInt(); eatChar(',');
    int in_ms  = parseInt(); eatChar('>');
    return new RtInOutFunc(new RtIntConst(out_ms), new RtIntConst(in_ms));
  }

  // InOutFuncX<OUT_FUNC, IN_FUNC>  (function args — typically Int<N>)
  if (!strcmp(name, "InOutFuncX")) {
    if (!eatChar('<')) return new RtIntConst(16384);
    RtFuncNode* out_ms = parseFuncOrInt(); eatChar(',');
    RtFuncNode* in_ms  = parseFuncOrInt();
    skipToClose(); eatChar('>');
    return new RtInOutFunc(out_ms, in_ms);
  }

  // InOutHelperF<EXTENSION_FUNC, ALLOW_DISABLE=1>
  if (!strcmp(name, "InOutHelperF")) {
    if (!eatChar('<')) return new RtIntConst(0);
    RtFuncNode* ext = parseFuncOrInt();
    bool allow_disable = true;
    if (eatChar(',')) {
      skipWS();
      if (s_ < end_ && isdigit((uint8_t)*s_))
        allow_disable = parseInt() != 0;
      else
        skipToClose();
    }
    eatChar('>');
    return new RtInOutHelperF(ext, allow_disable);
  }

  // Ifon<A, B>
  if (!strcmp(name, "Ifon")) {
    if (!eatChar('<')) return new RtIntConst(0);
    RtFuncNode* a = parseFuncOrInt(); eatChar(',');
    RtFuncNode* b = parseFuncOrInt(); eatChar('>');
    return new RtIfon(a, b);
  }

  // SmoothStep<POS, WIDTH>
  if (!strcmp(name, "SmoothStep")) {
    if (!eatChar('<')) return new RtIntConst(0);
    RtFuncNode* pos   = parseFuncOrInt(); eatChar(',');
    RtFuncNode* width = parseFuncOrInt(); eatChar('>');
    return new RtSmoothStep(pos, width);
  }

  // Scale<F, A, B>
  if (!strcmp(name, "Scale")) {
    if (!eatChar('<')) return new RtIntConst(0);
    RtFuncNode* f = parseFuncOrInt(); eatChar(',');
    RtFuncNode* a = parseFuncOrInt(); eatChar(',');
    RtFuncNode* b = parseFuncOrInt(); eatChar('>');
    return new RtScale(f, a, b);
  }

  // Bump<POS, WIDTH>
  if (!strcmp(name, "Bump")) {
    if (!eatChar('<')) return new RtIntConst(0);
    RtFuncNode* pos   = parseFuncOrInt();
    RtFuncNode* width = eatChar(',') ? parseFuncOrInt() : new RtIntConst(16385);
    eatChar('>');
    return new RtBump(pos, width);
  }

  // BladeAngle / BladeAngleX
  if (!strcmp(name, "BladeAngle") || !strcmp(name, "BladeAngleX")) {
    if (peekChar('<')) {
      eatChar('<');
      RtFuncNode* mn = parseFuncOrInt(); eatChar(',');
      RtFuncNode* mx = parseFuncOrInt(); eatChar('>');
      return new RtBladeAngle(mn, mx);
    }
    return new RtBladeAngle(new RtIntConst(0), new RtIntConst(32768));
  }

  // HumpFlickerF<WIDTH> — simplified: random single-value function
  if (!strcmp(name, "HumpFlickerF") || !strcmp(name, "HumpFlickerFX")) {
    if (peekChar('<')) skipTemplateArgs();
    // Return a zero-constant as a safe stub; actual hump flicker needs per-frame random
    return new RtIntConst(0);
  }

  // RandomF / RandomPerLEDF — stub
  if (!strcmp(name, "RandomF") || !strcmp(name, "RandomPerLEDF")) {
    if (peekChar('<')) skipTemplateArgs();
    return new RtIntConst(16384);
  }

  // Unknown function
  STDOUT.print("SDStyle: unknown function '");
  STDOUT.print(name);
  STDOUT.println("' — using Int<0>");
  if (peekChar('<')) skipTemplateArgs();
  return new RtIntConst(0);
}

// ---------------------------------------------------------------------------
// SDStyleFactory
// ---------------------------------------------------------------------------

class SDStyleFactory : public StyleFactory {
public:
  explicit SDStyleFactory(const char* path) : path_(path) {}

  BladeStyle* make() override {
    FileReader f;
    if (!f.Open(path_)) {
      STDOUT.print("SDStyle: cannot open '");
      STDOUT.print(path_);
      STDOUT.println("' — style will be Black");
      return new RuntimeBladeStyle(new RtRgb(Color16()));
    }

    // Read up to 4 KB; styles beyond that should be split across presets.
    static const int kMaxStyleBytes = 4096;
    char* buf = (char*)malloc(kMaxStyleBytes);
    if (!buf) {
      STDOUT.println("SDStyle: out of memory");
      f.Close();
      return new RuntimeBladeStyle(new RtRgb(Color16()));
    }

    int len = f.Read((uint8_t*)buf, kMaxStyleBytes - 1);
    f.Close();
    // Trim leading/trailing whitespace and null-terminate
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'
                    || buf[len-1] == ' '  || buf[len-1] == '\t'))
      --len;
    buf[len] = '\0';

    SDStyleParser parser(buf, len);
    RtColorNode* root = parser.parseColor();
    free(buf);

    STDERR << "SDStyle loaded: " << path_ << " (" << len << " bytes)\n";
    return new RuntimeBladeStyle(root);
  }

private:
  const char* path_;
};

// ---------------------------------------------------------------------------
// Public API: StyleFromSD("styles/foo.style")
// ---------------------------------------------------------------------------

// Returns a StyleFactory* (compatible with StyleAllocator) that reads the
// style from the SD card at each preset activation.
// The path string must remain valid for the lifetime of the preset (use a
// string literal).
inline StyleFactory* StyleFromSD(const char* path) {
  return new SDStyleFactory(path);
}

#endif  // STYLES_SD_STYLE_H
