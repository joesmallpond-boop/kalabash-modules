// kb.abacus~ — ABACUS granular texture synthesizer for Max for Live (DSP: Beads by Emilie Gillet, MIT)
#include "../common/kb_max.h"
#include "cores/BeadsCore.h"

using namespace kbmax;

struct t_abacus {
  t_pxobject ob;
  kb::BeadsCore* core;
  kb::BeadsParams* p;
  Params* params;
  TransportClock* clock;
  int div;
  double sr;
  std::vector<float>* buf[4];
};

static t_class* s_class = nullptr;

static void* abacus_new(t_symbol*, long, t_atom*) {
  t_abacus* x = (t_abacus*) object_alloc(s_class);
  if (!x) return nullptr;
  dsp_setup((t_pxobject*) x, 2);
  outlet_new(x, "signal");
  outlet_new(x, "signal");
  x->ob.z_misc |= Z_NO_INPLACE;
  x->core = new kb::BeadsCore();
  x->p = new kb::BeadsParams();
  x->p->quality = 0;
  x->clock = new TransportClock();
  x->clock->attach();
  for (auto& b : x->buf) b = new std::vector<float>(4096);
  x->div = 0;
  x->sr = sys_getsr() > 0 ? sys_getsr() : 48000.0;
  x->core->prepare(x->sr);
  auto* P = x->p;
  x->params = new Params();
  Params& m = *x->params;
  auto u = [&m](const char* n, float* dst) { m.add(n, [dst](float v) { *dst = clampf(v, 0, 1); }); };
  auto b = [&m](const char* n, float* dst) { m.add(n, [dst](float v) { *dst = clampf(v, -1, 1); }); };
  auto t = [&m](const char* n, bool* dst) { m.add(n, [dst](float v) { *dst = v > 0.5f; }); };
  u("time", &P->time); u("size", &P->size); u("shape", &P->shape); u("pitch", &P->pitchKnob);
  b("timeAmt", &P->timeAmt); b("sizeAmt", &P->sizeAmt); b("shapeAmt", &P->shapeAmt); b("pitchAmt", &P->pitchAmt);
  u("density", &P->density); u("feedback", &P->feedback); u("dryWet", &P->dryWet); u("reverb", &P->reverb);
  t("mono", &P->mono); t("freeze", &P->freeze); t("seed", &P->seedButton); t("latch", &P->latched); t("synth", &P->synth);
  m.add("quality", [P](float v) { P->quality = (int) std::lround(v); });
  m.add("clock", [x](float v) { x->div = (int) std::lround(v); });
  m.add("inGain", [P](float v) { P->inGain = std::pow(10.0f, clampf(v, -24, 24) / 20.0f); });
  m.add("outGain", [P](float v) { P->outGain = std::pow(10.0f, clampf(v, -24, 24) / 20.0f); });
  return x;
}

static void abacus_free(t_abacus* x) {
  dsp_free((t_pxobject*) x);
  x->clock->detach();
  delete x->core; delete x->p; delete x->params; delete x->clock;
  for (auto& b : x->buf) delete b;
}

static void abacus_anything(t_abacus* x, t_symbol* s, long argc, t_atom* argv) {
  if (argc < 1) return;
  if (!x->params->set(s->s_name, (float) atom_getfloat(argv)))
    object_warn((t_object*) x, "unknown parameter %s", s->s_name);
}

static void abacus_perform64(t_abacus* x, t_object*, double** ins, long, double** outs, long, long n, long, void*) {
  for (auto& b : x->buf) if ((long) b->size() < n) b->resize((size_t) n);
  float* il = x->buf[0]->data(); float* ir = x->buf[1]->data();
  float* ol = x->buf[2]->data(); float* orr = x->buf[3]->data();
  for (long i = 0; i < n; ++i) { il[i] = (float) ins[0][i]; ir[i] = (float) ins[1][i]; }
  x->clock->update(x->sr);
  x->p->seedPatched = x->div > 0;
  const int chunk = 16;
  for (long s = 0; s < n; s += chunk) {
    const int c = (int) std::min<long>(chunk, n - s);
    x->p->seedGate = x->clock->gateAt(x->div, (int) s);
    x->core->process(*x->p, il + s, ir + s, ol + s, orr + s, c);
  }
  for (long i = 0; i < n; ++i) { outs[0][i] = ol[i]; outs[1][i] = orr[i]; }
}

static void abacus_dsp64(t_abacus* x, t_object* dsp64, short*, double sr, long maxvs, long) {
  x->sr = sr;
  x->core->prepare(sr);
  for (auto& b : x->buf) b->resize((size_t) std::max<long>(maxvs, 64));
  object_method(dsp64, gensym("dsp_add64"), x, abacus_perform64, 0, NULL);
}

static void abacus_assist(t_abacus*, void*, long m, long a, char* s) {
  if (m == ASSIST_INLET) snprintf(s, 256, a == 0 ? "(signal) in left, <param> <value>" : "(signal) in right");
  else snprintf(s, 256, a == 0 ? "(signal) out left" : "(signal) out right");
}

extern "C" C74_EXPORT void ext_main(void*) {
  t_class* c = class_new("kb.abacus~", (method) abacus_new, (method) abacus_free, sizeof(t_abacus), 0L, A_GIMME, 0);
  class_addmethod(c, (method) abacus_dsp64, "dsp64", A_CANT, 0);
  class_addmethod(c, (method) abacus_anything, "anything", A_GIMME, 0);
  class_addmethod(c, (method) abacus_assist, "assist", A_CANT, 0);
  class_dspinit(c);
  class_register(CLASS_BOX, c);
  s_class = c;
}
