#include "PanelUI.h"

namespace kb {

using namespace juce;

static Font kbFont(float h, bool bold = false) {
  return Font(FontOptions(Font::getDefaultSansSerifFontName(), h, bold ? Font::bold : Font::plain));
}

KbLook::KbLook(const Theme& t) : theme(t) {
  setColour(PopupMenu::backgroundColourId, t.panel.darker(0.6f));
  setColour(PopupMenu::textColourId, t.ink);
  setColour(PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha(0.7f));
  setColour(ComboBox::textColourId, t.knob.contrasting(0.85f));
  setColour(TooltipWindow::backgroundColourId, t.panel.darker(0.7f));
  setColour(TooltipWindow::textColourId, t.ink);
}

void KbLook::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos,
                              float start, float end, Slider& s) {
  const auto bounds = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(2.0f);
  const float r = jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
  const auto c = bounds.getCentre();
  const bool bipolar = s.getProperties()["bipolar"];
  const float ang = start + pos * (end - start);

  // value track
  const float tr = r * 0.97f;
  Path track;
  track.addCentredArc(c.x, c.y, tr, tr, 0.0f, start, end, true);
  g.setColour(theme.ink.withAlpha(0.18f));
  g.strokePath(track, PathStrokeType(jmax(1.5f, r * 0.09f), PathStrokeType::curved, PathStrokeType::rounded));
  Path val;
  const float mid = (start + end) * 0.5f;
  if (bipolar) val.addCentredArc(c.x, c.y, tr, tr, 0.0f, jmin(mid, ang), jmax(mid, ang), true);
  else val.addCentredArc(c.x, c.y, tr, tr, 0.0f, start, ang, true);
  g.setColour(theme.accent);
  g.strokePath(val, PathStrokeType(jmax(1.5f, r * 0.09f), PathStrokeType::curved, PathStrokeType::rounded));

  // cap with knurled skirt
  const float cr = r * 0.78f;
  g.setColour(Colours::black.withAlpha(0.35f));
  g.fillEllipse(c.x - cr + r * 0.06f, c.y - cr + r * 0.1f, cr * 2, cr * 2);
  ColourGradient grad(theme.knob.brighter(0.35f), c.x - cr, c.y - cr, theme.knob.darker(0.5f), c.x + cr, c.y + cr, false);
  g.setGradientFill(grad);
  g.fillEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2);
  const int teeth = r > 30 ? 36 : 24;
  g.setColour(theme.knob.darker(0.7f).withAlpha(0.5f));
  for (int i = 0; i < teeth; ++i) {
    const float a = MathConstants<float>::twoPi * i / teeth;
    g.drawLine(c.x + std::sin(a) * cr * 0.86f, c.y - std::cos(a) * cr * 0.86f,
               c.x + std::sin(a) * cr, c.y - std::cos(a) * cr, 1.0f);
  }
  const float ir = cr * 0.72f;
  ColourGradient top(theme.knob.brighter(0.15f), c.x, c.y - ir, theme.knob.darker(0.2f), c.x, c.y + ir, false);
  g.setGradientFill(top);
  g.fillEllipse(c.x - ir, c.y - ir, ir * 2, ir * 2);
  // pointer
  Path p;
  const float pw = jmax(1.6f, r * 0.07f);
  p.addRoundedRectangle(-pw * 0.5f, -cr * 0.95f, pw, cr * 0.55f, pw * 0.5f);
  g.setColour(theme.ink.brighter(0.4f));
  g.fillPath(p, AffineTransform::rotation(ang).translated(c.x, c.y));
}

