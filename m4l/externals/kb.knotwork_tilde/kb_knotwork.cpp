// kb.knotwork~ — KNOTWORK macro oscillator for Max for Live (DSP: Plaits by Emilie Gillet, MIT)
#include "../common/kb_max.h"
#include "cores/PlaitsCore.h"

using namespace kbmax;

struct t_knot {
  t_pxobject ob;
  kb::PlaitsCore* core;
  kb::PlaitsParams* p;
  Params* params;
  EventQueue* q;
  MidiParser* midi;
  float coarse, cents, a4;
  bool tuned;
  std::vector<float>* bl;
  std::vector<float>* br;
  void* meterOut;
};

static t_class* s_class = nullptr;

static void knot_retune(t_knot* x) {
  const float c = x->tuned ? std::round(x->coarse) : x->coarse;
  x->p->fine = c + x->cents / 100.0f + 12.0f * std::log2(x->a4 / 440.0f);
}

static void* knot_new(t_symbol*, long, t_atom*) {
  t_knot* x = (t_knot*) object_alloc(s_class);
  if (!x) return nullptr;
  dsp_setup((t_pxobject*) x, 1);
  outlet_new(x, "signal");
  outlet_new(x, "signal");
  x->ob.z_misc |= Z_NO_INPLACE;
  x->core = new kb::PlaitsCore();
  x->p = new kb::PlaitsParams();
  x->q = new EventQueue();
  x->midi = new MidiParser();
  x->bl = new std::vector<float>(4096);
  x->br = new std::vector<float>(4096);
  x->coarse = 0; x->cents = 0; x->a4 = 440; x->tuned = true;
  x->core->prepare(sys_getsr() > 0 ? sys_getsr() : 48000.0);
  auto* P = x->p;
  x->params = new Params();
  Params& m = *x->params;
  m.add("model", [P](float v) { P->engine = (int) std::lround(v); });
  m.add("octave", [P](float v) { P->octave = (float) (std::lround(v) - 4) * 12.0f; });
  m.add("tune", [x](float v) { x->coarse = v; knot_retune(x); });
  m.add("tuned", [x](float v) { x->tuned = v > 0.5f; knot_retune(x); });
  m.add("cents", [x](float v) { x->cents = v; knot_retune(x); });
  m.add("a4", [x](float v) { x->a4 = clampf(v, 400.0f, 480.0f); knot_retune(x); });
  m.add("harmonics", [P](float v) { P->harmonics = clampf(v, 0, 1); });
  m.add("timbre", [P](float v) { P->timbre = clampf(v, 0, 1); });
  m.add("morph", [P](float v) { P->morph = clampf(v, 0, 1); });
  m.add("timbreAmt", [P](float v) { P->timbreAmt = clampf(v, -1, 1); });
  m.add("fmAmt", [P](float v) { P->fmAmt = clampf(v, -1, 1); });
  m.add("morphAmt", [P](float v) { P->morphAmt = clampf(v, -1, 1); });
  m.add("decay", [P](float v) { P->decay = clampf(v, 0, 1); });
  m.add("colour", [P](float v) { P->lpgColour = clampf(v, 0, 1); });
  m.add("amp", [P](float v) { P->ampMode = (int) std::lround(v); });
  m.add("voices", [P](float v) { P->voices = (int) std::lround(v) + 1; });
  m.add("glide", [P](float v) { P->glide = clampf(v, 0, 1); });
  m.add("aux", [P](float v) { P->auxMix = clampf(v, 0, 1); });
  m.add("level", [P](float v) { P->level = clampf(v, 0, 1.5f); });
  m.add("velTimbre", [P](float v) { P->velToTimbre = clampf(v, 0, 1); });
  m.add("bend", [P](float v) { P->bendRange = clampf(v, 0, 24); });
  knot_retune(x);
  return x;
}

static void knot_free(t_knot* x) {
  dsp_free((t_pxobject*) x);
  delete x->core; delete x->p; delete x->params; delete x->q; delete x->midi; delete x->bl; delete x->br;
}

static void knot_anything(t_knot* x, t_symbol* s, long argc, t_atom* argv) {
  if (argc < 1) return;
  if (!x->params->set(s->s_name, (float) atom_getfloat(argv)))
    object_warn((t_object*) x, "unknown parameter %s", s->s_name);
}
static void knot_midi(t_knot* x, long b) { x->midi->byte((int) b, *x->q); }
static void knot_note(t_knot* x, long pitch, long vel) { x->q->push({ vel > 0 ? 0 : 1, (int) pitch, (int) vel }); }
static void knot_panic(t_knot* x) { x->q->push({ 5, 0, 0 }); }

static void knot_handle(t_knot* x, const Event& e) {
  switch (e.type) {
    case 0: x->core->noteOn(e.a, e.b / 127.0f); break;
    case 1: x->core->noteOff(e.a); break;
    case 2: x->p->pitchBend = e.a / 8192.0f; break;
    case 3: if (e.a == 1) x->p->modWheel = e.b / 127.0f; break;
    case 5: x->core->allNotesOff(); break;
    default: break;
  }
}

static void knot_perform64(t_knot* x, t_object*, double**, long, double** outs, long, long n, long, void*) {
  Event e;
  while (x->q->pop(e)) knot_handle(x, e);
  if ((long) x->bl->size() < n) { x->bl->resize((size_t) n); x->br->resize((size_t) n); }
  float* l = x->bl->data();
  float* r = x->br->data();
  x->core->render(*x->p, l, r, (int) n);
  for (long i = 0; i < n; ++i) { outs[0][i] = l[i]; outs[1][i] = r[i]; }
}

static void knot_dsp64(t_knot* x, t_object* dsp64, short*, double sr, long maxvs, long) {
  x->core->prepare(sr);
  x->bl->resize((size_t) std::max<long>(maxvs, 64));
  x->br->resize((size_t) std::max<long>(maxvs, 64));
  object_method(dsp64, gensym("dsp_add64"), x, knot_perform64, 0, NULL);
}

static void knot_assist(t_knot*, void*, long m, long a, char* s) {
  if (m == ASSIST_INLET) snprintf(s, 256, "midi <byte>, note <pitch> <vel>, <param> <value>");
  else snprintf(s, 256, a == 0 ? "(signal) out left" : "(signal) out right / aux");
}

extern "C" C74_EXPORT void ext_main(void*) {
  t_class* c = class_new("kb.knotwork~", (method) knot_new, (method) knot_free, sizeof(t_knot), 0L, A_GIMME, 0);
  class_addmethod(c, (method) knot_dsp64, "dsp64", A_CANT, 0);
  class_addmethod(c, (method) knot_anything, "anything", A_GIMME, 0);
  class_addmethod(c, (method) knot_midi, "midi", A_LONG, 0);
  class_addmethod(c, (method) knot_note, "note", A_LONG, A_LONG, 0);
  class_addmethod(c, (method) knot_panic, "panic", 0);
  class_addmethod(c, (method) knot_assist, "assist", A_CANT, 0);
  class_dspinit(c);
  class_register(CLASS_BOX, c);
  s_class = c;
}
