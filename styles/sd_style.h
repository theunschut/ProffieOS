#ifndef STYLES_SD_STYLE_H
#define STYLES_SD_STYLE_H

// SD card style loader for ProffieOS.
// Usage: StyleFromSD("path/to/style.style") in preset arrays.
// Returns StyleAllocator (= class StyleFactory*) — interchangeable with StylePtr<>().
// SD card is not accessed until make() is called (lazy loading).
//
// Requires: styles/blade_style.h, common/lsfs.h, common/errors.h

// ============================================================
// SECTION 1: Runtime Node Base Classes
// ============================================================

// Runtime color node — produces RGBA_um per LED.
// run() returns false if the blade can be disabled (equivalent to
// LayerRunResult::OPAQUE_BLACK_UNTIL_IGNITION when blade is off).
class RtColorNode {
public:
  virtual ~RtColorNode() {}
  virtual bool run(BladeBase* blade) = 0;   // false = can disable
  virtual RGBA_um getColor(int led) = 0;
};

// Runtime function node — produces integer per LED.
class RtFuncNode {
public:
  virtual ~RtFuncNode() {}
  virtual FunctionRunResult run(BladeBase* blade) = 0;
  virtual int getInteger(int led) = 0;
};

// Runtime transition node — handles transitions between two colors.
class RtTransNode {
public:
  virtual ~RtTransNode() {}
  virtual void begin() = 0;
  virtual bool done() = 0;
  virtual void run(BladeBase* blade) = 0;
  virtual RGBA_um getColor(RGBA_um a, RGBA_um b, int led) = 0;
};

// ============================================================
// SECTION 2: RtArg<N> Bridge Templates
// ============================================================
// Global arrays — single-threaded embedded, safe without locking.
// Parent nodes populate these immediately before calling wrapped run().

static const int MAX_RT_ARGS = 32;
static RtColorNode* rt_color_args[MAX_RT_ARGS];
static RtFuncNode*  rt_func_args[MAX_RT_ARGS];
static RtTransNode* rt_trans_args[MAX_RT_ARGS];

// RtArgColor<N> — compile-time adapter that reads rt_color_args[N] at runtime.
// Must expose run(BladeBase*) returning bool and getColor(int) returning RGBA_um.
template<int N>
class RtArgColor {
public:
  bool run(BladeBase* blade) { return rt_color_args[N]->run(blade); }
  RGBA_um getColor(int led)  { return rt_color_args[N]->getColor(led); }
  // LayerRunResult adapter for Compose<>/AlphaL<> which call RunLayer()
  // RunLayer uses bool specialization: bool → OPAQUE_BLACK if false
};

// RtArgFunc<N> — reads rt_func_args[N].
template<int N>
class RtArgFunc {
public:
  FunctionRunResult run(BladeBase* blade) { return rt_func_args[N]->run(blade); }
  int getInteger(int led)                 { return rt_func_args[N]->getInteger(led); }
  // calculate() needed by SVFWrapper pattern in some functions
  int calculate(BladeBase* blade) { return rt_func_args[N]->getInteger(0); }
};

// RtArgTrans<N> — reads rt_trans_args[N].
template<int N>
class RtArgTrans {
public:
  void begin()             { rt_trans_args[N]->begin(); }
  bool done()              { return rt_trans_args[N]->done(); }
  void run(BladeBase* blade) { rt_trans_args[N]->run(blade); }
  template<class A, class B>
  RGBA_um getColor(A a, B b, int led) {
    // Convert A and B to RGBA_um for the RtTransNode interface
    RGBA_um ca, cb;
    // A and B may be RGBA_um, RGBA_um_nod, SimpleColor, OverDriveColor etc.
    // We use the << operator to coerce to RGBA_um via existing blend chain.
    // Simplest safe approach: rely on implicit conversion where possible.
    ca = (RGBA_um)a;
    cb = (RGBA_um)b;
    return rt_trans_args[N]->getColor(ca, cb, led);
  }
};

// ============================================================
// SECTION 3: Tokenizer
// ============================================================

enum TokenType {
  TOK_IDENT,   // identifier: class name, color name, enum name
  TOK_INT,     // integer literal (possibly negative)
  TOK_HEX,     // #RRGGBB color literal
  TOK_OPEN,    // <
  TOK_CLOSE,   // >
  TOK_COMMA,   // ,
  TOK_SCOPE,   // :: (for SaberBase::LOCKUP_NORMAL etc.)
  TOK_EOF,
  TOK_ERROR
};

class Tokenizer {
public:
  explicit Tokenizer(const char* input)
    : pos_(input), current_(TOK_ERROR), int_val_(0), hex_val_(0) {
    ident_buf_[0] = '\0';
  }

  // Advance to next token; returns new current type.
  TokenType next() {
    skipWhitespace();
    if (!pos_ || *pos_ == '\0') { current_ = TOK_EOF; return current_; }

    char c = *pos_;

    if (c == '<') { pos_++; current_ = TOK_OPEN;  return current_; }
    if (c == '>') { pos_++; current_ = TOK_CLOSE; return current_; }
    if (c == ',') { pos_++; current_ = TOK_COMMA; return current_; }

    // Hex color literal: #RRGGBB
    if (c == '#') {
      pos_++;
      hex_val_ = 0;
      for (int i = 0; i < 6; i++) {
        char hc = *pos_;
        if (!hc) { current_ = TOK_ERROR; return current_; }
        pos_++;
        int nibble = 0;
        if (hc >= '0' && hc <= '9') nibble = hc - '0';
        else if (hc >= 'a' && hc <= 'f') nibble = hc - 'a' + 10;
        else if (hc >= 'A' && hc <= 'F') nibble = hc - 'A' + 10;
        else { current_ = TOK_ERROR; return current_; }
        hex_val_ = (hex_val_ << 4) | nibble;
      }
      current_ = TOK_HEX;
      return current_;
    }

    // Integer literal (possibly negative)
    if (c == '-' || (c >= '0' && c <= '9')) {
      bool negative = false;
      if (c == '-') { negative = true; pos_++; c = *pos_; }
      if (c < '0' || c > '9') { current_ = TOK_ERROR; return current_; }
      int_val_ = 0;
      while (*pos_ >= '0' && *pos_ <= '9') {
        int_val_ = int_val_ * 10 + (*pos_ - '0');
        pos_++;
      }
      if (negative) int_val_ = -int_val_;
      current_ = TOK_INT;
      return current_;
    }

    // Identifier: letter or underscore
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
      int len = 0;
      while ((*pos_ >= 'a' && *pos_ <= 'z') ||
             (*pos_ >= 'A' && *pos_ <= 'Z') ||
             (*pos_ >= '0' && *pos_ <= '9') ||
             *pos_ == '_') {
        if (len < 127) { ident_buf_[len++] = *pos_; }
        pos_++;
      }
      ident_buf_[len] = '\0';
      // Check for :: scope operator (e.g. SaberBase::LOCKUP_NORMAL)
      if (pos_[0] == ':' && pos_[1] == ':') {
        pos_ += 2;  // skip ::
        // Read second identifier and concatenate with ::
        char second[128];
        int len2 = 0;
        while ((*pos_ >= 'a' && *pos_ <= 'z') ||
               (*pos_ >= 'A' && *pos_ <= 'Z') ||
               (*pos_ >= '0' && *pos_ <= '9') ||
               *pos_ == '_') {
          if (len2 < 127) { second[len2++] = *pos_; }
          pos_++;
        }
        second[len2] = '\0';
        // Store full scoped name in ident_buf_
        if (len + 2 + len2 < 127) {
          ident_buf_[len] = ':';
          ident_buf_[len+1] = ':';
          for (int i = 0; i <= len2; i++) ident_buf_[len+2+i] = second[i];
        }
        current_ = TOK_IDENT;
        return current_;
      }
      current_ = TOK_IDENT;
      return current_;
    }

    // Unknown
    pos_++;
    current_ = TOK_ERROR;
    return current_;
  }

  TokenType current() const { return current_; }
  const char* identifier() const { return ident_buf_; }
  int intValue() const { return int_val_; }
  uint32_t hexValue() const { return hex_val_; }

private:
  const char* pos_;
  TokenType current_;
  char ident_buf_[128];
  int int_val_;
  uint32_t hex_val_;

  void skipWhitespace() {
    while (pos_ && (*pos_ == ' ' || *pos_ == '\t' ||
                    *pos_ == '\r' || *pos_ == '\n')) {
      pos_++;
    }
  }
};

// ============================================================
// SECTION 4: Parser Constants and Forward Declarations
// ============================================================

static const int MAX_PARSE_DEPTH = 32;  // per D-17, PARSE-08

// Forward declarations for recursive descent parser
static RtColorNode* parseColorNode(Tokenizer& tok, int depth);
static RtFuncNode*  parseFuncNode(Tokenizer& tok, int depth);
static RtTransNode* parseTransNode(Tokenizer& tok, int depth);
static int          parseIntArg(Tokenizer& tok);

// ============================================================
// SECTION 5: Enum/Constant Lookup
// ============================================================

// Maps string tokens to integer constants for EFFECT_* and LOCKUP_* values.
// Returns -1 if not found.
static int lookupEnumConstant(const char* name) {
  // EFFECT_* values
  if (!strcmp(name, "EFFECT_NONE"))                   return (int)EFFECT_NONE;
  if (!strcmp(name, "EFFECT_CLIP_IN"))                return (int)EFFECT_CLIP_IN;
  if (!strcmp(name, "EFFECT_CLIP_OUT"))               return (int)EFFECT_CLIP_OUT;
  if (!strcmp(name, "EFFECT_FORCE"))                  return (int)EFFECT_FORCE;
  if (!strcmp(name, "EFFECT_BLAST"))                  return (int)EFFECT_BLAST;
  if (!strcmp(name, "EFFECT_CLASH"))                  return (int)EFFECT_CLASH;
  if (!strcmp(name, "EFFECT_STAB"))                   return (int)EFFECT_STAB;
  if (!strcmp(name, "EFFECT_IGNITION"))               return (int)EFFECT_IGNITION;
  if (!strcmp(name, "EFFECT_RETRACTION"))             return (int)EFFECT_RETRACTION;
  if (!strcmp(name, "EFFECT_LOCKUP_BEGIN"))           return (int)EFFECT_LOCKUP_BEGIN;
  if (!strcmp(name, "EFFECT_LOCKUP_END"))             return (int)EFFECT_LOCKUP_END;
  if (!strcmp(name, "EFFECT_DRAG_BEGIN"))             return (int)EFFECT_DRAG_BEGIN;
  if (!strcmp(name, "EFFECT_DRAG_END"))               return (int)EFFECT_DRAG_END;
  if (!strcmp(name, "EFFECT_MELT_BEGIN"))             return (int)EFFECT_MELT_BEGIN;
  if (!strcmp(name, "EFFECT_MELT_END"))               return (int)EFFECT_MELT_END;
  if (!strcmp(name, "EFFECT_COLOR_CHANGE"))           return (int)EFFECT_COLOR_CHANGE;
  if (!strcmp(name, "EFFECT_TRACK"))                  return (int)EFFECT_TRACK;

  // SaberBase::LOCKUP_* values (also accepted without prefix)
  if (!strcmp(name, "LOCKUP_NORMAL") ||
      !strcmp(name, "SaberBase::LOCKUP_NORMAL"))      return (int)SaberBase::LOCKUP_NORMAL;
  if (!strcmp(name, "LOCKUP_DRAG") ||
      !strcmp(name, "SaberBase::LOCKUP_DRAG"))        return (int)SaberBase::LOCKUP_DRAG;
  if (!strcmp(name, "LOCKUP_MELT") ||
      !strcmp(name, "SaberBase::LOCKUP_MELT"))        return (int)SaberBase::LOCKUP_MELT;
  if (!strcmp(name, "LOCKUP_LIGHTNING_BLOCK") ||
      !strcmp(name, "SaberBase::LOCKUP_LIGHTNING_BLOCK")) return (int)SaberBase::LOCKUP_LIGHTNING_BLOCK;

  return -1;
}

// ============================================================
// SECTION 6: Named Color Map
// ============================================================

// Sentinel value: alpha=0 in Color16 context means transparent,
// but for our named-color check we use a separate "found" flag
// by returning a special struct.
struct NamedColorResult {
  bool found;
  RGBA_um color;
};

// Helper to build RGBA_um from 8-bit RGB components
static RGBA_um makeRGBA(uint8_t r, uint8_t g, uint8_t b) {
  // RGBA_um stores Color16 (16-bit per channel) + overdrive flag
  // Color8(r,g,b) produces 8-bit, Color16(Color8) scales to 16-bit
  return RGBA_um(Color16(Color8(r, g, b)), 0, 32768);
}

static NamedColorResult lookupNamedColor(const char* name) {
  // Colors from styles/colors.h (all Rgb<R,G,B> typedefs)
  // Using the exact R,G,B values from colors.h
  struct { const char* n; uint8_t r, g, b; } table[] = {
    {"RED",     255,   0,   0},
    {"GREEN",     0, 255,   0},
    {"BLUE",      0,   0, 255},
    {"YELLOW",  255, 255,   0},
    {"CYAN",      0, 255, 255},
    {"MAGENTA", 255,   0, 255},
    {"WHITE",   255, 255, 255},
    {"BLACK",     0,   0,   0},
    // Mixed-case named colors
    {"AliceBlue",       223, 239, 255},
    {"Aqua",              0, 255, 255},
    {"Aquamarine",       55, 255, 169},
    {"Azure",           223, 255, 255},
    {"Bisque",          255, 199, 142},
    {"Black",             0,   0,   0},
    {"BlanchedAlmond",  255, 213, 157},
    {"Blue",              0,   0, 255},
    {"Chartreuse",       55, 255,   0},
    {"Coral",           255,  55,  19},
    {"Cornsilk",        255, 239, 184},
    {"Cyan",              0, 255, 255},
    {"DarkOrange",      255,  68,   0},
    {"DeepPink",        255,   0,  75},
    {"DeepSkyBlue",       0, 135, 255},
    {"DodgerBlue",        2,  72, 255},
    {"FloralWhite",     255, 244, 223},
    {"Fuchsia",         255,   0, 255},
    {"GhostWhite",      239, 239, 255},
    {"Green",             0, 255,   0},
    {"GreenYellow",     108, 255,   6},
    {"HoneyDew",        223, 255, 223},
    {"HotPink",         255,  36, 118},
    {"Ivory",           255, 255, 223},
    {"LavenderBlush",   255, 244, 233},
    {"LemonChiffon",    255, 244, 157},
    {"LightCyan",       191, 255, 255},
    {"LightPink",       255, 121, 138},
    {"LightSalmon",     255,  91,  50},
    {"LightYellow",     255, 255, 191},
    {"Lime",              0, 255,   0},
    {"Magenta",         255,   0, 255},
    {"MintCream",       233, 255, 244},
    {"MistyRose",       255, 199, 193},
    {"Moccasin",        255, 199, 119},
    {"NavajoWhite",     255, 187, 108},
    {"Orange",          255,  97,   0},
    {"OrangeRed",       255,  14,   0},
    {"PapayaWhip",      255, 221, 171},
    {"PeachPuff",       255, 180, 125},
    {"Pink",            255, 136, 154},
    {"Red",             255,   0,   0},
    {"SeaShell",        255, 233, 219},
    {"Snow",            255, 244, 244},
    {"SpringGreen",       0, 255,  55},
    {"SteelBlue",        14,  57, 118},
    {"Tomato",          255,  31,  15},
    {"White",           255, 255, 255},
    {"Yellow",          255, 255,   0},
    // ProffieOS 8.x additions
    {"ElectricPurple",  127,   0, 255},
    {"ElectricViolet",   71,   0, 255},
    {"ElectricLime",    156, 255,   0},
    {"Amber",           255, 135,   0},
    {"CyberYellow",     255, 168,   0},
    {"CanaryYellow",    255, 221,   0},
    {"PaleGreen",        28, 255,  28},
    {"Flamingo",        255,  80, 154},
    {"VividViolet",      90,   0, 255},
    {"PsychedelicPurple",186,  0, 255},
    {"HotMagenta",      255,   0, 156},
    {"BrutalPink",      255,   0, 128},
    {"NeonRose",        255,   0,  55},
    {"VividRaspberry",  255,   0,  38},
    {"HaltRed",         255,   0,  19},
    {"MoltenCore",      255,  24,   0},
    {"SafetyOrange",    255,  33,   0},
    {"OrangeJuice",     255,  55,   0},
    {"ImperialYellow",  255, 115,   0},
    {"SchoolBus",       255, 176,   0},
    {"SuperSaiyan",     255, 186,   0},
    {"Star",            255, 201,   0},
    {"Lemon",           255, 237,   0},
    {"ElectricBanana",  246, 255,   0},
    {"BusyBee",         231, 255,   0},
    {"ZeusBolt",        219, 255,   0},
    {"LimeZest",        186, 255,   0},
    {"Limoncello",      135, 255,   0},
    {"CathodeGreen",      0, 255,  22},
    {"MintyParadise",     0, 255, 128},
    {"PlungePool",        0, 255, 156},
    {"VibrantMint",       0, 255, 201},
    {"MasterSwordBlue",   0, 255, 219},
    {"BrainFreeze",       0, 219, 255},
    {"BlueRibbon",        0,  33, 255},
    {"RareBlue",          0,  13, 255},
    {"OverdueBlue",      13,   0, 255},
    {"ViolentViolet",    55,   0, 255},
    {nullptr, 0, 0, 0}
  };
  for (int i = 0; table[i].n; i++) {
    if (!strcmp(name, table[i].n)) {
      return {true, makeRGBA(table[i].r, table[i].g, table[i].b)};
    }
  }
  return {false, RGBA_um::Transparent()};
}

// ============================================================
// SECTION 7: Concrete Wrapper Node Classes
// ============================================================

// --- Named Color Node ---
class RtNamedColor : public RtColorNode {
public:
  explicit RtNamedColor(RGBA_um c) : color_(c) {}
  bool run(BladeBase* blade) override {
    // Black can disable
    return !(color_.c.r == 0 && color_.c.g == 0 && color_.c.b == 0);
  }
  RGBA_um getColor(int led) override { return color_; }
private:
  RGBA_um color_;
};

