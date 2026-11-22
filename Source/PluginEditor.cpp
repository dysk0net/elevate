#include "PluginEditor.h"

using namespace ui;

//==============================================================================
SeqGrid::SeqGrid(ElevateAudioProcessor& p, int index) : proc(p), idx(index)
{
    static const char* laneNames[] { "GATE", "PITCH", "VEL" };
    static const char laneChar[] { 'g', 'p', 'v' };
    const juce::String lenIds[] { ID::sq(idx, ID::seqLenG), ID::sq(idx, ID::seqLenP), ID::sq(idx, ID::seqLenV) };

    for (int l = 0; l < 3; ++l)
    {
        tabs[l].setButtonText(laneNames[l]);
        tabs[l].setClickingTogglesState(true);
        tabs[l].setRadioGroupId(101);
        tabs[l].setColour(juce::TextButton::buttonOnColourId, laneColours[l]);
        tabs[l].onClick = [this, l] { showLane(l); };
        addAndMakeVisible(tabs[l]);

        lengths[l].setSliderStyle(juce::Slider::LinearBar);
        lengths[l].setTextValueSuffix(" steps");
        lengths[l].setColour(juce::Slider::trackColourId, laneColours[l].withAlpha(0.6f));
        addAndMakeVisible(lengths[l]);
        sliderAtts.add(new SA(proc.apvts, lenIds[l], lengths[l]));

        for (int s = 0; s < 16; ++s)
        {
            auto& sl = steps[l][s];
            sl.setSliderStyle(juce::Slider::LinearVertical);
            sl.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            sl.getProperties().set("lane", l);
            sl.setPopupDisplayEnabled(true, true, nullptr);
            addAndMakeVisible(sl);
            sliderAtts.add(new SA(proc.apvts, ID::stepVal(laneChar[l], s, idx), sl));

            auto& b = stepOn[l][s];
            b.setButtonText(juce::String(s + 1));
            b.setClickingTogglesState(true);
            b.setColour(juce::TextButton::buttonOnColourId, laneColours[l]);
            addAndMakeVisible(b);
            buttonAtts.add(new BA(proc.apvts, ID::stepOn(laneChar[l], s, idx), b));
        }
    }
    for (int s = 0; s < 16; ++s)
    {
        octs[s].setSliderStyle(juce::Slider::LinearBar);
        octs[s].setTextValueSuffix(" oct");
        octs[s].setColour(juce::Slider::trackColourId, laneColours[1].withAlpha(0.45f));
        octs[s].setPopupDisplayEnabled(true, true, nullptr);
        addAndMakeVisible(octs[s]);
        sliderAtts.add(new SA(proc.apvts, ID::stepOct(s, idx), octs[s]));
    }

    if (idx == 1)   // routing bar: destination, amount, slew
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(ID::seq2Dest)))
            destBox.addItemList(c->choices, 1);
        destBox.setTooltip("Control driven by Sequencer 2");
        addAndMakeVisible(destBox);
        destAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, ID::seq2Dest, destBox);
        for (auto* sl : { &amtBar, &slewBar })
        { sl->setSliderStyle(juce::Slider::LinearBar); sl->setPopupDisplayEnabled(true, true, nullptr); addAndMakeVisible(sl); }
        amtBar.setTextValueSuffix(" amt");  slewBar.setTextValueSuffix(" slew");
        sliderAtts.add(new SA(proc.apvts, ID::seq2Amt, amtBar));
        sliderAtts.add(new SA(proc.apvts, ID::seq2Slew, slewBar));
    }

    tabs[0].setToggleState(true, juce::dontSendNotification);
    showLane(0);
    startTimerHz(30);
}

void SeqGrid::showLane(int l)
{
    lane = l;
    const bool link = proc.apvts.getRawParameterValue(ID::sq(idx, ID::seqLink))->load() > 0.5f;
    for (int i = 0; i < 3; ++i)
    {
        lengths[i].setVisible(link ? i == 0 : i == l);
        for (int s = 0; s < 16; ++s)
        { steps[i][s].setVisible(i == l); stepOn[i][s].setVisible(i == l); }
    }
    for (auto& o : octs) o.setVisible(l == 1);
    repaint();
}

