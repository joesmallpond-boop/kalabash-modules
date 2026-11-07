#include <cstdio>
#include "ElementsCore.h"

#include <cmath>
#include "../common/ZeroAlloc.h"
#include <algorithm>
#include "elements/dsp/part.h"

namespace kb {

static constexpr int kElementsBlock = 16;

ElementsCore::ElementsCore() {
  reverb_.assign(32768, 0);
  part_ = newZeroed<elements::Part>();
  part_->Init(reverb_.data());
  initPatch();
  uint32_t seed[3] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u };
  part_->Seed(seed, 3);
}

ElementsCore::~ElementsCore() { deleteZeroed(part_); }

// Part::Init only sets a few patch fields; several are smoothed from their previous
// value, so give every field a defined starting point (uninitialised memory -> NaN).
void ElementsCore::initPatch() {
  elements::Patch* pt = part_->mutable_patch();
  pt->exciter_envelope_shape = 0.5f;
  pt->exciter_bow_level = 0.0f; pt->exciter_bow_timbre = 0.5f;
  pt->exciter_blow_level = 0.0f; pt->exciter_blow_meta = 0.5f; pt->exciter_blow_timbre = 0.5f;
  pt->exciter_strike_level = 0.8f; pt->exciter_strike_meta = 0.5f; pt->exciter_strike_timbre = 0.5f;
  pt->exciter_signature = 0.0f;
  pt->resonator_geometry = 0.5f; pt->resonator_brightness = 0.5f; pt->resonator_damping = 0.5f;
  pt->resonator_position = 0.3f; pt->space = 0.4f;
  pt->modulation_frequency = 0.0f;
}

void ElementsCore::prepare(double hostRate) {
  bridge_.setup(hostRate, elements::kSampleRate, kElementsBlock, true);
}

void ElementsCore::noteOn(int note, float velocity) {
  held_.erase(std::remove(held_.begin(), held_.end(), note), held_.end());
  held_.push_back(note);
  note_ = note;
  velocity_ = velocity;
  rng_ = rng_ * 1664525u + 1013904223u;
  randomValue_ = (float) (rng_ >> 8) / 16777216.0f * 2.0f - 1.0f;
  if (gate_) retrigger_ = true;  // re-strike on a new note while held
  gate_ = true;
}

void ElementsCore::noteOff(int note) {
  held_.erase(std::remove(held_.begin(), held_.end(), note), held_.end());
  if (note == note_) {
    if (!held_.empty()) note_ = held_.back();
    else gate_ = false;
  }
}

void ElementsCore::allNotesOff() { held_.clear(); gate_ = false; }

void ElementsCore::render(const ElementsParams& p, const float* inL, const float* inR,
                          float* outL, float* outR, int n) {
  static thread_local std::vector<float> zeros;
  if ((int) zeros.size() < n) zeros.assign((size_t) n, 0.0f);
  const float* a = inL ? inL : zeros.data();
  const float* b = inR ? inR : a;
  bridge_.processEffect(a, b, outL, outR, n,
      [this, &p](const float* il, const float* ir, float* l, float* r, int bn) { renderNative(p, il, ir, l, r, bn); });
}

static inline float clamp01(float x, float hi = 0.9995f) { return x < 0.0f ? 0.0f : (x > hi ? hi : x); }