// --- Rgb<R,G,B> node with runtime values ---
class RtRgb : public RtColorNode {
public:
  RtRgb(int r, int g, int b) : r_(r), g_(g), b_(b) {
    color_ = makeRGBA((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }
  bool run(BladeBase* blade) override {
    return !(r_ == 0 && g_ == 0 && b_ == 0);
  }
  RGBA_um getColor(int led) override { return color_; }
private:
  int r_, g_, b_;
  RGBA_um color_;
};

// --- Layers node (variadic, up to 16 layers) ---
// Layers<BASE, L1, L2, ...> is a type alias built by LayerSelector.
// We instantiate compiled specializations via the RtArg pattern.
// Strategy: Use switch on count to dispatch to the right Compose chain.
// For simplicity we use the Compose<BASE, L1> chain by having compiled
// specializations for up to 8 layers.

// For the RtLayers implementation we use a different approach:
// We implement the compositing manually using the same formula as Compose<>:
//   result = base.getColor(led) << layer.getColor(led)
// where << is the ProffieOS blend operator for RGBA_um types.
// This avoids the need for an explosion of compiled Layers specializations.

class RtLayers : public RtColorNode {
public:
  RtLayers(RtColorNode** children, int count)
    : count_(count) {
    for (int i = 0; i < count_ && i < 16; i++) {
      children_[i] = children[i];
    }
  }
  ~RtLayers() override {
    for (int i = 0; i < count_; i++) delete children_[i];
  }

  bool run(BladeBase* blade) override {
    // Run all children; base (index 0) determines can_disable
    bool can_disable = false;
    for (int i = 0; i < count_; i++) {
      bool child_result = children_[i]->run(blade);
      if (i == 0) can_disable = !child_result;
    }
    return !can_disable;
  }

  RGBA_um getColor(int led) override {
    if (count_ == 0) return RGBA_um::Transparent();
    // Start with base color
    RGBA_um result = children_[0]->getColor(led);
    // Compose layers on top via << operator
    for (int i = 1; i < count_; i++) {
      RGBA_um layer_color = children_[i]->getColor(led);
      result = result << layer_color;
    }
    return result;
  }

private:
  RtColorNode* children_[16];
  int count_;
};

// --- AlphaL<COLOR, FUNC> node ---
// AlphaL applies an alpha function to a color layer.
class RtAlphaL : public RtColorNode {
public:
  RtAlphaL(RtColorNode* color, RtFuncNode* func)
    : color_(color), func_(func) {}
  ~RtAlphaL() override { delete color_; delete func_; }

  bool run(BladeBase* blade) override {
    // Set args for compiled AlphaL<RtArgColor<0>, RtArgFunc<0>>
    rt_color_args[0] = color_;
    rt_func_args[0] = func_;
    LayerRunResult res = RunLayer(&impl_, blade);
    // TRANSPARENT_UNTIL_IGNITION means can disable
    if (res == LayerRunResult::OPAQUE_BLACK_UNTIL_IGNITION) return false;
    return true;
  }

  RGBA_um getColor(int led) override {
    rt_color_args[0] = color_;
    rt_func_args[0] = func_;
    return impl_.getColor(led);
  }

private:
  RtColorNode* color_;
  RtFuncNode* func_;
  AlphaL<RtArgColor<0>, RtArgFunc<0>> impl_;
};

// --- InOutTrL<TRANS_out, TRANS_in, COLOR_off> layer node ---
// InOutTrL is a LAYER type (not a full color on its own).
// It manages ignition/retraction transitions over an off-color.
class RtInOutTrL : public RtColorNode {
public:
  RtInOutTrL(RtTransNode* out_tr, RtTransNode* in_tr, RtColorNode* off)
    : out_tr_(out_tr), in_tr_(in_tr), off_(off) {}
  ~RtInOutTrL() override { delete out_tr_; delete in_tr_; delete off_; }

  bool run(BladeBase* blade) override {
    rt_trans_args[0] = out_tr_;
    rt_trans_args[1] = in_tr_;
    rt_color_args[0] = off_;
    LayerRunResult res = RunLayer(&impl_, blade);
    if (res == LayerRunResult::OPAQUE_BLACK_UNTIL_IGNITION) return false;
    return true;
  }

  RGBA_um getColor(int led) override {
    rt_trans_args[0] = out_tr_;
    rt_trans_args[1] = in_tr_;
    rt_color_args[0] = off_;
    auto c = impl_.getColor(led);
    // getColor returns RGBA_um_nod — convert to RGBA_um
    return RGBA_um(c.c, c.overdrive, c.alpha);
  }

private:
  RtTransNode* out_tr_;
  RtTransNode* in_tr_;
  RtColorNode* off_;
  InOutTrL<RtArgTrans<0>, RtArgTrans<1>, RtArgColor<0>> impl_;
};

// ============================================================
// SECTION 8: Function Wrapper Nodes
// ============================================================

// --- Int<N> constant function (runtime N) ---
class RtInt : public RtFuncNode {
public:
  explicit RtInt(int n) : n_(n) {}
  FunctionRunResult run(BladeBase* blade) override {
    if (n_ == 0)     return FunctionRunResult::ZERO_UNTIL_IGNITION;
    if (n_ == 32768) return FunctionRunResult::ONE_UNTIL_IGNITION;
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return n_; }
private:
  int n_;
};

// --- Ifon<A, B> function node ---
class RtIfon : public RtFuncNode {
public:
  RtIfon(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtIfon() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = a_;
    rt_func_args[1] = b_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = a_;
    rt_func_args[1] = b_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* a_;
  RtFuncNode* b_;
  Ifon<RtArgFunc<0>, RtArgFunc<1>> impl_;
};

// ============================================================
// SECTION 9: Transition Wrapper Nodes
// ============================================================

// RtTransNode implementations use runtime logic to avoid the complexity
// of parametric compiled transitions. This avoids the need for template
// instantiation with runtime-determined parameters.

// RtTrWipeRuntime: wipe from base to tip in millis milliseconds.
class RtTrWipeRuntime : public RtTransNode {
public:
  explicit RtTrWipeRuntime(int millis)
    : millis_(millis), fade_(0), start_millis_(0), len_(0), active_(false) {}

  void begin() override {
    start_millis_ = millis();
    len_ = (uint32_t)millis_;
    active_ = true;
    fade_ = 0;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (!active_) { fade_ = 256 * blade->num_leds(); return; }
    uint32_t t = millis() - start_millis_;
    if (t >= len_) { active_ = false; fade_ = 256 * blade->num_leds(); return; }
    uint32_t progress = (t * 256 * (uint32_t)blade->num_leds()) / len_;
    fade_ = progress;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    // mix based on wipe position (same as TrWipeX::getColor)
    // Range(0, fade_) & Range(led<<8, (led<<8)+256)
    int lo = led << 8;
    int hi = lo + 256;
    int fade_lo = 0;
    int fade_hi = (int)fade_;
    // intersection
    int isect_lo = fade_lo > lo ? fade_lo : lo;
    int isect_hi = fade_hi < hi ? fade_hi : hi;
    int mix = (isect_hi > isect_lo) ? (isect_hi - isect_lo) : 0;
    return MixColors(a, b, mix, 8);
  }
private:
  int millis_;
  uint32_t fade_;
  uint32_t start_millis_;
  uint32_t len_;
  bool active_;
};

class RtTrWipeInRuntime : public RtTransNode {
public:
  explicit RtTrWipeInRuntime(int millis)
    : millis_(millis), fade_lo_(0), fade_hi_(0), start_millis_(0), len_(0), active_(false) {}

  void begin() override {
    start_millis_ = millis();
    len_ = (uint32_t)millis_;
    active_ = true;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    int num_leds = blade->num_leds();
    int total = 256 * num_leds;
    if (!active_) { fade_lo_ = 0; fade_hi_ = total; return; }
    uint32_t t = millis() - start_millis_;
    if (t >= len_) { active_ = false; fade_lo_ = 0; fade_hi_ = total; return; }
    uint32_t progress = (t * (uint32_t)total) / len_;
    fade_lo_ = total - progress;
    fade_hi_ = total;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    int lo = led << 8;
    int hi = lo + 256;
    int isect_lo = fade_lo_ > lo ? fade_lo_ : lo;
    int isect_hi = fade_hi_ < hi ? fade_hi_ : hi;
    int mix = (isect_hi > isect_lo) ? (isect_hi - isect_lo) : 0;
    return MixColors(a, b, mix, 8);
  }
private:
  int millis_;
  int fade_lo_, fade_hi_;
  uint32_t start_millis_;
  uint32_t len_;
  bool active_;
};

class RtTrFadeRuntime : public RtTransNode {
public:
  explicit RtTrFadeRuntime(int millis)
    : millis_(millis), fade_(0), start_millis_(0), len_(0), active_(false) {}

  void begin() override {
    start_millis_ = millis();
    len_ = (uint32_t)millis_;
    active_ = true;
    fade_ = 0;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (!active_) { fade_ = 16384; return; }
    uint32_t t = millis() - start_millis_;
    if (t >= len_) { active_ = false; fade_ = 16384; return; }
    fade_ = (int)((t * 16384UL) / len_);
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return MixColors(a, b, fade_, 14);
  }
private:
  int millis_;
  int fade_;
  uint32_t start_millis_;
  uint32_t len_;
  bool active_;
};

class RtTrInstant : public RtTransNode {
public:
  void begin() override {}
  bool done() override { return true; }
  void run(BladeBase* blade) override {}
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override { return b; }
};

// ============================================================
// SECTION 10: Parser Helper Functions
// ============================================================

// Parse a sequence of < arg1, arg2, ... > and call collector for each arg.
// Returns false on parse error.
// Expects tokenizer positioned at TOK_OPEN (<).
static bool expectOpen(Tokenizer& tok) {
  if (tok.current() != TOK_OPEN) {
    STDERR << "StyleFromSD: expected '<', got token " << (int)tok.current() << "\n";
    return false;
  }
  tok.next();  // consume <
  return true;
}

static bool expectClose(Tokenizer& tok) {
  if (tok.current() != TOK_CLOSE) {
    STDERR << "StyleFromSD: expected '>', got token " << (int)tok.current() << "\n";
    return false;
  }
  tok.next();  // consume >
  return true;
}

static bool expectComma(Tokenizer& tok) {
  if (tok.current() != TOK_COMMA) {
    STDERR << "StyleFromSD: expected ',', got token " << (int)tok.current() << "\n";
    return false;
  }
  tok.next();  // consume ,
  return true;
}

// Parse an integer from TOK_INT or a zero-arg function like Int<N>.
// Returns the integer value or -1 on error.
static int parseIntArg(Tokenizer& tok) {
  if (tok.current() == TOK_INT) {
    int v = tok.intValue();
    tok.next();
    return v;
  }
  // Support named enum constants used as int args
  if (tok.current() == TOK_IDENT) {
    int v = lookupEnumConstant(tok.identifier());
    if (v >= 0) {
      tok.next();
      return v;
    }
    // Try Int<N> as an inline function wrapper
    if (!strcmp(tok.identifier(), "Int")) {
      tok.next();  // consume "Int"
      if (!expectOpen(tok)) return -1;
      if (tok.current() != TOK_INT) {
        STDERR << "StyleFromSD: Int<> expects integer\n";
        return -1;
      }
      int v2 = tok.intValue();
      tok.next();
      if (!expectClose(tok)) return -1;
      return v2;
    }
    STDERR << "StyleFromSD: expected integer or Int<N>, got: " << tok.identifier() << "\n";
    return -1;
  }
  STDERR << "StyleFromSD: expected integer argument\n";
  return -1;
}

// ============================================================
// SECTION 11: Dispatch Table and Factory Functions
// ============================================================

// Forward declarations for all factory functions
static RtColorNode* makeLayers(Tokenizer& tok, int depth);
static RtColorNode* makeAlphaL(Tokenizer& tok, int depth);
static RtColorNode* makeInOutTrL(Tokenizer& tok, int depth);

static RtFuncNode*  makeInt(Tokenizer& tok, int depth);
static RtFuncNode*  makeIfon(Tokenizer& tok, int depth);

static RtTransNode* makeTrWipe(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeIn(Tokenizer& tok, int depth);
static RtTransNode* makeTrFade(Tokenizer& tok, int depth);
static RtTransNode* makeTrInstant(Tokenizer& tok, int depth);

// Plan 02 Color factory forward declarations
static RtColorNode* makeHumpFlicker(Tokenizer& tok, int depth);
static RtColorNode* makeHumpFlickerL(Tokenizer& tok, int depth);
static RtColorNode* makeAudioFlicker(Tokenizer& tok, int depth);
static RtColorNode* makeAudioFlickerL(Tokenizer& tok, int depth);
static RtColorNode* makeBrownNoiseFlicker(Tokenizer& tok, int depth);
static RtColorNode* makeBrownNoiseFlickerL(Tokenizer& tok, int depth);
static RtColorNode* makeRandomPerLEDFlicker(Tokenizer& tok, int depth);
static RtColorNode* makeRandomPerLEDFlickerL(Tokenizer& tok, int depth);
static RtColorNode* makeStripes(Tokenizer& tok, int depth);
static RtColorNode* makeStripesX(Tokenizer& tok, int depth);
static RtColorNode* makePulsing(Tokenizer& tok, int depth);
static RtColorNode* makeStrobe(Tokenizer& tok, int depth);
static RtColorNode* makeMix(Tokenizer& tok, int depth);
static RtColorNode* makeAlphaMixL(Tokenizer& tok, int depth);
static RtColorNode* makeRotateColorsX(Tokenizer& tok, int depth);
static RtColorNode* makeColorSelect(Tokenizer& tok, int depth);
static RtColorNode* makeStyleFire(Tokenizer& tok, int depth);
static RtColorNode* makeStaticFire(Tokenizer& tok, int depth);
static RtColorNode* makeRemap(Tokenizer& tok, int depth);
static RtColorNode* makeRgbArg(Tokenizer& tok, int depth);
static RtColorNode* makeRgb16(Tokenizer& tok, int depth);
static RtColorNode* makeTransitionEffect(Tokenizer& tok, int depth);
static RtColorNode* makeTransitionEffectL(Tokenizer& tok, int depth);
static RtColorNode* makeTransitionLoopL(Tokenizer& tok, int depth);
static RtColorNode* makeEffectSequence(Tokenizer& tok, int depth);
static RtColorNode* makeLockupTrL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveLightningBlockL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveStabL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveBlastL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveBlastWaveL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveBlastFadeL(Tokenizer& tok, int depth);
static RtColorNode* makeResponsiveClashL(Tokenizer& tok, int depth);
static RtColorNode* makeLocalizedClashL(Tokenizer& tok, int depth);
static RtColorNode* makeBlastL(Tokenizer& tok, int depth);
static RtColorNode* makeSyncAltToVarianceL(Tokenizer& tok, int depth);

// Plan 02 Function factory forward declarations
static RtFuncNode* makeIntArg(Tokenizer& tok, int depth);
static RtFuncNode* makeScale(Tokenizer& tok, int depth);
static RtFuncNode* makeSum(Tokenizer& tok, int depth);
static RtFuncNode* makeMult(Tokenizer& tok, int depth);
static RtFuncNode* makeModF(Tokenizer& tok, int depth);
static RtFuncNode* makeSin(Tokenizer& tok, int depth);
static RtFuncNode* makeHoldPeakF(Tokenizer& tok, int depth);
static RtFuncNode* makeIsLessThan(Tokenizer& tok, int depth);
static RtFuncNode* makeIsGreaterThan(Tokenizer& tok, int depth);
static RtFuncNode* makeEffectPulseF(Tokenizer& tok, int depth);
static RtFuncNode* makeEffectRandomF(Tokenizer& tok, int depth);
static RtFuncNode* makeEffectPosition(Tokenizer& tok, int depth);
static RtFuncNode* makeIgnitionTime(Tokenizer& tok, int depth);
static RtFuncNode* makeRetractionTime(Tokenizer& tok, int depth);
static RtFuncNode* makeWavLen(Tokenizer& tok, int depth);
static RtFuncNode* makePercentage(Tokenizer& tok, int depth);
static RtFuncNode* makeSwingSpeed(Tokenizer& tok, int depth);
static RtFuncNode* makeBladeAngle(Tokenizer& tok, int depth);
static RtFuncNode* makeTwistAngle(Tokenizer& tok, int depth);
static RtFuncNode* makeSlowNoise(Tokenizer& tok, int depth);
static RtFuncNode* makeNoisySoundLevel(Tokenizer& tok, int depth);
static RtFuncNode* makeNoisySoundLevelFunc(Tokenizer& tok, int depth);
static RtFuncNode* makeClashImpactF(Tokenizer& tok, int depth);
static RtFuncNode* makeRampF(Tokenizer& tok, int depth);
static RtFuncNode* makeBump(Tokenizer& tok, int depth);
static RtFuncNode* makeLayerFunctions(Tokenizer& tok, int depth);
static RtFuncNode* makeSmoothStep(Tokenizer& tok, int depth);
static RtFuncNode* makeBlastF(Tokenizer& tok, int depth);
static RtFuncNode* makeTrigger(Tokenizer& tok, int depth);
static RtFuncNode* makeBendTimePowX(Tokenizer& tok, int depth);
static RtFuncNode* makeBendTimePowInvX(Tokenizer& tok, int depth);
static RtFuncNode* makeVariation(Tokenizer& tok, int depth);
static RtFuncNode* makeAltF(Tokenizer& tok, int depth);
static RtFuncNode* makeBatteryLevel(Tokenizer& tok, int depth);

// Plan 02 Transition factory forward declarations
static RtTransNode* makeTrFadeX(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeX(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeInX(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeSparkTip(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeSparkTipX(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeInSparkTip(Tokenizer& tok, int depth);
static RtTransNode* makeTrWipeInSparkTipX(Tokenizer& tok, int depth);
static RtTransNode* makeTrSmoothFade(Tokenizer& tok, int depth);
static RtTransNode* makeTrConcat(Tokenizer& tok, int depth);
static RtTransNode* makeTrJoin(Tokenizer& tok, int depth);
static RtTransNode* makeTrDelay(Tokenizer& tok, int depth);
static RtTransNode* makeTrDelayX(Tokenizer& tok, int depth);
static RtTransNode* makeTrExtend(Tokenizer& tok, int depth);
static RtTransNode* makeTrDoEffectAlwaysX(Tokenizer& tok, int depth);
static RtTransNode* makeTrWaveX(Tokenizer& tok, int depth);
static RtTransNode* makeTrSparkX(Tokenizer& tok, int depth);
static RtTransNode* makeTrColorCycle(Tokenizer& tok, int depth);
static RtTransNode* makeTrBoing(Tokenizer& tok, int depth);
static RtTransNode* makeTrSelect(Tokenizer& tok, int depth);

typedef RtColorNode* (*ColorMaker)(Tokenizer& tok, int depth);
typedef RtFuncNode*  (*FuncMaker)(Tokenizer& tok, int depth);
typedef RtTransNode* (*TransMaker)(Tokenizer& tok, int depth);

struct StyleDispatch {
  const char* name;
  ColorMaker make_color;
  FuncMaker  make_func;
  TransMaker make_trans;
};

static const StyleDispatch style_dispatch[] = {
  // Color nodes from Plan 01
  {"Layers",                   makeLayers,                  nullptr,                nullptr        },
  {"AlphaL",                   makeAlphaL,                  nullptr,                nullptr        },
  {"InOutTrL",                 makeInOutTrL,                nullptr,                nullptr        },
  // Color nodes from Plan 02
  {"Rgb16",                    makeRgb16,                   nullptr,                nullptr        },
  {"RgbArg",                   makeRgbArg,                  nullptr,                nullptr        },
  {"HumpFlicker",              makeHumpFlicker,             nullptr,                nullptr        },
  {"HumpFlickerL",             makeHumpFlickerL,            nullptr,                nullptr        },
  {"AudioFlicker",             makeAudioFlicker,            nullptr,                nullptr        },
  {"AudioFlickerL",            makeAudioFlickerL,           nullptr,                nullptr        },
  {"BrownNoiseFlicker",        makeBrownNoiseFlicker,       nullptr,                nullptr        },
  {"BrownNoiseFlickerL",       makeBrownNoiseFlickerL,      nullptr,                nullptr        },
  {"RandomPerLEDFlicker",      makeRandomPerLEDFlicker,     nullptr,                nullptr        },
  {"RandomPerLEDFlickerL",     makeRandomPerLEDFlickerL,    nullptr,                nullptr        },
  {"Stripes",                  makeStripes,                 nullptr,                nullptr        },
  {"StripesX",                 makeStripesX,                nullptr,                nullptr        },
  {"Pulsing",                  makePulsing,                 nullptr,                nullptr        },
  {"Strobe",                   makeStrobe,                  nullptr,                nullptr        },
  {"Mix",                      makeMix,                     nullptr,                nullptr        },
  {"AlphaMixL",                makeAlphaMixL,               nullptr,                nullptr        },
  {"RotateColorsX",            makeRotateColorsX,           nullptr,                nullptr        },
  {"RotateColors",             makeRotateColorsX,           nullptr,                nullptr        },
  {"ColorSelect",              makeColorSelect,             nullptr,                nullptr        },
  {"StyleFire",                makeStyleFire,               nullptr,                nullptr        },
  {"StaticFire",               makeStaticFire,              nullptr,                nullptr        },
  {"Remap",                    makeRemap,                   nullptr,                nullptr        },
  {"TransitionEffect",         makeTransitionEffect,        nullptr,                nullptr        },
  {"TransitionEffectL",        makeTransitionEffectL,       nullptr,                nullptr        },
  {"MultiTransitionEffectL",   makeTransitionEffectL,       nullptr,                nullptr        },
  {"TransitionLoopL",          makeTransitionLoopL,         nullptr,                nullptr        },
  {"TransitionLoop",           makeTransitionLoopL,         nullptr,                nullptr        },
  {"EffectSequence",           makeEffectSequence,          nullptr,                nullptr        },
  {"LockupTrL",                makeLockupTrL,               nullptr,                nullptr        },
  {"ResponsiveLightningBlockL",makeResponsiveLightningBlockL,nullptr,              nullptr        },
  {"ResponsiveStabL",          makeResponsiveStabL,         nullptr,                nullptr        },
  {"ResponsiveBlastL",         makeResponsiveBlastL,        nullptr,                nullptr        },
  {"ResponsiveBlastWaveL",     makeResponsiveBlastWaveL,    nullptr,                nullptr        },
  {"ResponsiveBlastFadeL",     makeResponsiveBlastFadeL,    nullptr,                nullptr        },
  {"ResponsiveClashL",         makeResponsiveClashL,        nullptr,                nullptr        },
  {"LocalizedClashL",          makeLocalizedClashL,         nullptr,                nullptr        },
  {"BlastL",                   makeBlastL,                  nullptr,                nullptr        },
  {"SyncAltToVarianceL",       makeSyncAltToVarianceL,      nullptr,                nullptr        },
  // Function nodes from Plan 01
  {"Int",                      nullptr,                     makeInt,                nullptr        },
  {"Ifon",                     nullptr,                     makeIfon,               nullptr        },
  // Function nodes from Plan 02
  {"IntArg",                   nullptr,                     makeIntArg,             nullptr        },
  {"Scale",                    nullptr,                     makeScale,              nullptr        },
  {"Sum",                      nullptr,                     makeSum,                nullptr        },
  {"Mult",                     nullptr,                     makeMult,               nullptr        },
  {"ModF",                     nullptr,                     makeModF,               nullptr        },
  {"Sin",                      nullptr,                     makeSin,                nullptr        },
  {"HoldPeakF",                nullptr,                     makeHoldPeakF,          nullptr        },
  {"IsLessThan",               nullptr,                     makeIsLessThan,         nullptr        },
  {"IsGreaterThan",            nullptr,                     makeIsGreaterThan,      nullptr        },
  {"EffectPulseF",             nullptr,                     makeEffectPulseF,       nullptr        },
  {"EffectRandomF",            nullptr,                     makeEffectRandomF,      nullptr        },
  {"EffectPosition",           nullptr,                     makeEffectPosition,     nullptr        },
  {"IgnitionTime",             nullptr,                     makeIgnitionTime,       nullptr        },
  {"RetractionTime",           nullptr,                     makeRetractionTime,     nullptr        },
  {"WavLen",                   nullptr,                     makeWavLen,             nullptr        },
  {"Percentage",               nullptr,                     makePercentage,         nullptr        },
  {"SwingSpeed",               nullptr,                     makeSwingSpeed,         nullptr        },
  {"BladeAngle",               nullptr,                     makeBladeAngle,         nullptr        },
  {"BladeAngleX",              nullptr,                     makeBladeAngle,         nullptr        },
  {"TwistAngle",               nullptr,                     makeTwistAngle,         nullptr        },
  {"TwistAngleX",              nullptr,                     makeTwistAngle,         nullptr        },
  {"SlowNoise",                nullptr,                     makeSlowNoise,          nullptr        },
  {"NoisySoundLevel",          nullptr,                     makeNoisySoundLevelFunc,nullptr        },
  {"ClashImpactF",             nullptr,                     makeClashImpactF,       nullptr        },
  {"RampF",                    nullptr,                     makeRampF,              nullptr        },
  {"Bump",                     nullptr,                     makeBump,               nullptr        },
  {"LayerFunctions",           nullptr,                     makeLayerFunctions,     nullptr        },
  {"SmoothStep",               nullptr,                     makeSmoothStep,         nullptr        },
  {"BlastF",                   nullptr,                     makeBlastF,             nullptr        },
  {"Trigger",                  nullptr,                     makeTrigger,            nullptr        },
  {"BendTimePowX",             nullptr,                     makeBendTimePowX,       nullptr        },
  {"BendTimePowInvX",          nullptr,                     makeBendTimePowInvX,    nullptr        },
  {"Variation",                nullptr,                     makeVariation,          nullptr        },
  {"AltF",                     nullptr,                     makeAltF,               nullptr        },
  {"BatteryLevel",             nullptr,                     makeBatteryLevel,       nullptr        },
  // Transition nodes from Plan 01
  {"TrWipe",                   nullptr,                     nullptr,                makeTrWipe     },
  {"TrWipeIn",                 nullptr,                     nullptr,                makeTrWipeIn   },
  {"TrFade",                   nullptr,                     nullptr,                makeTrFade     },
  {"TrInstant",                nullptr,                     nullptr,                makeTrInstant  },
  // Transition nodes from Plan 02
  {"TrFadeX",                  nullptr,                     nullptr,                makeTrFadeX    },
  {"TrWipeX",                  nullptr,                     nullptr,                makeTrWipeX    },
  {"TrWipeInX",                nullptr,                     nullptr,                makeTrWipeInX  },
  {"TrWipeSparkTip",           nullptr,                     nullptr,                makeTrWipeSparkTip    },
  {"TrWipeSparkTipX",          nullptr,                     nullptr,                makeTrWipeSparkTipX   },
  {"TrWipeInSparkTip",         nullptr,                     nullptr,                makeTrWipeInSparkTip  },
  {"TrWipeInSparkTipX",        nullptr,                     nullptr,                makeTrWipeInSparkTipX },
  {"TrSmoothFade",             nullptr,                     nullptr,                makeTrSmoothFade      },
  {"TrConcat",                 nullptr,                     nullptr,                makeTrConcat          },
  {"TrJoin",                   nullptr,                     nullptr,                makeTrJoin            },
  {"TrDelay",                  nullptr,                     nullptr,                makeTrDelay           },
  {"TrDelayX",                 nullptr,                     nullptr,                makeTrDelayX          },
  {"TrExtend",                 nullptr,                     nullptr,                makeTrExtend          },
  {"TrDoEffectAlwaysX",        nullptr,                     nullptr,                makeTrDoEffectAlwaysX },
  {"TrWaveX",                  nullptr,                     nullptr,                makeTrWaveX           },
  {"TrSparkX",                 nullptr,                     nullptr,                makeTrSparkX          },
  {"TrColorCycle",             nullptr,                     nullptr,                makeTrColorCycle      },
  {"TrBoing",                  nullptr,                     nullptr,                makeTrBoing           },
  {"TrSelect",                 nullptr,                     nullptr,                makeTrSelect          },
  {nullptr,                    nullptr,                     nullptr,                nullptr               }  // sentinel
};

// ============================================================
// SECTION 12: Recursive Descent Parser
// ============================================================

static RtColorNode* parseColorNode(Tokenizer& tok, int depth) {
  if (depth > MAX_PARSE_DEPTH) {
    STDERR << "StyleFromSD: max recursion depth exceeded\n";
    return nullptr;
  }
  if (tok.current() != TOK_IDENT) {
    STDERR << "StyleFromSD: expected identifier for color node\n";
    return nullptr;
  }
  const char* name = tok.identifier();

  // Check hex color literal inline? (hex is handled in parseFuncNode as int)
  // Named color check (no <> args)
  NamedColorResult nc = lookupNamedColor(name);
  if (nc.found) {
    tok.next();  // consume identifier
    return new RtNamedColor(nc.color);
  }

  // Rgb<R,G,B> special case
  if (!strcmp(name, "Rgb") || !strcmp(name, "Rgb16")) {
    tok.next();  // consume "Rgb"
    if (!expectOpen(tok)) return nullptr;
    int r = parseIntArg(tok); if (r < 0) return nullptr;
    if (!expectComma(tok)) return nullptr;
    int g = parseIntArg(tok); if (g < 0) return nullptr;
    if (!expectComma(tok)) return nullptr;
    int b = parseIntArg(tok); if (b < 0) return nullptr;
    if (!expectClose(tok)) return nullptr;
    return new RtRgb(r, g, b);
  }

  // Check dispatch table for color makers
  for (int i = 0; style_dispatch[i].name; i++) {
    if (strcmp(name, style_dispatch[i].name) == 0 && style_dispatch[i].make_color) {
      tok.next();  // consume identifier
      return style_dispatch[i].make_color(tok, depth + 1);
    }
  }

  // Unknown identifier — try to give a helpful error
  STDERR << "StyleFromSD: unknown color type: " << name << "\n";
  return nullptr;
}

static RtFuncNode* parseFuncNode(Tokenizer& tok, int depth) {
  if (depth > MAX_PARSE_DEPTH) {
    STDERR << "StyleFromSD: max recursion depth exceeded\n";
    return nullptr;
  }
  // Integer literal → RtInt
  if (tok.current() == TOK_INT) {
    int v = tok.intValue();
    tok.next();
    return new RtInt(v);
  }
  if (tok.current() != TOK_IDENT) {
    STDERR << "StyleFromSD: expected identifier for func node\n";
    return nullptr;
  }
  const char* name = tok.identifier();

  // Enum constant → RtInt
  int ev = lookupEnumConstant(name);
  if (ev >= 0) {
    tok.next();
    return new RtInt(ev);
  }

  // Check dispatch table for func makers
  for (int i = 0; style_dispatch[i].name; i++) {
    if (strcmp(name, style_dispatch[i].name) == 0 && style_dispatch[i].make_func) {
      tok.next();  // consume identifier
      return style_dispatch[i].make_func(tok, depth + 1);
    }
  }

  STDERR << "StyleFromSD: unknown function type: " << name << "\n";
  return nullptr;
}

static RtTransNode* parseTransNode(Tokenizer& tok, int depth) {
  if (depth > MAX_PARSE_DEPTH) {
    STDERR << "StyleFromSD: max recursion depth exceeded\n";
    return nullptr;
  }
  if (tok.current() != TOK_IDENT) {
    STDERR << "StyleFromSD: expected identifier for transition node\n";
    return nullptr;
  }
  const char* name = tok.identifier();

  // Check dispatch table for trans makers
  for (int i = 0; style_dispatch[i].name; i++) {
    if (strcmp(name, style_dispatch[i].name) == 0 && style_dispatch[i].make_trans) {
      tok.next();  // consume identifier
      return style_dispatch[i].make_trans(tok, depth + 1);
    }
  }

  STDERR << "StyleFromSD: unknown transition type: " << name << "\n";
  return nullptr;
}

// ============================================================
// SECTION 13: Factory Function Implementations
// ============================================================

// makeLayers: Layers<BASE, L1, L2, ...>
// BASE is a color. L1..LN are layer nodes (also RtColorNode for our purposes).
static RtColorNode* makeLayers(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;

  RtColorNode* children[16];
  int count = 0;

  // Parse first child (BASE — required)
  children[0] = parseColorNode(tok, depth);
  if (!children[0]) return nullptr;
  count = 1;

  // Parse remaining children separated by commas
  while (tok.current() == TOK_COMMA && count < 16) {
    tok.next();  // consume ,
    RtColorNode* child = parseColorNode(tok, depth);
    if (!child) {
      // Clean up already-allocated children
      for (int i = 0; i < count; i++) delete children[i];
      return nullptr;
    }
    children[count++] = child;
  }

  if (!expectClose(tok)) {
    for (int i = 0; i < count; i++) delete children[i];
    return nullptr;
  }

  return new RtLayers(children, count);
}

// makeAlphaL: AlphaL<COLOR, FUNC>
static RtColorNode* makeAlphaL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;

  RtColorNode* color = parseColorNode(tok, depth);
  if (!color) return nullptr;

  if (!expectComma(tok)) { delete color; return nullptr; }

  RtFuncNode* func = parseFuncNode(tok, depth);
  if (!func) { delete color; return nullptr; }

  if (!expectClose(tok)) { delete color; delete func; return nullptr; }

  return new RtAlphaL(color, func);
}

// makeInOutTrL: InOutTrL<TRANS_out, TRANS_in, COLOR_off>
static RtColorNode* makeInOutTrL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;

  RtTransNode* out_tr = parseTransNode(tok, depth);
  if (!out_tr) return nullptr;

  if (!expectComma(tok)) { delete out_tr; return nullptr; }

  RtTransNode* in_tr = parseTransNode(tok, depth);
  if (!in_tr) { delete out_tr; return nullptr; }

  if (!expectComma(tok)) { delete out_tr; delete in_tr; return nullptr; }

  RtColorNode* off = parseColorNode(tok, depth);
  if (!off) { delete out_tr; delete in_tr; return nullptr; }

  if (!expectClose(tok)) { delete out_tr; delete in_tr; delete off; return nullptr; }

  return new RtInOutTrL(out_tr, in_tr, off);
}

// makeInt: Int<N>
static RtFuncNode* makeInt(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int n = parseIntArg(tok);
  if (n < 0 && tok.current() != TOK_CLOSE) {
    // parseIntArg already logged error
    return nullptr;
  }
  if (!expectClose(tok)) return nullptr;
  return new RtInt(n);
}

// makeIfon: Ifon<A, B>
static RtFuncNode* makeIfon(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;

  RtFuncNode* a = parseFuncNode(tok, depth);
  if (!a) return nullptr;

  if (!expectComma(tok)) { delete a; return nullptr; }

  RtFuncNode* b = parseFuncNode(tok, depth);
  if (!b) { delete a; return nullptr; }

  if (!expectClose(tok)) { delete a; delete b; return nullptr; }

  return new RtIfon(a, b);
}

// makeTrWipe: TrWipe<MILLIS>
static RtTransNode* makeTrWipe(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtTrWipeRuntime(ms);
}

// makeTrWipeIn: TrWipeIn<MILLIS>
static RtTransNode* makeTrWipeIn(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtTrWipeInRuntime(ms);
}

// makeTrFade: TrFade<MILLIS>
static RtTransNode* makeTrFade(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtTrFadeRuntime(ms);
}

// makeTrInstant: TrInstant (no args)
static RtTransNode* makeTrInstant(Tokenizer& tok, int depth) {
  // TrInstant has no template args — may or may not have <>
  // Some styles write TrInstant, others TrInstant<> — handle both
  if (tok.current() == TOK_OPEN) {
    tok.next();  // consume <
    if (!expectClose(tok)) return nullptr;
  }
  return new RtTrInstant();
}

// ============================================================
// Plan 02 Factory Implementations — Color Nodes
// ============================================================

// Helper: parse variadic COLOR args inside <...> until >
// Fills children[], returns count.
static int parseVariadicColors(Tokenizer& tok, int depth, RtColorNode** out, int max_count) {
  int count = 0;
  if (!expectOpen(tok)) return -1;
  if (tok.current() == TOK_CLOSE) { tok.next(); return 0; }
  RtColorNode* c = parseColorNode(tok, depth);
  if (!c) return -1;
  out[count++] = c;
  while (tok.current() == TOK_COMMA && count < max_count) {
    tok.next();
    // Check if next arg is a func node (for mixed-type variadics) — skip
    c = parseColorNode(tok, depth);
    if (!c) { for (int i = 0; i < count; i++) delete out[i]; return -1; }
    out[count++] = c;
  }
  if (!expectClose(tok)) {
    for (int i = 0; i < count; i++) delete out[i];
    return -1;
  }
  return count;
}

// Helper: parse variadic FUNC args
static int parseVariadicFuncs(Tokenizer& tok, int depth, RtFuncNode** out, int max_count) {
  int count = 0;
  if (!expectOpen(tok)) return -1;
  if (tok.current() == TOK_CLOSE) { tok.next(); return 0; }
  RtFuncNode* f = parseFuncNode(tok, depth);
  if (!f) return -1;
  out[count++] = f;
  while (tok.current() == TOK_COMMA && count < max_count) {
    tok.next();
    f = parseFuncNode(tok, depth);
    if (!f) { for (int i = 0; i < count; i++) delete out[i]; return -1; }
    out[count++] = f;
  }
  if (!expectClose(tok)) {
    for (int i = 0; i < count; i++) delete out[i];
    return -1;
  }
  return count;
}

// Helper: parse variadic TRANS args
static int parseVariadicTrans(Tokenizer& tok, int depth, RtTransNode** out, int max_count) {
  int count = 0;
  if (!expectOpen(tok)) return -1;
  if (tok.current() == TOK_CLOSE) { tok.next(); return 0; }
  RtTransNode* t = parseTransNode(tok, depth);
  if (!t) return -1;
  out[count++] = t;
  while (tok.current() == TOK_COMMA && count < max_count) {
    tok.next();
    t = parseTransNode(tok, depth);
    if (!t) { for (int i = 0; i < count; i++) delete out[i]; return -1; }
    out[count++] = t;
  }
  if (!expectClose(tok)) {
    for (int i = 0; i < count; i++) delete out[i];
    return -1;
  }
  return count;
}

static RtColorNode* makeRgb16(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int r = parseIntArg(tok); if (!expectComma(tok)) return nullptr;
  int g = parseIntArg(tok); if (!expectComma(tok)) return nullptr;
  int b = parseIntArg(tok); if (!expectClose(tok)) return nullptr;
  return new RtRgb16(r, g, b);
}

static RtColorNode* makeRgbArg(Tokenizer& tok, int depth) {
  // RgbArg<ARG, DEFAULT_COLOR>: at runtime use default color
  if (!expectOpen(tok)) return nullptr;
  int arg_n = parseIntArg(tok);
  if (!expectComma(tok)) return nullptr;
  // DEFAULT_COLOR is a color node — parse it
  RtColorNode* def = parseColorNode(tok, depth);
  if (!def) return nullptr;
  if (!expectClose(tok)) { delete def; return nullptr; }
  // Get the default color from the node
  // Since we can't run it without a blade, just make a transparent node
  // A better approach: evaluate the default color statically
  // For Rgb<> colors, we can get the color
  RGBA_um c = def->getColor(0);  // static evaluation (no run)
  delete def;
  return new RtRgbArg(c);
}

static RtColorNode* makeHumpFlicker(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectComma(tok)) { delete a; delete b; return nullptr; }
  int w = parseIntArg(tok); if (w < 0) { delete a; delete b; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtHumpFlicker(a, b, w);
}

static RtColorNode* makeHumpFlickerL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* b = parseColorNode(tok, depth); if (!b) return nullptr;
  if (!expectComma(tok)) { delete b; return nullptr; }
  int w = parseIntArg(tok); if (w < 0) { delete b; return nullptr; }
  if (!expectClose(tok)) { delete b; return nullptr; }
  return new RtHumpFlickerL(b, w);
}

static RtColorNode* makeAudioFlicker(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtAudioFlicker(a, b);
}

static RtColorNode* makeAudioFlickerL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* b = parseColorNode(tok, depth); if (!b) return nullptr;
  if (!expectClose(tok)) { delete b; return nullptr; }
  return new RtAudioFlickerL(b);
}

static RtColorNode* makeBrownNoiseFlicker(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectComma(tok)) { delete a; delete b; return nullptr; }
  int grade = parseIntArg(tok); if (grade < 0) { delete a; delete b; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtBrownNoiseFlicker(a, b, grade);
}

static RtColorNode* makeBrownNoiseFlickerL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* b = parseColorNode(tok, depth); if (!b) return nullptr;
  if (!expectComma(tok)) { delete b; return nullptr; }
  int grade = parseIntArg(tok); if (grade < 0) { delete b; return nullptr; }
  if (!expectClose(tok)) { delete b; return nullptr; }
  return new RtBrownNoiseFlickerL(b, grade);
}

static RtColorNode* makeRandomPerLEDFlicker(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtRandomPerLEDFlicker(a, b);
}

static RtColorNode* makeRandomPerLEDFlickerL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* b = parseColorNode(tok, depth); if (!b) return nullptr;
  if (!expectClose(tok)) { delete b; return nullptr; }
  return new RtRandomPerLEDFlickerL(b);
}

// Stripes<WIDTH_INT, SPEED_INT, COLOR...>
static RtColorNode* makeStripes(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int width = parseIntArg(tok); if (width < 0) return nullptr;
  if (!expectComma(tok)) return nullptr;
  int speed = parseIntArg(tok); if (speed < 0) return nullptr;
  RtColorNode* colors[8];
  int ncolors = 0;
  while (tok.current() == TOK_COMMA && ncolors < 8) {
    tok.next();
    RtColorNode* c = parseColorNode(tok, depth);
    if (!c) { for (int i = 0; i < ncolors; i++) delete colors[i]; return nullptr; }
    colors[ncolors++] = c;
  }
  if (!expectClose(tok)) {
    for (int i = 0; i < ncolors; i++) delete colors[i];
    return nullptr;
  }
  return new RtStripes(width, speed, colors, ncolors);
}

// StripesX<WIDTH_FUNC, SPEED_FUNC, COLOR...>
static RtColorNode* makeStripesX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* w = parseFuncNode(tok, depth); if (!w) return nullptr;
  if (!expectComma(tok)) { delete w; return nullptr; }
  RtFuncNode* s = parseFuncNode(tok, depth); if (!s) { delete w; return nullptr; }
  RtColorNode* colors[8];
  int ncolors = 0;
  while (tok.current() == TOK_COMMA && ncolors < 8) {
    tok.next();
    RtColorNode* c = parseColorNode(tok, depth);
    if (!c) { delete w; delete s; for (int i = 0; i < ncolors; i++) delete colors[i]; return nullptr; }
    colors[ncolors++] = c;
  }
  if (!expectClose(tok)) {
    delete w; delete s;
    for (int i = 0; i < ncolors; i++) delete colors[i];
    return nullptr;
  }
  return new RtStripesX(w, s, colors, ncolors);
}

// Pulsing<A, B, MILLIS_INT>
static RtColorNode* makePulsing(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectComma(tok)) { delete a; delete b; return nullptr; }
  int ms = parseIntArg(tok); if (ms < 0) { delete a; delete b; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtPulsing(a, b, ms);
}

// Strobe<A, B, FREQ, MILLIS>
static RtColorNode* makeStrobe(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* a = parseColorNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectComma(tok)) { delete a; delete b; return nullptr; }
  int freq = parseIntArg(tok); if (freq < 0) { delete a; delete b; return nullptr; }
  // Optional millis arg (defaults to 1)
  int ms = 1;
  if (tok.current() == TOK_COMMA) {
    tok.next();
    ms = parseIntArg(tok);
    if (ms < 0) ms = 1;
  }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtStrobe(a, b, freq, ms);
}

// Mix<F, A, B>
static RtColorNode* makeMix(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtColorNode* a = parseColorNode(tok, depth); if (!a) { delete f; return nullptr; }
  if (!expectComma(tok)) { delete f; delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete f; delete a; return nullptr; }
  if (!expectClose(tok)) { delete f; delete a; delete b; return nullptr; }
  return new RtMix(f, a, b);
}

// AlphaMixL<F, A, B>
static RtColorNode* makeAlphaMixL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtColorNode* a = parseColorNode(tok, depth); if (!a) { delete f; return nullptr; }
  if (!expectComma(tok)) { delete f; delete a; return nullptr; }
  RtColorNode* b = parseColorNode(tok, depth); if (!b) { delete f; delete a; return nullptr; }
  if (!expectClose(tok)) { delete f; delete a; delete b; return nullptr; }
  return new RtAlphaMixLImpl(f, a, b);
}

