#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace elp;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ELPProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float step, float def, const char* unit) {
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto bl = [&] (const char* id, const char* name, bool def) { v.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // Loudness compensation
    fl ("refLevel", "Reference level", 70, 105, 0.5f, 92, "dB SPL");
    bl ("earOn", "Loudness compensation", true);
    fl ("earDepth", "Compensation depth", 0, 100, 1, 100, "%");
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "levelSrc", 1 }, "Level source", StringArray { "Program (auto-calibrated)", "Mics", "Manual" }, 0));
    fl ("manualLevel", "Manual level", 60, 115, 0.5f, 95, "dB SPL");
    fl ("attack", "Attack", 0.1f, 5, 0.05f, 0.6f, "s");
    fl ("release", "Release", 0.5f, 20, 0.1f, 4, "s");
    bl ("diffuse", "Diffuse field", true);
    // Room correction
    bl ("roomOn", "Room correction", true);
    fl ("lfLift", "Target low-end lift", 0, 12, 0.5f, 4, "dB");
    fl ("hfTilt", "Target high-end tilt", -3, 0, 0.05f, -1, "dB/oct");
    fl ("maxBoost", "Max boost", 0, 12, 0.5f, 6, "dB");
    fl ("maxCut", "Max cut", 0, 18, 0.5f, 12, "dB");
    bl ("liveTrim", "Live trim from mics", false);
    fl ("trimRange", "Live trim range", 0, 6, 0.5f, 3, "dB");
    fl ("trimTime", "Live trim averaging", 10, 300, 1, 60, "s");
    bl ("eqBypass", "EQ bypass", false);
    // Mics
    for (int m = 1; m <= 3; ++m)
    {
        bl (("mic" + std::to_string (m) + "On").c_str(), ("Mic " + std::to_string (m)).c_str(), true);
        fl (("mic" + std::to_string (m) + "Off").c_str(), ("Mic " + std::to_string (m) + " 0 dBFS =").c_str(), 80, 150, 0.1f, 120, "dB SPL");
    }
    bl ("autoOffset", "Auto-calibrate program level", true);
    fl ("progOffset", "Program 0 dBFS =", 90, 150, 0.1f, 125, "dB SPL");
    // Outputs
    bl ("xoverOn", "Crossover", true);
    fl ("xoverFreq", "Crossover frequency", 40, 160, 1, 90, "Hz");
    bl ("mainsHpf", "High-pass mains", true);
    fl ("mainsGain", "Mains gain", -24, 12, 0.1f, 0, "dB");
    fl ("mainsDelay", "Mains delay", 0, 50, 0.01f, 0, "ms");
    fl ("subGain", "Sub gain", -24, 12, 0.1f, 0, "dB");
    fl ("subDelay", "Sub delay", 0, 50, 0.01f, 0, "ms");
    bl ("subInvert", "Sub polarity invert", false);
    bl ("limitOn", "Output limiter", true);
    fl ("ceiling", "Limiter ceiling", -12, 0, 0.1f, -1, "dBFS");
    bl ("pinkOn", "Pink noise", false);
    fl ("pinkLevel", "Pink noise level", -50, -6, 0.5f, -30, "dBFS");
    return { v.begin(), v.end() };
}

ELPProcessor::ELPProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Program", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Mics",    juce::AudioChannelSet::discreteChannels (3), true)
                        .withOutput ("Mains",   juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Sub",     juce::AudioChannelSet::mono(), true)),
      juce::Thread ("ELP analysis"),
      apvts (*this, nullptr, "ELP", createLayout())
{
    for (auto& t : eqTarget) t.store (0.0f);
    for (auto& h : hist) h.assign ((size_t) FFT_N, 0.0f);
    fftWork.assign ((size_t) FFT_N * 2, 0.0f);
    window.resize ((size_t) FFT_N);
    for (int i = 0; i < FFT_N; ++i) window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * PI * i / (FFT_N - 1)));
    for (auto& c : micCal) c.fill (0.0);
}

