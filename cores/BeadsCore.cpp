#include <cstdio>
#include "BeadsCore.h"

#include <cmath>
#include "../common/ZeroAlloc.h"
#include <algorithm>
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "stmlib/utils/gate_flags.h"

namespace kb {

static constexpr int kBlockRate = 4000;

BeadsCore::BeadsCore() {
  buf1_.assign(512 * 1024, 0);
  buf2_.assign(256 * 1024, 0);
  reverb_.assign(32768, 0);
  uint8_t* buffers[3] = { buf1_.data(), buf1_.data() + 256 * 1024, buf2_.data() };
  proc_ = newZeroed<beads::Processor>();
  proc_->Init(buffers, reverb_.data());
  synth_ = newZeroed<beads::Synth>();
  synth_->Init(reverb_.data());
}

BeadsCore::~BeadsCore() { deleteZeroed(proc_); deleteZeroed(synth_); }

void BeadsCore::prepare(double hostRate) {
  hostRate_ = hostRate;
  configured_ = false;
  configure(media_ < 0 ? 3 : media_, mono_, synthMode_);
}

void BeadsCore::configure(int media, bool mono, bool synth) {
  const bool needProcConfig = !configured_ || media != media_ || mono != mono_ || (synthMode_ && !synth);
  media_ = media; mono_ = mono; synthMode_ = synth;
  if (needProcConfig && !synth) proc_->Configure(beads::Processor::Media(media), mono);
  const float sr = synth ? 48000.0f : proc_->sample_rate();
  block_ = (int) sr / kBlockRate;
  const int oldLatency = bridge_.latencySamples();
  bridge_.setup(hostRate_, sr, block_, true);
  if (bridge_.latencySamples() != oldLatency) latencyChanged_ = true;
  configured_ = true;
}

float BeadsCore::pitchKnobToSemitones(float knob) {
  // Port of PotChannel::LAW_DETUNE from the module's cv_reader.h
  float detune = 2.0f * knob - 1.0f;
  const float sign = detune < 0.0f ? -1.0f : 1.0f;
  detune = detune * sign * 6.0f;
  int integral = (int) detune;
  if (integral > 6) integral = 6;
  float fractional = detune - (float) integral;
  static const float intervals[8] = { 0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f };
  fractional = std::max((fractional - 0.33333333f) * 1.5f, 0.0f);
  const float a = intervals[integral], b = intervals[integral + 1];
  const float t = fractional * fractional * (3.0f - 2.0f * fractional);
  return (a + (b - a) * t) * sign;
}

void BeadsCore::process(const BeadsParams& p, const float* inL, const float* inR,
                        float* outL, float* outR, int n) {
  const int media = 3 - std::max(0, std::min(3, p.quality));
  if (!configured_ || media != media_ || p.mono != mono_ || p.synth != synthMode_) {
    configure(media, p.mono, p.synth);
  }
  float pk = 0.0f;
  for (int i = 0; i < n; ++i) pk = std::max(pk, std::max(std::fabs(inL[i]), std::fabs(inR ? inR[i] : 0.0f)));
  inMeter_ = std::max(pk * p.inGain, inMeter_ * 0.9f);
  bridge_.processEffect(inL, inR, outL, outR, n,
      [this, &p](const float* il, const float* ir, float* l, float* r, int bn) { processNative(p, il, ir, l, r, bn); });
  pk = 0.0f;
  for (int i = 0; i < n; ++i) pk = std::max(pk, std::fabs(outL[i]));
  outMeter_ = std::max(pk, outMeter_ * 0.9f);
}

static inline float attLaw(float a) { return a * std::fabs(a); }

void BeadsCore::processNative(const BeadsParams& p, const float* il, const float* ir,
                              float* l, float* r, int n) {
  beads::FloatFrame in[24], out[24];
  bool freeze[24];
  stmlib::GateFlags seed[24];
  for (int i = 0; i < n; ++i) {
    float a = il[i] * p.inGain, b = ir[i] * p.inGain;
    if (p.mono) { a = b = 0.5f * (a + b); }
    in[i].l = std::max(-1.0f, std::min(1.0f, a));
    in[i].r = std::max(-1.0f, std::min(1.0f, b));
    freeze[i] = p.freeze;
    prevSeed_ = seed[i] = stmlib::ExtractGateFlags(prevSeed_, p.seedGate);
  }
  if (p.seedGate || p.seedButton) seedActivity_ = true;

  beads::Parameters prm;
  prm.time.base = p.time;   prm.time.attenuverter = attLaw(p.timeAmt);   prm.time.has_cv = false;  prm.time.cv = 0.0f;
  prm.size.base = p.size;   prm.size.attenuverter = attLaw(p.sizeAmt);   prm.size.has_cv = false;  prm.size.cv = 0.0f;
  prm.shape.base = p.shape; prm.shape.attenuverter = attLaw(p.shapeAmt); prm.shape.has_cv = false; prm.shape.cv = 0.0f;
  prm.pitch.base = pitchKnobToSemitones(p.pitchKnob);
  prm.pitch.attenuverter = attLaw(p.pitchAmt); prm.pitch.has_cv = false; prm.pitch.cv = 0.0f;
  prm.monophonic_mode = false;

  beads::ModulatedParameter d;
  d.base = p.density;
  d.cv = 0.0f;

  beads::Processor::MixParameters m;
  m.feedback = std::min(1.0f, p.feedback * 1.005f);
  m.dry_wet = std::min(1.0f, p.dryWet * 1.005f);
  m.reverb_amount = std::min(1.0f, p.reverb * 1.005f);
  m.output_gate = false;

  const beads::SeedButtonState sb = p.latched ? beads::SEED_BUTTON_STATE_LOCKED
      : (p.seedButton ? beads::SEED_BUTTON_STATE_ON : beads::SEED_BUTTON_STATE_OFF);

  if (synthMode_) {
    synth_->Render(sb, p.seedPatched ? seed : nullptr, freeze, beads::Synth::Resolution(media_), d, prm, m, out, (size_t) n);
  } else {
    proc_->Process(sb, p.seedPatched ? seed : nullptr, freeze, d, prm, m, in, out, (size_t) n);
    reserve_ = proc_->overwrite_reserve();
  }
  for (int i = 0; i < n; ++i) {
    float a = out[i].l, b = out[i].r;
    if (!std::isfinite(a)) a = 0.0f;
    if (!std::isfinite(b)) b = 0.0f;
    l[i] = std::max(-1.0f, std::min(1.0f, a)) * p.outGain;
    r[i] = std::max(-1.0f, std::min(1.0f, b)) * p.outGain;
  }
}

}  // namespace kb
