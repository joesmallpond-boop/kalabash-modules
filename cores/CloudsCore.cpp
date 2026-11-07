#include <cstdio>
#include "CloudsCore.h"

#include <cmath>
#include "../common/ZeroAlloc.h"
#include <algorithm>
#include "clouds/dsp/granular_processor.h"

namespace kb {

static constexpr int kCloudsBlock = 32;
static constexpr float kCloudsRate = 32000.0f;

CloudsCore::CloudsCore() {
  large_.assign(118784, 0);
  small_.assign(65536 - 128, 0);
  proc_ = newZeroed<clouds::GranularProcessor>();
  proc_->Init(large_.data(), large_.size(), small_.data(), small_.size());
  proc_->set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);
  proc_->set_quality(0);
  proc_->Prepare();
}

CloudsCore::~CloudsCore() { deleteZeroed(proc_); }

void CloudsCore::prepare(double hostRate) {
  bridge_.setup(hostRate, kCloudsRate, kCloudsBlock, true);
}

void CloudsCore::process(const CloudsParams& p, const float* inL, const float* inR,
                         float* outL, float* outR, int n) {
  if (p.trigger && !prevTrig_) pendingTrig_ = true;
  prevTrig_ = p.trigger;
  float pk = 0.0f;
  for (int i = 0; i < n; ++i) pk = std::max(pk, std::max(std::fabs(inL[i]), std::fabs(inR ? inR[i] : 0.0f)));
  inMeter_ = std::max(pk * p.inGain, inMeter_ * 0.9f);
  bridge_.processEffect(inL, inR, outL, outR, n,
      [this, &p](const float* il, const float* ir, float* l, float* r, int bn) { processNative(p, il, ir, l, r, bn); });
  pk = 0.0f;
  for (int i = 0; i < n; ++i) pk = std::max(pk, std::fabs(outL[i]));
  outMeter_ = std::max(pk, outMeter_ * 0.9f);
}

static inline short toShort(float x) {
  x *= 32767.0f;
  if (x > 32767.0f) x = 32767.0f;
  if (x < -32768.0f) x = -32768.0f;
  return (short) x;
}

void CloudsCore::processNative(const CloudsParams& p, const float* il, const float* ir,
                               float* l, float* r, int n) {
  proc_->set_playback_mode(clouds::PlaybackMode(std::max(0, std::min(3, p.mode))));
  proc_->set_quality(std::max(0, std::min(3, p.quality)));
  // The module calls Prepare() continuously from its main loop; the WSOLA
  // correlator needs several passes per audio block to keep up.
  const int passes = p.mode == 1 ? 2 : 1;
  for (int k = 0; k < passes; ++k) proc_->Prepare();

  clouds::Parameters* q = proc_->mutable_parameters();
  q->position = p.position;
  q->size = p.size;
  q->pitch = std::max(-48.0f, std::min(48.0f, p.pitch));
  q->density = p.density;
  q->texture = p.texture;
  float dw = p.dryWet * 1.05f - 0.025f;
  q->dry_wet = std::max(0.0f, std::min(1.0f, dw));
  q->stereo_spread = p.spread;
  q->feedback = p.feedback;
  q->reverb = p.reverb;
  q->freeze = p.freeze;
  q->trigger = pendingTrig_;
  q->gate = p.trigger;
  pendingTrig_ = false;

  clouds::ShortFrame in[kCloudsBlock], out[kCloudsBlock];
  for (int i = 0; i < n; ++i) {
    in[i].l = toShort(il[i] * p.inGain);
    in[i].r = toShort(ir[i] * p.inGain);
  }
  proc_->Process(in, out, (size_t) n);
  for (int i = 0; i < n; ++i) {
    l[i] = out[i].l / 32768.0f * p.outGain;
    r[i] = out[i].r / 32768.0f * p.outGain;
  }
}

}  // namespace kb
