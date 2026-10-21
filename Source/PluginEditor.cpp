#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace elp;

//==============================================================================
ELPLookAndFeel::ELPLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::Label::textColourId, ui::ink);
    setColour (juce::Slider::textBoxTextColourId, ui::ink);
    setColour (juce::Slider::textBoxBackgroundColourId, ui::panel2);
    setColour (juce::Slider::textBoxOutlineColourId, ui::line);
    setColour (juce::TextButton::buttonColourId, ui::panel2);
    setColour (juce::TextButton::buttonOnColourId, ui::warn);
    setColour (juce::TextButton::textColourOffId, ui::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::textColourId, ui::ink);
    setColour (juce::ComboBox::arrowColourId, ui::muted);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel2);
    setColour (juce::PopupMenu::textColourId, ui::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::bar.withAlpha (0.35f));
    setColour (juce::TabbedButtonBar::tabTextColourId, ui::muted);
    setColour (juce::TabbedButtonBar::frontTextColourId, ui::ink);
    setColour (juce::TabbedComponent::backgroundColourId, ui::panel);
    setColour (juce::TabbedComponent::outlineColourId, ui::line);
    setColour (juce::ScrollBar::thumbColourId, ui::line);
    setColour (juce::ToggleButton::textColourId, ui::ink);
    setColour (juce::ToggleButton::tickColourId, ui::bar);
}

void ELPLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (ui::line);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (s.isEnabled() ? ui::bar : ui::muted);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.setColour (ui::ink);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

void ELPLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - 9, 34, 18);
    const bool on = b.getToggleState();
    g.setColour (on ? ui::ok.withAlpha (0.85f) : ui::line);
    g.fillRoundedRectangle (box, 9.0f);
    g.setColour (on ? juce::Colours::black : ui::muted);
    const float kx = on ? box.getRight() - 16 : box.getX() + 2;
    g.fillEllipse (kx, box.getY() + 2, 14, 14);
    g.setColour (ui::ink);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (44), juce::Justification::centredLeft);
}

//==============================================================================
void ParamPanel::heading (const juce::String& text)
{
    auto* l = new juce::Label ({}, text.toUpperCase()); owned.add (l);
    l->setFont (juce::FontOptions (12.5f, juce::Font::bold));
    l->setColour (juce::Label::textColourId, ui::muted);
    addAndMakeVisible (l);
    rows.push_back ({ l, nullptr, 30, true });
}
void ParamPanel::note (const juce::String& text, int height)
{
    auto* l = new juce::Label ({}, text); owned.add (l);
    l->setFont (juce::FontOptions (13.0f));
    l->setColour (juce::Label::textColourId, ui::muted);
    l->setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (l);
    rows.push_back ({ l, nullptr, height, true });
}
void ParamPanel::slider (const char* id, const juce::String& label)
{
    auto* s = new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight); owned.add (s);
    s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 86, 22);
    addAndMakeVisible (s);
    sa.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *s));
    if (auto* prm = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (id))) s->setTextValueSuffix (" " + prm->getLabel());
    auto l = std::make_unique<juce::Label> (juce::String(), label);
    l->setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (*l);
    rows.push_back ({ s, std::move (l), 32, false });
}
void ParamPanel::toggle (const char* id, const juce::String& label)
{
    auto* b = new juce::ToggleButton (label); owned.add (b);
    addAndMakeVisible (b);
    ba.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *b));
    rows.push_back ({ b, nullptr, 30, true });
}
void ParamPanel::combo (const char* id, const juce::String& label)
{
    auto* c = new juce::ComboBox(); owned.add (c);
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id))) c->addItemList (prm->choices, 1);
    addAndMakeVisible (c);
    ca.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *c));
    auto l = std::make_unique<juce::Label> (juce::String(), label);
    l->setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (*l);
    rows.push_back ({ c, std::move (l), 32, false });
}
void ParamPanel::custom (juce::Component& c, int height) { addAndMakeVisible (c); rows.push_back ({ &c, nullptr, height, true }); }
int ParamPanel::preferredHeight() const { int h = 8; for (auto& r : rows) h += r.height; return h + 8; }
void ParamPanel::resized()
{
    int y = 8; const int w = getWidth() - 24;
    for (auto& r : rows)
    {
        if (r.full) r.control->setBounds (12, y, w, r.height - 2);
        else
        {
            const int lw = juce::jlimit (110, 190, (int) (w * 0.38));
            r.label->setBounds (12, y, lw, r.height - 2);
            r.control->setBounds (12 + lw, y + 2, w - lw, r.height - 6);
        }
        y += r.height;
    }
}

