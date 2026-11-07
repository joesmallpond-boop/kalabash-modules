// kb.alloy~ — ALLOY modal synthesis voice for Max for Live (DSP: Elements by Emilie Gillet, MIT)
#include "../common/kb_max.h"
#include "cores/ElementsCore.h"

using namespace kbmax;

struct t_alloy {
  t_pxobject ob;
  kb::ElementsCore* core;
  kb::ElementsParams* p;
  Params* params;
  EventQueue* q;
  MidiParser* midi;
  float coarse, cents, a4;
  bool tuned;
  std::vector<float>* bl;
  std::vector<float>* br;
};

static t_class* s_class = nullptr;

static void alloy_retune(t_alloy* x) {
  x->p->coarse = x->tuned ? std::round(x->coarse) : x->coarse;
  x->p->fine = x->cents / 100.0f + 12.0f * std::log2(x->a4 / 440.0f);
  x->p->tuned = x->tuned;
}

static void* alloy_new(t_symbol*, long, t_atom*) {
  t_alloy* x = (t_alloy*) object_alloc(s_class);
  if (!x) return nullptr;
  dsp_setup((t_pxobject*) x, 1);
  outlet_new(x, "signal");
  outlet_new(x, "signal");
  x->ob.z_misc |= Z_NO_INPLACE;
  x->core = new kb::ElementsCore();
  x->p = new kb::ElementsParams();
  x->q = new EventQueue();
  x->midi = new MidiParser();
  x->bl = new std::vector<float>(4096);
  x->br = new std::vector<float>(4096);
  x->coarse = 0; x->cents = 0; x->a4 = 440; x->tuned = true;
  x->core->prepare(sys_getsr() > 0 ? sys_getsr() : 48000.0);
  auto* P = x->p;
  x->params = new Params();
  Params& m = *x->params;
  auto u = [&m](const char* n, float* dst) { m.add(n, [dst](float v) { *dst = clampf(v, 0, 1); }); };
  auto b = [&m](const char* n, float* dst) { m.add(n, [dst](float v) { *dst = clampf(v, -1, 1); }); };
  u("contour", &P->contour); u("bow", &P->bowLevel); u("blow", &P->blowLevel); u("strike", &P->strikeLevel);
  u("flow", &P->flow); u("mallet", &P->mallet);
  u("bowTimbre", &P->bowTimbre); u("blowTimbre", &P->blowTimbre); u("strikeTimbre", &P->strikeTimbre);
  u("geometry", &P->geometry); u("brightness", &P->brightness); u("damping", &P->damping); u("position", &P->position);
  m.add("space", [P](float v) { P->space = clampf(v, 0, 2); });
  b("fm", &P->fmAmt);
  b("mBowTimbre", &P->modBowTimbre); b("mFlow", &P->modFlow); b("mBlowTimbre", &P->modBlowTimbre);
  b("mMallet", &P->modMallet); b("mStrikeTimbre", &P->modStrikeTimbre); b("mGeometry", &P->modGeometry);
  b("mBrightness", &P->modBrightness); b("mDamping", &P->modDamping); b("mPosition", &P->modPosition); b("mSpace", &P->modSpace);
  u("lfoRate", &P->lfoRate); u("velStrength", &P->velToStrength);
  m.add("modSrc", [P](float v) { P->modSource = (int) std::lround(v); });
  m.add("model", [P](float v) { P->model = (int) std::lround(v); });
  m.add("ominous", [P](float v) { P->ominous = v > 0.5f; });
  m.add("play", [P](float v) { P->play = v > 0.5f; });
  m.add("level", [P](float v) { P->level = clampf(v, 0, 1.5f); });
  m.add("bend", [P](float v) { P->bendRange = clampf(v, 0, 24); });
  m.add("coarse", [x](float v) { x->coarse = v; alloy_retune(x); });
  m.add("fine", [x](float v) { x->cents = v; alloy_retune(x); });
  m.add("tuned", [x](float v) { x->tuned = v > 0.5f; alloy_retune(x); });
  m.add("a4", [x](float v) { x->a4 = clampf(v, 400.0f, 480.0f); alloy_retune(x); });
  alloy_retune(x);
  return x;
}

static void alloy_free(t_alloy* x) {
  dsp_free((t_pxobject*) x);
  delete x->core; delete x->p; delete x->params; delete x->q; delete x->midi; delete x->bl; delete x->br;
}

static void alloy_anything(t_alloy* x, t_symbol* s, long argc, t_atom* argv) {
  if (argc < 1) return;
  if (!x->params->set(s->s_name, (float) atom_getfloat(argv)))
    object_warn((t_object*) x, "unknown parameter %s", s->s_name);
}
static void alloy_midi(t_alloy* x, long b) { x->midi->byte((int) b, *x->q); }
static void alloy_note(t_alloy* x, long pitch, long vel) { x->q->push({ vel > 0 ? 0 : 1, (int) pitch, (int) vel }); }
static void alloy_panic(t_alloy* x) { x->q->push({ 5, 0, 0 }); }

static void alloy_handle(t_alloy* x, const Event& e) {
  switch (e.type) {
    case 0: x->core->noteOn(e.a, e.b / 127.0f); break;
    case 1: x->core->noteOff(e.a); break;
    case 2: x->p->pitchBend = e.a / 8192.0f; break;
    case 3: if (e.a == 1) x->p->modWheel = e.b / 127.0f; break;
    case 4: x->p->aftertouch = e.a / 127.0f; break;
    case 5: x->core->allNotesOff(); break;
    default: break;
  }
}

static void alloy_perform64(t_alloy* x, t_object*, double**, long, double** outs, long, long n, long, void*) {
  Event e;
  while (x->q->pop(e)) alloy_handle(x, e);
  if ((long) x->bl->size() < n) { x->bl->resize((size_t) n); x->br->resize((size_t) n); }
  float* l = x->bl->data();
  float* r = x->br->data();
  x->core->render(*x->p, nullptr, nullptr, l, r, (int) n);
  for (long i = 0; i < n; ++i) { outs[0][i] = l[i]; outs[1][i] = r[i]; }
}

static void alloy_dsp64(t_alloy* x, t_object* dsp64, short*, double sr, long maxvs, long) {
  x->core->prepare(sr);
  x->bl->resize((size_t) std::max<long>(maxvs, 64));
  x->br->resize((size_t) std::max<long>(maxvs, 64));
  object_method(dsp64, gensym("dsp_add64"), x, alloy_perform64, 0, NULL);
}

static void alloy_assist(t_alloy*, void*, long m, long a, char* s) {
  if (m == ASSIST_INLET) snprintf(s, 256, "midi <byte>, note <pitch> <vel>, <param> <value>");
  else snprintf(s, 256, a == 0 ? "(signal) out left" : "(signal) out right");
}

extern "C" C74_EXPORT void ext_main(void*) {
  t_class* c = class_new("kb.alloy~", (method) alloy_new, (method) alloy_free, sizeof(t_alloy), 0L, A_GIMME, 0);
  class_addmethod(c, (method) alloy_dsp64, "dsp64", A_CANT, 0);
  class_addmethod(c, (method) alloy_anything, "anything", A_GIMME, 0);
  class_addmethod(c, (method) alloy_midi, "midi", A_LONG, 0);
  class_addmethod(c, (method) alloy_note, "note", A_LONG, A_LONG, 0);
  class_addmethod(c, (method) alloy_panic, "panic", 0);
  class_addmethod(c, (method) alloy_assist, "assist", A_CANT, 0);
  class_dspinit(c);
  class_register(CLASS_BOX, c);
  s_class = c;
}
