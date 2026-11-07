// DICEBAG — random sampler / sequencer / modulator for Max for Live
// Port of the Marbles random-generation algorithms by Emilie Gillet (MIT licence),
// adapted to run at control rate inside Live. kalabash.
//
// Inlet 0: parameter messages ("rate 0.5", ...), "ms <cpuclock>", "pos bars beats units", MIDI bytes ("midi b")
// Outlet 0: raw MIDI bytes -> midiout
// Outlet 1: modulation values "mod x1 x2 x3 y" (0..1)
// Outlet 2: display data for the view
autowatch = 1;
inlets = 1;
outlets = 3;

// @@TABLES@@

// ---------------------------------------------------------------- utilities
var MAXU32 = 4294967296.0;
function clamp(x, a, b) { return x < a ? a : (x > b ? b : x); }
function semis(x) { return Math.pow(2, x / 12); }
function interp(table, x, size) {
  x *= size;
  var i = Math.floor(x);
  if (i < 0) { i = 0; x = 0; }
  if (i >= table.length - 1) return table[table.length - 1];
  var f = x - i;
  return table[i] + (table[i + 1] - table[i]) * f;
}
function rnd() { return Math.random(); }
// Math.imul is not available in Max's JavaScript engine
function imul32(a, b) {
  var ah = (a >>> 16) & 0xffff, al = a & 0xffff, bh = (b >>> 16) & 0xffff, bl = b & 0xffff;
  return ((al * bl) + (((ah * bl + al * bh) << 16) >>> 0)) >>> 0;
}
function lcg(w) { return (imul32(w >>> 0, 1664525) + 1013904223) >>> 0; }

function betaSample(uniform, spread, bias) {
  var flip = bias > 0.5;
  if (flip) { uniform = 1 - uniform; bias = 1 - bias; }
  bias *= (5 - 1) * 2;
  spread *= (9 - 1);
  var bi = Math.floor(bias), bf = bias - bi;
  var si = Math.floor(spread), sf = spread - si;
  var cell = bi * 10 + si;
  var offset = 0;
  if (uniform <= 0.05) { offset = 129; uniform *= 20; }
  else if (uniform >= 0.95) { offset = 258; uniform = (uniform - 0.95) * 20; }
  function look(c) {
    var t = DIST_DATA[DIST_TABLES[Math.min(c, DIST_TABLES.length - 1)]];
    var x = uniform * 128, i = Math.floor(x);
    if (i > 127) i = 127;
    var f = x - i;
    return t[offset + i] + (t[offset + i + 1] - t[offset + i]) * f;
  }
  var x1y1 = look(cell), x2y1 = look(cell + 1), x1y2 = look(cell + 10), x2y2 = look(cell + 11);
  var y1 = x1y1 + (x2y1 - x1y1) * sf;
  var y2 = x1y2 + (x2y2 - x1y2) * sf;
  var y = y1 + (y2 - y1) * bf;
  return flip ? 1 - y : y;
}
function fastBeta(u) { return interp(DIST_ICDF_4_3, u, 128); }

