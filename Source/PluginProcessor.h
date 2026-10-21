#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

// Snapshot of everything the editor draws, copied out of the analysis thread.
struct DisplayData
{
    std::array<double, elp::NB> progSpl {}, mic[3] {}, live {}, setup {}, target {}, ear {}, room {}, total {};
    std::array<bool, elp::NB> liveValid {};
    bool hasLive = false, hasSetup = false;
    double listenLevel = -200, programLevel = -200, micLevel[3] { -200, -200, -200 }, micRawFs[3] { -200, -200, -200 };
    double progOffset = 0, loudnessSone = 0, loudnessPhon = 0, limiterMains = 0, limiterSub = 0;
    bool micActive[3] {};
    double captureProgress = -1;
    int sampleRate = 0;
};

struct Trace
{
    juce::String name, source;
    bool enabled = true;
    std::array<double, elp::NB> resp {};
};

class ELPProcessor : public juce::AudioProcessor, private juce::Thread
{
public:
    ELPProcessor();
    ~ELPProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- called from the editor (message thread)
    DisplayData getDisplay() const { const juce::SpinLock::ScopedLockType l (displayLock); return display; }
    std::vector<Trace> getTraces() const { const juce::ScopedLock l (traceLock); return traces; }
    int getTraceVersion() const { return traceVersion.load(); }
    void setTraceEnabled (int index, bool on);
    void removeTrace (int index);
    void clearTraces();
    bool importTrace (const juce::File&);
    void startCapture (double seconds);
    bool isCapturing() const { return captureRequest.load() || capturing.load(); }
    void calibrateMic (int m);                      // sets mic m so its current level reads 94 dB SPL
    bool loadMicCal (int m, const juce::File&);
    void clearMicCal (int m);
    juce::String getMicCalName (int m) const { const juce::ScopedLock l (traceLock); return micCalName[m]; }

private:
    void run() override;                            // analysis thread
    void analyse (int hopFrames);
    void recomputeSetupAverage();
    void addTraceFromCapture (const std::array<double, elp::NB>& resp, int mic);

    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    // ---- audio thread state
    double fs = 48000;
    std::array<std::atomic<float>, elp::NB> eqTarget {};
    std::array<double, elp::NB> eqCurrent {};
    elp::Biquad eq[2][elp::NB];
    elp::Biquad xHp[2][2], xLp[2];
    elp::Delay dMain[2], dSub;
    elp::Limiter limMain, limSub;
    std::vector<float> pink; int pinkPos = 0; float pinkGainNow = 0.0f, progGainNow = 1.0f;
    float lastXover = -1;
    juce::AudioBuffer<float> scratch;
    std::atomic<float> limGrMain { 0 }, limGrSub { 0 };

    // ---- FIFO from audio thread to analysis: 0 program (pre-EQ mono), 1 reference (post-EQ mono), 2..4 mics
    static constexpr int NCH = 5, FIFO_FRAMES = 1 << 17;
    juce::AbstractFifo fifo { FIFO_FRAMES };
    juce::AudioBuffer<float> fifoBuf { NCH, FIFO_FRAMES };
    std::atomic<int> micChannelsAvailable { 0 };

    // ---- analysis thread state
    static constexpr int FFT_ORDER = 15, FFT_N = 1 << FFT_ORDER, HOP = 2048;
    juce::dsp::FFT fft { FFT_ORDER };
    std::vector<float> hist[NCH], fftWork, window;
    int histPos = 0, histFill = 0;
    std::array<std::pair<int, int>, elp::NB> bins {};
    elp::GraphicEqDesigner designer;
    std::array<double, elp::NB> slowProg {}, slowMic[3] {}, liveMic[3] {}, liveRef[3] {}, liveW[3] {};
    double listenSmoothed = -200, progOffsetLearned = 0;
    bool offsetInit = false;
    std::atomic<bool> captureRequest { false }, capturing { false };
    std::atomic<double> captureSeconds { 10 };
    double captureElapsed = 0; bool captureAutoPink = false;
    std::array<double, elp::NB> capMic[3] {}, capRef {};
    std::atomic<bool> pinkForced { false };
    std::atomic<int> calibrateRequest { -1 };
    std::atomic<double> analysisRate { 48000 };
    double lastSr = 0;

    // ---- shared
    mutable juce::SpinLock displayLock;
    DisplayData display;
    mutable juce::CriticalSection traceLock;
    std::vector<Trace> traces;
    std::array<double, elp::NB> setupAvg {}; bool setupValid = false;
    std::array<double, elp::NB> micCal[3] {};
    juce::String micCalName[3];
    std::atomic<int> traceVersion { 0 };
    int captureCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ELPProcessor)
};