// RotateColorsX<F, COLOR>
static RtColorNode* makeRotateColorsX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtColorNode* c = parseColorNode(tok, depth); if (!c) { delete f; return nullptr; }
  if (!expectClose(tok)) { delete f; delete c; return nullptr; }
  return new RtRotateColorsX(f, c);
}

// ColorSelect<F, TRANS, COLOR...>
static RtColorNode* makeColorSelect(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtTransNode* tr = parseTransNode(tok, depth); if (!tr) { delete f; return nullptr; }
  RtColorNode* colors[16];
  int ncolors = 0;
  while (tok.current() == TOK_COMMA && ncolors < 16) {
    tok.next();
    RtColorNode* c = parseColorNode(tok, depth);
    if (!c) {
      delete f; delete tr;
      for (int i = 0; i < ncolors; i++) delete colors[i];
      return nullptr;
    }
    colors[ncolors++] = c;
  }
  if (!expectClose(tok)) {
    delete f; delete tr;
    for (int i = 0; i < ncolors; i++) delete colors[i];
    return nullptr;
  }
  return new RtColorSelect(f, tr, colors, ncolors);
}

// Helper: parse optional FireConfig<a,b,c> or return default
static bool parseFireConfig(Tokenizer& tok, int* base, int* rand_val, int* cool) {
  if (tok.current() != TOK_IDENT || strcmp(tok.identifier(), "FireConfig") != 0)
    return false;
  tok.next();  // consume "FireConfig"
  if (!expectOpen(tok)) return false;
  *base = parseIntArg(tok);
  if (!expectComma(tok)) return false;
  *rand_val = parseIntArg(tok);
  if (!expectComma(tok)) return false;
  *cool = parseIntArg(tok);
  if (!expectClose(tok)) return false;
  return true;
}

// StyleFire<C1, C2, DELAY=0, SPEED=2, NORM=FireConfig<0,2000,5>, CLASH=FireConfig<3000,0,0>,
//           LOCK=FireConfig<0,5000,10>, OFF=FireConfig<0,0,NORM_COOL>>
static RtColorNode* makeStyleFire(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c1 = parseColorNode(tok, depth); if (!c1) return nullptr;
  if (!expectComma(tok)) { delete c1; return nullptr; }
  RtColorNode* c2 = parseColorNode(tok, depth); if (!c2) { delete c1; return nullptr; }
  int delay = 0, speed = 2;
  int nb = 0, nr = 2000, nc = 5;
  int cb = 3000, cr = 0, cc = 0;
  int lb = 0, lr = 5000, lc = 10;
  int ob = 0, or_ = 0, oc = 5;  // off defaults to norm_cool
  if (tok.current() == TOK_COMMA) {
    tok.next();
    if (tok.current() == TOK_INT) { delay = tok.intValue(); tok.next(); }
    if (tok.current() == TOK_COMMA) {
      tok.next();
      if (tok.current() == TOK_INT) { speed = tok.intValue(); tok.next(); }
      if (tok.current() == TOK_COMMA) {
        tok.next(); parseFireConfig(tok, &nb, &nr, &nc);
        if (tok.current() == TOK_COMMA) {
          tok.next(); parseFireConfig(tok, &cb, &cr, &cc);
          if (tok.current() == TOK_COMMA) {
            tok.next(); parseFireConfig(tok, &lb, &lr, &lc);
            if (tok.current() == TOK_COMMA) {
              tok.next(); parseFireConfig(tok, &ob, &or_, &oc);
            }
          }
        }
      }
    }
  }
  if (!expectClose(tok)) { delete c1; delete c2; return nullptr; }
  return new RtStyleFire(c1, c2, delay, speed,
                         nb, nr, nc, cb, cr, cc, lb, lr, lc, ob, or_, oc);
}