// ---------------------------------------------------------------- RandomSequence (deja vu)
var DV = 16, HIST = 16;
function RandomSequence() {
  this.loop = []; this.history = [];
  for (var i = 0; i < DV; ++i) this.loop.push(rnd());
  for (i = 0; i < HIST; ++i) this.history.push(0);
  this.writeHead = 0; this.length = 8; this.step = 0;
  this.recordHead = 0; this.replayHead = -1; this.replayStart = 0;
  this.dejaVu = 0; this.replayHash = 0; this.replayShift = 0;
  this.redoRead = 0; this.redoWrite = -1; this.redoWriteHist = -1;
}
RandomSequence.prototype.clone = function (s) {
  this.loop = s.loop.slice(0); this.history = s.history.slice(0);
  this.writeHead = s.writeHead; this.length = s.length; this.step = s.step;
  this.recordHead = s.recordHead; this.replayHead = s.replayHead; this.replayStart = s.replayStart;
  this.replayHash = s.replayHash; this.replayShift = s.replayShift; this.dejaVu = s.dejaVu;
  this.redoRead = s.redoRead; this.redoWrite = s.redoWrite; this.redoWriteHist = s.redoWriteHist;
};
RandomSequence.prototype.record = function () { this.replayStart = this.recordHead; this.replayHead = -1; };
RandomSequence.prototype.replayPseudoRandom = function (h) { this.replayHead = this.replayStart; this.replayHash = h; this.replayShift = 0; };
RandomSequence.prototype.replayShifted = function (s) { this.replayHead = this.replayStart; this.replayHash = 0; this.replayShift = s; };
RandomSequence.prototype.getReplayValue = function () {
  var h = (this.replayHead - 1 - this.replayShift + 2 * HIST) % HIST;
  if (!this.replayHash) return this.history[h];
  var word = Math.floor(this.history[h] * MAXU32) >>> 0;
  word = (imul32((word ^ this.replayHash) >>> 0, 1664525) + 1013904223) >>> 0;
  return word / MAXU32;
};
RandomSequence.prototype.rewriteValue = function (value) {
  if (this.replayHead >= 0) return this.getReplayValue();
  if (this.redoWrite >= 0) this.loop[this.redoWrite] = 1 + value;
  var r = this.loop[this.redoRead];
  if (r >= 1) r -= 1; else r = 0.5;
  if (this.redoWriteHist >= 0) this.history[this.redoWriteHist] = r;
  return r;
};
RandomSequence.prototype.nextValue = function (deterministic, value) {
  if (this.replayHead >= 0) {
    this.replayHead = (this.replayHead + 1) % HIST;
    return this.getReplayValue();
  }
  var ps = 2 * this.dejaVu - 1, p = ps * ps;
  var mutate = rnd() < p;
  if (mutate && this.dejaVu <= 0.5) {
    this.redoWrite = this.writeHead;
    this.loop[this.writeHead] = deterministic ? 1 + value : rnd();
    this.writeHead = (this.writeHead + 1) % DV;
    this.step = this.length - 1;
  } else {
    this.redoWrite = -1;
    if (mutate) this.step = Math.floor(rnd() * this.length);
    else { this.step = this.step + 1; if (this.step >= this.length) this.step = 0; }
  }
  var i = (this.writeHead + DV - this.length + this.step) % DV;
  this.redoRead = i;
  var r = this.loop[i];
  if (r >= 1) r -= 1; else if (deterministic) r = 0.5;
  this.redoWriteHist = this.recordHead;
  this.history[this.recordHead] = r;
  this.recordHead = (this.recordHead + 1) % HIST;
  return r;
};
RandomSequence.prototype.nextVector = function (n) {
  var seed = this.nextValue(false, 0);
  var word = Math.floor(seed * MAXU32) >>> 0;
  var out = [];
  for (var i = 0; i < n; ++i) { out.push(word / MAXU32); word = lcg(word); }
  return out;
};
RandomSequence.prototype.setLength = function (l) {
  if (l < 1 || l > DV) return;
  this.length = l; this.step = this.step % l;
};
RandomSequence.prototype.reset = function () { this.step = this.length - 1; };

// ---------------------------------------------------------------- SlaveRamp
var MAX_RAMP = 0.9999;
function SlaveRamp() { this.init(); }
SlaveRamp.prototype.init = function () {
  this.phase = 0; this.maxPhase = MAX_RAMP; this.ratio = 1; this.pw = 0; this.target = 1;
  this.pulseMs = 0; this.bernoulli = false; this.mustComplete = false;
};
SlaveRamp.prototype.reset = function () { this.init(); this.phase = 1; };
SlaveRamp.prototype.initPattern = function (len, ratio, pw) {
  this.bernoulli = false; this.phase = 0; this.maxPhase = len * MAX_RAMP;
  this.ratio = ratio[0] / ratio[1]; this.pw = pw; this.target = 1; this.pulseMs = 0;
};
SlaveRamp.prototype.initBernoulli = function (mustComplete, pw, expected) {
  this.bernoulli = true;
  if (this.mustComplete) { this.phase = 0; this.pw = pw; this.ratio = 1; this.pulseMs = 0; }
  this.ratio = mustComplete ? 1 - this.phase : (1 - this.phase) * expected;
  this.mustComplete = mustComplete;
};
SlaveRamp.prototype.process = function (freq, dtMs) {
  var out;
  if (this.bernoulli) {
    this.phase += freq * this.ratio;
    out = this.phase >= 1 ? 1 : this.phase;
  } else {
    this.phase += freq;
    if (this.phase >= this.maxPhase) this.phase = this.maxPhase;
    out = this.phase * this.ratio;
    if (out > this.target) { this.pulseMs = 0; this.target += 1; }
    out -= Math.floor(out);
  }
  this.out = out;
  this.gate = this.pw === 0 ? (this.pulseMs < 1.0 && out <= 0.5) : out < this.pw;
  this.pulseMs += dtMs;
};