void KbLook::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool hi, bool down) {
  auto r = b.getLocalBounds().toFloat().reduced(1.5f);
  const bool on = b.getToggleState() || down;
  const float cr = jmin(r.getHeight(), r.getWidth()) * 0.22f;
  g.setColour(Colours::black.withAlpha(0.35f));
  g.fillRoundedRectangle(r.translated(0, 1.5f), cr);
  ColourGradient grad(theme.knob.brighter(hi ? 0.4f : 0.25f), r.getX(), r.getY(), theme.knob.darker(0.4f), r.getX(), r.getBottom(), false);
  g.setGradientFill(grad);
  g.fillRoundedRectangle(r, cr);
  if (on) {
    g.setColour(theme.accent.withAlpha(0.85f));
    g.drawRoundedRectangle(r.reduced(1.0f), cr, 1.6f);
  }
}

void KbLook::drawButtonText(Graphics& g, TextButton& b, bool, bool down) {
  const bool on = b.getToggleState() || down;
  g.setColour(on ? theme.accent.brighter(0.5f) : theme.knob.contrasting(0.85f));
  g.setFont(kbFont(b.getHeight() * 0.42f, true));
  g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(2), Justification::centred, 2);
}

void KbLook::drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) {
  auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(1.0f);
  g.setColour(theme.knob.darker(0.3f));
  g.fillRoundedRectangle(r, 3.0f);
  g.setColour(theme.ink.withAlpha(0.35f));
  g.drawRoundedRectangle(r, 3.0f, 1.0f);
  Path arrow;
  const float ax = (float) w - 10.0f, ay = h * 0.5f;
  arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
  g.setColour(theme.accent);
  g.fillPath(arrow);
}

Font KbLook::getComboBoxFont(ComboBox& b) { return kbFont(b.getHeight() * 0.58f, true); }
Font KbLook::getPopupMenuFont() { return kbFont(15.0f); }
Font KbLook::getLabelFont(Label&) { return kbFont(12.0f); }
void KbLook::positionComboBoxText(ComboBox& b, Label& l) {
  l.setBounds(4, 0, b.getWidth() - 18, b.getHeight());
  l.setFont(getComboBoxFont(b));
  l.setJustificationType(Justification::centredLeft);
}

//------------------------------------------------------------------------------

class MomentaryButton : public TextButton {
 public:
  MomentaryButton(RangedAudioParameter* p) : param(p) {}
  void buttonStateChanged() override {
    const bool down = isDown();
    if (down != last) {
      last = down;
      if (param) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(down ? 1.0f : 0.0f);
        param->endChangeGesture();
      }
    }
  }
  RangedAudioParameter* param;
  bool last = false;
};

PanelEditor::PanelEditor(AudioProcessor& p, AudioProcessorValueTreeState& st, const Theme& theme,
                         String title, String subtitle, int width, int height, std::vector<Widget> layout)
    : AudioProcessorEditor(p), state(st), look_(theme), title_(std::move(title)), subtitle_(std::move(subtitle)),
      baseW_(width), baseH_(height), layout_(std::move(layout)) {
  setLookAndFeel(&look_);
  for (auto& w : layout_) {
    auto it = std::make_unique<Item>();
    it->w = w;
    auto* prm = w.param.isNotEmpty() ? st.getParameter(w.param) : nullptr;
    switch (w.type) {
      case W::BigKnob: case W::Knob: case W::SmallKnob: case W::Atten: {
        auto s = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
        s->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        s->setPopupDisplayEnabled(true, true, this);
        s->setVelocityBasedMode(false);
        s->setMouseDragSensitivity(w.type == W::BigKnob ? 320 : 220);
        if (w.type == W::Atten) s->getProperties().set("bipolar", true);
        if (prm) {
          if (auto* fp = dynamic_cast<AudioParameterFloat*>(prm))
            if (fp->getNormalisableRange().start < 0.0f && fp->getNormalisableRange().end > 0.0f)
              s->getProperties().set("bipolar", true);
          s->setTooltip(prm->getName(64));
          it->sa = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(st, w.param, *s);
        }
        s->setDoubleClickReturnValue(true, prm ? prm->convertFrom0to1(prm->getDefaultValue()) : 0.0);
        it->comp = std::move(s);
        break;
      }
      case W::Button: {
        auto b = std::make_unique<MomentaryButton>(prm);
        b->setButtonText(w.label);
        it->comp = std::move(b);
        break;
      }
      case W::Toggle: {
        auto b = std::make_unique<TextButton>(w.label);
        b->setClickingTogglesState(true);
        if (prm) it->ba = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(st, w.param, *b);
        it->comp = std::move(b);
        break;
      }
      case W::Choice: {
        auto c = std::make_unique<ComboBox>();
        if (auto* cp = dynamic_cast<AudioParameterChoice*>(prm)) c->addItemList(cp->choices, 1);
        if (prm) it->ca = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(st, w.param, *c);
        it->comp = std::move(c);
        break;
      }
      default: break;
    }
    if (it->comp) addAndMakeVisible(*it->comp);
    items_.push_back(std::move(it));
  }
  setResizable(true, true);
  setResizeLimits(baseW_ * 3 / 5, baseH_ * 3 / 5, baseW_ * 2, baseH_ * 2);
  if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) baseW_ / baseH_);
  setSize(baseW_, baseH_);
  startTimerHz(30);
}