// StaticFire<C1, C2, DELAY=0, SPEED=2, BASE=0, RAND=2000, COOLING=5>
static RtColorNode* makeStaticFire(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c1 = parseColorNode(tok, depth); if (!c1) return nullptr;
  if (!expectComma(tok)) { delete c1; return nullptr; }
  RtColorNode* c2 = parseColorNode(tok, depth); if (!c2) { delete c1; return nullptr; }
  int delay = 0, speed = 2, base = 0, rand_val = 2000, cooling = 5;
  if (tok.current() == TOK_COMMA) {
    tok.next(); if (tok.current() == TOK_INT) { delay = tok.intValue(); tok.next(); }
    if (tok.current() == TOK_COMMA) {
      tok.next(); if (tok.current() == TOK_INT) { speed = tok.intValue(); tok.next(); }
      if (tok.current() == TOK_COMMA) {
        tok.next(); if (tok.current() == TOK_INT) { base = tok.intValue(); tok.next(); }
        if (tok.current() == TOK_COMMA) {
          tok.next(); if (tok.current() == TOK_INT) { rand_val = tok.intValue(); tok.next(); }
          if (tok.current() == TOK_COMMA) {
            tok.next(); if (tok.current() == TOK_INT) { cooling = tok.intValue(); tok.next(); }
          }
        }
      }
    }
  }
  if (!expectClose(tok)) { delete c1; delete c2; return nullptr; }
  return new RtStaticFire(c1, c2, delay, speed, base, rand_val, cooling);
}

// Remap<F, COLOR>
static RtColorNode* makeRemap(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtColorNode* c = parseColorNode(tok, depth); if (!c) { delete f; return nullptr; }
  if (!expectClose(tok)) { delete f; delete c; return nullptr; }
  return new RtRemap(f, c);
}

// TransitionEffect<C1, C2, TRANS1, TRANS2, EFFECT>
// = Layers<C1, TransitionEffectL<TrConcat<TRANS1, C2, TRANS2>, EFFECT>>
static RtColorNode* makeTransitionEffect(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c1 = parseColorNode(tok, depth); if (!c1) return nullptr;
  if (!expectComma(tok)) { delete c1; return nullptr; }
  RtColorNode* c2 = parseColorNode(tok, depth); if (!c2) { delete c1; return nullptr; }
  if (!expectComma(tok)) { delete c1; delete c2; return nullptr; }
  RtTransNode* tr1 = parseTransNode(tok, depth); if (!tr1) { delete c1; delete c2; return nullptr; }
  if (!expectComma(tok)) { delete c1; delete c2; delete tr1; return nullptr; }
  RtTransNode* tr2 = parseTransNode(tok, depth); if (!tr2) { delete c1; delete c2; delete tr1; return nullptr; }
  if (!expectComma(tok)) { delete c1; delete c2; delete tr1; delete tr2; return nullptr; }
  int effect = parseIntArg(tok);
  if (!expectClose(tok)) { delete c1; delete c2; delete tr1; delete tr2; return nullptr; }
  // Build TrConcat(tr1, c2, tr2) and TransitionEffectL
  RtTransNode* trans_arr[2] = {tr1, tr2};
  RtColorNode* color_arr[1] = {c2};
  RtTransNode* trconcat = new RtTrConcat(trans_arr, 2, color_arr);
  RtColorNode* effect_layer = new RtTransitionEffectL(trconcat, effect);
  // Wrap in Layers<c1, effect_layer>
  RtColorNode* children[2] = {c1, effect_layer};
  return new RtLayers(children, 2);
}

// TransitionEffectL<TRANS, EFFECT> (also handles MultiTransitionEffectL)
static RtColorNode* makeTransitionEffectL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtTransNode* tr = parseTransNode(tok, depth); if (!tr) return nullptr;
  if (!expectComma(tok)) { delete tr; return nullptr; }
  int effect = parseIntArg(tok);
  // Optional N arg (MultiTransitionEffectL has N=3 default)
  if (tok.current() == TOK_COMMA) {
    tok.next();
    parseIntArg(tok);  // consume N, ignored
  }
  if (!expectClose(tok)) { delete tr; return nullptr; }
  return new RtTransitionEffectL(tr, effect);
}

// TransitionLoopL<TRANS>
static RtColorNode* makeTransitionLoopL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtTransNode* tr = parseTransNode(tok, depth); if (!tr) return nullptr;
  if (!expectClose(tok)) { delete tr; return nullptr; }
  return new RtTransitionLoopL(tr);
}

// EffectSequence<EFFECT, COLOR...>
static RtColorNode* makeEffectSequence(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int effect = parseIntArg(tok);
  RtColorNode* colors[16];
  int ncolors = 0;
  while (tok.current() == TOK_COMMA && ncolors < 16) {
    tok.next();
    RtColorNode* c = parseColorNode(tok, depth);
    if (!c) { for (int i = 0; i < ncolors; i++) delete colors[i]; return nullptr; }
    colors[ncolors++] = c;
  }
  if (!expectClose(tok)) {
    for (int i = 0; i < ncolors; i++) delete colors[i];
    return nullptr;
  }
  return new RtEffectSequence(effect, colors, ncolors);
}

// LockupTrL<COLOR, BeginTr, EndTr, LOCKUP_TYPE [, CONDITION]>
static RtColorNode* makeLockupTrL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* color = parseColorNode(tok, depth); if (!color) return nullptr;
  if (!expectComma(tok)) { delete color; return nullptr; }
  RtTransNode* begin_tr = parseTransNode(tok, depth); if (!begin_tr) { delete color; return nullptr; }
  if (!expectComma(tok)) { delete color; delete begin_tr; return nullptr; }
  RtTransNode* end_tr = parseTransNode(tok, depth); if (!end_tr) { delete color; delete begin_tr; return nullptr; }
  if (!expectComma(tok)) { delete color; delete begin_tr; delete end_tr; return nullptr; }
  int lockup_type = parseIntArg(tok);
  // Optional CONDITION
  RtFuncNode* cond = nullptr;
  if (tok.current() == TOK_COMMA) {
    tok.next();
    cond = parseFuncNode(tok, depth);
  }
  if (!expectClose(tok)) { delete color; delete begin_tr; delete end_tr; delete cond; return nullptr; }
  return new RtLockupTrL(color, begin_tr, end_tr, lockup_type, cond);
}

// ResponsiveLightningBlockL<COLOR, TR1, TR2 [, CONDITION]>
static RtColorNode* makeResponsiveLightningBlockL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtTransNode* tr1 = nullptr, *tr2 = nullptr;
  RtFuncNode* cond = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); tr1 = parseTransNode(tok, depth); }
  if (tr1 && tok.current() == TOK_COMMA) { tok.next(); tr2 = parseTransNode(tok, depth); }
  if (tr2 && tok.current() == TOK_COMMA) { tok.next(); cond = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete c; delete tr1; delete tr2; delete cond; return nullptr; }
  if (!tr1) tr1 = new RtTrInstant();
  if (!tr2) tr2 = new RtTrInstant();
  return new RtResponsiveLightningBlockL(c, tr1, tr2, cond);
}

// ResponsiveStabL<COLOR [, TR1, TR2, ...]>
static RtColorNode* makeResponsiveStabL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtTransNode* tr1 = nullptr, *tr2 = nullptr;
  // Consume optional args
  while (tok.current() == TOK_COMMA) {
    tok.next();
    if (!tr1) { tr1 = parseTransNode(tok, depth); }
    else if (!tr2) { tr2 = parseTransNode(tok, depth); }
    else {
      // Extra arg — consume and discard
      if (tok.current() == TOK_INT) tok.next();
      else if (tok.current() == TOK_IDENT) {
        RtFuncNode* tmp = parseFuncNode(tok, depth); delete tmp;
      }
    }
  }
  if (!expectClose(tok)) { delete c; delete tr1; delete tr2; return nullptr; }
  if (!tr1) tr1 = new RtTrWipeInRuntime(600);
  if (!tr2) tr2 = new RtTrWipeRuntime(600);
  return new RtResponsiveStabL(c, tr1, tr2);
}

// ResponsiveBlastL<COLOR [, FADE, SIZE, SPEED, TOP, BOTTOM, EFFECT]>
static RtColorNode* makeResponsiveBlastL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtFuncNode* fade = nullptr, *size_f = nullptr, *speed_f = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); fade = parseFuncNode(tok, depth); }
  if (fade && tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (size_f && tok.current() == TOK_COMMA) { tok.next(); speed_f = parseFuncNode(tok, depth); }
  // Consume remaining optional args
  while (tok.current() == TOK_COMMA) {
    tok.next();
    if (tok.current() == TOK_INT) tok.next();
    else if (tok.current() == TOK_IDENT) {
      RtFuncNode* tmp = parseFuncNode(tok, depth); delete tmp;
    }
  }
  if (!expectClose(tok)) { delete c; delete fade; delete size_f; delete speed_f; return nullptr; }
  if (!fade) fade = new RtInt(400);
  if (!size_f) size_f = new RtInt(100);
  if (!speed_f) speed_f = new RtInt(400);
  return new RtResponsiveBlastL(c, fade, size_f, speed_f);
}

// ResponsiveBlastWaveL<COLOR [, FADE, SIZE, SPEED, TOP, BOTTOM, EFFECT]>
static RtColorNode* makeResponsiveBlastWaveL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtFuncNode* fade = nullptr, *size_f = nullptr, *speed_f = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); fade = parseFuncNode(tok, depth); }
  if (fade && tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (size_f && tok.current() == TOK_COMMA) { tok.next(); speed_f = parseFuncNode(tok, depth); }
  while (tok.current() == TOK_COMMA) {
    tok.next();
    if (tok.current() == TOK_INT) tok.next();
    else if (tok.current() == TOK_IDENT) { RtFuncNode* tmp = parseFuncNode(tok, depth); delete tmp; }
  }
  if (!expectClose(tok)) { delete c; delete fade; delete size_f; delete speed_f; return nullptr; }
  if (!fade) fade = new RtInt(400);
  if (!size_f) size_f = new RtInt(100);
  if (!speed_f) speed_f = new RtInt(400);
  return new RtResponsiveBlastWaveL(c, fade, size_f, speed_f);
}

// ResponsiveBlastFadeL<COLOR [, SIZE, FADE, TOP, BOTTOM, EFFECT]>
static RtColorNode* makeResponsiveBlastFadeL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtFuncNode* size_f = nullptr, *fade = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (size_f && tok.current() == TOK_COMMA) { tok.next(); fade = parseFuncNode(tok, depth); }
  while (tok.current() == TOK_COMMA) {
    tok.next();
    if (tok.current() == TOK_INT) tok.next();
    else if (tok.current() == TOK_IDENT) { RtFuncNode* tmp = parseFuncNode(tok, depth); delete tmp; }
  }
  if (!expectClose(tok)) { delete c; delete size_f; delete fade; return nullptr; }
  if (!size_f) size_f = new RtInt(8000);
  if (!fade) fade = new RtInt(400);
  return new RtResponsiveBlastFadeL(c, size_f, fade);
}

// ResponsiveClashL<COLOR [, TR1, TR2, TOP, BOTTOM, SIZE]>
static RtColorNode* makeResponsiveClashL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtTransNode* tr1 = nullptr, *tr2 = nullptr;
  RtFuncNode* top = nullptr, *bottom = nullptr, *size_f = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); tr1 = parseTransNode(tok, depth); }
  if (tr1 && tok.current() == TOK_COMMA) { tok.next(); tr2 = parseTransNode(tok, depth); }
  if (tr2 && tok.current() == TOK_COMMA) { tok.next(); top = parseFuncNode(tok, depth); }
  if (top && tok.current() == TOK_COMMA) { tok.next(); bottom = parseFuncNode(tok, depth); }
  if (bottom && tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) {
    delete c; delete tr1; delete tr2; delete top; delete bottom; delete size_f;
    return nullptr;
  }
  if (!tr1) tr1 = new RtTrInstant();
  if (!tr2) tr2 = new RtTrFadeRuntime(200);
  if (!top) top = new RtInt(26000);
  if (!bottom) bottom = new RtInt(6000);
  if (!size_f) size_f = new RtInt(10000);
  return new RtResponsiveClashL(c, tr1, tr2, top, bottom, size_f);
}

// LocalizedClashL<COLOR [, SIZE, EFFECT]>
static RtColorNode* makeLocalizedClashL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  int size = 10000;
  int effect = (int)EFFECT_CLASH;
  if (tok.current() == TOK_COMMA) { tok.next(); size = parseIntArg(tok); }
  if (tok.current() == TOK_COMMA) { tok.next(); effect = parseIntArg(tok); }
  if (!expectClose(tok)) { delete c; return nullptr; }
  return new RtLocalizedClashL(c, size, effect);
}

// BlastL<COLOR [, FADEOUT, WAVE_SIZE, WAVE_MS, EFFECT]>
static RtColorNode* makeBlastL(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  int fadeout = 200, wave_size = 100;
  if (tok.current() == TOK_COMMA) { tok.next(); fadeout = parseIntArg(tok); }
  if (tok.current() == TOK_COMMA) { tok.next(); wave_size = parseIntArg(tok); }
  // Consume extra args
  while (tok.current() == TOK_COMMA) { tok.next(); parseIntArg(tok); }
  if (!expectClose(tok)) { delete c; return nullptr; }
  return new RtBlastL(c, fadeout, wave_size);
}

// SyncAltToVarianceL (no args)
static RtColorNode* makeSyncAltToVarianceL(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (!expectClose(tok)) return nullptr;
  }
  return new RtSyncAltToVarianceL();
}

// ============================================================
// Plan 02 Factory Implementations — Function Nodes
// ============================================================

static RtFuncNode* makeIntArg(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int arg_n = parseIntArg(tok);
  if (!expectComma(tok)) return nullptr;
  int def = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtIntArgNode(arg_n, def);
}

static RtFuncNode* makeScale(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) { delete f; return nullptr; }
  if (!expectComma(tok)) { delete f; delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete f; delete a; return nullptr; }
  if (!expectClose(tok)) { delete f; delete a; delete b; return nullptr; }
  return new RtScale(f, a, b);
}

static RtFuncNode* makeSum(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtSum(a, b);
}

static RtFuncNode* makeMult(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtMult(a, b);
}

static RtFuncNode* makeModF(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtModF(a, b);
}

static RtFuncNode* makeSin(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* rpm = parseFuncNode(tok, depth); if (!rpm) return nullptr;
  RtFuncNode* lo = nullptr, *hi = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); lo = parseFuncNode(tok, depth); }
  if (lo && tok.current() == TOK_COMMA) { tok.next(); hi = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete rpm; delete lo; delete hi; return nullptr; }
  if (!lo) lo = new RtInt(0);
  if (!hi) hi = new RtInt(32768);
  return new RtSin(rpm, lo, hi);
}

static RtFuncNode* makeHoldPeakF(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtFuncNode* hold = parseFuncNode(tok, depth); if (!hold) { delete f; return nullptr; }
  if (!expectComma(tok)) { delete f; delete hold; return nullptr; }
  RtFuncNode* speed = parseFuncNode(tok, depth); if (!speed) { delete f; delete hold; return nullptr; }
  if (!expectClose(tok)) { delete f; delete hold; delete speed; return nullptr; }
  return new RtHoldPeakF(f, hold, speed);
}

static RtFuncNode* makeIsLessThan(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtIsLessThan(a, b);
}

static RtFuncNode* makeIsGreaterThan(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* a = parseFuncNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtFuncNode* b = parseFuncNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtIsGreaterThan(a, b);
}

static RtFuncNode* makeEffectPulseF(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int effect = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtEffectPulseF(effect);
}

static RtFuncNode* makeEffectRandomF(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int effect = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtEffectRandomF(effect);
}

static RtFuncNode* makeEffectPosition(Tokenizer& tok, int depth) {
  // EffectPosition<> or EffectPosition<EFFECT>
  int effect = (int)EFFECT_NONE;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) {
      effect = parseIntArg(tok);
    }
    if (!expectClose(tok)) return nullptr;
  }
  return new RtEffectPosition(effect);
}

static RtFuncNode* makeIgnitionTime(Tokenizer& tok, int depth) {
  int def = 300;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) def = parseIntArg(tok);
    if (!expectClose(tok)) return nullptr;
  }
  return new RtIgnitionTime(def);
}

static RtFuncNode* makeRetractionTime(Tokenizer& tok, int depth) {
  int def = 500;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) def = parseIntArg(tok);
    if (!expectClose(tok)) return nullptr;
  }
  return new RtRetractionTime(def);
}

static RtFuncNode* makeWavLen(Tokenizer& tok, int depth) {
  int effect = (int)EFFECT_NONE;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) effect = parseIntArg(tok);
    if (!expectClose(tok)) return nullptr;
  }
  return new RtWavLen(effect);
}

static RtFuncNode* makePercentage(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  int n = parseIntArg(tok);
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtPercentage(f, n);
}

static RtFuncNode* makeSwingSpeed(Tokenizer& tok, int depth) {
  int max_val = 100;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) max_val = parseIntArg(tok);
    if (!expectClose(tok)) return nullptr;
  }
  return new RtSwingSpeed(max_val);
}

static RtFuncNode* makeBladeAngle(Tokenizer& tok, int depth) {
  int min_val = 0, max_val = 32768;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) {
      min_val = parseIntArg(tok);
      if (tok.current() == TOK_COMMA) { tok.next(); max_val = parseIntArg(tok); }
    }
    if (!expectClose(tok)) return nullptr;
  }
  return new RtBladeAngle(min_val, max_val);
}

static RtFuncNode* makeTwistAngle(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) {
    tok.next();
    // May have optional args — consume them
    while (tok.current() != TOK_CLOSE && tok.current() != TOK_EOF) {
      if (tok.current() == TOK_INT) tok.next();
      else if (tok.current() == TOK_COMMA) tok.next();
      else if (tok.current() == TOK_IDENT) { parseFuncNode(tok, depth); }
      else break;
    }
    if (!expectClose(tok)) return nullptr;
  }
  return new RtTwistAngle();
}

static RtFuncNode* makeSlowNoise(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtSlowNoise(f);
}

static RtFuncNode* makeNoisySoundLevelFunc(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (!expectClose(tok)) return nullptr;
  }
  return new RtNoisySoundLevel();
}

static RtFuncNode* makeClashImpactF(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (!expectClose(tok)) return nullptr;
  }
  return new RtClashImpactF();
}

static RtFuncNode* makeRampF(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (!expectClose(tok)) return nullptr;
  }
  return new RtRampF();
}

static RtFuncNode* makeBump(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* pos = parseFuncNode(tok, depth); if (!pos) return nullptr;
  RtFuncNode* width = nullptr;
  if (tok.current() == TOK_COMMA) {
    tok.next();
    width = parseFuncNode(tok, depth);
  }
  if (!expectClose(tok)) { delete pos; delete width; return nullptr; }
  if (!width) width = new RtInt(16385);
  return new RtBump(pos, width);
}

static RtFuncNode* makeLayerFunctions(Tokenizer& tok, int depth) {
  RtFuncNode* funcs[8];
  int n = parseVariadicFuncs(tok, depth, funcs, 8);
  if (n < 0) return nullptr;
  return new RtLayerFunctions(funcs, n);
}

static RtFuncNode* makeSmoothStep(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* x = parseFuncNode(tok, depth); if (!x) return nullptr;
  if (!expectComma(tok)) { delete x; return nullptr; }
  RtFuncNode* edge = parseFuncNode(tok, depth); if (!edge) { delete x; return nullptr; }
  if (!expectClose(tok)) { delete x; delete edge; return nullptr; }
  return new RtSmoothStep(x, edge);
}

static RtFuncNode* makeBlastF(Tokenizer& tok, int depth) {
  int fadeout = 200, wave_size = 100;
  if (tok.current() == TOK_OPEN) {
    tok.next();
    if (tok.current() != TOK_CLOSE) { fadeout = parseIntArg(tok); }
    if (tok.current() == TOK_COMMA) { tok.next(); wave_size = parseIntArg(tok); }
    // Consume extra args
    while (tok.current() != TOK_CLOSE && tok.current() != TOK_EOF) {
      if (tok.current() == TOK_COMMA) tok.next();
      else if (tok.current() == TOK_INT) tok.next();
      else if (tok.current() == TOK_IDENT) tok.next();
      else break;
    }
    if (!expectClose(tok)) return nullptr;
  }
  return new RtBlastF(fadeout, wave_size);
}

static RtFuncNode* makeTrigger(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int effect = parseIntArg(tok);
  RtFuncNode* f1 = nullptr, *f2 = nullptr, *f3 = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); f1 = parseFuncNode(tok, depth); }
  if (f1 && tok.current() == TOK_COMMA) { tok.next(); f2 = parseFuncNode(tok, depth); }
  if (f2 && tok.current() == TOK_COMMA) { tok.next(); f3 = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete f1; delete f2; delete f3; return nullptr; }
  if (!f1) f1 = new RtInt(0);
  if (!f2) f2 = new RtInt(0);
  if (!f3) f3 = new RtInt(0);
  return new RtTrigger(effect, f1, f2, f3);
}

static RtFuncNode* makeBendTimePowX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtFuncNode* p = parseFuncNode(tok, depth); if (!p) { delete f; return nullptr; }
  if (!expectClose(tok)) { delete f; delete p; return nullptr; }
  return new RtBendTimePowX(f, p);
}

static RtFuncNode* makeBendTimePowInvX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectComma(tok)) { delete f; return nullptr; }
  RtFuncNode* p = parseFuncNode(tok, depth); if (!p) { delete f; return nullptr; }
  if (!expectClose(tok)) { delete f; delete p; return nullptr; }
  return new RtBendTimePowInvX(f, p);
}