// ---------------------------------------------------------------- T generator
var DIVIDER_PATTERNS = [
  [[1,1],[1,1],1],[[1,1],[2,1],1],[[1,2],[1,1],2],[[1,1],[4,1],1],[[1,2],[2,1],2],[[1,1],[3,2],2],
  [[1,4],[4,1],4],[[1,4],[2,1],4],[[1,2],[3,2],2],[[1,1],[8,1],1],[[1,1],[3,1],1],[[1,3],[1,1],3],
  [[1,1],[5,4],4],[[1,2],[5,4],4],[[1,1],[6,1],1],[[1,3],[2,1],3],[[1,1],[16,1],1]];
var FIXED_DIVIDER_PATTERNS = [
  [[8,1],[1,8],8],[[6,1],[1,6],6],[[4,1],[1,4],4],[[3,1],[1,3],3],[[2,1],[1,2],2],[[3,2],[2,3],6],
  [[4,3],[3,4],12],[[5,4],[4,5],20],[[1,1],[1,1],1],[[4,5],[5,4],20],[[3,4],[4,3],12],[[2,2],[3,2],6],
  [[1,2],[2,1],2],[[1,3],[3,1],3],[[1,4],[4,1],4],[[1,6],[6,1],6],[[1,8],[8,1],8]];
var DRUM_PATTERNS = [
  [1,0,0,0,2,0,0,0],[0,0,1,0,2,0,0,0],[1,0,1,0,2,0,0,0],[0,0,1,0,2,0,0,2],[1,0,1,0,2,0,1,0],
  [0,2,1,0,2,0,0,2],[1,0,0,0,2,0,1,0],[0,2,1,0,2,0,1,2],[1,0,0,1,2,0,0,0],[0,2,1,1,2,0,1,2],
  [1,0,0,1,2,0,1,0],[0,2,1,1,2,2,1,2],[1,0,0,1,2,0,1,2],[0,2,0,1,2,0,1,2],[1,0,1,1,2,0,1,2],
  [2,0,1,2,0,1,2,0],[1,2,1,1,2,0,1,2],[2,0,1,2,0,1,2,2]];
var T_MODELS = ["coin", "ratio", "drums", "indep", "divider", "three", "markov"];