ELPProcessor::~ELPProcessor() { stopThread (3000); }

bool ELPProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1 && l.inputBuses[1].size() > 3) return false;
    if (l.outputBuses.size() > 1 && l.outputBuses[1].size() > 1) return false;
    return true;
}

void ELPProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    stopThread (3000);
    fs = sampleRate;
    analysisRate.store (sampleRate);
    scratch.setSize (5, std::max (samplesPerBlock, 32), false, true, false);
    eqCurrent.fill (0.0);
    for (auto& ch : eq) for (auto& b : ch) { b.k = {}; b.reset(); }
    for (auto& ch : xHp) for (auto& b : ch) b.reset();
    for (auto& b : xLp) b.reset();
    lastXover = -1;
    for (auto& d : dMain) d.prepare ((int) (0.06 * fs));
    dSub.prepare ((int) (0.06 * fs));
    limMain.prepare (fs, 2); limSub.prepare (fs, 1);
    pink = makePinkLoop (fs); pinkPos = 0;
    fifo.reset(); histFill = 0; histPos = 0;
    startThread (juce::Thread::Priority::low);
}

void ELPProcessor::releaseResources() {}

//==============================================================================
void ELPProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize (5, n, false, true, true);

    // 1. Copy every input before any output is written (input and output channels share the buffer).
    auto progIn = getBusBuffer (buffer, true, 0);
    scratch.copyFrom (0, 0, progIn, 0, 0, n);
    scratch.copyFrom (1, 0, progIn, progIn.getNumChannels() > 1 ? 1 : 0, 0, n);
    int micCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto mics = getBusBuffer (buffer, true, 1);
        micCh = std::min (3, mics.getNumChannels());
        for (int m = 0; m < micCh; ++m) scratch.copyFrom (2 + m, 0, mics, m, 0, n);
    }
    for (int m = micCh; m < 3; ++m) scratch.clear (2 + m, 0, n);
    micChannelsAvailable.store (micCh);

    float* L = scratch.getWritePointer (0);
    float* R = scratch.getWritePointer (1);

    // 2. Pink noise replaces the program while it is on.
    const bool pinkActive = p ("pinkOn") > 0.5f || pinkForced.load();
    const float pinkTarget = pinkActive ? std::pow (10.0f, (p ("pinkLevel") + 20.0f) / 20.0f) : 0.0f;
    const float progTarget = pinkActive ? 0.0f : 1.0f;
    const float step = 1.0f / (float) (0.05 * fs);
    std::vector<float>& pk = pink;
    for (int i = 0; i < n; ++i)
    {
        pinkGainNow += juce::jlimit (-step, step, pinkTarget - pinkGainNow);
        progGainNow += juce::jlimit (-step, step, progTarget - progGainNow);
        const float pn = pk.empty() ? 0.0f : pk[(size_t) pinkPos] * pinkGainNow;
        if (! pk.empty() && ++pinkPos >= (int) pk.size()) pinkPos = 0;
        L[i] = L[i] * progGainNow + pn;
        R[i] = R[i] * progGainNow + pn;
    }

    // 3. FIFO: program pre-EQ (mono).
    int s1, z1, s2, z2;
    fifo.prepareToWrite (n, s1, z1, s2, z2);
    const bool fifoOk = (z1 + z2) == n;
    auto pushMono = [&] (int ch, const float* a, const float* b) {
        if (! fifoOk) return;
        float* d = fifoBuf.getWritePointer (ch);
        for (int i = 0; i < z1; ++i) d[s1 + i] = 0.5f * (a[i] + b[i]);
        for (int i = 0; i < z2; ++i) d[s2 + i] = 0.5f * (a[z1 + i] + b[z1 + i]);
    };
    pushMono (0, L, R);

    // 4. Correction EQ (31 minimum-phase peaking bands), gains glide toward the analysis target.
    const bool bypass = p ("eqBypass") > 0.5f;
    const double a = 1.0 - std::exp (-(double) n / (0.05 * fs));
    const auto& fc = bandCentres();
    for (int b = 0; b < NB; ++b)
    {
        const double t = bypass ? 0.0 : (double) eqTarget[(size_t) b].load();
        const double g = eqCurrent[(size_t) b] + a * (t - eqCurrent[(size_t) b]);
        if (std::abs (g - eqCurrent[(size_t) b]) > 0.002 || (eq[0][b].k.b0 == 1.0 && std::abs (g) > 0.002))
        {
            const auto k = peaking (fs, fc[(size_t) b], GraphicEqDesigner::Q, g);
            eq[0][b].k = k; eq[1][b].k = k;
        }
        eqCurrent[(size_t) b] = g;
    }
    for (int c = 0; c < 2; ++c)
    {
        float* x = c == 0 ? L : R;
        for (int i = 0; i < n; ++i)
        {
            double y = x[i];
            for (int b = 0; b < NB; ++b) y = eq[c][b].process (y);
            x[i] = (float) y;
        }
    }

    // 5. FIFO: reference post-EQ (what the system is fed) and the mics.
    pushMono (1, L, R);
    if (fifoOk)
    {
        for (int m = 0; m < 3; ++m)
        {
            const float* src = scratch.getReadPointer (2 + m);
            float* d = fifoBuf.getWritePointer (2 + m);
            std::copy (src, src + z1, d + s1);
            std::copy (src + z1, src + z1 + z2, d + s2);
        }
        fifo.finishedWrite (n);
    }

    // 6. Crossover (Linkwitz-Riley 24 dB/oct), gains, delays, polarity.
    const float xf = p ("xoverFreq");
    if (xf != lastXover)
    {
        const auto hp = butter2 (fs, xf, true), lp = butter2 (fs, xf, false);
        for (auto& ch : xHp) for (auto& b : ch) b.k = hp;
        for (auto& b : xLp) b.k = lp;
        lastXover = xf;
    }
    const bool xo = p ("xoverOn") > 0.5f, hpf = p ("mainsHpf") > 0.5f;
    const float mg = juce::Decibels::decibelsToGain (p ("mainsGain"));
    const float sgain = juce::Decibels::decibelsToGain (p ("subGain")) * (p ("subInvert") > 0.5f ? -1.0f : 1.0f);
    dMain[0].setDelay ((int) std::round (p ("mainsDelay") * 0.001 * fs));
    dMain[1].setDelay ((int) std::round (p ("mainsDelay") * 0.001 * fs));
    dSub.setDelay ((int) std::round (p ("subDelay") * 0.001 * fs));
    float* S = scratch.getWritePointer (2); // mic data already pushed; reuse as sub workspace
    for (int i = 0; i < n; ++i)
    {
        const double mono = 0.5 * ((double) L[i] + (double) R[i]);
        double sub = xo ? xLp[1].process (xLp[0].process (mono)) : 0.0;
        double l = L[i], r = R[i];
        if (xo && hpf) { l = xHp[0][1].process (xHp[0][0].process (l)); r = xHp[1][1].process (xHp[1][0].process (r)); }
        L[i] = dMain[0].process ((float) l * mg);
        R[i] = dMain[1].process ((float) r * mg);
        S[i] = dSub.process ((float) sub * sgain);
    }

    // 7. Limiters.
    const bool lim = p ("limitOn") > 0.5f;
    const double ceil = juce::Decibels::decibelsToGain ((double) p ("ceiling"));
    float* mains[2] = { L, R };
    float* subs[1] = { S };
    limGrMain.store ((float) limMain.process (mains, 2, n, ceil, lim));
    limGrSub.store ((float) limSub.process (subs, 1, n, ceil, lim));

    // 8. Outputs.
    auto outMain = getBusBuffer (buffer, false, 0);
    outMain.copyFrom (0, 0, L, n);
    if (outMain.getNumChannels() > 1) outMain.copyFrom (1, 0, R, n);
    if (getBusCount (false) > 1 && getBus (false, 1)->isEnabled())
    {
        auto outSub = getBusBuffer (buffer, false, 1);
        if (outSub.getNumChannels() > 0) outSub.copyFrom (0, 0, S, n);
    }
    for (int c = getTotalNumOutputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

//==============================================================================
// Analysis thread: band levels, level tracking, live response, correction design.
void ELPProcessor::run()
{
    while (! threadShouldExit())
    {
        if (fifo.getNumReady() >= HOP)
        {
            int s1, z1, s2, z2;
            fifo.prepareToRead (HOP, s1, z1, s2, z2);
            for (int c = 0; c < NCH; ++c)
            {
                const float* src = fifoBuf.getReadPointer (c);
                auto& h = hist[c];
                int pos = histPos;
                for (int i = 0; i < z1; ++i) { h[(size_t) pos] = src[s1 + i]; if (++pos >= FFT_N) pos = 0; }
                for (int i = 0; i < z2; ++i) { h[(size_t) pos] = src[s2 + i]; if (++pos >= FFT_N) pos = 0; }
            }
            fifo.finishedRead (z1 + z2);
            histPos = (histPos + HOP) % FFT_N;
            histFill = std::min (FFT_N, histFill + HOP);
            if (histFill >= FFT_N / 2) analyse (HOP);
        }
        else wait (4);
    }
}

static double targetAt (int b, double lf, double hf)
{
    const double f = bandCentres()[(size_t) b];
    return lf * (120.0 * 120.0) / (f * f + 120.0 * 120.0) + hf * std::max (0.0, std::log2 (f / 1000.0));
}

void ELPProcessor::analyse (int hopFrames)
{
    const double sr = analysisRate.load();
    if (sr != lastSr)
    {
        const double hz = sr / FFT_N;
        for (int b = 0; b < NB; ++b)
            bins[(size_t) b] = { std::max (1, (int) std::ceil (bandLo (b) / hz)), std::min (FFT_N / 2 - 1, (int) std::ceil (bandHi (b) / hz) - 1) };
        designer.prepare (sr);
        lastSr = sr;
    }
    const double dt = hopFrames / sr;
    const double norm = (double) FFT_N * FFT_N * 0.375 * 0.25;   // full-scale sine -> 1.0

    std::array<double, NB> P[NCH];
    for (int c = 0; c < NCH; ++c)
    {
        const auto& h = hist[c];
        for (int i = 0; i < FFT_N; ++i) fftWork[(size_t) i] = h[(size_t) ((histPos + i) % FFT_N)] * window[(size_t) i];
        std::fill (fftWork.begin() + FFT_N, fftWork.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftWork.data(), true);
        for (int b = 0; b < NB; ++b)
        {
            double s = 0;
            for (int k = bins[(size_t) b].first; k <= bins[(size_t) b].second; ++k) s += (double) fftWork[(size_t) k] * fftWork[(size_t) k];
            P[c][(size_t) b] = s / norm;
        }
    }

    // mic correction files
    std::array<double, NB> cal[3];
    { const juce::ScopedLock l (traceLock); for (int m = 0; m < 3; ++m) cal[m] = micCal[m]; }
    for (int m = 0; m < 3; ++m) for (int b = 0; b < NB; ++b) P[2 + m][(size_t) b] /= dbToPow (cal[m][(size_t) b]);

    auto cLevel = [] (const std::array<double, NB>& pw) { double s = 0; for (int b = 0; b < NB; ++b) s += pw[(size_t) b] * dbToPow (cWeightDb (bandCentres()[(size_t) b])); return powToDb (s); };
    auto zLevel = [] (const std::array<double, NB>& pw) { double s = 0; for (auto v : pw) s += v; return powToDb (s); };

    // slow (1 s) display smoothing
    const double as = 1.0 - std::exp (-dt / 1.0);
    for (int b = 0; b < NB; ++b)
    {
        slowProg[(size_t) b] += as * (P[0][(size_t) b] - slowProg[(size_t) b]);
        for (int m = 0; m < 3; ++m) slowMic[m][(size_t) b] += as * (P[2 + m][(size_t) b] - slowMic[m][(size_t) b]);
    }

    const int micAvail = micChannelsAvailable.load();
    const char* onIds[3] = { "mic1On", "mic2On", "mic3On" };
    const char* offIds[3] = { "mic1Off", "mic2Off", "mic3Off" };
    bool active[3]; double micC[3], micOffs[3];
    double micPowSum = 0; int nActive = 0;
    for (int m = 0; m < 3; ++m)
    {
        micOffs[m] = p (offIds[m]);
        const double raw = cLevel (P[2 + m]);
        active[m] = m < micAvail && p (onIds[m]) > 0.5f && zLevel (slowMic[m]) > -110.0;
        micC[m] = raw + micOffs[m];
        if (active[m]) { micPowSum += dbToPow (micC[m]); ++nActive; }
    }
    const double micAvgC = nActive ? powToDb (micPowSum / nActive) : -200.0;

    // program level and its SPL offset
    const double progFs = cLevel (P[0]);
    const bool auto0 = p ("autoOffset") > 0.5f;
    if (! offsetInit) { progOffsetLearned = p ("progOffset"); offsetInit = true; }
    if (auto0 && nActive && progFs > -60.0)
        progOffsetLearned += (1.0 - std::exp (-dt / 20.0)) * ((micAvgC - progFs) - progOffsetLearned);
    if (! auto0) progOffsetLearned = p ("progOffset");
    const double progOffset = progOffsetLearned;

    const int src = (int) p ("levelSrc");
    double inst = src == 2 ? (double) p ("manualLevel") : (src == 1 && nActive) ? micAvgC : progFs + progOffset;
    const bool silent = progFs < -70.0 && src != 2;
    if (listenSmoothed < -150) listenSmoothed = silent ? (double) p ("refLevel") : inst;
    if (! silent)
    {
        const double tc = inst > listenSmoothed ? (double) p ("attack") : (double) p ("release");
        listenSmoothed += (1.0 - std::exp (-dt / tc)) * (inst - listenSmoothed);
    }

    // loudness compensation
    const double Pr = clampd (p ("refLevel"), 20, 100), Pp = clampd (listenSmoothed, 20, 100), depth = p ("earDepth") / 100.0;
    std::array<double, NB> ear {};
    for (int b = 0; b < NB; ++b) ear[(size_t) b] = depth * ((iso226::phonToSpl (b, Pp) - Pp) - (iso226::phonToSpl (b, Pr) - Pr));

    // live response: mic vs post-EQ reference, long average, gated on reference energy
    const double at = 1.0 - std::exp (-dt / std::max (1.0f, p ("trimTime")));
    std::array<double, NB> live {}; std::array<bool, NB> liveValid {}; bool hasLive = false;
    for (int b = 0; b < NB; ++b)
    {
        double s = 0; int k = 0;
        for (int m = 0; m < 3; ++m)
        {
            if (active[m] && P[1][(size_t) b] > 1e-8)
            {
                liveMic[m][(size_t) b] += at * (P[2 + m][(size_t) b] - liveMic[m][(size_t) b]);
                liveRef[m][(size_t) b] += at * (P[1][(size_t) b] - liveRef[m][(size_t) b]);
                liveW[m][(size_t) b] += at * (1.0 - liveW[m][(size_t) b]);
            }
            if (active[m] && liveW[m][(size_t) b] > 0.5 && liveRef[m][(size_t) b] > 0)
            { s += liveMic[m][(size_t) b] / liveRef[m][(size_t) b]; ++k; }
        }
        if (k) { live[(size_t) b] = powToDb (s / k); liveValid[(size_t) b] = true; }
    }
    { double s = 0; int k = 0; for (int b = 11; b <= 23; ++b) if (liveValid[(size_t) b]) { s += live[(size_t) b]; ++k; }
      if (k >= 8) { hasLive = true; for (auto& v : live) v -= s / k; } }

    // room response = setup traces, optionally trimmed toward the live measurement
    std::array<double, NB> setup; bool hasSetup;
    { const juce::ScopedLock l (traceLock); setup = setupAvg; hasSetup = setupValid; }
    const bool trimOn = p ("liveTrim") > 0.5f;
    const double range = p ("trimRange");
    std::array<double, NB> roomResp {}; std::array<bool, NB> roomValid {};
    for (int b = 0; b < NB; ++b)
    {
        if (hasSetup)
        {
            roomResp[(size_t) b] = setup[(size_t) b] + ((trimOn && hasLive && liveValid[(size_t) b]) ? clampd (live[(size_t) b] - setup[(size_t) b], -range, range) : 0.0);
            roomValid[(size_t) b] = true;
        }
        else if (trimOn && hasLive && liveValid[(size_t) b]) { roomResp[(size_t) b] = live[(size_t) b]; roomValid[(size_t) b] = true; }
    }
    std::array<double, NB> target {};
    { double m = 0; for (int b = 11; b <= 23; ++b) m += targetAt (b, p ("lfLift"), p ("hfTilt")); m /= 13;
      for (int b = 0; b < NB; ++b) target[(size_t) b] = targetAt (b, p ("lfLift"), p ("hfTilt")) - m; }
    std::array<double, NB> room {};
    if (p ("roomOn") > 0.5f)
        for (int b = 1; b <= 29; ++b) if (roomValid[(size_t) b]) room[(size_t) b] = target[(size_t) b] - roomResp[(size_t) b];

    const bool earOn = p ("earOn") > 0.5f;
    std::array<double, NB> total {};
    for (int b = 0; b < NB; ++b)
    {
        const double v = (earOn ? ear[(size_t) b] : 0.0) + room[(size_t) b];
        total[(size_t) b] = bandCentres()[(size_t) b] < 0.45 * sr ? clampd (v, -(double) p ("maxCut"), (double) p ("maxBoost")) : 0.0;
    }
    const auto g = designer.solve (total);
    for (int b = 0; b < NB; ++b) eqTarget[(size_t) b].store ((float) g[(size_t) b]);

    // setup capture: all active mics at once against the post-EQ reference
    double capProg = -1;
    if (captureRequest.exchange (false))
    {
        capturing = true; captureElapsed = 0;
        for (auto& c : capMic) c.fill (0); capRef.fill (0);
        captureAutoPink = p ("pinkOn") < 0.5f;
        if (captureAutoPink) pinkForced = true;
    }
    if (capturing.load())
    {
        captureElapsed += dt;
        const double settle = 1.5, dur = captureSeconds.load();
        if (captureElapsed > settle)
        {
            for (int b = 0; b < NB; ++b) { capRef[(size_t) b] += P[1][(size_t) b]; for (int m = 0; m < 3; ++m) capMic[m][(size_t) b] += P[2 + m][(size_t) b]; }
        }
        capProg = clampd (captureElapsed / (settle + dur), 0, 1);
        if (captureElapsed >= settle + dur)
        {
            capturing = false; capProg = -1;
            if (captureAutoPink) pinkForced = false;
            ++captureCount;
            for (int m = 0; m < 3; ++m)
            {
                if (! active[m]) continue;
                std::array<double, NB> r {}; double s = 0; int k = 0;
                for (int b = 0; b < NB; ++b) { r[(size_t) b] = (capRef[(size_t) b] > 0 && capMic[m][(size_t) b] > 0) ? powToDb (capMic[m][(size_t) b] / capRef[(size_t) b]) : 0.0; if (b >= 11 && b <= 23) { s += r[(size_t) b]; ++k; } }
                for (auto& v : r) v -= s / k;
                addTraceFromCapture (r, m);
            }
        }
    }

    // calibrator request
    const int cr = calibrateRequest.exchange (-1);
    if (cr >= 0 && cr < 3)
    {
        const double rawZ = zLevel (slowMic[cr]);
        if (rawZ > -110)
            if (auto* prm = apvts.getParameter (offIds[cr]))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) clampd (94.0 - rawZ, 80, 150)));
    }

    // loudness readout from mics if available, otherwise predicted from the program
    std::array<double, NB> lb {};
    for (int b = 0; b < NB; ++b)
    {
        double s = 0; int k = 0;
        for (int m = 0; m < 3; ++m) if (active[m]) { s += slowMic[m][(size_t) b] * dbToPow (micOffs[m]); ++k; }
        lb[(size_t) b] = k ? powToDb (s / k) : powToDb (slowProg[(size_t) b]) + progOffset;
    }
    const auto z = zwicker (lb, p ("diffuse") > 0.5f);

    DisplayData d;
    for (int b = 0; b < NB; ++b)
    {
        d.progSpl[(size_t) b] = powToDb (slowProg[(size_t) b]) + progOffset;
        for (int m = 0; m < 3; ++m) d.mic[m][(size_t) b] = powToDb (slowMic[m][(size_t) b]) + micOffs[m];
    }
    d.live = live; d.liveValid = liveValid; d.hasLive = hasLive;
    d.setup = setup; d.hasSetup = hasSetup;
    d.target = target; d.ear = earOn ? ear : std::array<double, NB> {}; d.room = room;
    for (int b = 0; b < NB; ++b) d.total[(size_t) b] = p ("eqBypass") > 0.5f ? 0.0 : total[(size_t) b];
    d.listenLevel = listenSmoothed; d.programLevel = progFs + progOffset; d.progOffset = progOffset;
    for (int m = 0; m < 3; ++m) { d.micLevel[m] = micC[m]; d.micActive[m] = active[m]; d.micRawFs[m] = zLevel (slowMic[m]); }
    d.loudnessSone = z.N; d.loudnessPhon = z.LN;
    d.limiterMains = limGrMain.load(); d.limiterSub = limGrSub.load();
    d.captureProgress = capturing.load() ? capProg : -1;
    d.sampleRate = (int) sr;
    { const juce::SpinLock::ScopedLockType l (displayLock); display = d; }
}