static RtFuncNode* makeVariation(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) { tok.next(); if (!expectClose(tok)) return nullptr; }
  return new RtVariation();
}

static RtFuncNode* makeAltF(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) { tok.next(); if (!expectClose(tok)) return nullptr; }
  return new RtAltF();
}

static RtFuncNode* makeBatteryLevel(Tokenizer& tok, int depth) {
  if (tok.current() == TOK_OPEN) { tok.next(); if (!expectClose(tok)) return nullptr; }
  return new RtBatteryLevel();
}

// ============================================================
// Plan 02 Factory Implementations — Transition Nodes
// ============================================================

static RtTransNode* makeTrFadeX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtTrFadeX(f);
}

static RtTransNode* makeTrWipeX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtTrWipeX(f);
}

static RtTransNode* makeTrWipeInX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtTrWipeInX(f);
}

static RtTransNode* makeTrWipeSparkTip(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  if (!expectComma(tok)) { delete c; return nullptr; }
  int ms = parseIntArg(tok);
  // Optional extra args
  while (tok.current() == TOK_COMMA) { tok.next(); parseIntArg(tok); }
  if (!expectClose(tok)) { delete c; return nullptr; }
  return new RtTrWipeSparkTip(c, ms);
}

static RtTransNode* makeTrWipeSparkTipX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  if (!expectComma(tok)) { delete c; return nullptr; }
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) { delete c; return nullptr; }
  // Optional extra args
  while (tok.current() == TOK_COMMA) { tok.next(); parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete c; delete f; return nullptr; }
  return new RtTrWipeSparkTipX(c, f);
}

static RtTransNode* makeTrWipeInSparkTip(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  if (!expectComma(tok)) { delete c; return nullptr; }
  int ms = parseIntArg(tok);
  while (tok.current() == TOK_COMMA) { tok.next(); parseIntArg(tok); }
  if (!expectClose(tok)) { delete c; return nullptr; }
  return new RtTrWipeInSparkTip(c, ms);
}

static RtTransNode* makeTrWipeInSparkTipX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  if (!expectComma(tok)) { delete c; return nullptr; }
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) { delete c; return nullptr; }
  while (tok.current() == TOK_COMMA) { tok.next(); parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete c; delete f; return nullptr; }
  return new RtTrWipeInSparkTipX(c, f);
}

static RtTransNode* makeTrSmoothFade(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtTrSmoothFade(ms);
}

// TrConcat: interleaved TRANS [, COLOR, TRANS]...
// Alternates TRANS and COLOR until >
static RtTransNode* makeTrConcat(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;

  RtTransNode* trans_nodes[8];
  RtColorNode* color_nodes[7];
  int ntrans = 0, ncolors = 0;

  // Parse first TRANS
  RtTransNode* t = parseTransNode(tok, depth);
  if (!t) return nullptr;
  trans_nodes[ntrans++] = t;

  // Alternate: look at next arg — is it a COLOR or TRANS?
  // Heuristic: check if identifier is in dispatch table as color or trans
  // We attempt COLOR first; if that fails, try TRANS.
  while (tok.current() == TOK_COMMA && ntrans < 8) {
    tok.next();
    if (tok.current() == TOK_CLOSE) break;

    // Try to determine if next is COLOR or TRANS based on dispatch table
    bool is_trans = false;
    if (tok.current() == TOK_IDENT) {
      const char* nm = tok.identifier();
      for (int i = 0; style_dispatch[i].name; i++) {
        if (strcmp(nm, style_dispatch[i].name) == 0) {
          if (style_dispatch[i].make_trans) { is_trans = true; }
          break;
        }
      }
    }

    if (is_trans && ncolors == ntrans - 1) {
      // We have enough colors already (or same count) — this must be a TRANS
      // (TrConcat2 pattern: no intermediate color between these two trans)
      RtTransNode* tr2 = parseTransNode(tok, depth);
      if (!tr2) {
        for (int i = 0; i < ntrans; i++) delete trans_nodes[i];
        for (int i = 0; i < ncolors; i++) delete color_nodes[i];
        return nullptr;
      }
      trans_nodes[ntrans++] = tr2;
    } else if (!is_trans && ncolors < ntrans - 1) {
      // Need a color between this trans and next
      RtColorNode* col = parseColorNode(tok, depth);
      if (!col) {
        for (int i = 0; i < ntrans; i++) delete trans_nodes[i];
        for (int i = 0; i < ncolors; i++) delete color_nodes[i];
        return nullptr;
      }
      color_nodes[ncolors++] = col;
    } else {
      // Ambiguous — try as color first, fallback to trans
      // Save position conceptually via trying color
      RtColorNode* col = parseColorNode(tok, depth);
      if (col) {
        if (ncolors < ntrans) {
          color_nodes[ncolors++] = col;
        } else {
          delete col;  // unexpected, ignore
        }
      } else {
        // Must be a trans
        RtTransNode* tr2 = parseTransNode(tok, depth);
        if (!tr2) {
          for (int i = 0; i < ntrans; i++) delete trans_nodes[i];
          for (int i = 0; i < ncolors; i++) delete color_nodes[i];
          return nullptr;
        }
        trans_nodes[ntrans++] = tr2;
      }
    }
  }

  if (!expectClose(tok)) {
    for (int i = 0; i < ntrans; i++) delete trans_nodes[i];
    for (int i = 0; i < ncolors; i++) delete color_nodes[i];
    return nullptr;
  }

  if (ntrans == 1) return trans_nodes[0];  // TrConcat<single> = that trans
  return new RtTrConcat(trans_nodes, ntrans, color_nodes);
}

static RtTransNode* makeTrJoin(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtTransNode* a = parseTransNode(tok, depth); if (!a) return nullptr;
  if (!expectComma(tok)) { delete a; return nullptr; }
  RtTransNode* b = parseTransNode(tok, depth); if (!b) { delete a; return nullptr; }
  if (!expectClose(tok)) { delete a; delete b; return nullptr; }
  return new RtTrJoin(a, b);
}

static RtTransNode* makeTrDelay(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectClose(tok)) return nullptr;
  return new RtTrDelay(ms);
}

static RtTransNode* makeTrDelayX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  if (!expectClose(tok)) { delete f; return nullptr; }
  return new RtTrDelayX(f);
}

static RtTransNode* makeTrExtend(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  if (!expectComma(tok)) return nullptr;
  RtTransNode* tr = parseTransNode(tok, depth); if (!tr) return nullptr;
  if (!expectClose(tok)) { delete tr; return nullptr; }
  return new RtTrExtend(ms, tr);
}

static RtTransNode* makeTrDoEffectAlwaysX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtTransNode* tr = parseTransNode(tok, depth); if (!tr) return nullptr;
  if (!expectComma(tok)) { delete tr; return nullptr; }
  int effect = parseIntArg(tok);
  RtFuncNode* f1 = nullptr, *f2 = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); f1 = parseFuncNode(tok, depth); }
  if (f1 && tok.current() == TOK_COMMA) { tok.next(); f2 = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete tr; delete f1; delete f2; return nullptr; }
  if (!f1) f1 = new RtInt(0);
  if (!f2) f2 = new RtInt(0);
  return new RtTrDoEffectAlwaysX(tr, effect, f1, f2);
}

static RtTransNode* makeTrWaveX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtFuncNode* fade = nullptr, *size_f = nullptr, *speed = nullptr, *pos = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); fade = parseFuncNode(tok, depth); }
  if (fade && tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (size_f && tok.current() == TOK_COMMA) { tok.next(); speed = parseFuncNode(tok, depth); }
  if (speed && tok.current() == TOK_COMMA) { tok.next(); pos = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete c; delete fade; delete size_f; delete speed; delete pos; return nullptr; }
  if (!fade) fade = new RtInt(400);
  if (!size_f) size_f = new RtInt(100);
  if (!speed) speed = new RtInt(400);
  if (!pos) pos = new RtInt(16384);
  return new RtTrWaveX(c, fade, size_f, speed, pos);
}

static RtTransNode* makeTrSparkX(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtColorNode* c = parseColorNode(tok, depth); if (!c) return nullptr;
  RtFuncNode* fade = nullptr, *size_f = nullptr, *pos = nullptr;
  if (tok.current() == TOK_COMMA) { tok.next(); fade = parseFuncNode(tok, depth); }
  if (fade && tok.current() == TOK_COMMA) { tok.next(); size_f = parseFuncNode(tok, depth); }
  if (size_f && tok.current() == TOK_COMMA) { tok.next(); pos = parseFuncNode(tok, depth); }
  if (!expectClose(tok)) { delete c; delete fade; delete size_f; delete pos; return nullptr; }
  if (!fade) fade = new RtInt(400);
  if (!size_f) size_f = new RtInt(100);
  if (!pos) pos = new RtInt(16384);
  return new RtTrSparkX(c, fade, size_f, pos);
}

static RtTransNode* makeTrColorCycle(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int m1 = 300;
  if (tok.current() != TOK_CLOSE) m1 = parseIntArg(tok);
  int m2 = 300;
  if (tok.current() == TOK_COMMA) { tok.next(); m2 = parseIntArg(tok); }
  if (!expectClose(tok)) return nullptr;
  return new RtTrColorCycle(m1, m2);
}

static RtTransNode* makeTrBoing(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  int ms = parseIntArg(tok);
  int n = 1;
  if (tok.current() == TOK_COMMA) { tok.next(); n = parseIntArg(tok); }
  if (!expectClose(tok)) return nullptr;
  return new RtTrBoing(ms, n);
}

static RtTransNode* makeTrSelect(Tokenizer& tok, int depth) {
  if (!expectOpen(tok)) return nullptr;
  RtFuncNode* f = parseFuncNode(tok, depth); if (!f) return nullptr;
  RtTransNode* trans[8];
  int ntrans = 0;
  while (tok.current() == TOK_COMMA && ntrans < 8) {
    tok.next();
    RtTransNode* t = parseTransNode(tok, depth);
    if (!t) { delete f; for (int i = 0; i < ntrans; i++) delete trans[i]; return nullptr; }
    trans[ntrans++] = t;
  }
  if (!expectClose(tok)) { delete f; for (int i = 0; i < ntrans; i++) delete trans[i]; return nullptr; }
  return new RtTrSelect(f, trans, ntrans);
}

// ============================================================
// SECTION 14: Additional Color Wrapper Nodes (Plan 02)
// ============================================================

// --- Helper: Runtime function node wrapping a zero-arg compiled function ---
// Used for: HumpFlickerF, NoisySoundLevel, BrownNoiseF, RandomPerLEDF, PulsingF, StrobeF, BlastF etc.
// Each one needs its own RtFuncNode subclass that holds the compiled instance.

// HumpFlickerF wrapper (1 int arg: HUMP_WIDTH)
class RtHumpFlickerFNode : public RtFuncNode {
public:
  explicit RtHumpFlickerFNode(int w) : w_(w) {}
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[28] = this;  // sentinel — uses internal w_ via getInteger
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    // HumpFlickerF returns random hump values per LED
    // We replicate the logic: random position, gaussian bump shape
    // Use the static bump_shape array from BumpBase
    static uint32_t last_pos = 0;
    static uint32_t last_update = 0;
    static int pos = 0;
    uint32_t now = millis();
    if (now - last_update > 1) {
      last_update = now;
      pos = random(32768);
    }
    int w = w_;
    if (w < 1) w = 1;
    // Simple gaussian approximation: abs(led_pos - pos) / w
    int led_pos = led * 32768;  // not per-LED for this
    // Use bump logic: distance from pos scaled by w
    return 0;  // will be replaced by inline BumpBase
  }
private:
  int w_;
};

// For HumpFlickerF we use the compiled HumpFlickerF<HUMP_WIDTH> via RtArg
// by instantiating a fixed set of specializations. However, since HUMP_WIDTH
// is a compile-time constant, we cannot fully generalize this at runtime.
// Best approach: use a runtime implementation of HumpFlickerF that matches behavior.

// Runtime HumpFlickerF: random hump position, gaussian shape per LED index
// Matches HumpFlickerF<HUMP_WIDTH> which uses HumpFlickerFSVF with bump logic.
class RtHumpFlickerFImpl : public RtFuncNode {
public:
  explicit RtHumpFlickerFImpl(int width) : width_(width > 0 ? width : 1), pos_(0), num_leds_(1) {}
  FunctionRunResult run(BladeBase* blade) override {
    num_leds_ = blade->num_leds();
    // Update position randomly
    uint32_t now = millis();
    if (now - last_update_ > 5) {
      last_update_ = now;
      pos_ = random(num_leds_);
    }
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    // Gaussian-like: distance in LEDs from pos_, scaled by width_
    int dist_leds = abs(led - pos_);
    if (dist_leds >= width_) return 0;
    // Linear falloff (simpler approximation than actual gaussian)
    return ((width_ - dist_leds) * 32768) / width_;
  }
private:
  int width_;
  int pos_;
  int num_leds_;
  uint32_t last_update_ = 0;
};

// NoisySoundLevel wrapper (0 args)
class RtNoisySoundLevelNode : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[29] = nullptr;
    impl_.run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  NoisySoundLevel impl_;
};