function TGenerator() {
  this.model = 0; this.range = 1; this.rate = 0; this.bias = 0.5; this.jitter = 0;
  this.pwMean = 0.5; this.pwStd = 0;
  this.masterPhase = 0; this.jitterMul = 1; this.phaseDiff = 0;
  this.dividerLen = 0; this.streak = [0, 0]; this.markov = []; for (var i = 0; i < 16; ++i) this.markov.push(0);
  this.markovPtr = 0; this.drumStep = 0; this.drumIndex = 0;
  this.seq = new RandomSequence();
  this.slave = [new SlaveRamp(), new SlaveRamp()];
}
TGenerator.prototype.pulseWidth = function (u) {
  if (this.pwStd === 0) return 0.05 + 0.9 * this.pwMean;
  return 0.05 + 0.9 * betaSample(u, this.pwStd, this.pwMean);
};
TGenerator.prototype.schedule = function (v, mask) {
  for (var i = 0; i < 2; ++i) { this.slave[i].initBernoulli(!!(mask & 1), this.pulseWidth(v[i]), 0.5); mask >>= 1; }
};
TGenerator.prototype.configure = function (v) {
  // v = [pw0, pw1, u0, u1, p, jitter]
  var u = [v[2], v[3]], b = this.bias, mask = 0, i;
  switch (this.model) {
    case 0: for (i = 0; i < 2; ++i) if ((u[i >> 1] > b) !== !!(i & 1)) mask |= 1 << i; this.schedule(v, mask); break;
    case 3: for (i = 0; i < 2; ++i) if ((u[i] > b) !== !!(i & 1)) mask |= 1 << i; this.schedule(v, mask); break;
    case 5: {
      var pNone = 0.75 - Math.abs(b - 0.5), th = pNone + (1 - pNone) * (0.25 + b * 0.5);
      for (i = 0; i < 2; ++i) { var uu = u[i >> 1]; if (uu > pNone && ((uu > th) !== !!(i & 1))) mask |= 1 << i; }
      this.schedule(v, mask); break;
    }
    case 2: {
      ++this.drumStep;
      if (this.drumStep >= 8) {
        this.drumStep = 0;
        var du = u[0] * 2 * Math.abs(b - 0.5);
        this.drumIndex = Math.floor(18 * du);
        if (this.drumIndex > 17) this.drumIndex = 17;
        if (b <= 0.5) this.drumIndex -= this.drumIndex % 2;
      }
      this.schedule(v, DRUM_PATTERNS[this.drumIndex][this.drumStep]); break;
    }
    case 6: {
      var bb = 1.5 * b - 0.5, pp = this.markovPtr;
      this.markov[pp] = 0;
      for (i = 0; i < 2; ++i) {
        var m = 1 << i;
        var periodic = this.markov[(pp + 8) % 16] & m, simultaneous = this.markov[(pp + 8) % 16] & ~m;
        var dense = this.markov[(pp + 1) % 16] & m, alternate = this.markov[(pp + 4) % 16] & ~m;
        var logit = -1.5;
        logit += this.streak[i] > 24 ? 10 : 0;
        logit += 8 * Math.abs(bb) * (periodic ? bb : -bb);
        logit -= 2 * (simultaneous ? bb : -bb);
        logit -= 1 * (dense ? bb : 0);
        logit += 1 * (alternate ? bb : 0);
        logit = clamp(logit, -10, 10);
        var prob = LUT_LOGIT[Math.floor(logit * 12.8 + 128)];
        var st = u[i] < prob;
        if (this.seq.dejaVu >= v[4]) st = !!(this.markov[(pp + this.seq.length) % 16] & m);
        if (st) { mask |= m; this.streak[i] = 0; } else ++this.streak[i];
      }
      this.markov[pp] |= mask;
      this.markovPtr = (pp + 15) % 16;
      this.schedule(v, mask); break;
    }
    case 1: case 4: {
      --this.dividerLen;
      if (this.dividerLen <= 0) {
        var pat;
        if (this.model === 4) pat = FIXED_DIVIDER_PATTERNS[clamp(Math.floor(b * 17), 0, 16)];
        else {
          var strength = Math.abs(b - 0.5) * 2, x = u[0];
          x *= (x + strength * strength * (1 - x)); x *= strength;
          pat = DIVIDER_PATTERNS[clamp(Math.floor(x * 17), 0, 16)];
          if (b < 0.5) pat = [pat[1], pat[0], pat[2]];
        }
        for (i = 0; i < 2; ++i) this.slave[i].initPattern(pat[2], pat[i], this.pulseWidth(v[i]));
        this.dividerLen = pat[2];
      }
      break;
    }
  }
};
TGenerator.prototype.reset = function () {
  this.slave[0].reset(); this.slave[1].reset();
  this.seq.reset(); this.dividerLen = 0; this.drumStep = 8;
  if (this.model !== 4) this.configure(this.seq.nextVector(6));
};
// frequency: master phase increment for this update (cycles)
TGenerator.prototype.process = function (frequency, dtMs) {
  var jf = frequency * this.jitterMul;
  this.masterPhase += jf;
  this.phaseDiff += frequency - jf;
  this.wrapped = false;
  if (this.masterPhase > 1) {
    this.masterPhase -= Math.floor(this.masterPhase);
    var v = this.seq.nextVector(6);
    var amt = Math.pow(this.jitter, 4) * 36;
    var x = fastBeta(v[5]);
    var mul = semis((x * 2 - 1) * amt);
    mul *= this.phaseDiff > 0 ? 1 + this.phaseDiff : 1 / (1 - this.phaseDiff);
    this.jitterMul = mul;
    this.configure(v);
    this.wrapped = true;
  }
  this.slave[0].process(frequency * this.jitterMul, dtMs);
  this.slave[1].process(frequency * this.jitterMul, dtMs);
};