//==============================================================================
void ELPProcessor::recomputeSetupAverage()
{
    // caller holds traceLock
    std::array<double, NB> s {}; int k = 0;
    for (auto& t : traces) if (t.enabled) { for (int b = 0; b < NB; ++b) s[(size_t) b] += dbToPow (t.resp[(size_t) b]); ++k; }
    setupValid = k > 0;
    if (k)
    {
        for (int b = 0; b < NB; ++b) s[(size_t) b] = powToDb (s[(size_t) b] / k);
        double m = 0; for (int b = 11; b <= 23; ++b) m += s[(size_t) b]; m /= 13;
        for (auto& v : s) v -= m;
    }
    setupAvg = s;
    ++traceVersion;
}

void ELPProcessor::addTraceFromCapture (const std::array<double, NB>& resp, int mic)
{
    const juce::ScopedLock l (traceLock);
    Trace t; t.name = "Capture " + juce::String (captureCount) + ", mic " + juce::String (mic + 1);
    t.source = juce::Time::getCurrentTime().toString (false, true, false);
    t.resp = resp;
    traces.push_back (t);
    recomputeSetupAverage();
}
void ELPProcessor::setTraceEnabled (int i, bool on) { const juce::ScopedLock l (traceLock); if (i >= 0 && i < (int) traces.size()) { traces[(size_t) i].enabled = on; recomputeSetupAverage(); } }
void ELPProcessor::removeTrace (int i) { const juce::ScopedLock l (traceLock); if (i >= 0 && i < (int) traces.size()) { traces.erase (traces.begin() + i); recomputeSetupAverage(); } }
void ELPProcessor::clearTraces() { const juce::ScopedLock l (traceLock); traces.clear(); recomputeSetupAverage(); }

