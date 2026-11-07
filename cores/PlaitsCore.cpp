#include <cstdio>
#include "PlaitsCore.h"

#include <cmath>
#include "../common/ZeroAlloc.h"
#include <algorithm>
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

namespace kb {

static constexpr int kPlaitsBlock = 12;
static constexpr size_t kVoiceMem = 16384;

PlaitsCore::PlaitsCore() {
  for (auto& s : slots_) {
    s.voice = newZeroed<plaits::Voice>();
    s.mem.assign(kVoiceMem, 0);
    stmlib::BufferAllocator allocator(s.mem.data(), kVoiceMem);
    s.voice->Init(&allocator);
  }
}

PlaitsCore::~PlaitsCore() {
  for (auto& s : slots_) deleteZeroed(s.voice);
}

void PlaitsCore::prepare(double hostRate) {
  // Run at the module's real (slightly off-48k) rate so tuning is exact.
  bridge_.setup(hostRate, plaits::kCorrectedSampleRate, kPlaitsBlock, false);
}

int PlaitsCore::allocate(int note) {
  const int nv = current_ ? std::max(1, std::min(kMaxVoices, current_->voices)) : 4;
  // same note already sounding -> reuse
  for (int i = 0; i < nv; ++i) if (slots_[(size_t) i].note == note) return i;
  // free voice (oldest released)
  int best = -1; uint32_t bestAge = UINT32_MAX;
  for (int i = 0; i < nv; ++i) {
    const Slot& s = slots_[(size_t) i];
    if (!s.gate && s.age < bestAge) { best = i; bestAge = s.age; }
  }
  if (best >= 0) return best;
  // steal oldest
  bestAge = UINT32_MAX;
  for (int i = 0; i < nv; ++i) {
    if (slots_[(size_t) i].age < bestAge) { best = i; bestAge = slots_[(size_t) i].age; }
  }
  return best < 0 ? 0 : best;
}

void PlaitsCore::noteOn(int note, float velocity) {
  const int nv = current_ ? current_->voices : 4;
  if (nv <= 1) {
    heldStack_.erase(std::remove(heldStack_.begin(), heldStack_.end(), note), heldStack_.end());
    heldStack_.push_back(note);
    Slot& s = slots_[0];
    s.note = note;
    s.targetNote = (float) note;
    if (!s.everPlayed) s.currentNote = (float) note;
    s.velocity = velocity;
    s.retrigBlocks = s.gate ? 1 : 0;
    s.gate = true;
    s.silentBlocks = 0;
    s.age = ++clock_;
    s.everPlayed = true;
    return;
  }
  const int i = allocate(note);
  Slot& s = slots_[(size_t) i];
  const bool wasOn = s.gate;
  s.note = note;
  s.targetNote = (float) note;
  if (!s.everPlayed || (current_ && current_->glide <= 0.0f)) s.currentNote = (float) note;
  s.velocity = velocity;
  s.retrigBlocks = wasOn ? 1 : 0;
  s.gate = true;
  s.silentBlocks = 0;
  s.age = ++clock_;
  s.everPlayed = true;
}

void PlaitsCore::noteOff(int note) {
  const int nv = current_ ? current_->voices : 4;
  if (nv <= 1) {
    heldStack_.erase(std::remove(heldStack_.begin(), heldStack_.end(), note), heldStack_.end());
    Slot& s = slots_[0];
    if (s.note == note) {
      if (!heldStack_.empty()) {
        s.note = heldStack_.back();
        s.targetNote = (float) s.note;   // legato return, no retrigger
      } else {
        s.gate = false;
      }
    }
    return;
  }
  for (auto& s : slots_) if (s.note == note && s.gate) { s.gate = false; }
}

void PlaitsCore::allNotesOff() {
  heldStack_.clear();
  for (auto& s : slots_) { s.gate = false; }
}

void PlaitsCore::render(const PlaitsParams& p, float* outL, float* outR, int n) {
  current_ = &p;
  bridge_.processGenerator(outL, outR, n,
      [this, &p](const float*, const float*, float* l, float* r, int bn) { renderNative(p, l, r, bn); });
  float peak = 0.0f;
  for (int i = 0; i < n; ++i) peak = std::max(peak, std::fabs(outL[i]));
  meter_ = std::max(peak, meter_ * 0.9f);
}

void PlaitsCore::renderNative(const PlaitsParams& p, float* l, float* r, int n) {
  std::fill(l, l + n, 0.0f);
  std::fill(r, r + n, 0.0f);
  const int plaitsEngine = (std::max(0, std::min(23, p.engine)) + 8) % 24;
  const int nv = std::max(1, std::min(kMaxVoices, p.voices));
  const float glideCoef = p.glide <= 0.0f ? 1.0f
      : 1.0f - std::exp(-1.0f / (0.5f + p.glide * p.glide * 400.0f)); // per block
  const float bend = p.pitchBend * p.bendRange;
  const float auxMix = std::max(0.0f, std::min(1.0f, p.auxMix));

  plaits::Voice::Frame frames[kPlaitsBlock];

  for (int i = 0; i < nv; ++i) {
    Slot& s = slots_[(size_t) i];
    const bool drone = p.ampMode == 2;
    if (!s.everPlayed && !(drone && i == 0)) continue;
    if (drone && i > 0) continue;
    if (!drone && !s.gate && s.silentBlocks > 4000) continue;  // idle voice

    s.currentNote += (s.targetNote - s.currentNote) * glideCoef;

    plaits::Patch patch;
    patch.note = 60.0f + p.octave + p.fine;
    patch.harmonics = p.harmonics;
    patch.timbre = std::min(1.0f, p.timbre + p.velToTimbre * s.velocity * 0.5f);
    patch.morph = std::min(1.0f, p.morph + p.modWheel * 0.5f);
    patch.frequency_modulation_amount = p.fmAmt;
    patch.timbre_modulation_amount = p.timbreAmt;
    patch.morph_modulation_amount = p.morphAmt;
    patch.engine = plaitsEngine;
    patch.decay = p.decay;
    patch.lpg_colour = p.lpgColour;

    plaits::Modulations m = {};
    m.engine = 0.0f;
    m.note = (s.everPlayed ? s.currentNote : 60.0f) - 60.0f + bend;
    m.frequency = 0.0f;
    m.harmonics = 0.0f;
    m.timbre = 0.0f;
    m.morph = 0.0f;
    m.frequency_patched = false;
    m.timbre_patched = false;
    m.morph_patched = false;

    float gain = 1.0f;
    if (drone) {
      m.trigger_patched = false;
      m.level_patched = false;
      m.trigger = 0.0f;
      m.level = 0.0f;
    } else {
      m.trigger_patched = true;
      bool trig = s.gate;
      if (s.retrigBlocks > 0) { trig = false; --s.retrigBlocks; }
      m.trigger = trig ? 1.0f : 0.0f;
      if (p.ampMode == 1) {
        m.level_patched = true;
        m.level = s.gate ? (0.35f + 0.65f * s.velocity) : 0.0f;
      } else {
        m.level_patched = false;
        m.level = 0.0f;
        gain = 0.25f + 0.75f * s.velocity;
      }
    }

    float vpeak = 0.0f;
    for (int done = 0; done < n; done += kPlaitsBlock) {
      const int bn = std::min(kPlaitsBlock, n - done);
      s.voice->Render(patch, m, frames, (size_t) bn);
      for (int k = 0; k < bn; ++k) {
        const float o = -frames[k].out / 32768.0f * gain;
        const float a = -frames[k].aux / 32768.0f * gain;
        vpeak = std::max(vpeak, std::max(std::fabs(o), std::fabs(a)));
        l[done + k] += o;
        r[done + k] += o + (a - o) * auxMix;
      }
    }
    if (s.gate || vpeak > 2e-4f) s.silentBlocks = 0; else ++s.silentBlocks;
    if (i == 0 || s.gate) activeEngine_ = s.voice->active_engine();
  }

  const float lvl = p.level * (nv > 1 ? 0.6f : 1.0f);
  for (int k = 0; k < n; ++k) {
    l[k] = std::tanh(l[k] * lvl);
    r[k] = std::tanh(r[k] * lvl);
  }
}

}  // namespace kb