// ---------------------------------------------------------------- Quantizer / Lag / OutputChannel
function HystQ() { this.q = 0; }
HystQ.prototype.process = function (value, n) {
  value *= (n - 1);
  var fb = value > this.q ? -0.25 : 0.25;
  var q = Math.floor(value + fb + 0.5);
  this.q = clamp(q, 0, n - 1);
  return this.q;
};
function Quantizer(scale) {
  var n = scale.deg.length;
  this.n = n; this.base = scale.base; this.voltage = [];
  var second = 0, i, t;
  for (i = 0; i < n; ++i) {
    this.voltage.push(scale.deg[i][0]);
    var w = scale.deg[i][1];
    if (w !== 255 && w >= second) second = w;
  }
  var th = [0, 16, 32, 64, 128, 192, 255];
  if (second > 192) th[5] = second;
  this.levels = [];
  for (t = 0; t < 7; ++t) {
    var mask = [], first = -1, last = 0;
    for (i = 0; i < n; ++i) {
      var on = scale.deg[i][1] >= th[t];
      mask.push(on);
      if (on) { if (first < 0) first = i; last = i; }
    }
    this.levels.push({ mask: mask, first: first < 0 ? 0 : first, last: last });
  }
  this.hq = new HystQ();
}
Quantizer.prototype.process = function (value, amount) {
  var level = this.hq.process(amount, 8);
  if (level <= 0) return value;
  level -= 1;
  var note = value / this.base;
  var ni = Math.floor(note);
  var nf = (note - ni) * this.base;
  var L = this.levels[level];
  var a = this.voltage[L.last] - this.base, b = this.voltage[L.first] + this.base;
  for (var i = 0; i < this.n; ++i) {
    if (L.mask[i]) {
      var v = this.voltage[i];
      if (nf > v) a = v; else { b = v; break; }
    }
  }
  var q = nf < (a + b) * 0.5 ? a : b;
  return q + ni * this.base;
};
function Lag() { this.rampStart = 0; this.rampValue = 0; this.lp = 0; this.prevPhase = 0; }
Lag.prototype.resetRamp = function () { this.rampStart = this.rampValue; };
Lag.prototype.process = function (value, smooth, phase) {
  var f = phase - this.prevPhase;
  if (f < 0) f += 1;
  this.prevPhase = phase;
  f *= 0.25; f *= semis(84 * (1 - smooth));
  if (f >= 1) f = 1;
  if (smooth <= 0.05) f += 20 * (0.05 - smooth) * (1 - f);
  if (f > 1) f = 1;
  this.lp += (value - this.lp) * f;
  var ia = clamp((smooth - 0.6) * 5, 0, 1);
  var il = clamp((1 - smooth) * 5, 0, 1);
  var warped = interp(LUT_RAISED_COSINE, phase, 256);
  var ip = warped + (phase - warped) * il;
  var it = this.rampStart + (value - this.rampStart) * ip;
  this.rampValue = it;
  return this.lp + (it - this.lp) * ia;
};
function OutputChannel() {
  this.spread = 0.5; this.bias = 0.5; this.steps = 0.5; this.scaleIndex = 0;
  this.registerMode = false; this.registerValue = 0; this.registerTransposition = 0;
  this.prevPhase = 0; this.reacq = 0; this.voltage = 0; this.qvoltage = 0;
  this.scale = 10; this.offset = -5; this.lag = new Lag(); this.quantizers = [];
  this.out = 0; this.newValue = false;
}
OutputChannel.prototype.generate = function (seq) {
  var u = seq.nextValue(this.registerMode, this.registerValue);
  if (this.registerMode) return 10 * (u - 0.5) + this.registerTransposition;
  var deg = clamp(1.25 - this.spread * 25, 0, 1);
  var ber = clamp(this.spread * 25 - 23.75, 0, 1);
  var value = betaSample(u, this.spread, this.bias);
  var bv = u >= (1 - this.bias) ? 0.999999 : 0;
  value += deg * (this.bias - value);
  value += ber * (bv - value);
  return value * this.scale + this.offset;
};
OutputChannel.prototype.quantize = function (v, amount) { return this.quantizers[this.scaleIndex].process(v, amount); };
OutputChannel.prototype.process = function (seq, phase) {
  this.newValue = false;
  if (this.reacq) {
    --this.reacq;
    var u = seq.rewriteValue(this.registerValue);
    this.voltage = 10 * (u - 0.5) + this.registerTransposition;
    this.qvoltage = this.quantize(this.voltage, 2 * this.steps - 1);
  }
  if (phase < this.prevPhase) {
    this.voltage = this.generate(seq);
    this.lag.resetRamp();
    this.qvoltage = this.quantize(this.voltage, 2 * this.steps - 1);
    if (this.registerMode) this.reacq = 2;
    this.newValue = true;
  }
  if (this.steps >= 0.5) this.out = this.qvoltage;
  else this.out = this.lag.process(this.voltage, 1 - 2 * this.steps, phase);
  this.prevPhase = phase;
};