//==============================================================================
static float xOfF (double f, juce::Rectangle<float> r) { return r.getX() + (float) (std::log (f / 18.0) / std::log (22000.0 / 18.0)) * r.getWidth(); }

static void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, double lo, double hi, double step, const juce::String& unit)
{
    g.setFont (juce::FontOptions (11.0f));
    for (double v = lo; v <= hi + 1e-9; v += step)
    {
        const float y = r.getBottom() - (float) ((v - lo) / (hi - lo)) * r.getHeight();
        g.setColour (std::abs (v) < 1e-9 && lo < 0 ? ui::muted.withAlpha (0.6f) : ui::grid);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (ui::muted);
        g.drawText ((v > 0 && lo < 0 ? "+" : "") + juce::String ((int) v), (int) r.getX() - 40, (int) y - 7, 34, 14, juce::Justification::centredRight);
    }
    const double fs[] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    const char* fl[] = { "31.5", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = xOfF (fs[i], r);
        g.setColour (ui::grid); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (ui::muted); g.drawText (fl[i], (int) x - 20, (int) r.getBottom() + 3, 40, 14, juce::Justification::centred);
    }
    g.setColour (ui::muted);
    g.drawText (unit, (int) r.getX() - 44, (int) r.getY() - 16, 60, 14, juce::Justification::centredLeft);
}

static void drawCurve (juce::Graphics& g, juce::Rectangle<float> r, const std::array<double, NB>& v, double lo, double hi, juce::Colour c, float w,
                       bool dashed = false, const std::array<bool, NB>* valid = nullptr, int from = 0, int to = NB - 1)
{
    juce::Path p; bool on = false;
    for (int b = from; b <= to; ++b)
    {
        if ((valid && ! (*valid)[(size_t) b]) || v[(size_t) b] < -150) { on = false; continue; }
        const float x = xOfF (bandCentres()[(size_t) b], r);
        const float y = r.getBottom() - (float) ((clampd (v[(size_t) b], lo, hi) - lo) / (hi - lo)) * r.getHeight();
        if (! on) { p.startNewSubPath (x, y); on = true; } else p.lineTo (x, y);
    }
    g.setColour (c);
    juce::PathStrokeType st (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    if (dashed) { juce::Path d; const float dl[] = { 6.0f, 4.0f }; st.createDashedStroke (d, p, dl, 2); g.fillPath (d); }
    else g.strokePath (p, st);
}

static void legend (juce::Graphics& g, juce::Rectangle<float> r, std::initializer_list<std::pair<juce::String, juce::Colour>> items)
{
    g.setFont (juce::FontOptions (12.0f));
    float x = r.getRight();
    std::vector<std::pair<juce::String, juce::Colour>> v (items);
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), it->first) + 26.0f;
        x -= tw;
        g.setColour (it->second); g.fillRect (x, r.getY() + 6, 14.0f, 3.0f);
        g.setColour (ui::muted); g.drawText (it->first, (int) x + 18, (int) r.getY(), (int) tw, 16, juce::Justification::centredLeft);
    }
}

void Plots::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto inner = r.reduced (14.0f);
    auto top = inner.removeFromTop (inner.getHeight() * 0.52f);
    inner.removeFromTop (10);
    drawRta (g, top);
    drawCorr (g, inner);
}

