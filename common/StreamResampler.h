// kalabash modules — streaming band-limited resampler.
// Push one frame at a time at the input rate; output frames at the output
// rate are emitted through a callback as soon as they can be computed.
// Kaiser-windowed sinc, table-driven, arbitrary (fixed per setup) ratio.
#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

namespace kb {

class StreamResampler {
 public:
  static constexpr int kMaxChannels = 2;

  void setup(double inRate, double outRate, int channels, int baseHalfTaps = 16) {
    channels_ = std::min(channels, kMaxChannels);
    step_ = inRate / outRate;              // input samples per output sample
    const double ratio = outRate / inRate; // < 1 when downsampling
    const double cutoffScale = std::min(1.0, ratio);
    fc_ = 0.5 * cutoffScale * 0.91;       // cycles / input sample
    halfTaps_ = (int) std::ceil(baseHalfTaps / cutoffScale);
    buildTable();
    histLen_ = 2 * halfTaps_ + 4;
    for (int c = 0; c < channels_; ++c) hist_[c].assign((size_t) histLen_ * 2, 0.0f);
    writePos_ = 0;
    inCount_ = 0;
    nextOut_ = 0.0;
    passthrough_ = std::fabs(step_ - 1.0) < 1e-9;
  }

  void reset() {
    for (int c = 0; c < channels_; ++c) std::fill(hist_[c].begin(), hist_[c].end(), 0.0f);
    writePos_ = 0; inCount_ = 0; nextOut_ = 0.0;
  }

  // Delay in output samples introduced by the filter.
  double latencyOut() const { return passthrough_ ? 0.0 : (double) halfTaps_ / step_; }

  template <typename Emit>
  inline void push(const float* frame, Emit&& emit) {
    if (passthrough_) { emit(frame); return; }
    for (int c = 0; c < channels_; ++c) {
      hist_[c][(size_t) writePos_] = frame[c];
      hist_[c][(size_t) (writePos_ + histLen_)] = frame[c];
    }
    writePos_ = (writePos_ + 1) % histLen_;
    ++inCount_;
    // newest input index = inCount_-1. An output at time t needs inputs up to floor(t)+halfTaps_.
    float out[kMaxChannels];
    while (nextOut_ + halfTaps_ <= (double) (inCount_ - 1)) {
      const double t = nextOut_;
      const long base = (long) std::floor(t);
      const double frac = t - (double) base;
      // index in ring of input sample (base - halfTaps_ + 1)
      const long newest = inCount_ - 1;
      const long first = base - halfTaps_ + 1;
      int ringStart = (int) (((writePos_ - 1 - (newest - first)) % histLen_ + histLen_) % histLen_);
      for (int c = 0; c < channels_; ++c) out[c] = 0.0f;
      const int n = 2 * halfTaps_;
      for (int k = 0; k < n; ++k) {
        // offset x = t - (first + k) = frac + halfTaps_ - 1 - k
        const double x = frac + (double) (halfTaps_ - 1 - k);
        const float h = kernel(x);
        const int idx = ringStart + k;
        for (int c = 0; c < channels_; ++c) out[c] += h * hist_[c][(size_t) idx];
      }
      emit(out);
      nextOut_ += step_;
    }
    // keep numbers small
    if (inCount_ > (1L << 30)) {
      const long shift = 1L << 29;
      inCount_ -= shift;
      nextOut_ -= (double) shift;
    }
  }

 private:
  void buildTable() {
    const int P = kPhases;
    tableHalf_ = halfTaps_ * P;
    table_.assign((size_t) tableHalf_ + 2, 0.0f);
    const double beta = 8.6;
    const double i0b = besselI0(beta);
    for (int i = 0; i <= tableHalf_ + 1; ++i) {
      const double x = (double) i / P;
      double s;
      if (x == 0.0) s = 2.0 * fc_;
      else s = std::sin(2.0 * M_PI * fc_ * x) / (M_PI * x);
      const double r = x / halfTaps_;
      double w = 0.0;
      if (r < 1.0) w = besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
      table_[(size_t) i] = (float) (s * w);
    }
  }

  inline float kernel(double x) const {
    x = std::fabs(x) * kPhases;
    const int i = (int) x;
    if (i >= tableHalf_) return 0.0f;
    const float f = (float) (x - i);
    return table_[(size_t) i] + f * (table_[(size_t) i + 1] - table_[(size_t) i]);
  }

  static double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k) {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum += term;
    }
    return sum;
  }

  static constexpr int kPhases = 512;
  int channels_ = 2;
  double step_ = 1.0;
  double fc_ = 0.45;
  int halfTaps_ = 16;
  int tableHalf_ = 0;
  std::vector<float> table_;
  std::vector<float> hist_[kMaxChannels];
  int histLen_ = 0;
  int writePos_ = 0;
  long inCount_ = 0;
  double nextOut_ = 0.0;
  bool passthrough_ = false;
};