function RampDivider() { this.phase = 0; this.train = 0; this.maxTrain = 1; this.fr = 0.99999; this.counter = 1; this.resetNext = false; }
RampDivider.prototype.process = function (ratio, p) {
  var f = p - this.phase;
  if (f < 0) {
    if (this.resetNext) { this.resetNext = false; this.counter = 1; }
    f += 1; --this.counter;
    if (!this.counter) { this.train = p; this.counter = ratio[1]; this.fr = ratio[0] / ratio[1] * MAX_RAMP; f = 0; this.maxTrain = ratio[1]; }
  }
  this.train += f;
  if (this.train >= this.maxTrain) this.train = this.maxTrain;
  var o = this.train * this.fr;
  this.phase = p;
  return o - Math.floor(o);
};

// ---------------------------------------------------------------- engine state
var P = {
  clock: 0,          // 0 free, 1 sync to Live
  rate: 0.5, division: 3, trange: 1, jitter: 0, tbias: 0.5, tmodel: 0,
  gatelen: 0.5, gaternd: 0,
  dejavu: 0.5, length: 0.5, tdv: 0, xdv: 0,      // 0 off, 1 on, 2 locked
  spread: 0.5, xbias: 0.5, steps: 0.5, xrange: 0, xmode: 0, scale: 0, root: 48, ext: 0, xclock: 0,
  ydiv: 0.5, yspread: 0.5, ybias: 0.5, ysteps: 0,
  lane1: 1, lane2: 0, lane3: 1, vel: 100, yvel: 0, thru: 1, notes: 1, channel: 1
};
var DIVISIONS = [4, 2, 1, 0.5, 0.25, 0.125, 2 / 3, 1 / 3, 1 / 6, 1.5, 0.75]; // quarter notes per master pulse
var LOOP_LENGTH = [1, 2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,4,4,4,4, 5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6, 7,7, 8,8,8,8,8,8,8,8,8, 10,10,10, 12,12,12,12,12,12,12, 14,14, 16];
var Y_DIV = [[1,64],[1,48],[1,32],[1,24],[1,16],[1,12],[1,8],[1,6],[1,4],[1,3],[1,2],[1,1]];
var HASHES = [0, 0xbeca55e5, 0xf0cacc1a];

var T = new TGenerator();
var seqs = [new RandomSequence(), new RandomSequence(), new RandomSequence(), new RandomSequence()];
var chans = [new OutputChannel(), new OutputChannel(), new OutputChannel(), new OutputChannel()];
var useShifted = [false, false, false, false];
var ydiv = new RampDivider();
(function initScales() {
  for (var c = 0; c < 4; ++c)
    for (var s = 0; s < 6; ++s) chans[c].quantizers.push(new Quantizer(PRESET_SCALES[s]));
})();

var lastMs = -1, songPos = 0, lastPosMs = -1, qpms = 120 / 60000, playing = false, lastPlaying = false;
var gatePrev = [false, false, false], heldNote = [-1, -1, -1];
var extNote = -1, lastExt = 0.5;
var modAccum = 0, viewAccum = 0;
var view = { t: [0, 0, 0], x: [0, 0, 0, 0] };

function lengthValue() { return LOOP_LENGTH[clamp(Math.floor(P.length * LOOP_LENGTH.length), 0, LOOP_LENGTH.length - 1)]; }
function dejaVuKnob() {
  var d = P.dejavu;
  if (d < 0.47) d *= 1.06382978723; else if (d > 0.53) d = 0.5 + (d - 0.53) * 1.06382978723; else d = 0.5;
  return d;
}
function dvFor(state) { return state === 2 ? 0.5 : (state === 1 ? dejaVuKnob() : 0); }

// ---------------------------------------------------------------- MIDI out
function sendNote(on, note, vel) {
  var ch = clamp(P.channel, 1, 16) - 1;
  outlet(0, (on ? 0x90 : 0x80) | ch);
  outlet(0, note);
  outlet(0, on ? vel : 0);
}
function allOff() {
  for (var i = 0; i < 3; ++i) if (heldNote[i] >= 0) { sendNote(false, heldNote[i], 0); heldNote[i] = -1; }
}
function voltsToNote(v) { return clamp(Math.round(P.root + v * 12), 0, 127); }
function normVolt(v, rangeIdx) {
  if (rangeIdx === 0) return clamp(v / 2, 0, 1);
  if (rangeIdx === 1) return clamp(v / 5, 0, 1);
  return clamp((v + 5) / 10, 0, 1);
}

