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
  {"Layers",    makeLayers,   nullptr,    nullptr   },
  {"AlphaL",    makeAlphaL,   nullptr,    nullptr   },
  {"InOutTrL",  makeInOutTrL, nullptr,    nullptr   },
  {"Int",       nullptr,      makeInt,    nullptr   },
  {"Ifon",      nullptr,      makeIfon,   nullptr   },
  {"TrWipe",    nullptr,      nullptr,    makeTrWipe    },
  {"TrWipeIn",  nullptr,      nullptr,    makeTrWipeIn  },
  {"TrFade",    nullptr,      nullptr,    makeTrFade    },
  {"TrInstant", nullptr,      nullptr,    makeTrInstant },
  {nullptr,     nullptr,      nullptr,    nullptr   }  // sentinel
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
// SECTION 14: RuntimeBladeStyle
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