void SeqGrid::timerCallback()
{
    const bool link = proc.apvts.getRawParameterValue(ID::sq(idx, ID::seqLink))->load() > 0.5f;
    if (link != lengths[0].isVisible() || (! link && ! lengths[lane].isVisible())) showLane(lane);
    repaint();
}

void SeqGrid::resized()
{
    auto r = getLocalBounds().reduced(4, 0);
    auto top = r.removeFromTop(24);
    for (int i = 0; i < 3; ++i) tabs[i].setBounds(top.removeFromLeft(70).reduced(2, 0));
    for (auto& l : lengths) l.setBounds(top.removeFromRight(150).reduced(2, 2));
    if (idx == 1)
    {
        top.removeFromLeft(12);
        destBox.setBounds(top.removeFromLeft(240).reduced(2, 1));
        amtBar.setBounds(top.removeFromLeft(110).reduced(2, 2));
        slewBar.setBounds(top.removeFromLeft(110).reduced(2, 2));
    }

    const int w = r.getWidth() / 16;
    const int stepsTop = r.getY() + 4;
    for (int s = 0; s < 16; ++s)
    {
        const int x = r.getX() + s * w;
        for (int l = 0; l < 3; ++l)
        {
            stepOn[l][s].setBounds(x + 2, stepsTop, w - 4, 20);
            steps[l][s].setBounds(x + 2, stepsTop + 24, w - 4, getHeight() - stepsTop - 24 - 24);
        }
        octs[s].setBounds(x + 2, getHeight() - 22, w - 4, 18);
    }
}

void SeqGrid::paint(juce::Graphics& g)
{
    const int w = (getWidth() - 8) / 16;
    const int cur = proc.sequencers[idx].uiStep[lane].load();
    const bool link = proc.apvts.getRawParameterValue(ID::sq(idx, ID::seqLink))->load() > 0.5f;
    const juce::String lenId = ID::sq(idx, (lane == 0 || link) ? ID::seqLenG : (lane == 1 ? ID::seqLenP : ID::seqLenV));
    const int len = (int) std::lround(proc.apvts.getRawParameterValue(lenId)->load());

    for (int s = 0; s < 16; ++s)
    {
        auto col = juce::Rectangle<int>(4 + s * w, 28, w, getHeight() - 28).toFloat();
        if (s >= len) { g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(col, 4.0f); }
        else if (s == cur)
        { g.setColour(laneColours[lane].withAlpha(0.22f)); g.fillRoundedRectangle(col, 4.0f); }
    }
}