void Plots::drawRta (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (ui::ink); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("LIVE RTA", area.removeFromTop (20).toNearestInt(), juce::Justification::centredLeft);
    legend (g, area.withHeight (16).translated (0, -20), { { "Program (predicted SPL)", ui::muted }, { "Mic 1", ui::mic[0] }, { "Mic 2", ui::mic[1] }, { "Mic 3", ui::mic[2] } });
    auto r = area.withTrimmedLeft (44).withTrimmedBottom (18).withTrimmedTop (16);
    const double lo = 40, hi = 130;
    // ISO 226 contours
    g.setColour (ui::muted.withAlpha (0.22f));
    for (double P : { 40.0, 60.0, 80.0, 100.0 })
    {
        juce::Path p;
        for (int b = 0; b <= 28; ++b)
        {
            const float x = xOfF (bandCentres()[(size_t) b], r), y = r.getBottom() - (float) ((clampd (iso226::phonToSpl (b, P), lo, hi) - lo) / (hi - lo)) * r.getHeight();
            b ? p.lineTo (x, y) : p.startNewSubPath (x, y);
        }
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    drawFreqGrid (g, r, lo, hi, 20, "dB SPL");
    // program as bars
    const float bw = (xOfF (bandCentres()[1], r) - xOfF (bandCentres()[0], r)) * 0.7f;
    g.setColour (ui::muted.withAlpha (0.35f));
    for (int b = 0; b < NB; ++b)
    {
        const double v = data.progSpl[(size_t) b]; if (v < lo) continue;
        const float x = xOfF (bandCentres()[(size_t) b], r), y = r.getBottom() - (float) ((clampd (v, lo, hi) - lo) / (hi - lo)) * r.getHeight();
        g.fillRect (x - bw / 2, y, bw, r.getBottom() - y);
    }
    for (int m = 0; m < 3; ++m) if (data.micActive[m]) drawCurve (g, r, data.mic[m], lo, hi, ui::mic[m], 2.0f);
}

void Plots::drawCorr (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (ui::ink); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("SYSTEM RESPONSE & CORRECTION", area.removeFromTop (20).toNearestInt(), juce::Justification::centredLeft);
    legend (g, area.withHeight (16).translated (0, -20), { { "Setup traces", ui::muted }, { "Live", ui::bar }, { "Target", ui::ref }, { "Loudness comp.", ui::comp.withAlpha (0.6f) }, { "Applied EQ", ui::ink } });
    auto r = area.withTrimmedLeft (44).withTrimmedBottom (18).withTrimmedTop (16);
    drawFreqGrid (g, r, -18, 18, 6, "dB");
    if (data.hasSetup) drawCurve (g, r, data.setup, -18, 18, ui::muted, 2.0f, false, nullptr, 1, 29);
    if (data.hasLive) drawCurve (g, r, data.live, -18, 18, ui::bar, 2.0f, false, &data.liveValid, 1, 29);
    drawCurve (g, r, data.target, -18, 18, ui::ref, 1.8f, true, nullptr, 1, 29);
    drawCurve (g, r, data.ear, -18, 18, ui::comp.withAlpha (0.6f), 1.4f, true);
    drawCurve (g, r, data.total, -18, 18, ui::ink, 2.6f);
    if (! data.hasSetup && ! data.hasLive)
    {
        g.setColour (ui::muted); g.setFont (juce::FontOptions (13.0f));
        g.drawText ("No room data yet. Capture setup traces on the Room tab, or turn on Live trim.", r.reduced (10).removeFromTop (20).toNearestInt(), juce::Justification::centredLeft);
    }
}

//==============================================================================
void TraceList::refresh()
{
    const auto t = proc.getTraces();
    rows.clear();
    for (int i = 0; i < (int) t.size(); ++i)
    {
        Row r;
        r.on = std::make_unique<juce::ToggleButton> ("");
        r.on->setToggleState (t[(size_t) i].enabled, juce::dontSendNotification);
        r.on->onClick = [this, i, b = r.on.get()] { proc.setTraceEnabled (i, b->getToggleState()); };
        r.name = std::make_unique<juce::Label> (juce::String(), t[(size_t) i].name + "   " + t[(size_t) i].source);
        r.name->setFont (juce::FontOptions (13.5f));
        r.del = std::make_unique<juce::TextButton> ("Delete");
        r.del->onClick = [this, i] { juce::MessageManager::callAsync ([this, i] { proc.removeTrace (i); }); };
        addAndMakeVisible (*r.on); addAndMakeVisible (*r.name); addAndMakeVisible (*r.del);
        rows.push_back (std::move (r));
    }
    setSize (getWidth() > 0 ? getWidth() : 300, preferredHeight());
    resized(); repaint();
}
void TraceList::resized()
{
    int y = 2;
    for (auto& r : rows)
    {
        r.on->setBounds (0, y, 44, 26);
        r.del->setBounds (getWidth() - 64, y + 2, 64, 22);
        r.name->setBounds (44, y, getWidth() - 44 - 70, 26);
        y += 30;
    }
}
void TraceList::paint (juce::Graphics& g)
{
    if (rows.empty())
    {
        g.setColour (ui::line); g.drawRect (getLocalBounds(), 1);
        g.setColour (ui::muted); g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("No setup traces. Capture with pink noise before doors, or import a measurement file (Smaart, REW or any frequency/dB text file).", getLocalBounds().reduced (8), juce::Justification::centredLeft, 3);
    }
}

//==============================================================================
ELPEditor::ELPEditor (ELPProcessor& p)
    : AudioProcessorEditor (&p), proc (p), loudness (p.apvts), room (p.apvts), mics (p.apvts), outputs (p.apvts), traceList (p)
{
    setLookAndFeel (&lnf);
   #if JucePlugin_Build_Standalone
    if (juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->getMuteInputValue().setValue (false);   // this app is meant to process its inputs
   #endif

    addAndMakeVisible (plots);
    bypassBtn.setClickingTogglesState (true); pinkBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn); addAndMakeVisible (pinkBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "eqBypass", bypassBtn);
    pinkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "pinkOn", pinkBtn);

    buildPages();
    addAndMakeVisible (tabs);
    tabs.setTabBarDepth (32);
    tabs.setIndent (0);

    setResizable (true, true);
    setResizeLimits (1000, 680, 2400, 1600);
    setSize (1280, 800);
    startTimerHz (20);
}

