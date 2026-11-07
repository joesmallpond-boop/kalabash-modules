#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
using namespace juce;
int main(int argc, char** argv) {
  ScopedJuceInitialiser_GUI gui;
  String path = argv[1]; String png = argv[2]; bool synth = String(argv[3]) == "synth";
  AudioPluginFormatManager fm; fm.addDefaultFormats();
  OwnedArray<PluginDescription> descs; VST3PluginFormat vst3;
  vst3.findAllTypesForFile(descs, path);
  if (descs.isEmpty()) { printf("no plugin\n"); return 1; }
  String err; auto inst = fm.createPluginInstance(*descs[0], 44100, 512, err);
  if (!inst) { printf("load fail %s\n", err.toRawUTF8()); return 1; }
  printf("%s ins %d outs %d params %d latency %d\n", inst->getName().toRawUTF8(), inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels(), (int)inst->getParameters().size(), 0);
  inst->enableAllBuses();
  inst->prepareToPlay(44100, 512);
  printf("latency %d\n", inst->getLatencySamples());
  int ch = jmax(inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels());
  AudioBuffer<float> buf(ch, 512); double peak=0, sum=0; int nan=0; long N=0;
  auto t0 = Time::getMillisecondCounterHiRes();
  for (int b = 0; b < 86*4; ++b) {
    buf.clear(); MidiBuffer mb;
    if (synth) { if (b % 20 == 0) { mb.addEvent(MidiMessage::noteOn(1, 48 + (b/20)%12, (uint8)100), 10); } if (b % 20 == 12) mb.addEvent(MidiMessage::noteOff(1, 48 + (b/20)%12), 0); }
    else for (int c = 0; c < jmin(2, ch); ++c) for (int i = 0; i < 512; ++i) { double t = (b*512+i)/44100.0; buf.setSample(c, i, (float)(0.3*std::sin(2*M_PI*220*t)*std::exp(-4*std::fmod(t,0.5)))); }
    inst->processBlock(buf, mb);
    for (int c = 0; c < jmin(2,inst->getTotalNumOutputChannels()); ++c) for (int i = 0; i < 512; ++i) { float s = buf.getSample(c,i); if (!std::isfinite(s)) nan++; peak = jmax(peak, (double)std::abs(s)); sum += s*s; N++; }
  }
  auto t1 = Time::getMillisecondCounterHiRes();
  printf("4s audio: peak %.3f rms %.4f nan %d  cpu %.1f%% realtime\n", peak, std::sqrt(sum/N), nan, (t1-t0)/40.0);
  MemoryBlock st; inst->getStateInformation(st); inst->setStateInformation(st.getData(), (int)st.getSize()); printf("state %d bytes\n", (int)st.getSize());
  if (auto* ed = inst->createEditorIfNeeded()) {
    ed->setVisible(true);
    for (int i=0;i<10;++i) MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
    File f(png); f.deleteFile(); FileOutputStream os(f); PNGImageFormat().writeImageToStream(img, os);
    printf("editor %dx%d\n", ed->getWidth(), ed->getHeight());
    delete ed;
  }
  inst->releaseResources();
  return 0;
}