// Simple FIFO of stereo frames.
class FrameFifo {
 public:
  void setCapacity(int frames) { buf_.assign((size_t) frames * 2, 0.0f); cap_ = frames; clear(); }
  void clear() { r_ = w_ = size_ = 0; }
  int size() const { return size_; }
  void push(float l, float r) {
    if (size_ >= cap_) return; // drop on overflow
    buf_[(size_t) w_ * 2] = l; buf_[(size_t) w_ * 2 + 1] = r;
    w_ = (w_ + 1) % cap_; ++size_;
  }
  bool pop(float& l, float& r) {
    if (size_ == 0) { l = r = 0.0f; return false; }
    l = buf_[(size_t) r_ * 2]; r = buf_[(size_t) r_ * 2 + 1];
    r_ = (r_ + 1) % cap_; --size_;
    return true;
  }
 private:
  std::vector<float> buf_;
  int cap_ = 0, r_ = 0, w_ = 0, size_ = 0;
};

// Runs a fixed-rate, fixed-block stereo processor inside a host at any rate.
// Effect mode: host in -> native -> process -> host out (constant latency).
// Generator mode: pull native blocks until enough host-rate output exists.
class RateBridge {
 public:
  // block: native block size. process(inL, inR, outL, outR, n) at native rate.
  void setup(double hostRate, double nativeRate, int block, bool effect) {
    hostRate_ = hostRate; nativeRate_ = nativeRate; block_ = block; effect_ = effect;
    down_.setup(hostRate, nativeRate, 2);
    up_.setup(nativeRate, hostRate, 2);
    nativeIn_.setCapacity(block * 8 + 64);
    hostOut_.setCapacity(8192 + (int) std::ceil(block * hostRate / nativeRate) * 8);
    inL_.assign((size_t) block, 0.0f); inR_.assign((size_t) block, 0.0f);
    outL_.assign((size_t) block, 0.0f); outR_.assign((size_t) block, 0.0f);
    prime_ = 0;
    if (effect_) {
      prime_ = (int) std::ceil(block * hostRate / nativeRate) + 2
             + (int) std::ceil(down_.latencyOut() * hostRate / nativeRate + up_.latencyOut()) + 2;
      for (int i = 0; i < prime_; ++i) hostOut_.push(0.0f, 0.0f);
    }
  }

  int latencySamples() const { return prime_; }

  void reset() {
    down_.reset(); up_.reset(); nativeIn_.clear(); hostOut_.clear();
    for (int i = 0; i < prime_; ++i) hostOut_.push(0.0f, 0.0f);
  }

  template <typename Proc>
  void processEffect(const float* inL, const float* inR, float* outL, float* outR, int n, Proc&& proc) {
    for (int i = 0; i < n; ++i) {
      float fr[2] = { inL[i], inR ? inR[i] : inL[i] };
      down_.push(fr, [this](const float* o) { nativeIn_.push(o[0], o[1]); });
      while (nativeIn_.size() >= block_) runBlock(proc);
      float l, r;
      hostOut_.pop(l, r);
      outL[i] = l; if (outR) outR[i] = r;
    }
  }

  template <typename Proc>
  void processGenerator(float* outL, float* outR, int n, Proc&& proc) {
    for (int i = 0; i < n; ++i) {
      while (hostOut_.size() < 1) runBlock(proc);
      float l, r;
      hostOut_.pop(l, r);
      outL[i] = l; if (outR) outR[i] = r;
    }
  }

 private:
  template <typename Proc>
  void runBlock(Proc&& proc) {
    for (int k = 0; k < block_; ++k) {
      float l = 0.0f, r = 0.0f;
      if (effect_) nativeIn_.pop(l, r);
      inL_[(size_t) k] = l; inR_[(size_t) k] = r;
    }
    proc(inL_.data(), inR_.data(), outL_.data(), outR_.data(), block_);
    for (int k = 0; k < block_; ++k) {
      float fr[2] = { outL_[(size_t) k], outR_[(size_t) k] };
      up_.push(fr, [this](const float* o) { hostOut_.push(o[0], o[1]); });
    }
  }

  double hostRate_ = 48000, nativeRate_ = 48000;
  int block_ = 16;
  bool effect_ = true;
  int prime_ = 0;
  StreamResampler down_, up_;
  FrameFifo nativeIn_, hostOut_;
  std::vector<float> inL_, inR_, outL_, outR_;
};

}  // namespace kb