bool ELPProcessor::importTrace (const juce::File& f)
{
    std::array<double, NB> r {};
    if (! parseFreqDbText (f.loadFileAsString().toStdString(), r)) return false;
    double m = 0; for (int b = 11; b <= 23; ++b) m += r[(size_t) b]; m /= 13;
    for (auto& v : r) v -= m;
    const juce::ScopedLock l (traceLock);
    Trace t; t.name = f.getFileNameWithoutExtension(); t.source = "Imported"; t.resp = r;
    traces.push_back (t);
    recomputeSetupAverage();
    return true;
}
void ELPProcessor::startCapture (double seconds) { captureSeconds = seconds; captureRequest = true; }
void ELPProcessor::calibrateMic (int m) { calibrateRequest = m; }
bool ELPProcessor::loadMicCal (int m, const juce::File& f)
{
    std::array<double, NB> r {};
    if (! parseFreqDbText (f.loadFileAsString().toStdString(), r)) return false;
    const juce::ScopedLock l (traceLock);
    micCal[m] = r; micCalName[m] = f.getFileName(); ++traceVersion;
    return true;
}
void ELPProcessor::clearMicCal (int m) { const juce::ScopedLock l (traceLock); micCal[m].fill (0.0); micCalName[m] = {}; ++traceVersion; }