//==============================================================================
ElevateEditor::ElevateEditor(ElevateAudioProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    addAndMakeVisible(content);
    auto& s = proc.apvts;

    auto panel = [&](const juce::String& t, int cols)
    { auto* pn = new Panel(t, cols); panels.add(pn); content.addAndMakeVisible(pn); return pn; };
    auto knob   = [&](Panel* pn, const juce::String& id, const juce::String& n)
    { auto* c = new KnobBox(s, id, n);   owned.add(c); pn->add(c); };
    auto choice = [&](Panel* pn, const juce::String& id, const juce::String& n)
    { auto* c = new ChoiceBox(s, id, n); owned.add(c); pn->add(c); };
    auto toggle = [&](Panel* pn, const juce::String& id, const juce::String& n)
    { auto* c = new ToggleBox(s, id, n); owned.add(c); pn->add(c); };

    auto* osc1 = panel("OSC 1", 5);
    choice(osc1, ID::o1Oct, "Octave"); knob(osc1, ID::o1Coarse, "Tune"); knob(osc1, ID::o1Fine, "Fine"); knob(osc1, ID::o1Shape, "Shape");
    knob(osc1, ID::o1ShMod, "Shape Mod"); choice(osc1, ID::o1ShSrc, "Mod Src"); knob(osc1, ID::subLvl, "Sub"); choice(osc1, ID::subOct, "Sub Oct");
    knob(osc1, ID::o1Uni, "Unison"); knob(osc1, ID::o1UniDet, "Uni Detune");

    auto* osc2 = panel("OSC 2 / WARP / PITCH", 7);
    choice(osc2, ID::o2Oct, "Octave"); knob(osc2, ID::o2Coarse, "Tune"); knob(osc2, ID::o2Fine, "Fine"); knob(osc2, ID::o2Shape, "Shape");
    knob(osc2, ID::o2ShMod, "Shape Mod"); knob(osc2, ID::pitchMod, "Pitch Mod");
    knob(osc2, ID::warp, "Warp"); knob(osc2, ID::warpMod, "Warp Mod"); choice(osc2, ID::warpSrc, "Warp Src");
    choice(osc2, ID::warpMode, "Warp Mode"); choice(osc2, ID::pitchSrc, "Pitch Src");
    knob(osc2, ID::o2Uni, "Unison"); knob(osc2, ID::o2UniDet, "Uni Detune");

    auto* mix = panel("MIXER", 2);
    knob(mix, ID::o1Vol, "Osc 1"); knob(mix, ID::o2Vol, "Osc 2"); knob(mix, ID::noiseVol, "Noise"); choice(mix, ID::noiseCol, "Color");

    auto* flt = panel("FILTER", 5);
    knob(flt, ID::drive, "Drive"); knob(flt, ID::cutoff, "Cutoff"); knob(flt, ID::res, "Res"); knob(flt, ID::growl, "Growl"); knob(flt, ID::fKey, "Key");
    knob(flt, ID::fEnv, "Env"); knob(flt, ID::fLfo, "LFO"); knob(flt, ID::hp, "High Pass"); toggle(flt, ID::toHp, "To HP");

    auto* fenv = panel("FILTER ENV", 5);
    knob(fenv, ID::fA, "A"); knob(fenv, ID::fD, "D"); knob(fenv, ID::fS, "S"); knob(fenv, ID::fR, "R"); knob(fenv, ID::fV, "Vel");
    auto* aenv = panel("AMP ENV", 5);
    knob(aenv, ID::aA, "A"); knob(aenv, ID::aD, "D"); knob(aenv, ID::aS, "S"); knob(aenv, ID::aR, "R"); knob(aenv, ID::aV, "Vel");

    auto* lfo = panel("LFO", 3);
    knob(lfo, ID::lfoRate, "Rate"); choice(lfo, ID::lfoDiv, "Division"); knob(lfo, ID::lfoTilt, "Tilt");
    choice(lfo, ID::lfoShape, "Shape"); toggle(lfo, ID::lfoSync, "Sync"); toggle(lfo, ID::lfoRetrig, "Retrig");

    auto* voice = panel("VOICE", 5);
    choice(voice, ID::mode, "Mode"); toggle(voice, ID::legato, "Legato"); knob(voice, ID::detune, "Detune"); knob(voice, ID::glide, "Glide");
    knob(voice, ID::tune, "Tune"); knob(voice, ID::bend, "Bend"); choice(voice, ID::modDest, "Wheel Dest"); knob(voice, ID::modAmt, "Wheel Amt");
    knob(voice, ID::volume, "Volume");

    auto* chorus = panel("CHORUS", 3);
    toggle(chorus, ID::chOn, "On"); toggle(chorus, ID::chSt, "Stereo"); knob(chorus, ID::chInt, "Intensity");
    auto* modfx = panel("MOD FX", 4);
    toggle(modfx, ID::mfOn, "On"); choice(modfx, ID::mfMode, "Mode"); knob(modfx, ID::mfRate, "Rate"); knob(modfx, ID::mfInt, "Intensity");
    auto* space = panel("SPACE FX", 5);
    knob(space, ID::spAmt, "Amount"); choice(space, ID::spMode, "Mode"); knob(space, ID::spTime, "Time");
    knob(space, ID::spFb, "Fdbk / Tone"); toggle(space, ID::spSync, "Sync");

    Panel* seqPanels[2];
    for (int n = 0; n < 2; ++n)
    {
        auto* seq = seqPanels[n] = panel(n == 0 ? "SEQUENCER" : "SEQUENCER 2  (MOD ROUTING)", 4);
        toggle(seq, ID::sq(n, ID::seqOn), "On"); choice(seq, ID::sq(n, ID::seqRate), "Rate");
        knob(seq, ID::sq(n, ID::seqSwing), "Swing"); knob(seq, ID::sq(n, ID::seqGate), "Gate Scale");
        choice(seq, ID::sq(n, ID::seqDir), "Direction"); toggle(seq, ID::sq(n, ID::seqLink), "Link Lanes");
        toggle(seq, ID::sq(n, ID::seqRetrig), "Retrigger");
    }

    // Layout in rows of panels.
    const int gap = 8, rowH = 182 + gap;
    auto row = [&](std::initializer_list<Panel*> list, int y)
    {
        int x = 14;
        for (auto* pn : list) { pn->setBounds(x, y, pn->preferredWidth(), pn->preferredHeight()); x += pn->getWidth() + gap; }
        return x;
    };
    // Stretches panels (by column count) so they fill the full row width.
    auto fillRow = [&](std::initializer_list<Panel*> list, int y, int h)
    {
        int totalCols = 0;
        for (auto* pn : list) totalCols += pn->getCols();
        const int avail = baseW - 28 - gap * ((int) list.size() - 1);
        int x = 14;
        for (auto* pn : list) { const int w = avail * pn->getCols() / totalCols; pn->setBounds(x, y, w, h); x += w + gap; }
    };
    row({ osc1, osc2, mix, flt }, 58);
    row({ fenv, aenv, lfo, voice }, 58 + rowH);
    fillRow({ chorus, modfx, space }, 58 + 2 * rowH, 106);
    const int seqY = 58 + 2 * rowH + 106 + gap;
    for (int n = 0; n < 2; ++n)
    {
        const int y = seqY + n * rowH;
        const int right = row({ seqPanels[n] }, y);
        seqPanels[n]->setSize(seqPanels[n]->getWidth(), 182);
        seqGrids[n] = std::make_unique<SeqGrid>(proc, n);
        content.addAndMakeVisible(*seqGrids[n]);
        seqGrids[n]->setBounds(right, y, baseW - right - 14, 182);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram(presetBox.getSelectedId() - 1); };
    prevBtn.onClick = [this]
    { const int n = proc.getNumPrograms(); proc.setCurrentProgram((proc.getCurrentProgram() + n - 1) % n);
      presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification); };
    nextBtn.onClick = [this]
    { proc.setCurrentProgram((proc.getCurrentProgram() + 1) % proc.getNumPrograms());
      presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification); };
    presetBox.setBounds(520, 14, 220, 26); prevBtn.setBounds(486, 14, 30, 26); nextBtn.setBounds(744, 14, 30, 26);
    for (auto* c : { (juce::Component*) &presetBox, (juce::Component*) &prevBtn, (juce::Component*) &nextBtn })
        content.addAndMakeVisible(c);

    setResizable(true, true);
    setResizeLimits(baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio((double) baseW / baseH);
    setSize(baseW, baseH);
    startTimerHz(5);
}

ElevateEditor::~ElevateEditor() { setLookAndFeel(nullptr); }

void ElevateEditor::timerCallback()
{
    const int id = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != id) presetBox.setSelectedId(id, juce::dontSendNotification);
}

void ElevateEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.addTransform(juce::AffineTransform::scale((float) getWidth() / (float) baseW));
    g.setColour(accent);
    g.setFont(font(28.0f, true));
    g.drawText("ELEVATE", 16, 8, 220, 38, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(font(11.0f));
    g.drawText("ANALOG SYNTHESIZER", 178, 20, 180, 20, juce::Justification::centredLeft);
}

void ElevateEditor::resized()
{
    content.setBounds(0, 0, baseW, baseH);
    content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) baseW));
}
