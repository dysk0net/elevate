#pragma once
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg       { 0xff131417 };
    const juce::Colour panel    { 0xff1c1d22 };
    const juce::Colour panelEdge{ 0xff2c2e35 };
    const juce::Colour accent   { 0xffffb300 };
    const juce::Colour text     { 0xffd5d8de };
    const juce::Colour muted    { 0xff7e828c };
    const juce::Colour knobBody { 0xff2b2d34 };
    const juce::Colour laneColours[3] { juce::Colour(0xffff7043), juce::Colour(0xff4fc3f7), juce::Colour(0xff9ccc65) };

    inline juce::Font font(float h, bool bold = false)
    { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

    class LnF : public juce::LookAndFeel_V4
    {
    public:
        LnF()
        {
            setColour(juce::ComboBox::backgroundColourId, knobBody);
            setColour(juce::ComboBox::textColourId, text);
            setColour(juce::ComboBox::outlineColourId, panelEdge);
            setColour(juce::ComboBox::arrowColourId, accent);
            setColour(juce::PopupMenu::backgroundColourId, panel);
            setColour(juce::PopupMenu::textColourId, text);
            setColour(juce::PopupMenu::highlightedBackgroundColourId, accent);
            setColour(juce::PopupMenu::highlightedTextColourId, bg);
            setColour(juce::TextButton::buttonColourId, knobBody);
            setColour(juce::TextButton::buttonOnColourId, accent);
            setColour(juce::TextButton::textColourOffId, muted);
            setColour(juce::TextButton::textColourOnId, bg);
            setColour(juce::Slider::trackColourId, accent);
            setColour(juce::Slider::backgroundColourId, panelEdge);
            setColour(juce::TooltipWindow::backgroundColourId, panel);
            setColour(juce::TooltipWindow::textColourId, text);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                              float startAngle, float endAngle, juce::Slider& s) override
        {
            auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
            const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f;
            const auto c = b.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);

            juce::Path track;
            track.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, startAngle, endAngle, true);
            g.setColour(panelEdge);
            g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            float a0 = startAngle, a1 = angle;
            if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            {
                a0 = startAngle + (float) s.valueToProportionOfLength(0.0) * (endAngle - startAngle);
                if (a0 > a1) std::swap(a0, a1);
            }
            if (a1 - a0 > 0.001f)
            {
                juce::Path val;
                val.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, a0, a1, true);
                g.setColour(accent);
                g.strokePath(val, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            const float br = r - 7.0f;
            g.setGradientFill(juce::ColourGradient(knobBody.brighter(0.25f), c.x, c.y - br, knobBody.darker(0.5f), c.x, c.y + br, false));
            g.fillEllipse(c.x - br, c.y - br, br * 2.0f, br * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

            juce::Path ptr;
            ptr.addRoundedRectangle(-1.2f, -br + 2.0f, 2.4f, br * 0.55f, 1.0f);
            g.setColour(text);
            g.fillPath(ptr, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                              juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (style != juce::Slider::LinearVertical)
            { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, s); return; }

            const int lane = (int) s.getProperties().getWithDefault("lane", -1);
            const auto col = lane >= 0 ? laneColours[lane] : accent;
            auto r = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(w * 0.22f, 0.0f);
            g.setColour(panelEdge.darker(0.3f));
            g.fillRoundedRectangle(r, 3.0f);
            const bool tied = lane == 0 && s.getValue() > 1.0001;
            g.setColour(tied ? juce::Colours::white : col);
            g.fillRoundedRectangle(r.withTop(pos), 3.0f);
            if (tied)       // bracket marks a legato (tied) gate step
            {
                g.setColour(juce::Colours::white);
                g.drawRect(r.expanded(1.0f), 1.0f);
            }
        }

        juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(11.0f, true); }
        juce::Font getComboBoxFont(juce::ComboBox&) override { return font(11.0f); }
        juce::Font getPopupMenuFont() override { return font(12.0f); }
    };

    //==========================================================================
    class KnobBox : public juce::Component
    {
    public:
        KnobBox(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
        {
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            slider.setPopupDisplayEnabled(true, true, nullptr);   // value shown on hover / drag
            addAndMakeVisible(slider);
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(font(10.5f));
            label.setColour(juce::Label::textColourId, muted);
            label.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(label);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
        }
        void resized() override
        { auto r = getLocalBounds(); label.setBounds(r.removeFromBottom(14)); slider.setBounds(r); }
    private:
        juce::Slider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    class ChoiceBox : public juce::Component
    {
    public:
        ChoiceBox(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(s.getParameter(id)))
                combo.addItemList(p->choices, 1);
            addAndMakeVisible(combo);
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(font(10.5f));
            label.setColour(juce::Label::textColourId, muted);
            addAndMakeVisible(label);
            att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, id, combo);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds(r.removeFromBottom(14));
            combo.setBounds(r.withSizeKeepingCentre(r.getWidth() - 2, 22));
        }
    private:
        juce::ComboBox combo; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    class ToggleBox : public juce::Component
    {
    public:
        ToggleBox(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
        {
            button.setButtonText(name);
            button.setClickingTogglesState(true);
            addAndMakeVisible(button);
            att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s, id, button);
        }
        void resized() override { button.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth() - 4, 24)); }
    private:
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };

    class Panel : public juce::Component
    {
    public:
        static constexpr int cellW = 58, cellH = 76, pad = 8, header = 22;
        Panel(const juce::String& t, int c) : title(t), cols(c) {}
        void add(juce::Component* c) { items.push_back(c); addAndMakeVisible(c); }
        int preferredWidth() const { return cols * cellW + pad * 2; }
        int preferredHeight() const { return header + rows() * cellH + pad; }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(panel); g.fillRoundedRectangle(r, 8.0f);
            g.setColour(panelEdge); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
            g.setColour(accent); g.setFont(font(11.0f, true));
            g.drawText(title, pad + 2, 4, getWidth() - pad * 2, 16, juce::Justification::centredLeft);
        }
        int getCols() const { return cols; }
        void resized() override
        {
            const int cw = (getWidth() - pad * 2) / cols;
            const int ch = std::max(cellH, (getHeight() - header - pad) / std::max(1, rows()));
            for (size_t i = 0; i < items.size(); ++i)
                items[i]->setBounds(pad + ((int) i % cols) * cw, header + ((int) i / cols) * ch, cw, ch);
        }
    private:
        int rows() const { return ((int) items.size() + cols - 1) / cols; }
        juce::String title; int cols; std::vector<juce::Component*> items;
    };

    //==========================================================================
    // 3-lane x 16-step sequencer view.
    class SeqGrid : public juce::Component, private juce::Timer
    {
    public:
        SeqGrid(ElevateAudioProcessor& p, int index);
        ~SeqGrid() override { stopTimer(); }
        void paint(juce::Graphics&) override;
        void resized() override;
    private:
        void timerCallback() override;
        void showLane(int lane);

        using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
        using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

        ElevateAudioProcessor& proc;
        int idx = 0, lane = 0;           // idx: which sequencer (0 or 1)
        juce::ComboBox destBox;          // Sequencer 2 routing
        juce::Slider amtBar, slewBar;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> destAtt;
        juce::TextButton tabs[3];
        juce::Slider steps[3][16], octs[16], lengths[3];
        juce::TextButton stepOn[3][16];
        juce::OwnedArray<SA> sliderAtts;
        juce::OwnedArray<BA> buttonAtts;
    };
}

class ElevateEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ElevateEditor(ElevateAudioProcessor&);
    ~ElevateEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ElevateAudioProcessor& proc;
    ui::LnF lnf;
    juce::Component content;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<ui::Panel> panels;
    std::unique_ptr<ui::SeqGrid> seqGrids[2];
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    static constexpr int baseW = 1240, baseH = 944;
};
