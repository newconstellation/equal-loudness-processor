#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg      { 0xff101514 }, panel { 0xff171e1d }, panel2 { 0xff1d2624 }, line { 0xff2b3633 };
    const juce::Colour ink     { 0xffe3eae8 }, muted { 0xff8f9e9a }, grid { 0xff222b29 };
    const juce::Colour bar     { 0xff5cb0cf }, ref   { 0xffeea24c }, comp  { 0xffb394e8 }, ok { 0xff7fd19c }, warn { 0xfff08a74 };
    const juce::Colour mic[3]  { juce::Colour (0xff5cb0cf), juce::Colour (0xffe6c35c), juce::Colour (0xffe07fb0) };
}

class ELPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ELPLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// A vertical list of labelled controls bound to parameters.
class ParamPanel : public juce::Component
{
public:
    explicit ParamPanel (juce::AudioProcessorValueTreeState& s) : state (s) {}
    void heading (const juce::String& text);
    void slider (const char* id, const juce::String& label);
    void toggle (const char* id, const juce::String& label);
    void combo (const char* id, const juce::String& label);
    void custom (juce::Component& c, int height);
    void note (const juce::String& text, int height = 34);
    int preferredHeight() const;
    void resized() override;

private:
    struct Row { juce::Component* control = nullptr; std::unique_ptr<juce::Label> label; int height = 30; bool full = false; };
    juce::AudioProcessorValueTreeState& state;
    std::vector<Row> rows;
    juce::OwnedArray<juce::Component> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sa;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> ba;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> ca;
};

class Plots : public juce::Component
{
public:
    void setData (const DisplayData& d) { data = d; repaint(); }
    void paint (juce::Graphics&) override;
private:
    void drawRta (juce::Graphics&, juce::Rectangle<float>);
    void drawCorr (juce::Graphics&, juce::Rectangle<float>);
    DisplayData data;
};

class TraceList : public juce::Component
{
public:
    explicit TraceList (ELPProcessor& p) : proc (p) {}
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    int preferredHeight() const { return std::max (60, (int) rows.size() * 30 + 4); }
private:
    struct Row { std::unique_ptr<juce::ToggleButton> on; std::unique_ptr<juce::Label> name; std::unique_ptr<juce::TextButton> del; };
    ELPProcessor& proc;
    std::vector<Row> rows;
};

// Lays children out left to right; width -1 takes the remaining space.
class HRow : public juce::Component
{
public:
    void add (juce::Component& c, int width) { items.push_back ({ &c, width }); addAndMakeVisible (c); }
    void resized() override
    {
        int fixed = 0, flex = 0; for (auto& i : items) { if (i.second < 0) ++flex; else fixed += i.second; }
        const int rest = flex ? std::max (40, (getWidth() - fixed) / flex) : 0;
        int x = 0;
        for (auto& i : items) { const int w = i.second < 0 ? rest : i.second; i.first->setBounds (x, 2, w - 6, getHeight() - 4); x += w; }
    }
private:
    std::vector<std::pair<juce::Component*, int>> items;
};

class ScrollPage : public juce::Component
{
public:
    explicit ScrollPage (juce::Component& content) : inner (content) { view.setViewedComponent (&inner, false); view.setScrollBarsShown (true, false); addAndMakeVisible (view); }
    void resized() override { view.setBounds (getLocalBounds()); layoutInner(); }
    std::function<int()> contentHeight;
    void layoutInner() { const int h = contentHeight ? contentHeight() : 600; inner.setBounds (0, 0, getWidth() - 10, h); }
private:
    juce::Component& inner;
    juce::Viewport view;
};

class ELPEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ELPEditor (ELPProcessor&);
    ~ELPEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildPages();
    void chooseFile (const juce::String& title, std::function<void (const juce::File&)> then);

    ELPProcessor& proc;
    ELPLookAndFeel lnf;
    Plots plots;
    juce::TextButton bypassBtn { "EQ bypass" }, pinkBtn { "Pink noise" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, pinkAtt;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    ParamPanel loudness, room, mics, outputs;
    std::unique_ptr<ScrollPage> pages[4];

    // room page extras
    juce::ComboBox capLen;
    juce::TextButton capBtn { "Capture setup trace" }, importBtn { "Import trace file" }, clearBtn { "Clear all traces" };
    HRow capRow, fileRow;
    juce::Viewport traceView;
    TraceList traceList;
    juce::Label capStatus;

    // mic page extras
    struct MicRow { juce::TextButton cal { "Calibrate 94 dB" }, load { "Load cal file" }, clear { "Clear cal" }; juce::Label status; HRow box; };
    MicRow micRows[3];
    juce::Label offsetInfo;

    std::unique_ptr<juce::FileChooser> chooser;
    DisplayData last;
    int seenTraceVersion = -1;
    bool clearArmed = false; juce::uint32 clearArmTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ELPEditor)
};