// ---------------------------------------------------------------- main update
function update(dtMs) {
  // ---- T section settings
  T.model = P.tmodel; T.bias = P.tbias; T.jitter = P.jitter;
  T.pwMean = P.gatelen; T.pwStd = P.gaternd;
  T.seq.dejaVu = dvFor(P.tdv); T.seq.setLength(lengthValue());
  var rangeMul = P.trange === 0 ? 0.25 : (P.trange === 2 ? 4 : 1);
  var freq;
  if (P.clock === 1) {
    if (!playing) return;
    var qPerPulse = DIVISIONS[clamp(P.division, 0, DIVISIONS.length - 1)] / rangeMul;
    freq = dtMs * qpms / qPerPulse;
  } else {
    var rateSemis = (P.rate * 120 - 60);
    freq = dtMs / 1000 * 2 * rangeMul * semis(rateSemis);
  }
  if (freq > 0.45) freq = 0.45;
  T.process(freq, dtMs);
  var g1 = T.slave[0].gate, g2 = T.masterPhase < 0.5, g3 = T.slave[1].gate;
  var master = T.masterPhase, s1 = T.slave[0].out, s3 = T.slave[1].out;

  // ---- X/Y section
  var dvx = dvFor(P.xdv), len = lengthValue();
  var regMode = P.ext ? true : false;
  if (regMode && extNote >= 0) lastExt = clamp(((extNote - P.root) / 12 + 5) / 10, 0, 1);
  var ramps;
  if (P.xclock === 1) ramps = [s1, s1, s1];
  else if (P.xclock === 2) ramps = [master, master, master];
  else if (P.xclock === 3) ramps = [s3, s3, s3];
  else ramps = [s1, master, s3];
  var yr = ydiv.process(Y_DIV[clamp(Math.floor(P.ydiv * 12), 0, 11)], ramps[1]);
  ramps.push(yr);
  for (var i = 0; i < 4; ++i) {
    var ch = chans[i], isY = i === 3;
    var mode = isY ? 0 : P.xmode, rng = isY ? 2 : P.xrange;
    if (rng === 0) { ch.scale = 2; ch.offset = 0; } else if (rng === 1) { ch.scale = 5; ch.offset = 0; } else { ch.scale = 10; ch.offset = -5; }
    var amount = 1;
    if (mode === 1) amount = i === 1 ? 1 : -1;
    else if (mode === 2) amount = i - 1;
    var sp = isY ? P.yspread : P.spread, bi = isY ? P.ybias : P.xbias, stp = isY ? P.ysteps : P.steps;
    var reg = isY ? false : regMode;
    ch.spread = 0.5 + (sp - 0.5) * amount;
    ch.bias = 0.5 + (bi - 0.5) * amount;
    ch.steps = 0.5 + (stp - 0.5) * (reg ? 1 : amount);
    ch.scaleIndex = clamp(P.scale, 0, 5);
    ch.registerMode = reg; ch.registerValue = lastExt;
    ch.registerTransposition = 4 * sp * (bi - 0.5) * amount;
    var sq = seqs[i];
    sq.record(); sq.setLength(isY ? 1 : len); sq.dejaVu = isY ? 0 : dvx;
    var shifted = false;
    if (P.xclock !== 0 && i > 0 && i < 3) {
      sq = seqs[0];
      if (reg) { shifted = true; sq.replayShifted(mode === 0 ? i : (mode === 1 ? (i === 2 ? 1 : 0) : 0)); }
      else sq.replayPseudoRandom(HASHES[i]);
    }
    if (!shifted && useShifted[i]) seqs[i].clone(seqs[0]);
    useShifted[i] = shifted;
    ch.process(sq, ramps[i]);
  }

  // ---- notes: lane k = gate t(k) + pitch X(k)
  var gates = [g1, g2, g3], lanes = [P.lane1, P.lane2, P.lane3];
  for (var k = 0; k < 3; ++k) {
    var g = gates[k];
    if (g && !gatePrev[k]) {
      view.t[k] = 1;
      if (P.notes && lanes[k]) {
        if (heldNote[k] >= 0) sendNote(false, heldNote[k], 0);
        var n = voltsToNote(chans[k].out);
        var vel = P.yvel ? clamp(Math.round(20 + 107 * normVolt(chans[3].out, 2)), 1, 127) : clamp(P.vel, 1, 127);
        sendNote(true, n, vel);
        heldNote[k] = n;
      }
    } else if (!g && gatePrev[k]) {
      if (heldNote[k] >= 0) { sendNote(false, heldNote[k], 0); heldNote[k] = -1; }
    }
    gatePrev[k] = g;
  }

  // ---- modulation + view (throttled)
  modAccum += dtMs; viewAccum += dtMs;
  if (modAccum >= 5) {
    modAccum = 0;
    outlet(1, "mod", normVolt(chans[0].out, P.xrange), normVolt(chans[1].out, P.xrange),
           normVolt(chans[2].out, P.xrange), normVolt(chans[3].out, 2));
  }
  if (viewAccum >= 33) {
    viewAccum = 0;
    outlet(2, "frame", view.t[0], g2 ? 1 : 0, view.t[2],
           normVolt(chans[0].out, P.xrange), normVolt(chans[1].out, P.xrange),
           normVolt(chans[2].out, P.xrange), normVolt(chans[3].out, 2),
           T.seq.dejaVu, dvx, len);
    view.t[0] = g1 ? 1 : 0; view.t[2] = g3 ? 1 : 0;
  }
}