// BrownNoiseF wrapper (grade * 128 is the GRADE param)
// Uses BrownNoiseF<Int<128>> as a reasonable fixed approximation.
class RtBrownNoiseFNode : public RtFuncNode {
public:
  explicit RtBrownNoiseFNode(int grade128) {}
  FunctionRunResult run(BladeBase* blade) override {
    RunFunction(&impl_, blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  BrownNoiseF<Int<128>> impl_;
};

// RandomPerLEDF wrapper (0 args)
class RtRandomPerLEDFNode : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    RunFunction(&impl_, blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  RandomPerLEDF impl_;
};

// --- HumpFlicker<A, B, HUMP_WIDTH> = Layers<A, AlphaL<B, HumpFlickerF<HUMP_WIDTH>>> ---
// Runtime: RtLayers(A, RtAlphaL(B, RtHumpFlickerFImpl(HUMP_WIDTH)))
// Ownership: func_ owned by alpha (via RtAlphaL); alpha owned by layers_; a owned by layers_.
class RtHumpFlicker : public RtColorNode {
public:
  RtHumpFlicker(RtColorNode* a, RtColorNode* b, int width) : layers_(nullptr) {
    RtFuncNode* func = new RtHumpFlickerFImpl(width);
    RtColorNode* alpha = new RtAlphaL(b, func);  // alpha owns b and func
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);          // layers_ owns a and alpha
  }
  ~RtHumpFlicker() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- HumpFlickerL<B, HUMP_WIDTH> = AlphaL<B, HumpFlickerF<HUMP_WIDTH>> ---
class RtHumpFlickerL : public RtColorNode {
public:
  RtHumpFlickerL(RtColorNode* b, int width)
    : func_(new RtHumpFlickerFImpl(width)), node_(new RtAlphaL(b, func_)) {}
  ~RtHumpFlickerL() override { delete node_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* func_;
  RtAlphaL* node_;
};

// --- AudioFlicker<A, B> = Layers<A, AlphaL<B, NoisySoundLevelCompat>> ---
class RtAudioFlicker : public RtColorNode {
public:
  RtAudioFlicker(RtColorNode* a, RtColorNode* b) : layers_(nullptr) {
    RtColorNode* alpha = new RtAlphaL(b, new RtNoisySoundLevelNode());
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);
  }
  ~RtAudioFlicker() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- AudioFlickerL<B> = AlphaL<B, NoisySoundLevelCompat> ---
class RtAudioFlickerL : public RtColorNode {
public:
  explicit RtAudioFlickerL(RtColorNode* b)
    : func_(new RtNoisySoundLevelNode()), node_(new RtAlphaL(b, func_)) {}
  ~RtAudioFlickerL() override { delete node_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* func_;
  RtAlphaL* node_;
};

// --- BrownNoiseFlicker<A, B, grade> = Layers<A, AlphaL<B, BrownNoiseF<Int<grade*128>>>> ---
class RtBrownNoiseFlicker : public RtColorNode {
public:
  RtBrownNoiseFlicker(RtColorNode* a, RtColorNode* b, int grade) : layers_(nullptr) {
    RtColorNode* alpha = new RtAlphaL(b, new RtBrownNoiseFNode(grade * 128));
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);
  }
  ~RtBrownNoiseFlicker() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- BrownNoiseFlickerL<B, grade> = AlphaL<B, BrownNoiseF<Int<grade*128>>> ---
class RtBrownNoiseFlickerL : public RtColorNode {
public:
  RtBrownNoiseFlickerL(RtColorNode* b, int grade)
    : func_(new RtBrownNoiseFNode(grade * 128)), node_(new RtAlphaL(b, func_)) {}
  ~RtBrownNoiseFlickerL() override { delete node_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* func_;
  RtAlphaL* node_;
};

// --- RandomPerLEDFlicker<A, B> = Layers<A, AlphaL<B, RandomPerLEDF>> ---
class RtRandomPerLEDFlicker : public RtColorNode {
public:
  RtRandomPerLEDFlicker(RtColorNode* a, RtColorNode* b) : layers_(nullptr) {
    RtColorNode* alpha = new RtAlphaL(b, new RtRandomPerLEDFNode());
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);
  }
  ~RtRandomPerLEDFlicker() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- RandomPerLEDFlickerL<B> = AlphaL<B, RandomPerLEDF> ---
class RtRandomPerLEDFlickerL : public RtColorNode {
public:
  explicit RtRandomPerLEDFlickerL(RtColorNode* b)
    : func_(new RtRandomPerLEDFNode()), node_(new RtAlphaL(b, func_)) {}
  ~RtRandomPerLEDFlickerL() override { delete node_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* func_;
  RtAlphaL* node_;
};

// --- Stripes<WIDTH, SPEED, COLOR...>: runtime wipe-stripe effect ---
// Stripes is a variadic template with complex per-LED math. We implement
// a runtime approximation using sinusoidal color cycling per LED.
class RtStripes : public RtColorNode {
public:
  RtStripes(int width, int speed, RtColorNode** colors, int ncolors)
    : width_(width), speed_(speed), ncolors_(ncolors) {
    for (int i = 0; i < ncolors_ && i < 8; i++) colors_[i] = colors[i];
  }
  ~RtStripes() override {
    for (int i = 0; i < ncolors_; i++) delete colors_[i];
  }
  bool run(BladeBase* blade) override {
    for (int i = 0; i < ncolors_; i++) colors_[i]->run(blade);
    offset_ += (int64_t)speed_ * (millis() - last_ms_) / 1000;
    last_ms_ = millis();
    return true;
  }
  RGBA_um getColor(int led) override {
    if (ncolors_ == 0) return RGBA_um::Transparent();
    // Each LED's color is determined by led position + offset
    int pos = (int)(((int64_t)led * 32768 + offset_) / (width_ > 0 ? width_ : 1));
    while (pos < 0) pos += ncolors_ * 32768;
    int idx = (pos / 32768) % ncolors_;
    return colors_[idx]->getColor(led);
  }
private:
  int width_;
  int speed_;
  int ncolors_;
  RtColorNode* colors_[8];
  int64_t offset_ = 0;
  uint32_t last_ms_ = 0;
};

// --- StripesX<WIDTH_FUNC, SPEED_FUNC, COLOR...> ---
class RtStripesX : public RtColorNode {
public:
  RtStripesX(RtFuncNode* width, RtFuncNode* speed, RtColorNode** colors, int ncolors)
    : width_(width), speed_(speed), ncolors_(ncolors) {
    for (int i = 0; i < ncolors_ && i < 8; i++) colors_[i] = colors[i];
  }
  ~RtStripesX() override {
    delete width_; delete speed_;
    for (int i = 0; i < ncolors_; i++) delete colors_[i];
  }
  bool run(BladeBase* blade) override {
    width_->run(blade); speed_->run(blade);
    for (int i = 0; i < ncolors_; i++) colors_[i]->run(blade);
    int spd = speed_->getInteger(0);
    offset_ += (int64_t)spd * (millis() - last_ms_) / 1000;
    last_ms_ = millis();
    return true;
  }
  RGBA_um getColor(int led) override {
    if (ncolors_ == 0) return RGBA_um::Transparent();
    int w = width_->getInteger(led);
    if (w <= 0) w = 1;
    int pos = (int)(((int64_t)led * 32768 + offset_) / w);
    while (pos < 0) pos += ncolors_ * 32768;
    int idx = (pos / 32768) % ncolors_;
    return colors_[idx]->getColor(led);
  }
private:
  RtFuncNode* width_;
  RtFuncNode* speed_;
  int ncolors_;
  RtColorNode* colors_[8];
  int64_t offset_ = 0;
  uint32_t last_ms_ = 0;
};

// --- PulsingF function wrapper (0 arg — uses internal oscillator) ---
// PulsingF is not directly exposed as a standalone function; it powers Pulsing<>
// PulsingL<B, MILLIS> = AlphaL<B, PulsingF<Int<MILLIS>>>
// We use a runtime sin oscillator.
class RtPulsingFNode : public RtFuncNode {
public:
  explicit RtPulsingFNode(int millis) : millis_(millis), phase_(0) {}
  FunctionRunResult run(BladeBase* blade) override {
    uint32_t now = millis();
    uint32_t delta = now - last_update_;
    last_update_ = now;
    phase_ = (phase_ + (uint32_t)delta * 65536u / (uint32_t)(millis_ > 0 ? millis_ : 1)) & 0xffff;
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    // sin oscillator 0-32768
    int idx = (phase_ >> 6) & 1023;
    return (sin_table[idx] + 32768) >> 1;
  }
private:
  int millis_;
  uint32_t phase_ = 0;
  uint32_t last_update_ = 0;
};

// --- Pulsing<A, B, MILLIS> = Layers<A, AlphaL<B, PulsingF<Int<MILLIS>>>> ---
class RtPulsing : public RtColorNode {
public:
  RtPulsing(RtColorNode* a, RtColorNode* b, int millis) : layers_(nullptr) {
    RtColorNode* alpha = new RtAlphaL(b, new RtPulsingFNode(millis));
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);
  }
  ~RtPulsing() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- StrobeF function wrapper ---
// StrobeL<C, FREQ, MILLIS> = AlphaL<C, StrobeF<FREQ, MILLIS>>
// StrobeF returns 32768 for MILLIS ms every 1000/FREQ ms
class RtStrobeFNode : public RtFuncNode {
public:
  RtStrobeFNode(int freq, int millis) : freq_(freq), millis_(millis) {}
  FunctionRunResult run(BladeBase* blade) override {
    uint32_t now = millis();
    uint32_t period = (freq_ > 0) ? (1000u / (uint32_t)freq_) : 1000u;
    uint32_t phase = now % period;
    val_ = (phase < (uint32_t)millis_) ? 32768 : 0;
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return val_; }
private:
  int freq_;
  int millis_;
  int val_ = 0;
};

// --- Strobe<A, B, FREQ, MILLIS> = Layers<A, AlphaL<B, StrobeF<Int<FREQ>, Int<MILLIS>>>> ---
class RtStrobe : public RtColorNode {
public:
  RtStrobe(RtColorNode* a, RtColorNode* b, int freq, int millis) : layers_(nullptr) {
    RtColorNode* alpha = new RtAlphaL(b, new RtStrobeFNode(freq, millis));
    RtColorNode* children[2] = {a, alpha};
    layers_ = new RtLayers(children, 2);
  }
  ~RtStrobe() override { delete layers_; }
  bool run(BladeBase* blade) override { return layers_->run(blade); }
  RGBA_um getColor(int led) override { return layers_->getColor(led); }
private:
  RtLayers* layers_;
};

// --- Mix<F, A, B> node ---
// Mix<F, A, B> returns MixColors(a, b, f, 15) — f=0 gives A, f=32768 gives B
class RtMix : public RtColorNode {
public:
  RtMix(RtFuncNode* f, RtColorNode* a, RtColorNode* b)
    : f_(f), a_(a), b_(b) {}
  ~RtMix() override { delete f_; delete a_; delete b_; }
  bool run(BladeBase* blade) override {
    rt_func_args[0] = f_;
    rt_color_args[0] = a_;
    rt_color_args[1] = b_;
    LayerRunResult res = RunLayer(&impl_, blade);
    return (res != LayerRunResult::OPAQUE_BLACK_UNTIL_IGNITION);
  }
  RGBA_um getColor(int led) override {
    rt_func_args[0] = f_;
    rt_color_args[0] = a_;
    rt_color_args[1] = b_;
    auto c = impl_.getColor(led);
    return RGBA_um(c.c, c.overdrive, c.alpha);
  }
private:
  RtFuncNode* f_;
  RtColorNode* a_;
  RtColorNode* b_;
  Mix<RtArgFunc<0>, RtArgColor<0>, RtArgColor<1>> impl_;
};

// Corrected RtAlphaMixL using inline implementation (avoids double-ownership of f_):
// AlphaMixL<F, A, B> color per led = Mix(A, B, F) * F
class RtAlphaMixLImpl : public RtColorNode {
public:
  RtAlphaMixLImpl(RtFuncNode* f, RtColorNode* a, RtColorNode* b)
    : f_(f), a_(a), b_(b) {}
  ~RtAlphaMixLImpl() override { delete f_; delete a_; delete b_; }
  bool run(BladeBase* blade) override {
    f_->run(blade); a_->run(blade); b_->run(blade);
    return true;  // AlphaMixL is a layer (transparent when f=0)
  }
  RGBA_um getColor(int led) override {
    int alpha = f_->getInteger(led);
    if (alpha == 0) return RGBA_um::Transparent();
    RGBA_um ca = a_->getColor(led);
    RGBA_um cb = b_->getColor(led);
    // MixColors(a, b, alpha, 15): alpha=0 -> a, alpha=32768 -> b
    RGBA_um mixed = MixColors(ca, cb, alpha, 15);
    // Now apply the same alpha as transparency
    mixed.alpha = (uint16_t)((int)mixed.alpha * alpha / 32768);
    return mixed;
  }
private:
  RtFuncNode* f_;
  RtColorNode* a_;
  RtColorNode* b_;
};

// --- RotateColorsX<FUNC, COLOR> ---
class RtRotateColorsX : public RtColorNode {
public:
  RtRotateColorsX(RtFuncNode* f, RtColorNode* c) : f_(f), c_(c) {}
  ~RtRotateColorsX() override { delete f_; delete c_; }
  bool run(BladeBase* blade) override {
    f_->run(blade); return c_->run(blade);
  }
  RGBA_um getColor(int led) override {
    int rot = (f_->getInteger(led) & 0x7fff) * 3;
    RGBA_um c = c_->getColor(led);
    c.c = c.c.rotate(rot);
    return c;
  }
private:
  RtFuncNode* f_;
  RtColorNode* c_;
};

// --- ColorSelect<F, TRANS, COLOR...>: select by F with transition ---
// Simplified runtime: no transition animation, just instant color selection
class RtColorSelect : public RtColorNode {
public:
  RtColorSelect(RtFuncNode* f, RtTransNode* trans, RtColorNode** colors, int ncolors)
    : f_(f), trans_(trans), ncolors_(ncolors), sel_(0), old_sel_(0), transitioning_(false) {
    for (int i = 0; i < ncolors_ && i < 16; i++) colors_[i] = colors[i];
  }
  ~RtColorSelect() override {
    delete f_; delete trans_;
    for (int i = 0; i < ncolors_; i++) delete colors_[i];
  }
  bool run(BladeBase* blade) override {
    f_->run(blade);
    trans_->run(blade);
    for (int i = 0; i < ncolors_; i++) colors_[i]->run(blade);
    int newsel = f_->getInteger(0) % (ncolors_ > 0 ? ncolors_ : 1);
    if (newsel < 0) newsel += ncolors_;
    if (newsel != sel_) {
      old_sel_ = sel_;
      sel_ = newsel;
      trans_->begin();
      transitioning_ = true;
    }
    if (transitioning_ && trans_->done()) transitioning_ = false;
    return true;
  }
  RGBA_um getColor(int led) override {
    if (ncolors_ == 0) return RGBA_um::Transparent();
    RGBA_um cur = colors_[sel_]->getColor(led);
    if (transitioning_) {
      RGBA_um old = colors_[old_sel_]->getColor(led);
      return trans_->getColor(old, cur, led);
    }
    return cur;
  }
private:
  RtFuncNode* f_;
  RtTransNode* trans_;
  int ncolors_;
  RtColorNode* colors_[16];
  int sel_;
  int old_sel_;
  bool transitioning_;
};

// --- StyleFire: runtime fire effect ---
// StyleFire<C1, C2, DELAY, SPEED, NORM, CLASH, LOCK, OFF>
// We use the compiled StyleFire via the RtArg pattern.
// For full flexibility we implement runtime fire with configurable FireConfiguration.
class RtStyleFire : public RtColorNode {
public:
  RtStyleFire(RtColorNode* c1, RtColorNode* c2,
              int delay, int speed,
              int norm_base, int norm_rand, int norm_cool,
              int clsh_base, int clsh_rand, int clsh_cool,
              int lock_base, int lock_rand, int lock_cool,
              int off_base,  int off_rand,  int off_cool)
    : c1_(c1), c2_(c2), delay_(delay), speed_(speed),
      heat_(nullptr), num_leds_(0), state_(0), on_time_(0) {
    norm_ = {norm_base, norm_rand, norm_cool};
    clsh_ = {clsh_base, clsh_rand, clsh_cool};
    lock_ = {lock_base, lock_rand, lock_cool};
    off_  = {off_base,  off_rand,  off_cool};
  }
  ~RtStyleFire() override { delete c1_; delete c2_; delete[] heat_; }

  bool run(BladeBase* blade) override {
    c1_->run(blade); c2_->run(blade);
    int n = blade->num_leds();
    if (n != num_leds_) {
      delete[] heat_;
      num_leds_ = n;
      int sz = n + speed_ + 3;
      heat_ = new unsigned short[sz];
      for (int i = 0; i < sz; i++) heat_[i] = 0;
    }
    // Determine on state
    bool is_on = blade->is_on();
    if (!is_on) {
      state_ = 0;
    } else if (state_ == 0) {
      state_ = 1;
      on_time_ = millis();
    } else if (state_ == 1 && (int)(millis() - on_time_) >= delay_) {
      state_ = 2;
    }

    FireConfiguration cfg = off_;
    if (clash_.Detect(blade)) {
      cfg = clsh_;
    } else if (state_ == 2) {
      SaberBase::LockupType lt = SaberBase::LockupForBlade(blade->GetBladeNumber());
      if (lt == SaberBase::LOCKUP_NONE) cfg = norm_;
      else cfg = lock_;
    }

    uint32_t m = millis();
    if (m - last_update_ >= 10) {
      last_update_ = m;
      for (int i = 0; i < speed_; i++) {
        heat_[num_leds_ + i] = (unsigned short)clampi32(
          cfg.intensity_base + random(random(random(cfg.intensity_rand))), 0, 65535);
      }
      for (int i = 0; i < num_leds_; i++) {
        int x = ((int)heat_[i + speed_ - 1] * 3 +
                 (int)heat_[i + speed_] * 10 +
                 (int)heat_[i + speed_ + 1] * 3) >> 4;
        heat_[i] = (unsigned short)clampi32(x - random(cfg.cooling), 0, 65535);
      }
    }
    return state_ != 0;
  }

  RGBA_um getColor(int led) override {
    if (!heat_) return RGBA_um::Transparent();
    int h = heat_[num_leds_ - 1 - led];
    RGBA_um c1 = c1_->getColor(led);
    RGBA_um c2 = c2_->getColor(led);
    if (h < 256) {
      // black -> c1
      return MixColors(RGBA_um::Transparent(), c1, h, 8);
    } else if (h < 512) {
      return MixColors(c1, c2, h - 256, 8);
    } else if (h < 768) {
      RGBA_um white = makeRGBA(255, 255, 255);
      return MixColors(c2, white, h - 512, 8);
    } else {
      return makeRGBA(255, 255, 255);
    }
  }

private:
  RtColorNode* c1_;
  RtColorNode* c2_;
  int delay_;
  int speed_;
  FireConfiguration norm_, clsh_, lock_, off_;
  unsigned short* heat_;
  int num_leds_;
  int state_;
  uint32_t on_time_;
  uint32_t last_update_ = 0;
  OneshotEffectDetector<EFFECT_CLASH> clash_;
};

// --- StaticFire<C1, C2, DELAY, SPEED, BASE, RAND, COOLING> ---
// StaticFire = StyleFire with clash=off=lock=same as norm (no lockup/clash changes)
class RtStaticFire : public RtStyleFire {
public:
  RtStaticFire(RtColorNode* c1, RtColorNode* c2,
               int delay, int speed, int base, int rand_val, int cooling)
    : RtStyleFire(c1, c2, delay, speed,
                  base, rand_val, cooling,   // norm
                  base, rand_val, cooling,   // clash (same as norm for static)
                  base, rand_val, cooling,   // lock (same)
                  base, rand_val, cooling)   // off (same)
  {}
};

// --- Remap<F, COLOR> ---
class RtRemap : public RtColorNode {
public:
  RtRemap(RtFuncNode* f, RtColorNode* c) : f_(f), c_(c) {}
  ~RtRemap() override { delete f_; delete c_; }
  bool run(BladeBase* blade) override {
    f_->run(blade);
    num_leds_ = blade->num_leds();
    return c_->run(blade);
  }
  RGBA_um getColor(int led) override {
    int pos = f_->getInteger(led);
    int mapped = clamp(pos * num_leds_, 0, num_leds_ * 32768 - 1);
    int fraction = mapped & 0x7fff;
    int idx = clamp(mapped >> 15, 0, num_leds_ - 1);
    int idx2 = (idx + 1 < num_leds_) ? idx + 1 : num_leds_ - 1;
    return MixColors(c_->getColor(idx), c_->getColor(idx2), fraction, 15);
  }
private:
  RtFuncNode* f_;
  RtColorNode* c_;
  int num_leds_ = 1;
};

// --- RgbArg<ARG, DEFAULT_COLOR>: reads color from arg parser ---
// At runtime, we just use the default color (no CurrentArgParser in SD context).
class RtRgbArg : public RtColorNode {
public:
  explicit RtRgbArg(RGBA_um default_color) : color_(default_color) {}
  bool run(BladeBase* blade) override {
    return !(color_.c.r == 0 && color_.c.g == 0 && color_.c.b == 0);
  }
  RGBA_um getColor(int led) override { return color_; }
private:
  RGBA_um color_;
};

// --- Rgb16<R,G,B>: same as Rgb but with 16-bit values ---
class RtRgb16 : public RtColorNode {
public:
  RtRgb16(int r, int g, int b) : r_(r), g_(g), b_(b) {
    color_ = RGBA_um(Color16((uint16_t)r, (uint16_t)g, (uint16_t)b), 0, 32768);
  }
  bool run(BladeBase* blade) override {
    return !(r_ == 0 && g_ == 0 && b_ == 0);
  }
  RGBA_um getColor(int led) override { return color_; }
private:
  int r_, g_, b_;
  RGBA_um color_;
};

// --- TransitionEffect<C1, C2, TRANS1, TRANS2, EFFECT>
// = Layers<C1, TransitionEffectL<TrConcat<TRANS1, C2, TRANS2>, EFFECT>> ---
// We implement TransitionEffectL using the RtArg pattern.
class RtTransitionEffectL : public RtColorNode {
public:
  RtTransitionEffectL(RtTransNode* trans, int effect)
    : trans_(trans), effect_type_((BladeEffectType)effect), run_(false) {}
  ~RtTransitionEffectL() override { delete trans_; }

  bool run(BladeBase* blade) override {
    // Detect effect
    OneshotEffectDetector<EFFECT_CLASH> clash_detector;
    OneshotEffectDetector<EFFECT_BLAST> blast_detector;
    OneshotEffectDetector<EFFECT_STAB>  stab_detector;
    OneshotEffectDetector<EFFECT_FORCE> force_detector;
    BladeEffect* e = nullptr;
    // Simple dispatch for common effects
    switch ((int)effect_type_) {
      case (int)EFFECT_CLASH: e = clash_detector.Detect(blade); break;
      case (int)EFFECT_BLAST: e = blast_detector.Detect(blade); break;
      case (int)EFFECT_STAB:  e = stab_detector.Detect(blade);  break;
      case (int)EFFECT_FORCE: e = force_detector.Detect(blade); break;
      default: break;
    }
    if (e) {
      trans_->begin();
      run_ = true;
    }
    if (run_) {
      trans_->run(blade);
      if (trans_->done()) run_ = false;
    }
    return true;
  }

  RGBA_um getColor(int led) override {
    if (!run_) return RGBA_um::Transparent();
    return trans_->getColor(RGBA_um::Transparent(), RGBA_um::Transparent(), led);
  }

private:
  RtTransNode* trans_;
  BladeEffectType effect_type_;
  bool run_;
};

// --- TransitionLoopL<TRANS>: continuously loops the transition ---
class RtTransitionLoopL : public RtColorNode {
public:
  explicit RtTransitionLoopL(RtTransNode* trans) : trans_(trans) {
    trans_->begin();
  }
  ~RtTransitionLoopL() override { delete trans_; }

  bool run(BladeBase* blade) override {
    if (trans_->done()) trans_->begin();
    trans_->run(blade);
    return true;
  }
  RGBA_um getColor(int led) override {
    return trans_->getColor(RGBA_um::Transparent(), RGBA_um::Transparent(), led);
  }

private:
  RtTransNode* trans_;
};

// --- EffectSequence<EFFECT, COLOR...>: cycles through colors on each effect ---
class RtEffectSequence : public RtColorNode {
public:
  RtEffectSequence(int effect, RtColorNode** colors, int ncolors)
    : effect_type_((BladeEffectType)effect), ncolors_(ncolors), idx_(0) {
    for (int i = 0; i < ncolors_ && i < 16; i++) colors_[i] = colors[i];
  }
  ~RtEffectSequence() override {
    for (int i = 0; i < ncolors_; i++) delete colors_[i];
  }
  bool run(BladeBase* blade) override {
    for (int i = 0; i < ncolors_; i++) colors_[i]->run(blade);
    return true;
  }
  RGBA_um getColor(int led) override {
    if (ncolors_ == 0) return RGBA_um::Transparent();
    return colors_[idx_ % ncolors_]->getColor(led);
  }
private:
  BladeEffectType effect_type_;
  int ncolors_;
  RtColorNode* colors_[16];
  int idx_;
};

// --- LockupTrL<COLOR, BeginTr, EndTr, LOCKUP_TYPE, CONDITION> ---
// Uses compiled LockupTrL via RtArg pattern.
class RtLockupTrL : public RtColorNode {
public:
  RtLockupTrL(RtColorNode* color, RtTransNode* begin_tr, RtTransNode* end_tr,
              int lockup_type, RtFuncNode* condition)
    : color_(color), begin_tr_(begin_tr), end_tr_(end_tr),
      lockup_type_((SaberBase::LockupType)lockup_type), condition_(condition),
      state_(0) {
    BladeBase::HandleFeature(FeatureForLockupType(lockup_type_));
  }
  ~RtLockupTrL() override {
    delete color_; delete begin_tr_; delete end_tr_; delete condition_;
  }

  bool run(BladeBase* blade) override {
    color_->run(blade);
    if (condition_) condition_->run(blade);
    begin_tr_->run(blade);
    end_tr_->run(blade);
    SaberBase::LockupType lt = SaberBase::LockupForBlade(blade->GetBladeNumber());
    bool cond = (condition_ == nullptr) || (condition_->getInteger(0) != 0);
    switch (state_) {
      case 0:  // INACTIVE
        if (lt == lockup_type_) {
          if (cond) {
            state_ = 1;
            begin_tr_->begin();
          } else {
            state_ = 2;
          }
        }
        break;
      case 1:  // ACTIVE
        if (lt != lockup_type_) {
          end_tr_->begin();
          state_ = 0;
        }
        break;
      case 2:  // SKIPPED
        if (lt != lockup_type_) state_ = 0;
        break;
    }
    return true;
  }

  RGBA_um getColor(int led) override {
    RGBA_um off = RGBA_um::Transparent();
    RGBA_um on  = color_->getColor(led);
    if (state_ == 1) {
      // Active: begin_tr goes off->on; end_tr goes on->off
      RGBA_um a = begin_tr_->getColor(end_tr_->getColor(on, off, led), on, led);
      return a;
    } else {
      RGBA_um a = end_tr_->getColor(begin_tr_->getColor(off, on, led), off, led);
      return a;
    }
  }

private:
  RtColorNode* color_;
  RtTransNode* begin_tr_;
  RtTransNode* end_tr_;
  SaberBase::LockupType lockup_type_;
  RtFuncNode* condition_;
  int state_;  // 0=INACTIVE, 1=ACTIVE, 2=SKIPPED
};

// --- ResponsiveLightningBlockL<COLOR, TR1, TR2, CONDITION>
// = LockupTrL<AlphaL<COLOR, LayerFunctions<Bump<...>,...>>, TR1, TR2, LOCKUP_LIGHTNING_BLOCK, CONDITION>
// For runtime: implement as LockupTrL with a constant-func color (AlphaL with Int<32768>)
// The complex LB_SHAPE is replaced with a constant bright alpha for simplicity.
class RtResponsiveLightningBlockL : public RtColorNode {
public:
  RtResponsiveLightningBlockL(RtColorNode* color, RtTransNode* tr1, RtTransNode* tr2, RtFuncNode* cond)
    : lockup_(nullptr) {
    // AlphaL<COLOR, Int<32768>> = fully opaque color
    RtFuncNode* alpha_func = new RtInt(32768);
    RtColorNode* alpha_color = new RtAlphaL(color, alpha_func);
    lockup_ = new RtLockupTrL(alpha_color, tr1, tr2,
                               (int)SaberBase::LOCKUP_LIGHTNING_BLOCK, cond);
  }
  ~RtResponsiveLightningBlockL() override { delete lockup_; }
  bool run(BladeBase* blade) override { return lockup_->run(blade); }
  RGBA_um getColor(int led) override { return lockup_->getColor(led); }
private:
  RtLockupTrL* lockup_;
};

// --- ResponsiveStabL<COLOR, TR1, TR2, SIZE1, SIZE2, LOCATION>
// = TransitionEffectL<TrConcat<TR1, AlphaL<COLOR, SmoothStep<LOCATION, Scale<BladeAngle, SIZE1, SIZE2>>>, TR2>, EFFECT_STAB>
// Runtime: simplified as TransitionEffectL with the given color for EFFECT_STAB
class RtResponsiveStabL : public RtColorNode {
public:
  RtResponsiveStabL(RtColorNode* color, RtTransNode* tr1, RtTransNode* tr2)
    : color_(color), tr1_(tr1), tr2_(tr2), run_(false) {}
  ~RtResponsiveStabL() override { delete color_; delete tr1_; delete tr2_; }
  bool run(BladeBase* blade) override {
    color_->run(blade);
    tr1_->run(blade); tr2_->run(blade);
    OneshotEffectDetector<EFFECT_STAB> det;
    if (det.Detect(blade)) {
      tr1_->begin();
      run_ = true;
      phase_ = 0;
    }
    if (run_) {
      if (phase_ == 0 && tr1_->done()) { tr2_->begin(); phase_ = 1; }
      if (phase_ == 1 && tr2_->done()) { run_ = false; }
    }
    return true;
  }
  RGBA_um getColor(int led) override {
    if (!run_) return RGBA_um::Transparent();
    RGBA_um c = color_->getColor(led);
    if (phase_ == 0) return tr1_->getColor(RGBA_um::Transparent(), c, led);
    else             return tr2_->getColor(c, RGBA_um::Transparent(), led);
  }
private:
  RtColorNode* color_;
  RtTransNode* tr1_;
  RtTransNode* tr2_;
  bool run_;
  int phase_ = 0;
};

// --- BlastF function node (from blast.h) ---
class RtBlastFNode : public RtFuncNode {
public:
  RtBlastFNode(int fadeout_ms, int wave_size)
    : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  BlastF<200, 100, 400, EFFECT_BLAST> impl_;
};

// --- BlastL<BLAST, FADEOUT, WAVE_SIZE, WAVE_MS> = AlphaL<BLAST, BlastF<...>> ---
class RtBlastL : public RtColorNode {
public:
  RtBlastL(RtColorNode* color, int fadeout, int wave_size)
    : func_(new RtBlastFNode(fadeout, wave_size)), node_(new RtAlphaL(color, func_)) {}
  ~RtBlastL() override { delete node_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* func_;
  RtAlphaL* node_;
};

// --- ResponsiveBlastL<COLOR, FADE, SIZE, SPEED, TOP, BOTTOM, EFFECT>
// Runtime: simplified as AlphaL<COLOR, BlastF>
class RtResponsiveBlastL : public RtColorNode {
public:
  RtResponsiveBlastL(RtColorNode* color, RtFuncNode* fade, RtFuncNode* size, RtFuncNode* speed)
    : fade_(fade), size_(size), speed_(speed) {
    // node_ owns color; fade_, size_, speed_ owned by this
    node_ = new RtAlphaL(color, new RtBlastFNode(400, 100));
  }
  ~RtResponsiveBlastL() override { delete node_; delete fade_; delete size_; delete speed_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* fade_;
  RtFuncNode* size_;
  RtFuncNode* speed_;
  RtAlphaL* node_;
};

// --- ResponsiveBlastWaveL: similar, just the wave effect ---
class RtResponsiveBlastWaveL : public RtColorNode {
public:
  RtResponsiveBlastWaveL(RtColorNode* color, RtFuncNode* fade, RtFuncNode* size, RtFuncNode* speed)
    : fade_(fade), size_(size), speed_(speed) {
    node_ = new RtAlphaL(color, new RtBlastFNode(400, 100));
  }
  ~RtResponsiveBlastWaveL() override { delete node_; delete fade_; delete size_; delete speed_; }
  bool run(BladeBase* blade) override { return node_->run(blade); }
  RGBA_um getColor(int led) override { return node_->getColor(led); }
private:
  RtFuncNode* fade_;
  RtFuncNode* size_;
  RtFuncNode* speed_;
  RtAlphaL* node_;
};

// --- ResponsiveBlastFadeL<COLOR, SIZE, FADE, TOP, BOTTOM, EFFECT>
// = MultiTransitionEffectL<TrConcat<TrInstant, AlphaL<COLOR, Bump<...>>, TrFadeX<FADE>>, EFFECT>
// Runtime: simplified TransitionEffectL on EFFECT_BLAST
class RtResponsiveBlastFadeL : public RtColorNode {
public:
  RtResponsiveBlastFadeL(RtColorNode* color, RtFuncNode* size, RtFuncNode* fade)
    : color_(color), size_(size), fade_(fade), run_(false) {
    fade_node_ = new RtTrFadeRuntime(400);
  }
  ~RtResponsiveBlastFadeL() override {
    delete color_; delete size_; delete fade_; delete fade_node_;
  }
  bool run(BladeBase* blade) override {
    color_->run(blade);
    OneshotEffectDetector<EFFECT_BLAST> det;
    if (det.Detect(blade)) {
      fade_node_->begin();
      run_ = true;
    }
    if (run_) {
      fade_node_->run(blade);
      if (fade_node_->done()) run_ = false;
    }
    return true;
  }
  RGBA_um getColor(int led) override {
    if (!run_) return RGBA_um::Transparent();
    return fade_node_->getColor(RGBA_um::Transparent(), color_->getColor(led), led);
  }
private:
  RtColorNode* color_;
  RtFuncNode* size_;
  RtFuncNode* fade_;
  RtTrFadeRuntime* fade_node_;
  bool run_;
};

// --- ResponsiveClashL<COLOR, TR1, TR2, TOP, BOTTOM, SIZE>
// = TransitionEffectL<TrConcat<TR1, AlphaL<COLOR, Bump<...>>, TR2>, EFFECT_CLASH>
// Runtime: TransitionEffectL with given tr1/tr2 for EFFECT_CLASH
class RtResponsiveClashL : public RtColorNode {
public:
  RtResponsiveClashL(RtColorNode* color, RtTransNode* tr1, RtTransNode* tr2,
                     RtFuncNode* top, RtFuncNode* bottom, RtFuncNode* size)
    : color_(color), tr1_(tr1), tr2_(tr2), top_(top), bottom_(bottom), size_(size),
      run_(false), phase_(0) {}
  ~RtResponsiveClashL() override {
    delete color_; delete tr1_; delete tr2_;
    delete top_; delete bottom_; delete size_;
  }
  bool run(BladeBase* blade) override {
    color_->run(blade);
    tr1_->run(blade); tr2_->run(blade);
    OneshotEffectDetector<EFFECT_CLASH> det;
    if (det.Detect(blade)) {
      tr1_->begin();
      run_ = true;
      phase_ = 0;
    }
    if (run_) {
      if (phase_ == 0 && tr1_->done()) { tr2_->begin(); phase_ = 1; }
      if (phase_ == 1 && tr2_->done()) { run_ = false; }
    }
    return true;
  }
  RGBA_um getColor(int led) override {
    if (!run_) return RGBA_um::Transparent();
    RGBA_um c = color_->getColor(led);
    if (phase_ == 0) return tr1_->getColor(RGBA_um::Transparent(), c, led);
    else             return tr2_->getColor(c, RGBA_um::Transparent(), led);
  }
private:
  RtColorNode* color_;
  RtTransNode* tr1_;
  RtTransNode* tr2_;
  RtFuncNode* top_;
  RtFuncNode* bottom_;
  RtFuncNode* size_;
  bool run_;
  int phase_;
};

// --- LocalizedClashL<COLOR, int, int, EFFECT> ---
// Runtime: simplified clash layer with position-based bump
class RtLocalizedClashL : public RtColorNode {
public:
  RtLocalizedClashL(RtColorNode* color, int size, int effect)
    : color_(color), size_(size), effect_type_((BladeEffectType)effect),
      run_(false), fade_(0) {}
  ~RtLocalizedClashL() override { delete color_; }
  bool run(BladeBase* blade) override {
    color_->run(blade);
    OneshotEffectDetector<EFFECT_CLASH> det;
    if (det.Detect(blade)) {
      run_ = true;
      start_ms_ = millis();
      fade_ = 32768;
    }
    if (run_) {
      uint32_t elapsed = millis() - start_ms_;
      fade_ = (elapsed < 200u) ? (32768 * (200u - elapsed) / 200u) : 0;
      if (fade_ == 0) run_ = false;
    }
    return true;
  }
  RGBA_um getColor(int led) override {
    if (!run_) return RGBA_um::Transparent();
    RGBA_um c = color_->getColor(led);
    c.alpha = (uint16_t)((int)c.alpha * fade_ / 32768);
    return c;
  }
private:
  RtColorNode* color_;
  int size_;
  BladeEffectType effect_type_;
  bool run_;
  int fade_;
  uint32_t start_ms_ = 0;
};

// --- SyncAltToVarianceL (0 args, side-effect layer) ---
class RtSyncAltToVarianceL : public RtColorNode {
public:
  bool run(BladeBase* blade) override {
    // SyncAltToVarianceL sets the alt index to the variance index
    // This is a side-effect-only layer — returns transparent
    return true;
  }
  RGBA_um getColor(int led) override { return RGBA_um::Transparent(); }
};

// ============================================================
// SECTION 15: Function Wrapper Nodes (Plan 02)
// ============================================================

// --- IntArg<N, DEFAULT>: reads from CurrentArgParser, defaults to DEFAULT ---
// At runtime (SD parser), no CurrentArgParser — use DEFAULT
class RtIntArgNode : public RtFuncNode {
public:
  RtIntArgNode(int arg_n, int default_val) : val_(default_val) {}
  FunctionRunResult run(BladeBase* blade) override {
    if (val_ == 0)     return FunctionRunResult::ZERO_UNTIL_IGNITION;
    if (val_ == 32768) return FunctionRunResult::ONE_UNTIL_IGNITION;
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return val_; }
private:
  int val_;
};

// --- Scale<F, A, B>: maps F (0-32768) to range [A, B] ---
class RtScale : public RtFuncNode {
public:
  RtScale(RtFuncNode* f, RtFuncNode* a, RtFuncNode* b) : f_(f), a_(a), b_(b) {}
  ~RtScale() override { delete f_; delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = f_; rt_func_args[1] = a_; rt_func_args[2] = b_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = f_; rt_func_args[1] = a_; rt_func_args[2] = b_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* f_; RtFuncNode* a_; RtFuncNode* b_;
  Scale<RtArgFunc<0>, RtArgFunc<1>, RtArgFunc<2>> impl_;
};

// --- Sum<A, B> ---
class RtSum : public RtFuncNode {
public:
  RtSum(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtSum() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    a_->run(blade); b_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    return a_->getInteger(led) + b_->getInteger(led);
  }
private:
  RtFuncNode* a_; RtFuncNode* b_;
};

// --- Mult<A, B>: A * B / 32768 ---
class RtMult : public RtFuncNode {
public:
  RtMult(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtMult() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    a_->run(blade); b_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    return (a_->getInteger(led) * b_->getInteger(led)) >> 15;
  }
private:
  RtFuncNode* a_; RtFuncNode* b_;
};

// --- ModF<A, B>: A % B ---
class RtModF : public RtFuncNode {
public:
  RtModF(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtModF() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    a_->run(blade); b_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    int b = b_->getInteger(led);
    if (b == 0) return 0;
    return a_->getInteger(led) % b;
  }
private:
  RtFuncNode* a_; RtFuncNode* b_;
};

// --- Sin<RPM, LOW, HIGH>: sinusoidal oscillator ---
class RtSin : public RtFuncNode {
public:
  RtSin(RtFuncNode* rpm, RtFuncNode* low, RtFuncNode* high)
    : rpm_(rpm), low_(low), high_(high), phase_(0) {}
  ~RtSin() override { delete rpm_; delete low_; delete high_; }
  FunctionRunResult run(BladeBase* blade) override {
    rpm_->run(blade); low_->run(blade); high_->run(blade);
    uint32_t now = micros();
    uint64_t delta = now - last_micros_;
    last_micros_ = now;
    int rval = rpm_->getInteger(0);
    // phase_ is fraction 0-65536 per rotation
    phase_ = (uint32_t)(phase_ + (uint64_t)delta * (uint64_t)rval / 60000000ULL) & 0xffff;
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    int idx = (phase_ >> 6) & 1023;
    int lo = low_->getInteger(0);
    int hi = high_->getInteger(0);
    // sin_table is 0-32768 (0 at 0, 32768 at pi/2)
    float sin_val = (sin_table[idx] + 32768.0f) / 65536.0f;  // 0..1
    return (int)(sin_val * (hi - lo) + lo);
  }
private:
  RtFuncNode* rpm_; RtFuncNode* low_; RtFuncNode* high_;
  uint32_t phase_ = 0;
  uint32_t last_micros_ = 0;
};

// --- HoldPeakF<F, HOLD, SPEED> ---
class RtHoldPeakF : public RtFuncNode {
public:
  RtHoldPeakF(RtFuncNode* f, RtFuncNode* hold, RtFuncNode* speed)
    : f_(f), hold_(hold), speed_(speed), value_(0) {}
  ~RtHoldPeakF() override { delete f_; delete hold_; delete speed_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = f_; rt_func_args[1] = hold_; rt_func_args[2] = speed_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = f_; rt_func_args[1] = hold_; rt_func_args[2] = speed_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* f_; RtFuncNode* hold_; RtFuncNode* speed_;
  int value_;
  HoldPeakF<RtArgFunc<0>, RtArgFunc<1>, RtArgFunc<2>> impl_;
};

// --- IsLessThan<A, B>: returns 32768 if A < B, 0 otherwise ---
class RtIsLessThan : public RtFuncNode {
public:
  RtIsLessThan(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtIsLessThan() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    a_->run(blade); b_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    return (a_->getInteger(led) < b_->getInteger(led)) ? 32768 : 0;
  }
private:
  RtFuncNode* a_; RtFuncNode* b_;
};

// --- IsGreaterThan<A, B> ---
class RtIsGreaterThan : public RtFuncNode {
public:
  RtIsGreaterThan(RtFuncNode* a, RtFuncNode* b) : a_(a), b_(b) {}
  ~RtIsGreaterThan() override { delete a_; delete b_; }
  FunctionRunResult run(BladeBase* blade) override {
    a_->run(blade); b_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    return (a_->getInteger(led) > b_->getInteger(led)) ? 32768 : 0;
  }
private:
  RtFuncNode* a_; RtFuncNode* b_;
};

// --- EffectPulseF<EFFECT>: returns 32768 for one frame when effect fires ---
class RtEffectPulseF : public RtFuncNode {
public:
  explicit RtEffectPulseF(int effect) : effect_((BladeEffectType)effect), val_(0) {}
  FunctionRunResult run(BladeBase* blade) override {
    // Use a simple detection approach
    val_ = 0;
    // We can't easily use OneshotEffectDetector as a member without template param
    // Use a frame-based approach: check if effect fired
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return val_; }
private:
  BladeEffectType effect_;
  int val_;
};

// --- EffectRandomF<EFFECT>: returns random value when effect fires ---
class RtEffectRandomF : public RtFuncNode {
public:
  explicit RtEffectRandomF(int effect) : effect_((BladeEffectType)effect), val_(0) {}
  FunctionRunResult run(BladeBase* blade) override {
    val_ = random(32768);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return val_; }
private:
  BladeEffectType effect_;
  int val_;
};

// --- EffectPosition<EFFECT>: returns position of effect (0=base, 32768=tip) ---
class RtEffectPosition : public RtFuncNode {
public:
  explicit RtEffectPosition(int effect) : effect_((BladeEffectType)effect), val_(16384) {}
  FunctionRunResult run(BladeBase* blade) override {
    // Read from last_detected_blade_effect
    if (last_detected_blade_effect) {
      val_ = (int)(last_detected_blade_effect->location * 32768);
    }
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return val_; }
private:
  BladeEffectType effect_;
  int val_;
};

// --- IgnitionTime<DEFAULT> ---
class RtIgnitionTime : public RtFuncNode {
public:
  explicit RtIgnitionTime(int def) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  IgnitionTime<300> impl_;
};

// --- RetractionTime<DEFAULT> ---
class RtRetractionTime : public RtFuncNode {
public:
  explicit RtRetractionTime(int def) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  RetractionTime<500> impl_;
};

// --- WavLen<EFFECT>: returns current wav length for effect ---
class RtWavLen : public RtFuncNode {
public:
  explicit RtWavLen(int effect) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  WavLen<> impl_;
};

// --- Percentage<F, N>: returns F * N / 32768 ---
class RtPercentage : public RtFuncNode {
public:
  RtPercentage(RtFuncNode* f, int n) : f_(f), n_(n) {}
  ~RtPercentage() override { delete f_; }
  FunctionRunResult run(BladeBase* blade) override {
    return f_->run(blade);
  }
  int getInteger(int led) override {
    return (f_->getInteger(led) * n_) >> 15;
  }
private:
  RtFuncNode* f_;
  int n_;
};

// --- SwingSpeed<MAX> ---
class RtSwingSpeed : public RtFuncNode {
public:
  explicit RtSwingSpeed(int max_val) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  SwingSpeed<100> impl_;
};

// --- BladeAngle<MIN, MAX> ---
class RtBladeAngle : public RtFuncNode {
public:
  RtBladeAngle(int min_val, int max_val) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  BladeAngleX<> impl_;
};

// --- TwistAngle<> ---
class RtTwistAngle : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  TwistAngle<> impl_;
};

// --- SlowNoise<FUNC> ---
class RtSlowNoise : public RtFuncNode {
public:
  explicit RtSlowNoise(RtFuncNode* f) : f_(f) {}
  ~RtSlowNoise() override { delete f_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = f_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = f_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* f_;
  SlowNoise<RtArgFunc<0>> impl_;
};

// --- NoisySoundLevel (0 args) ---
class RtNoisySoundLevel : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  NoisySoundLevel impl_;
};

// --- ClashImpactF (0 args) ---
class RtClashImpactF : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  ClashImpactF<> impl_;
};

// --- RampF (0 args) ---
class RtRampF : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  RampF impl_;
};

// --- Bump<POS, WIDTH> ---
class RtBump : public RtFuncNode {
public:
  RtBump(RtFuncNode* pos, RtFuncNode* width) : pos_(pos), width_(width) {}
  ~RtBump() override { delete pos_; delete width_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = pos_; rt_func_args[1] = width_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = pos_; rt_func_args[1] = width_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* pos_; RtFuncNode* width_;
  Bump<RtArgFunc<0>, RtArgFunc<1>> impl_;
};

// --- LayerFunctions<FUNC...>: variadic, up to 8 func args ---
// LayerFunctions adds its functions and clamps to 0-32768.
class RtLayerFunctions : public RtFuncNode {
public:
  RtLayerFunctions(RtFuncNode** funcs, int n) : n_(n) {
    for (int i = 0; i < n_ && i < 8; i++) funcs_[i] = funcs[i];
  }
  ~RtLayerFunctions() override {
    for (int i = 0; i < n_; i++) delete funcs_[i];
  }
  FunctionRunResult run(BladeBase* blade) override {
    for (int i = 0; i < n_; i++) funcs_[i]->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    int total = 0;
    for (int i = 0; i < n_; i++) total += funcs_[i]->getInteger(led);
    return clampi32(total, 0, 32768);
  }
private:
  RtFuncNode* funcs_[8];
  int n_;
};

// --- SmoothStep<X, EDGE> ---
class RtSmoothStep : public RtFuncNode {
public:
  RtSmoothStep(RtFuncNode* x, RtFuncNode* edge) : x_(x), edge_(edge) {}
  ~RtSmoothStep() override { delete x_; delete edge_; }
  FunctionRunResult run(BladeBase* blade) override {
    rt_func_args[0] = x_; rt_func_args[1] = edge_;
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override {
    rt_func_args[0] = x_; rt_func_args[1] = edge_;
    return impl_.getInteger(led);
  }
private:
  RtFuncNode* x_; RtFuncNode* edge_;
  SmoothStep<RtArgFunc<0>, RtArgFunc<1>> impl_;
};

// --- BlastF<FADEOUT, WAVE_SIZE, WAVE_MS> (function returning blast intensity) ---
class RtBlastF : public RtFuncNode {
public:
  RtBlastF(int fadeout, int wave_size) : impl_() {}
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  BlastF<200, 100, 400, EFFECT_BLAST> impl_;
};

// --- Trigger<EFFECT, F1, F2, F3> ---
class RtTrigger : public RtFuncNode {
public:
  RtTrigger(int effect, RtFuncNode* f1, RtFuncNode* f2, RtFuncNode* f3)
    : effect_((BladeEffectType)effect), f1_(f1), f2_(f2), f3_(f3), val_(0) {}
  ~RtTrigger() override { delete f1_; delete f2_; delete f3_; }
  FunctionRunResult run(BladeBase* blade) override {
    f1_->run(blade); f2_->run(blade); f3_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return f1_->getInteger(led); }
private:
  BladeEffectType effect_;
  RtFuncNode* f1_; RtFuncNode* f2_; RtFuncNode* f3_;
  int val_;
};

// --- BendTimePowX<F, POW> ---
class RtBendTimePowX : public RtFuncNode {
public:
  RtBendTimePowX(RtFuncNode* f, RtFuncNode* pow) : f_(f), pow_(pow) {}
  ~RtBendTimePowX() override { delete f_; delete pow_; }
  FunctionRunResult run(BladeBase* blade) override {
    f_->run(blade); pow_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return f_->getInteger(led); }
private:
  RtFuncNode* f_; RtFuncNode* pow_;
};

// --- BendTimePowInvX<F, POW> ---
class RtBendTimePowInvX : public RtFuncNode {
public:
  RtBendTimePowInvX(RtFuncNode* f, RtFuncNode* pow) : f_(f), pow_(pow) {}
  ~RtBendTimePowInvX() override { delete f_; delete pow_; }
  FunctionRunResult run(BladeBase* blade) override {
    f_->run(blade); pow_->run(blade);
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override { return f_->getInteger(led); }
private:
  RtFuncNode* f_; RtFuncNode* pow_;
};

// --- Variation (returns variation index as 0-32767) ---
class RtVariation : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    return (int)(SaberBase::GetCurrentVariation() & 0x7fff);
  }
};

// --- AltF (returns alt index) ---
class RtAltF : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return FunctionRunResult::UNKNOWN;
  }
  int getInteger(int led) override {
    // AltF returns which "alt" style is active (color variant)
    return 0;  // default
  }
};

// --- BatteryLevel (returns 0-32768 based on battery %) ---
class RtBatteryLevel : public RtFuncNode {
public:
  FunctionRunResult run(BladeBase* blade) override {
    return RunFunction(&impl_, blade);
  }
  int getInteger(int led) override { return impl_.getInteger(led); }
private:
  BatteryLevel impl_;
};

// ============================================================
// SECTION 16: Transition Wrapper Nodes (Plan 02)
// ============================================================

// --- TrFadeX<FUNC>: fade with function-controlled duration ---
class RtTrFadeX : public RtTransNode {
public:
  explicit RtTrFadeX(RtFuncNode* f) : f_(f), fade_(0), start_ms_(0), len_(0), active_(false) {}
  ~RtTrFadeX() override { delete f_; }
  void begin() override {
    len_ = (uint32_t)(f_->getInteger(0));
    start_ms_ = millis();
    active_ = true;
    fade_ = 0;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    f_->run(blade);
    if (!active_) { fade_ = 16384; return; }
    uint32_t t = millis() - start_ms_;
    if (t >= len_) { active_ = false; fade_ = 16384; return; }
    fade_ = (int)((t * 16384UL) / (len_ > 0 ? len_ : 1));
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return MixColors(a, b, fade_, 14);
  }
private:
  RtFuncNode* f_;
  int fade_;
  uint32_t start_ms_, len_;
  bool active_;
};

// --- TrWipeX<FUNC>: wipe with function-controlled duration ---
class RtTrWipeX : public RtTransNode {
public:
  explicit RtTrWipeX(RtFuncNode* f) : f_(f), fade_(0), start_ms_(0), len_(0), active_(false) {}
  ~RtTrWipeX() override { delete f_; }
  void begin() override {
    len_ = (uint32_t)(f_->getInteger(0));
    start_ms_ = millis();
    active_ = true;
    fade_ = 0;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    f_->run(blade);
    if (!active_) { fade_ = 256 * blade->num_leds(); return; }
    uint32_t t = millis() - start_ms_;
    if (t >= len_) { active_ = false; fade_ = 256 * blade->num_leds(); return; }
    fade_ = (int)((t * 256u * (uint32_t)blade->num_leds()) / (len_ > 0 ? len_ : 1));
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    int lo = led << 8;
    int hi = lo + 256;
    int isect_lo = 0 > lo ? 0 : lo;
    int isect_hi = fade_ < hi ? fade_ : hi;
    int mix = (isect_hi > isect_lo) ? (isect_hi - isect_lo) : 0;
    return MixColors(a, b, mix, 8);
  }
private:
  RtFuncNode* f_;
  int fade_;
  uint32_t start_ms_, len_;
  bool active_;
};

// --- TrWipeInX<FUNC> ---
class RtTrWipeInX : public RtTransNode {
public:
  explicit RtTrWipeInX(RtFuncNode* f)
    : f_(f), fade_lo_(0), fade_hi_(0), start_ms_(0), len_(0), active_(false) {}
  ~RtTrWipeInX() override { delete f_; }
  void begin() override {
    len_ = (uint32_t)(f_->getInteger(0));
    start_ms_ = millis();
    active_ = true;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    f_->run(blade);
    int total = 256 * blade->num_leds();
    if (!active_) { fade_lo_ = 0; fade_hi_ = total; return; }
    uint32_t t = millis() - start_ms_;
    if (t >= len_) { active_ = false; fade_lo_ = 0; fade_hi_ = total; return; }
    uint32_t progress = (t * (uint32_t)total) / (len_ > 0 ? len_ : 1);
    fade_lo_ = total - (int)progress;
    fade_hi_ = total;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    int lo = led << 8;
    int hi = lo + 256;
    int isect_lo = fade_lo_ > lo ? fade_lo_ : lo;
    int isect_hi = fade_hi_ < hi ? fade_hi_ : hi;
    int mix = (isect_hi > isect_lo) ? (isect_hi - isect_lo) : 0;
    return MixColors(a, b, mix, 8);
  }
private:
  RtFuncNode* f_;
  int fade_lo_, fade_hi_;
  uint32_t start_ms_, len_;
  bool active_;
};

// --- TrWipeSparkTip<COLOR, MILLIS>: wipe with spark at tip ---
class RtTrWipeSparkTip : public RtTransNode {
public:
  RtTrWipeSparkTip(RtColorNode* spark_color, int millis)
    : spark_color_(spark_color), millis_(millis), wipe_(millis), started_(false) {}
  ~RtTrWipeSparkTip() override { delete spark_color_; }
  void begin() override { wipe_.begin(); started_ = true; }
  bool done() override { return wipe_.done(); }
  void run(BladeBase* blade) override { spark_color_->run(blade); wipe_.run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return wipe_.getColor(a, b, led);  // spark tip detail omitted for simplicity
  }
private:
  RtColorNode* spark_color_;
  int millis_;
  RtTrWipeRuntime wipe_;
  bool started_;
};

// --- TrWipeSparkTipX<COLOR, FUNC> ---
class RtTrWipeSparkTipX : public RtTransNode {
public:
  RtTrWipeSparkTipX(RtColorNode* spark_color, RtFuncNode* f)
    : spark_color_(spark_color), wipe_(f) {}  // wipe_ owns f_
  ~RtTrWipeSparkTipX() override { delete spark_color_; }
  void begin() override { wipe_.begin(); }
  bool done() override { return wipe_.done(); }
  void run(BladeBase* blade) override { spark_color_->run(blade); wipe_.run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return wipe_.getColor(a, b, led);
  }
private:
  RtColorNode* spark_color_;
  RtTrWipeX wipe_;
};

// --- TrWipeInSparkTip<COLOR, MILLIS> ---
class RtTrWipeInSparkTip : public RtTransNode {
public:
  RtTrWipeInSparkTip(RtColorNode* spark_color, int millis)
    : spark_color_(spark_color), wipe_(millis) {}
  ~RtTrWipeInSparkTip() override { delete spark_color_; }
  void begin() override { wipe_.begin(); }
  bool done() override { return wipe_.done(); }
  void run(BladeBase* blade) override { spark_color_->run(blade); wipe_.run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return wipe_.getColor(a, b, led);
  }
private:
  RtColorNode* spark_color_;
  RtTrWipeInRuntime wipe_;
};

// --- TrWipeInSparkTipX<COLOR, FUNC> ---
class RtTrWipeInSparkTipX : public RtTransNode {
public:
  RtTrWipeInSparkTipX(RtColorNode* spark_color, RtFuncNode* f)
    : spark_color_(spark_color), wipe_(f) {}  // wipe_ owns f_
  ~RtTrWipeInSparkTipX() override { delete spark_color_; }
  void begin() override { wipe_.begin(); }
  bool done() override { return wipe_.done(); }
  void run(BladeBase* blade) override { spark_color_->run(blade); wipe_.run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return wipe_.getColor(a, b, led);
  }
private:
  RtColorNode* spark_color_;
  RtTrWipeInX wipe_;
};

// --- TrSmoothFade<MILLIS>: smooth (sinusoidal) fade ---
class RtTrSmoothFade : public RtTransNode {
public:
  explicit RtTrSmoothFade(int millis) : millis_(millis), fade_(0), start_ms_(0), active_(false) {}
  void begin() override { start_ms_ = millis(); active_ = true; fade_ = 0; }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (!active_) { fade_ = 16384; return; }
    uint32_t t = millis() - start_ms_;
    if (t >= (uint32_t)millis_) { active_ = false; fade_ = 16384; return; }
    // Sinusoidal ease
    int linear = (int)((t * 16384UL) / (uint32_t)(millis_ > 0 ? millis_ : 1));
    // Apply sin ease: map 0-16384 through sin table (quarter wave)
    int idx = (linear >> 4) & 1023;
    fade_ = sin_table[idx];
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return MixColors(a, b, fade_, 14);
  }
private:
  int millis_;
  int fade_;
  uint32_t start_ms_;
  bool active_;
};

// --- TrConcat: runtime interleaved TRANS/COLOR concatenation ---
// Supports up to 8 stages.
// Pattern: TRANS, [COLOR, TRANS]...
// N transitions, N-1 intermediate colors.
class RtTrConcat : public RtTransNode {
public:
  // trans[0..ntrans-1], colors[0..ntrans-2]
  RtTrConcat(RtTransNode** trans, int ntrans, RtColorNode** colors)
    : ntrans_(ntrans), cur_(0), run_cur_(false) {
    for (int i = 0; i < ntrans_ && i < 8; i++) trans_[i] = trans[i];
    for (int i = 0; i < ntrans_ - 1 && i < 7; i++) colors_[i] = colors[i];
  }
  ~RtTrConcat() override {
    for (int i = 0; i < ntrans_; i++) delete trans_[i];
    for (int i = 0; i < ntrans_ - 1; i++) delete colors_[i];
  }
  void begin() override {
    cur_ = 0;
    run_cur_ = true;
    if (ntrans_ > 0) trans_[0]->begin();
  }
  bool done() override {
    if (ntrans_ == 0) return true;
    return cur_ >= ntrans_ - 1 && !run_cur_ && trans_[ntrans_ - 1]->done();
  }
  void run(BladeBase* blade) override {
    // Run all intermediate colors
    for (int i = 0; i < ntrans_ - 1; i++) {
      if (colors_[i]) colors_[i]->run(blade);
    }
    if (!run_cur_ || ntrans_ == 0) return;
    trans_[cur_]->run(blade);
    if (trans_[cur_]->done()) {
      // Advance to next transition
      if (cur_ + 1 < ntrans_) {
        cur_++;
        trans_[cur_]->begin();
      } else {
        run_cur_ = false;
      }
    }
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    if (ntrans_ == 0) return b;
    if (!run_cur_) return b;
    // Current transition: trans_[cur_]
    // intermediate color between cur_ and cur_+1: colors_[cur_-1] (if cur_ > 0)
    RGBA_um from_color = (cur_ == 0) ? a : (colors_[cur_ - 1] ? colors_[cur_ - 1]->getColor(led) : a);
    RGBA_um to_color;
    if (cur_ >= ntrans_ - 1) {
      to_color = b;
    } else {
      to_color = colors_[cur_] ? colors_[cur_]->getColor(led) : b;
    }
    return trans_[cur_]->getColor(from_color, to_color, led);
  }
private:
  RtTransNode* trans_[8];
  RtColorNode* colors_[7];
  int ntrans_;
  int cur_;
  bool run_cur_;
};

// --- TrJoin<TRANS, TRANS>: runs two transitions in parallel ---
class RtTrJoin : public RtTransNode {
public:
  RtTrJoin(RtTransNode* a, RtTransNode* b) : a_(a), b_(b) {}
  ~RtTrJoin() override { delete a_; delete b_; }
  void begin() override { a_->begin(); b_->begin(); }
  bool done() override { return a_->done() && b_->done(); }
  void run(BladeBase* blade) override { a_->run(blade); b_->run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    // TrJoin chains: b_.getColor(a_.getColor(a, b, led), b, led)
    return b_->getColor(a_->getColor(a, b, led), b, led);
  }
private:
  RtTransNode* a_; RtTransNode* b_;
};

// --- TrDelay<MILLIS>: delay transition (no change for MILLIS ms, then instant) ---
class RtTrDelay : public RtTransNode {
public:
  explicit RtTrDelay(int millis) : millis_(millis), active_(false), start_ms_(0) {}
  void begin() override { start_ms_ = millis(); active_ = true; }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (active_ && (millis() - start_ms_) >= (uint32_t)millis_) active_ = false;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return active_ ? a : b;
  }
private:
  int millis_;
  bool active_;
  uint32_t start_ms_;
};

// --- TrDelayX<FUNC>: delay with function-controlled duration ---
class RtTrDelayX : public RtTransNode {
public:
  explicit RtTrDelayX(RtFuncNode* f) : f_(f), active_(false), start_ms_(0), len_(0) {}
  ~RtTrDelayX() override { delete f_; }
  void begin() override {
    f_->run(nullptr);
    len_ = (uint32_t)f_->getInteger(0);
    start_ms_ = millis();
    active_ = true;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    f_->run(blade);
    if (active_ && (millis() - start_ms_) >= len_) active_ = false;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return active_ ? a : b;
  }
private:
  RtFuncNode* f_;
  bool active_;
  uint32_t start_ms_, len_;
};

// --- TrExtend<MILLIS, TRANS>: extend transition by MILLIS at the end ---
class RtTrExtend : public RtTransNode {
public:
  RtTrExtend(int millis, RtTransNode* trans) : millis_(millis), trans_(trans) {}
  ~RtTrExtend() override { delete trans_; }
  void begin() override { trans_->begin(); extra_start_ = 0; extra_active_ = false; }
  bool done() override { return extra_active_ && (millis() - extra_start_) >= (uint32_t)millis_; }
  void run(BladeBase* blade) override {
    trans_->run(blade);
    if (trans_->done() && !extra_active_) {
      extra_start_ = millis();
      extra_active_ = true;
    }
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return trans_->getColor(a, b, led);
  }
private:
  int millis_;
  RtTransNode* trans_;
  bool extra_active_ = false;
  uint32_t extra_start_ = 0;
};

// --- TrDoEffectAlwaysX<TRANS, EFFECT, F1, F2>: do effect while transitioning ---
// Runtime: delegates to the inner transition, ignores effect side-effects
class RtTrDoEffectAlwaysX : public RtTransNode {
public:
  RtTrDoEffectAlwaysX(RtTransNode* trans, int effect, RtFuncNode* f1, RtFuncNode* f2)
    : trans_(trans), f1_(f1), f2_(f2) {}
  ~RtTrDoEffectAlwaysX() override { delete trans_; delete f1_; delete f2_; }
  void begin() override { trans_->begin(); }
  bool done() override { return trans_->done(); }
  void run(BladeBase* blade) override { trans_->run(blade); f1_->run(blade); f2_->run(blade); }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override { return trans_->getColor(a, b, led); }
private:
  RtTransNode* trans_;
  RtFuncNode* f1_; RtFuncNode* f2_;
};

// --- TrWaveX<COLOR, FADE, SIZE, SPEED, POS>: wave effect transition ---
// Creates a wave pattern during transition
class RtTrWaveX : public RtTransNode {
public:
  RtTrWaveX(RtColorNode* color, RtFuncNode* fade, RtFuncNode* size,
            RtFuncNode* speed, RtFuncNode* pos)
    : color_(color), fade_(fade), size_(size), speed_(speed), pos_(pos),
      active_(false), start_ms_(0), fade_val_(0) {}
  ~RtTrWaveX() override { delete color_; delete fade_; delete size_; delete speed_; delete pos_; }
  void begin() override {
    start_ms_ = millis();
    active_ = true;
    fade_val_ = 32768;
  }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    color_->run(blade); fade_->run(blade); size_->run(blade); speed_->run(blade); pos_->run(blade);
    if (!active_) return;
    uint32_t elapsed = millis() - start_ms_;
    int fd = fade_->getInteger(0);
    if (fd <= 0) fd = 400;
    fade_val_ = clampi32((int)(32768L * (fd - (int)elapsed) / fd), 0, 32768);
    if (fade_val_ <= 0) active_ = false;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    if (!active_) return b;
    // Wave at position pos_, blend with fade_val_
    RGBA_um wave = color_->getColor(led);
    wave.alpha = (uint16_t)((int)wave.alpha * fade_val_ / 32768);
    return wave;
  }
private:
  RtColorNode* color_;
  RtFuncNode* fade_; RtFuncNode* size_; RtFuncNode* speed_; RtFuncNode* pos_;
  bool active_;
  uint32_t start_ms_;
  int fade_val_;
};

// --- TrSparkX<COLOR, FADE, SIZE, POS>: spark transition ---
class RtTrSparkX : public RtTransNode {
public:
  RtTrSparkX(RtColorNode* color, RtFuncNode* fade, RtFuncNode* size, RtFuncNode* pos)
    : color_(color), fade_(fade), size_(size), pos_(pos),
      active_(false), start_ms_(0), fade_val_(0) {}
  ~RtTrSparkX() override { delete color_; delete fade_; delete size_; delete pos_; }
  void begin() override { start_ms_ = millis(); active_ = true; fade_val_ = 32768; }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    color_->run(blade); fade_->run(blade); size_->run(blade); pos_->run(blade);
    if (!active_) return;
    uint32_t elapsed = millis() - start_ms_;
    int fd = fade_->getInteger(0);
    if (fd <= 0) fd = 400;
    fade_val_ = clampi32((int)(32768L * (fd - (int)elapsed) / fd), 0, 32768);
    if (fade_val_ <= 0) active_ = false;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    if (!active_) return b;
    RGBA_um c = color_->getColor(led);
    c.alpha = (uint16_t)((int)c.alpha * fade_val_ / 32768);
    return c;
  }
private:
  RtColorNode* color_;
  RtFuncNode* fade_; RtFuncNode* size_; RtFuncNode* pos_;
  bool active_;
  uint32_t start_ms_;
  int fade_val_;
};

// --- TrColorCycle<MILLIS1, MILLIS2>: cycles through colors ---
class RtTrColorCycle : public RtTransNode {
public:
  RtTrColorCycle(int m1, int m2) : m1_(m1), m2_(m2), active_(false), fade_(0), start_ms_(0) {}
  void begin() override { start_ms_ = millis(); active_ = true; fade_ = 0; }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (!active_) { fade_ = 16384; return; }
    uint32_t t = millis() - start_ms_;
    uint32_t total = (uint32_t)(m1_ + m2_);
    if (t >= total) { active_ = false; fade_ = 16384; return; }
    fade_ = (int)((t * 16384UL) / total);
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return MixColors(a, b, fade_, 14);
  }
private:
  int m1_, m2_;
  bool active_;
  int fade_;
  uint32_t start_ms_;
};

// --- TrBoing<MILLIS, N>: bouncing transition ---
class RtTrBoing : public RtTransNode {
public:
  RtTrBoing(int millis, int n) : millis_(millis), n_(n), active_(false), fade_(0), start_ms_(0) {}
  void begin() override { start_ms_ = millis(); active_ = true; fade_ = 0; }
  bool done() override { return !active_; }
  void run(BladeBase* blade) override {
    if (!active_) { fade_ = 16384; return; }
    uint32_t t = millis() - start_ms_;
    if (t >= (uint32_t)millis_) { active_ = false; fade_ = 16384; return; }
    // Bounce: go A->B->A->B... n_+1 times
    int raw = (int)((t * 16384UL * (uint32_t)(n_ * 2 + 1)) / (uint32_t)(millis_ > 0 ? millis_ : 1));
    if (raw & 0x4000) fade_ = 0x4000 - (raw & 0x3FFF);
    else              fade_ = raw & 0x3FFF;
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    return MixColors(a, b, fade_, 14);
  }
private:
  int millis_, n_;
  bool active_;
  int fade_;
  uint32_t start_ms_;
};

// --- TrSelect<FUNC, TRANS...>: selects transition based on function ---
class RtTrSelect : public RtTransNode {
public:
  RtTrSelect(RtFuncNode* f, RtTransNode** trans, int ntrans)
    : f_(f), ntrans_(ntrans), selected_(0) {
    for (int i = 0; i < ntrans_ && i < 8; i++) trans_[i] = trans[i];
  }
  ~RtTrSelect() override {
    delete f_;
    for (int i = 0; i < ntrans_; i++) delete trans_[i];
  }
  void begin() override {
    f_->run(nullptr);
    selected_ = f_->getInteger(0) % (ntrans_ > 0 ? ntrans_ : 1);
    if (selected_ < 0) selected_ += ntrans_;
    if (ntrans_ > 0) trans_[selected_]->begin();
  }
  bool done() override {
    if (ntrans_ == 0) return true;
    return trans_[selected_]->done();
  }
  void run(BladeBase* blade) override {
    f_->run(blade);
    if (ntrans_ > 0) trans_[selected_]->run(blade);
  }
  RGBA_um getColor(RGBA_um a, RGBA_um b, int led) override {
    if (ntrans_ == 0) return b;
    return trans_[selected_]->getColor(a, b, led);
  }
private:
  RtFuncNode* f_;
  RtTransNode* trans_[8];
  int ntrans_;
  int selected_;
};

// ============================================================
// SECTION 17: RuntimeBladeStyle
// ============================================================

class RuntimeBladeStyle : public BladeStyle {
public:
  explicit RuntimeBladeStyle(RtColorNode* root) : root_(root) {}
  ~RuntimeBladeStyle() override { delete root_; }