ELPEditor::~ELPEditor() { setLookAndFeel (nullptr); }

void ELPEditor::buildPages()
{
    // Loudness
    loudness.heading ("Loudness compensation");
    loudness.note ("Re-balances the EQ as the show gets louder or quieter, so the mix keeps the tonal balance it has at the reference level (ISO 226 equal-loudness contours).", 50);
    loudness.toggle ("earOn", "Loudness compensation on");
    loudness.slider ("refLevel", "Reference level");
    loudness.slider ("earDepth", "Depth");
    loudness.combo ("levelSrc", "Level source");
    loudness.slider ("manualLevel", "Manual level");
    loudness.heading ("Dynamics");
    loudness.note ("How fast the curve follows the music. Shorter attack reacts to builds; a longer release keeps quiet passages from pumping. The curve holds during silence.", 50);
    loudness.slider ("attack", "Attack");
    loudness.slider ("release", "Release");
    loudness.heading ("Listener");
    loudness.toggle ("diffuse", "Diffuse sound field (reverberant room)");

    // Room
    room.heading ("Room correction");
    room.toggle ("roomOn", "Room correction on");
    room.slider ("lfLift", "Target low-end lift");
    room.slider ("hfTilt", "Target tilt above 1 kHz");
    room.slider ("maxBoost", "Max boost");
    room.slider ("maxCut", "Max cut");
    room.heading ("Live trim from mics");
    room.note ("Compares each mic with the processed program during the show and nudges the setup correction toward it, within the trim range. Long averaging keeps crowd noise out.", 50);
    room.toggle ("liveTrim", "Live trim on");
    room.slider ("trimRange", "Trim range");
    room.slider ("trimTime", "Averaging time");
    room.heading ("Setup traces");
    capLen.addItemList ({ "5 s", "10 s", "20 s", "30 s" }, 1); capLen.setSelectedId (2);
    capRow.add (capLen, 90); capRow.add (capBtn, -1);
    fileRow.add (importBtn, -1); fileRow.add (clearBtn, -1);
    capBtn.onClick = [this] { const double s[] = { 5, 10, 20, 30 }; proc.startCapture (s[juce::jlimit (0, 3, capLen.getSelectedId() - 1)]); };
    importBtn.onClick = [this] { chooseFile ("Import a measurement trace", [this] (const juce::File& f) { if (! proc.importTrace (f)) capStatus.setText ("That file has no frequency/dB pairs this app can read.", juce::dontSendNotification); }); };
    clearBtn.onClick = [this] {
        if (! clearArmed) { clearArmed = true; clearArmTime = juce::Time::getMillisecondCounter(); clearBtn.setButtonText ("Confirm clear"); return; }
        clearArmed = false; clearBtn.setButtonText ("Clear all traces"); proc.clearTraces();
    };
    room.custom (capRow, 36);
    room.custom (capStatus, 26);
    capStatus.setFont (juce::FontOptions (13.0f)); capStatus.setColour (juce::Label::textColourId, ui::muted);
    capStatus.setText ("Captures every active mic at once with pink noise against the processed output.", juce::dontSendNotification);
    traceView.setViewedComponent (&traceList, false);
    traceView.setScrollBarsShown (true, false);
    room.custom (traceView, 200);
    room.custom (fileRow, 36);

    // Mics
    mics.note ("Inputs 3, 4 and 5 are the measurement mics. Calibrate each one with a 94 dB calibrator, and load its correction file if it has one.", 50);
    const char* on[] = { "mic1On", "mic2On", "mic3On" };
    const char* off[] = { "mic1Off", "mic2Off", "mic3Off" };
    for (int m = 0; m < 3; ++m)
    {
        mics.heading ("Mic " + juce::String (m + 1) + " | input " + juce::String (m + 3));
        mics.toggle (on[m], "Use mic " + juce::String (m + 1));
        mics.slider (off[m], "0 dBFS =");
        auto& r = micRows[m];
        r.box.add (r.cal, 136); r.box.add (r.load, 122); r.box.add (r.clear, -1);
        r.cal.onClick = [this, m] { proc.calibrateMic (m); };
        r.load.onClick = [this, m] { chooseFile ("Load mic correction file", [this, m] (const juce::File& f) { if (! proc.loadMicCal (m, f)) micRows[m].status.setText ("That file has no frequency/dB pairs.", juce::dontSendNotification); }); };
        r.clear.onClick = [this, m] { proc.clearMicCal (m); };
        mics.custom (r.box, 34);
        r.status.setFont (juce::FontOptions (13.0f)); r.status.setColour (juce::Label::textColourId, ui::muted);
        mics.custom (r.status, 24);
    }
    mics.heading ("Program level calibration");
    mics.note ("Maps program level to SPL so loudness compensation can react instantly without waiting on the mics. With auto on, it learns the offset from the mics during the show.", 50);
    mics.toggle ("autoOffset", "Learn from mics automatically");
    mics.slider ("progOffset", "Program 0 dBFS =");
    offsetInfo.setFont (juce::FontOptions (13.0f)); offsetInfo.setColour (juce::Label::textColourId, ui::muted);
    mics.custom (offsetInfo, 24);

    // Outputs
    outputs.heading ("Crossover");
    outputs.note ("Outputs 1-2 are the mains and output 3 is the sub. Linkwitz-Riley 24 dB/oct; the sub is fed from the mono sum.", 36);
    outputs.toggle ("xoverOn", "Crossover on");
    outputs.slider ("xoverFreq", "Frequency");
    outputs.toggle ("mainsHpf", "High-pass the mains");
    outputs.heading ("Mains");
    outputs.slider ("mainsGain", "Gain");
    outputs.slider ("mainsDelay", "Delay");
    outputs.heading ("Sub");
    outputs.slider ("subGain", "Gain");
    outputs.slider ("subDelay", "Delay");
    outputs.toggle ("subInvert", "Invert polarity");
    outputs.heading ("Protection");
    outputs.toggle ("limitOn", "Output limiter");
    outputs.slider ("ceiling", "Ceiling");
    outputs.heading ("Test signal");
    outputs.slider ("pinkLevel", "Pink noise level");
    outputs.note ("Pink noise replaces the program on all outputs while it is on. Start low.", 30);

    ParamPanel* ps[4] = { &loudness, &room, &mics, &outputs };
    const char* names[4] = { "Loudness", "Room", "Mics", "Outputs" };
    for (int i = 0; i < 4; ++i)
    {
        pages[i] = std::make_unique<ScrollPage> (*ps[i]);
        pages[i]->contentHeight = [pp = ps[i]] { return pp->preferredHeight(); };
        tabs.addTab (names[i], ui::panel, pages[i].get(), false);
    }
}