//==============================================================================
static juce::String arrToString (const std::array<double, NB>& a)
{
    juce::StringArray s; for (auto v : a) s.add (juce::String (v, 2)); return s.joinIntoString (" ");
}
static std::array<double, NB> stringToArr (const juce::String& str)
{
    std::array<double, NB> a {}; juce::StringArray s; s.addTokens (str, " ", "");
    for (int i = 0; i < NB && i < s.size(); ++i) a[(size_t) i] = s[i].getDoubleValue();
    return a;
}

void ELPProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree tr ("Traces");
    {
        const juce::ScopedLock l (traceLock);
        for (auto& t : traces)
        {
            juce::ValueTree v ("Trace");
            v.setProperty ("name", t.name, nullptr); v.setProperty ("source", t.source, nullptr);
            v.setProperty ("enabled", t.enabled, nullptr); v.setProperty ("resp", arrToString (t.resp), nullptr);
            tr.appendChild (v, nullptr);
        }
        for (int m = 0; m < 3; ++m)
        {
            juce::ValueTree v ("MicCal");
            v.setProperty ("mic", m, nullptr); v.setProperty ("name", micCalName[m], nullptr); v.setProperty ("resp", arrToString (micCal[m]), nullptr);
            tr.appendChild (v, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("Traces"), nullptr);
    state.appendChild (tr, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ELPProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    auto tr = state.getChildWithName ("Traces");
    {
        const juce::ScopedLock l (traceLock);
        traces.clear();
        for (int i = 0; i < tr.getNumChildren(); ++i)
        {
            auto v = tr.getChild (i);
            if (v.hasType ("Trace"))
            {
                Trace t; t.name = v["name"].toString(); t.source = v["source"].toString();
                t.enabled = (bool) v["enabled"]; t.resp = stringToArr (v["resp"].toString());
                traces.push_back (t);
            }
            else if (v.hasType ("MicCal"))
            {
                const int m = (int) v["mic"];
                if (m >= 0 && m < 3) { micCal[m] = stringToArr (v["resp"].toString()); micCalName[m] = v["name"].toString(); }
            }
        }
        recomputeSetupAverage();
    }
    state.removeChild (tr, nullptr);
    apvts.replaceState (state);
    offsetInit = false;
}

juce::AudioProcessorEditor* ELPProcessor::createEditor() { return new ELPEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ELPProcessor(); }