PanelEditor::~PanelEditor() { setLookAndFeel(nullptr); }

Rectangle<float> PanelEditor::unit(float x, float y, float w, float h) const {
  return { x * scale_, y * scale_, w * scale_, h * scale_ };
}

Component* PanelEditor::find(const String& id) {
  for (auto& it : items_) if (it->w.param == id && it->comp) return it->comp.get();
  return nullptr;
}

static float knobDiameter(W t) {
  switch (t) {
    case W::BigKnob: return 62.0f;
    case W::Knob: return 44.0f;
    case W::SmallKnob: return 32.0f;
    case W::Atten: return 26.0f;
    default: return 0.0f;
  }
}

void PanelEditor::resized() {
  scale_ = (float) getWidth() / (float) baseW_;
  for (auto& it : items_) {
    if (!it->comp) continue;
    const auto& w = it->w;
    const float d = knobDiameter(w.type);
    if (d > 0.0f) it->comp->setBounds(unit(w.x - d * 0.5f, w.y - d * 0.5f, d, d).toNearestInt());
    else it->comp->setBounds(unit(w.x, w.y, w.w, w.h).toNearestInt());
  }
}

void PanelEditor::drawLed(Graphics& g, float cx, float cy, float r, float amount, Colour c) const {
  auto b = unit(cx - r, cy - r, r * 2, r * 2);
  g.setColour(Colours::black.withAlpha(0.6f));
  g.fillEllipse(b.expanded(1.0f));
  g.setColour(c.withMultipliedBrightness(0.25f + 0.75f * amount).withMultipliedSaturation(0.4f + 0.6f * amount));
  g.fillEllipse(b);
  if (amount > 0.05f) {
    g.setColour(c.withAlpha(0.35f * amount));
    g.fillEllipse(b.expanded(b.getWidth() * 0.6f));
    g.setColour(Colours::white.withAlpha(0.5f * amount));
    g.fillEllipse(b.reduced(b.getWidth() * 0.32f).translated(-b.getWidth() * 0.12f, -b.getWidth() * 0.12f));
  }
}