void ELPEditor::chooseFile (const juce::String& title, std::function<void (const juce::File&)> then)
{
    chooser = std::make_unique<juce::FileChooser> (title, juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.txt;*.csv;*.frd;*.cal;*.trf;*");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [then] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) then (f); });
}

void ELPEditor::timerCallback()
{
    last = proc.getDisplay();
    plots.setData (last);

    if (proc.getTraceVersion() != seenTraceVersion)
    {
        seenTraceVersion = proc.getTraceVersion();
        traceList.refresh();
        traceList.setSize (traceView.getMaximumVisibleWidth(), traceList.preferredHeight());
        traceList.setSize (traceView.getMaximumVisibleWidth(), traceList.preferredHeight());
        for (int m = 0; m < 3; ++m) micRows[m].clear.setEnabled (proc.getMicCalName (m).isNotEmpty());
        room.resized(); pages[1]->layoutInner();
    }
    // keep the room page's custom row sizes in step with the trace count
    for (int m = 0; m < 3; ++m)
    {
        const auto cal = proc.getMicCalName (m);
        juce::String s = last.micActive[m] ? "Reading " + juce::String (last.micLevel[m], 1) + " dBC" : "No signal";
        s << (cal.isNotEmpty() ? "  |  correction: " + cal : "  |  no correction file");
        micRows[m].status.setText (s, juce::dontSendNotification);
    }
    offsetInfo.setText ("In use: program 0 dBFS = " + juce::String (last.progOffset, 1) + " dB SPL", juce::dontSendNotification);
    if (last.captureProgress >= 0)
    {
        capBtn.setEnabled (false);
        capStatus.setText ("Capturing... " + juce::String ((int) (last.captureProgress * 100)) + "%", juce::dontSendNotification);
    }
    else if (! capBtn.isEnabled() && ! proc.isCapturing())
    {
        capBtn.setEnabled (true);
        capStatus.setText ("Capture done. Each active mic added one trace.", juce::dontSendNotification);
    }
    if (clearArmed && juce::Time::getMillisecondCounter() - clearArmTime > 3000) { clearArmed = false; clearBtn.setButtonText ("Clear all traces"); }
    repaint (0, 0, getWidth(), 92);
}

void ELPEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    auto r = getLocalBounds().reduced (16).removeFromTop (76);
    g.setColour (ui::ink); g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("EQUAL-LOUDNESS PROCESSOR", r.removeFromTop (28), juce::Justification::centredLeft);
    r.removeFromTop (4);

    auto cell = [&] (const juce::String& k, const juce::String& v, const juce::String& sub, juce::Colour c) {
        auto a = r.removeFromLeft (168);
        g.setColour (ui::muted); g.setFont (juce::FontOptions (11.0f)); g.drawText (k.toUpperCase(), a.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (c); g.setFont (juce::FontOptions (21.0f, juce::Font::bold)); g.drawText (v, a.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (ui::muted); g.setFont (juce::FontOptions (11.5f)); g.drawText (sub, a, juce::Justification::centredLeft);
    };
    auto f1 = [] (double v, const char* u) { return v < -150 ? juce::String ("-") : juce::String (v, 1) + " " + u; };
    const double ref = proc.apvts.getRawParameterValue ("refLevel")->load();
    const double d = last.listenLevel - ref;
    const bool sig = last.listenLevel > -150;
    cell ("Listening level", f1 (last.listenLevel, "dBC"), sig ? (d >= 0 ? "+" : "") + juce::String (d, 1) + " vs reference " + juce::String (ref, 0) : juce::String ("Waiting for program"), ui::ink);
    cell ("Loudness", juce::String (last.loudnessSone, 1) + " sone", juce::String (last.loudnessPhon, 0) + " phon (ISO 532-1)", ui::ink);
    cell ("EQ at 63 Hz", (last.total[5] >= 0 ? "+" : "") + juce::String (last.total[5], 1) + " dB", "at 8 kHz " + juce::String (last.total[26], 1) + " dB", ui::comp);
    int nm = 0; for (bool a : last.micActive) nm += a ? 1 : 0;
    cell ("Mics", juce::String (nm) + " of 3", last.hasLive ? "Live response ready" : "Live response averaging", nm ? ui::ok : ui::muted);
    cell ("Limiter", juce::String (std::min (last.limiterMains, last.limiterSub), 1) + " dB", "Mains " + juce::String (last.limiterMains, 1) + " | sub " + juce::String (last.limiterSub, 1), std::min (last.limiterMains, last.limiterSub) < -0.5 ? ui::warn : ui::ink);
    cell ("Sample rate", last.sampleRate ? juce::String (last.sampleRate / 1000.0, 1) + " kHz" : "-", last.sampleRate ? "Program 0 dBFS = " + juce::String (last.progOffset, 0) + " dB SPL" : juce::String ("No audio device running"), ui::ink);
}

void ELPEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto head = r.removeFromTop (76);
    auto btns = head.removeFromRight (250).withSizeKeepingCentre (250, 34).translated (0, 10);
    pinkBtn.setBounds (btns.removeFromRight (120));
    btns.removeFromRight (10);
    bypassBtn.setBounds (btns.removeFromRight (120));
    r.removeFromTop (12);
    tabs.setBounds (r.removeFromRight (juce::jlimit (360, 460, r.getWidth() / 3)));
    r.removeFromRight (14);
    plots.setBounds (r);

    loudness.resized(); room.resized(); mics.resized(); outputs.resized();
    for (auto& pg : pages) if (pg) pg->layoutInner();
    traceList.setSize (traceView.getMaximumVisibleWidth(), traceList.preferredHeight());
}