void ElementsCore::renderNative(const ElementsParams& p, const float* inL, const float* inR,
                                float* l, float* r, int n) {
  if (p.model != model_) { part_->set_resonator_model(elements::ResonatorModel(std::max(0, std::min(2, p.model)))); model_ = p.model; }
  if (p.ominous != ominous_) { part_->set_easter_egg(p.ominous); ominous_ = p.ominous; }

  // modulation source value, -1..1 or 0..1
  const float lfoHz = 0.05f * std::pow(400.0f, p.lfoRate);
  lfoPhase_ += lfoHz * n / elements::kSampleRate;
  if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
  float mod = 0.0f;
  switch (p.modSource) {
    case MOD_VELOCITY: mod = velocity_; break;
    case MOD_WHEEL: mod = p.modWheel; break;
    case MOD_AFTERTOUCH: mod = p.aftertouch; break;
    case MOD_LFO: mod = std::sin(2.0f * (float) M_PI * lfoPhase_); break;
    case MOD_RANDOM: mod = randomValue_; break;
    default: break;
  }

  elements::Patch* pt = part_->mutable_patch();
  pt->exciter_envelope_shape = p.contour;
  pt->exciter_bow_level = p.bowLevel;
  pt->exciter_blow_level = p.blowLevel;
  pt->exciter_strike_level = p.strikeLevel;
  // smoothing like the hardware CV scaler
  auto smooth = [](float& dst, float v, float k) { dst += (v - dst) * k; };
  pt->exciter_bow_timbre = clamp01(p.bowTimbre + mod * p.modBowTimbre);
  smooth(pt->exciter_blow_meta, clamp01(p.flow + mod * p.modFlow), 0.05f);
  pt->exciter_blow_timbre = clamp01(p.blowTimbre + mod * p.modBlowTimbre);
  smooth(pt->exciter_strike_meta, clamp01(p.mallet + mod * p.modMallet), 0.05f);
  pt->exciter_strike_timbre = clamp01(p.strikeTimbre + mod * p.modStrikeTimbre, 0.995f);
  smooth(pt->resonator_geometry, clamp01(p.geometry + mod * p.modGeometry), 0.05f);
  pt->resonator_brightness = clamp01(p.brightness + mod * p.modBrightness);
  pt->resonator_damping = clamp01(p.damping + mod * p.modDamping);
  smooth(pt->resonator_position, clamp01(p.position + mod * p.modPosition), 0.01f);
  float sp = p.space + mod * p.modSpace;
  smooth(pt->space, sp < 0.0f ? 0.0f : (sp > 2.0f ? 2.0f : sp), 0.01f);

  elements::PerformanceState s;
  bool g = gate_ || p.play;
  if (retrigger_) { g = false; retrigger_ = false; }
  s.gate = g;
  s.note = (float) note_ + p.coarse + p.fine + p.pitchBend * p.bendRange;
  if (p.tuned && p.model != 0 && !p.ominous) {
    // The string models' delay loop comes out a fraction of a sample short at
    // 32 kHz, which makes high notes sharp. Ask for a slightly longer period.
    const float f = 440.0f * std::pow(2.0f, (s.note - 69.0f) / 12.0f);
    const float period = elements::kSampleRate / f;
    s.note -= 12.0f * std::log2((period + p.stringComp) / period);
  }
  s.modulation = p.fmAmt * 12.0f * mod;
  const float vel = gate_ ? velocity_ : 0.8f;
  s.strength = std::max(0.0f, std::min(1.0f, 1.0f - p.velToStrength + p.velToStrength * vel));

  float strike[kElementsBlock], blow[kElementsBlock], main[kElementsBlock], aux[kElementsBlock];
  const float kGate = 0.0001f;
  for (int i = 0; i < n; ++i) {
    // noise gate as on the module, then gain
    float st = inL[i] * p.extIn, bl = inR[i] * p.extIn;
    float e = st * st - strikeLevel_; strikeLevel_ += e * (e > 0 ? 0.1f : 0.0001f);
    float gn = strikeLevel_ <= kGate ? strikeLevel_ / kGate : 1.0f;
    strike[i] = st * gn;
    e = bl * bl - blowLevel_; blowLevel_ += e * (e > 0 ? 0.1f : 0.0001f);
    gn = blowLevel_ <= kGate ? blowLevel_ / kGate : 1.0f;
    blow[i] = bl * gn;
  }
  part_->Process(s, blow, strike, main, aux, (size_t) n);
  float pk = 0.0f;
  bool panic = false;
  for (int i = 0; i < n; ++i) {
    float a = aux[i] * p.level, m = main[i] * p.level;
    if (!std::isfinite(a) || !std::isfinite(m)) { a = m = 0.0f; panic = true; }
    l[i] = std::tanh(a);
    r[i] = std::tanh(m);
    pk = std::max(pk, std::fabs(m));
  }
  if (panic) {
    std::fill(reverb_.begin(), reverb_.end(), 0);
    part_->Init(reverb_.data());
    initPatch();
    model_ = -1; ominous_ = !p.ominous;
  }
  exciterMeter_ = part_->exciter_level();
  resonatorMeter_ = std::max(pk, resonatorMeter_ * 0.95f);
}

}  // namespace kb
