// kalabash modules — shared helpers for the Max externals that run the DSP cores
// inside Max for Live devices.
#pragma once

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_itm.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kbmax {

// Single-producer (scheduler/UI thread) / single-consumer (audio thread) event queue,
// so note events never touch the DSP while it is rendering.
struct Event { int type; int a; int b; };  // type: 0 note on, 1 note off, 2 pitch bend (a = -8192..8191), 3 CC (a=cc,b=val), 4 pressure (a), 5 all off
class EventQueue {
 public:
  bool push(const Event& e) {
    const int w = w_.load(std::memory_order_relaxed);
    const int n = (w + 1) & (kSize - 1);
    if (n == r_.load(std::memory_order_acquire)) return false;
    buf_[w] = e;
    w_.store(n, std::memory_order_release);
    return true;
  }
  bool pop(Event& e) {
    const int r = r_.load(std::memory_order_relaxed);
    if (r == w_.load(std::memory_order_acquire)) return false;
    e = buf_[r];
    r_.store((r + 1) & (kSize - 1), std::memory_order_release);
    return true;
  }
 private:
  static constexpr int kSize = 1024;
  Event buf_[kSize];
  std::atomic<int> r_ { 0 }, w_ { 0 };
};

// Raw MIDI bytes ([midiin] -> "midi <byte>") -> events.
class MidiParser {
 public:
  void byte(int b, EventQueue& q) {
    if (b >= 0xf8) return;
    if (b & 0x80) { status_ = b; n_ = 0; return; }
    if (!status_) return;
    data_[n_++] = b;
    const int hi = status_ & 0xf0;
    const int need = (hi == 0xc0 || hi == 0xd0) ? 1 : 2;
    if (n_ < need) return;
    n_ = 0;  // running status
    switch (hi) {
      case 0x90: q.push({ data_[1] ? 0 : 1, data_[0], data_[1] }); break;
      case 0x80: q.push({ 1, data_[0], 0 }); break;
      case 0xe0: q.push({ 2, (data_[0] | (data_[1] << 7)) - 8192, 0 }); break;
      case 0xb0:
        if (data_[0] == 123 || data_[0] == 120) q.push({ 5, 0, 0 });
        else q.push({ 3, data_[0], data_[1] });
        break;
      case 0xd0: q.push({ 4, data_[0], 0 }); break;
      case 0xa0: q.push({ 4, data_[1], 0 }); break;
      default: break;
    }
  }
 private:
  int status_ = 0, n_ = 0, data_[2] = { 0, 0 };
};

// Named parameters: messages like "timbre 0.4" from the device's dials.
class Params {
 public:
  void add(const char* name, std::function<void(float)> f) { map_[name] = std::move(f); }
  bool set(const char* name, float v) {
    auto it = map_.find(name);
    if (it == map_.end()) return false;
    it->second(v);
    return true;
  }
 private:
  std::unordered_map<std::string, std::function<void(float)>> map_;
};

// Live transport via Max's global ITM (follows Live's transport in Max for Live).
struct TransportClock {
  t_itm* itm = nullptr;
  bool playing = false;
  double ticks = 0.0, ticksPerSample = 0.0;
  void attach() {
    itm = (t_itm*) itm_getglobal();
    if (itm) itm_reference(itm);
  }
  void detach() { if (itm) itm_dereference(itm); itm = nullptr; }
  void update(double sr) {
    playing = false;
    if (!itm) return;
    playing = itm_getstate(itm) != 0;
    ticks = itm_getticks(itm);
    const double msPerBeat = itm_tickstoms(itm, 480.0);
    ticksPerSample = msPerBeat > 0.0 ? 480.0 / (msPerBeat * 0.001 * sr) : 0.0;
  }
  // division index: 0 off, then 1/1,1/2,1/4,1/8,1/16,1/32,1/4T,1/8T,1/16T
  bool gateAt(int idx, int sample) const {
    static const double beats[] = { 0, 4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    if (!playing || idx <= 0 || idx > 9) return false;
    const double t = (ticks + sample * ticksPerSample) / 480.0 / beats[idx];
    return (t - std::floor(t)) < 0.25;
  }
};

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

}  // namespace kbmax
