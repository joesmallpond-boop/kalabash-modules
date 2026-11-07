// kb.fogbank~ — FOGBANK texture synthesizer for Max for Live (DSP: Clouds by Emilie Gillet, MIT)
#include "../common/kb_max.h"
#include "cores/CloudsCore.h"

using namespace kbmax;

struct t_fog {
  t_pxobject ob;
  kb::CloudsCore* core;
  kb::CloudsParams* p;
  Params* params;
  TransportClock* clock;
  float pitch; bool snap; int div; bool manualTrig; bool lastTrig;
  double sr;
  std::vector<float>* buf[4];
};

static t_class* s_class = nullptr;

static void* fog_new(t_symbol*, long, t_atom*) {
  t_fog* x = (t_fog*) object_alloc(s_class);
  if (!x) return nullptr;
  dsp_setup((t_pxobject*) x, 2);
  outlet_new(x, "signal");
  outlet_new(x, "signal");
  x->ob.z_misc |= Z_NO_INPLACE;
  x->core = new kb::CloudsCore();
  x->p = new kb::CloudsParams();
  x->clock = new TransportClock();
  x->clock->attach();
  for (auto& b : x->buf) b = new std::vector<float>(4096);
  x->pitch = 0; x->snap = true; x->div = 0; x->manualTrig = false; x->lastTrig = false;
  x->sr = sys_getsr() > 0 ? sys_getsr() : 48000.0;
  x->core->prepare(x->sr);
  auto* P = x->p;
  x->params = new Params();
  Params& m = *x->params;
  auto u = [&m](const char* n, float* dst) { m.add(n, [dst](float v) { *dst = clampf(v, 0, 1); }); };
  u("position", &P->position); u("size", &P->size); u("density", &P->density); u("texture", &P->texture);
  u("dryWet", &P->dryWet); u("spread", &P->spread); u("feedback", &P->feedback); u("reverb", &P->reverb);
  auto pitch = [x]() { x->p->pitch = x->snap ? std::round(x->pitch) : x->pitch; };
  m.add("pitch", [x, pitch](float v) { x->pitch = clampf(v, -24, 24); pitch(); });
  m.add("pitchSnap", [x, pitch](float v) { x->snap = v > 0.5f; pitch(); });
  m.add("inGain", [P](float v) { P->inGain = std::pow(10.0f, clampf(v, -24, 24) / 20.0f); });
  m.add("outGain", [P](float v) { P->outGain = std::pow(10.0f, clampf(v, -24, 24) / 20.0f); });
  m.add("freeze", [P](float v) { P->freeze = v > 0.5f; });
  m.add("trig", [x](float v) { x->manualTrig = v > 0.5f; });
  m.add("clock", [x](float v) { x->div = (int) std::lround(v); });
  m.add("mode", [P](float v) { P->mode = (int) std::lround(v); });
  m.add("quality", [P](float v) { P->quality = (int) std::lround(v); });
  return x;
}

static void fog_free(t_fog* x) {
  dsp_free((t_pxobject*) x);
  x->clock->detach();
  delete x->core; delete x->p; delete x->params; delete x->clock;
  for (auto& b : x->buf) delete b;
}

static void fog_anything(t_fog* x, t_symbol* s, long argc, t_atom* argv) {
  if (argc < 1) return;
  if (!x->params->set(s->s_name, (float) atom_getfloat(argv)))
    object_warn((t_object*) x, "unknown parameter %s", s->s_name);
}

static void fog_perform64(t_fog* x, t_object*, double** ins, long, double** outs, long, long n, long, void*) {
  for (auto& b : x->buf) if ((long) b->size() < n) b->resize((size_t) n);
  float* il = x->buf[0]->data(); float* ir = x->buf[1]->data();
  float* ol = x->buf[2]->data(); float* orr = x->buf[3]->data();
  for (long i = 0; i < n; ++i) { il[i] = (float) ins[0][i]; ir[i] = (float) ins[1][i]; }
  x->clock->update(x->sr);
  const int chunk = 32;
  for (long s = 0; s < n; s += chunk) {
    const int c = (int) std::min<long>(chunk, n - s);
    x->p->trigger = x->manualTrig || x->clock->gateAt(x->div, (int) s);
    x->core->process(*x->p, il + s, ir + s, ol + s, orr + s, c);
  }
  for (long i = 0; i < n; ++i) { outs[0][i] = ol[i]; outs[1][i] = orr[i]; }
}

static void fog_dsp64(t_fog* x, t_object* dsp64, short*, double sr, long maxvs, long) {
  x->sr = sr;
  x->core->prepare(sr);
  for (auto& b : x->buf) b->resize((size_t) std::max<long>(maxvs, 64));
  object_method(dsp64, gensym("dsp_add64"), x, fog_perform64, 0, NULL);
}

static void fog_assist(t_fog*, void*, long m, long a, char* s) {
  if (m == ASSIST_INLET) snprintf(s, 256, a == 0 ? "(signal) in left, <param> <value>" : "(signal) in right");
  else snprintf(s, 256, a == 0 ? "(signal) out left" : "(signal) out right");
}

extern "C" C74_EXPORT void ext_main(void*) {
  t_class* c = class_new("kb.fogbank~", (method) fog_new, (method) fog_free, sizeof(t_fog), 0L, A_GIMME, 0);
  class_addmethod(c, (method) fog_dsp64, "dsp64", A_CANT, 0);
  class_addmethod(c, (method) fog_anything, "anything", A_GIMME, 0);
  class_addmethod(c, (method) fog_assist, "assist", A_CANT, 0);
  class_dspinit(c);
  class_register(CLASS_BOX, c);
  s_class = c;
}