// ---------------------------------------------------------------- message handlers
function ms(t) {
  if (lastMs < 0) { lastMs = t; return; }
  var dt = t - lastMs;
  lastMs = t;
  if (dt <= 0) return;
  if (dt > 100) dt = 100;
  // transport stopped? (no position for a while)
  if (P.clock === 1 && playing && lastPosMs >= 0 && t - lastPosMs > 120) {
    playing = false;
    allOff();
  }
  if (P.clock === 1 && playing) songPos += dt * qpms;
  var steps = Math.ceil(dt / 1.0);
  var sub = dt / steps;
  for (var i = 0; i < steps; ++i) update(sub);
}
ms.immediate = 1;

// "pos bars beats units" from [transport], once per 128th note while Live plays
function pos(bars, beats, units) {
  var q = (bars - 1) * 4 + (beats - 1) + units / 480;
  var now = lastMs;
  if (!playing) {
    playing = true;
    songPos = q;
    // re-align patterns at transport start, like a reset pulse
    T.reset(); ydiv.resetNext = true;
    for (var i = 0; i < 4; ++i) seqs[i].reset();
    T.masterPhase = 0.9999;
  } else if (lastPosMs >= 0 && now > lastPosMs) {
    var est = (q - lastQ) / (now - lastPosMs);
    if (est > 0 && est < 0.05) qpms += (est - qpms) * 0.2;
    // phase-lock: pull our running position toward the transport
    songPos += (q - songPos) * 0.5;
  }
  lastQ = q;
  lastPosMs = now;
}
pos.immediate = 1;
var lastQ = 0;

// raw MIDI input bytes
var midiState = [];
function midi(b) {
  if (b >= 0xf8) return;            // realtime
  if (b & 0x80) midiState = [b]; else if (midiState.length) midiState.push(b);
  if (midiState.length === 3 || (midiState.length === 2 && ((midiState[0] & 0xf0) === 0xc0 || (midiState[0] & 0xf0) === 0xd0))) {
    var st = midiState[0] & 0xf0;
    var isNoteOn = st === 0x90 && midiState[2] > 0;
    if (isNoteOn) extNote = midiState[1];
    if (P.thru) for (var i = 0; i < midiState.length; ++i) outlet(0, midiState[i]);
    midiState = [midiState[0]];   // running status
  }
}
midi.immediate = 1;

function anything() {
  var name = messagename, v = arguments[0];
  if (P.hasOwnProperty(name)) {
    var intParams = { clock: 1, division: 1, trange: 1, tmodel: 1, tdv: 1, xdv: 1, xrange: 1, xmode: 1, scale: 1, ext: 1,
                      xclock: 1, lane1: 1, lane2: 1, lane3: 1, yvel: 1, thru: 1, notes: 1, channel: 1, vel: 1, root: 1 };
    P[name] = intParams[name] ? Math.round(v) : v;
    if (name === "clock") { playing = false; allOff(); }
    if (name === "notes" && !P.notes) allOff();
  }
}
anything.immediate = 1;

function panic() { allOff(); }
function reset() { T.reset(); for (var i = 0; i < 4; ++i) seqs[i].reset(); }
function loadbang() { }

// exported for offline testing in node
if (typeof module !== "undefined") {
  module.exports = { P: P, ms: ms, pos: pos, midi: midi, T: T, chans: chans, anything: anything, imul32: imul32,
                     betaSample: betaSample, Quantizer: Quantizer };
}