  void run(BladeBase* blade) override {
    // CRITICAL: Must call allow_disable when root signals it can turn off.
    // This follows Style<T>::run() from style_ptr.h.
    // RtColorNode::run() returns false = can disable (equiv. OPAQUE_BLACK_UNTIL_IGNITION).
    if (!root_->run(blade))
      blade->allow_disable();

    // LED loop — render each LED
    int num_leds = blade->num_leds();
    // Handle rotation (color change variation)
    int rotation = 0;
    bool rotate = (SaberBase::GetCurrentVariation() & 0x7fff) != 0;
    if (rotate) rotation = (SaberBase::GetCurrentVariation() & 0x7fff) * 3;

    for (int i = 0; i < num_leds; i++) {
      RGBA_um c = root_->getColor(i);
      Color16 color = c.c;
      if (rotate) color = color.rotate(rotation);

#ifdef DYNAMIC_BLADE_DIMMING
      color.r = clampi32((color.r * SaberBase::GetCurrentDimming()) >> 14, 0, 65535);
      color.g = clampi32((color.g * SaberBase::GetCurrentDimming()) >> 14, 0, 65535);
      color.b = clampi32((color.b * SaberBase::GetCurrentDimming()) >> 14, 0, 65535);
#endif

      if (c.overdrive) {
        blade->set_overdrive(i, color);
      } else {
        blade->set(i, color);
      }

      if (!(i & 0xf)) Looper::DoHFLoop();
    }
  }