void PanelEditor::paint(Graphics& g) {
  const auto& t = look_.theme;
  auto r = getLocalBounds().toFloat();
  ColourGradient bg(t.panel.brighter(0.08f), 0, 0, t.panel.darker(0.25f), 0, r.getHeight(), false);
  g.setGradientFill(bg);
  g.fillRect(r);
  // brushed texture
  Random rng(1234);
  for (int i = 0; i < getHeight(); i += 2) {
    g.setColour(Colours::white.withAlpha(0.012f + 0.012f * rng.nextFloat()));
    g.fillRect(0.0f, (float) i, r.getWidth(), 1.0f);
  }
  g.setColour(t.panelEdge);
  g.drawRect(r, 2.0f);
  // rail screws
  for (float sx : { 14.0f, (float) baseW_ - 14.0f })
    for (float sy : { 12.0f, (float) baseH_ - 12.0f }) {
      auto s = unit(sx - 5, sy - 5, 10, 10);
      g.setColour(t.panel.darker(0.6f)); g.fillEllipse(s);
      g.setColour(t.ink.withAlpha(0.25f)); g.drawLine(s.getX() + 2, s.getCentreY(), s.getRight() - 2, s.getCentreY(), 1.2f);
    }

  // title
  g.setColour(t.ink);
  g.setFont(kbFont(26.0f * scale_, true));
  g.drawText(title_, unit(0, 14, (float) baseW_, 30).toNearestInt(), Justification::centred);
  g.setColour(t.ink.withAlpha(0.6f));
  g.setFont(kbFont(11.0f * scale_));
  g.drawText(subtitle_, unit(0, 42, (float) baseW_, 14).toNearestInt(), Justification::centred);

  for (auto& it : items_) {
    const auto& w = it->w;
    if (w.type == W::Section) {
      auto b = unit(w.x, w.y, w.w, w.h);
      g.setColour(t.section);
      g.fillRoundedRectangle(b, 8.0f * scale_);
      g.setColour(t.ink.withAlpha(0.22f));
      g.drawRoundedRectangle(b, 8.0f * scale_, 1.0f);
      if (w.label.isNotEmpty()) {
        g.setColour(t.ink.withAlpha(0.75f));
        g.setFont(kbFont(10.5f * scale_, true));
        g.drawText(w.label.toUpperCase(), unit(w.x + 10, w.y + 4, w.w - 20, 14).toNearestInt(), Justification::centredLeft);
      }
    } else if (w.type == W::Label) {
      g.setColour(t.ink.withAlpha(0.85f));
      g.setFont(kbFont((w.h > 0 ? w.h : 11.0f) * scale_, true));
      g.drawText(w.label, unit(w.x - 80, w.y, 160, (w.h > 0 ? w.h : 11.0f) + 4).toNearestInt(), Justification::centred);
    }
  }
  for (auto& it : items_) {
    const auto& w = it->w;
    const float d = knobDiameter(w.type);
    if (d > 0.0f && w.label.isNotEmpty()) {
      g.setColour(t.ink);
      g.setFont(kbFont((w.type == W::BigKnob ? 12.0f : (w.type == W::Atten ? 9.0f : 10.5f)) * scale_, true));
      g.drawText(w.label.toUpperCase(), unit(w.x - 50, w.y + d * 0.5f + 1, 100, 13).toNearestInt(), Justification::centred);
      if (w.sub.isNotEmpty()) {
        g.setColour(t.accent.withAlpha(0.8f));
        g.setFont(kbFont(9.0f * scale_));
        g.drawText(w.sub, unit(w.x - 50, w.y + d * 0.5f + 13, 100, 11).toNearestInt(), Justification::centred);
      }
    }
    if ((w.type == W::Choice) && w.label.isNotEmpty()) {
      g.setColour(t.ink.withAlpha(0.75f));
      g.setFont(kbFont(9.5f * scale_, true));
      g.drawText(w.label.toUpperCase(), unit(w.x, w.y - 13, w.w, 12).toNearestInt(), Justification::centredLeft);
    }
  }
  // brand
  g.setColour(t.ink.withAlpha(0.45f));
  g.setFont(kbFont(10.0f * scale_, true));
  g.drawText("k a l a b a s h", unit(0, (float) baseH_ - 22, (float) baseW_, 14).toNearestInt(), Justification::centred);
}

void PanelEditor::paintOverChildren(Graphics& g) { if (drawExtras) drawExtras(g, *this); }

}  // namespace kb