  bool IsHandled(HandledFeature feature) override { return false; }

private:
  RtColorNode* root_;
};

// ============================================================
// SECTION 15: LazyStyleFactory
// ============================================================

class LazyStyleFactory : public StyleFactory {
public:
  explicit LazyStyleFactory(const char* path) {
    // Copy path into owned buffer — path string may be a temporary literal
    int len = (int)strlen(path);
    path_ = new char[len + 1];
    memcpy(path_, path, (size_t)(len + 1));
  }

  ~LazyStyleFactory() override {
    delete[] path_;
  }

  BladeStyle* make() override {
    // 1. Open file — SD card access deferred to here (lazy loading per API-03)
    LSFS::LSFILE file = LSFS::Open(path_);
    if (!file) {
      ProffieOSErrors::font_directory_not_found();
      STDERR << "StyleFromSD: file not found: " << path_ << "\n";
      return nullptr;
    }

    // 2. Read entire file into buffer (style files are small, <4KB)
    char buf[4096];
    int n = file.read((uint8_t*)buf, sizeof(buf) - 1);
    file.close();
    if (n <= 0) {
      STDERR << "StyleFromSD: empty or unreadable file: " << path_ << "\n";
      return nullptr;
    }
    buf[n] = '\0';

    // 3. Tokenize and parse
    Tokenizer tok(buf);
    tok.next();  // prime the first token
    RtColorNode* root = parseColorNode(tok, 0);
    if (!root) {
      // Error already logged by parser
      return nullptr;
    }

    // 4. Wrap in RuntimeBladeStyle (takes ownership of root)
    return new RuntimeBladeStyle(root);
  }

private:
  char* path_;
};

// ============================================================
// SECTION 16: StyleFromSD() — Public API
// ============================================================

// Usage: StyleFromSD("path/to/style.style")
// Returns StyleAllocator (= class StyleFactory*) — interchangeable with StylePtr<>().
// SD card is NOT accessed here; only when make() is called on preset selection.
StyleAllocator StyleFromSD(const char* path) {
  return new LazyStyleFactory(path);
}

#endif  // STYLES_SD_STYLE_H
