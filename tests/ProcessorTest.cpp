#include "../Source/PluginProcessor.h"
#include <cstdio>
using namespace elp;
static void setP (ELPProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    ELPProcessor p;
    const double fs = 48000; const int bs = 512;
    juce::AudioProcessor::BusesLayout lay;
    lay.inputBuses.add (juce::AudioChannelSet::stereo()); lay.inputBuses.add (juce::AudioChannelSet::discreteChannels (3));
    lay.outputBuses.add (juce::AudioChannelSet::stereo()); lay.outputBuses.add (juce::AudioChannelSet::mono());
    printf("layout ok: %d\n", (int) p.setBusesLayout (lay));
    p.setRateAndBufferSizeDetails (fs, bs);
    p.prepareToPlay (fs, bs);
    // room: +6 dB at 63 Hz, -4 dB at 4 kHz, 20 ms delay to mics, mic offset 120 => program at -20 dBFS ~ 100 dB
    Biquad room1, room2; room1.k = peaking (fs, 63, 2, 6); room2.k = peaking (fs, 4000, 1.5, -4);
    Delay md; md.prepare (2000); md.setDelay (960);
    std::mt19937 rng (7); std::normal_distribution<float> nd (0, 1);
    Biquad pinkish; pinkish.k = butter2 (fs, 800, false);
    setP (p, "liveTrim", 1); setP (p, "trimTime", 10); setP (p, "trimRange", 6); setP (p, "roomOn", 1);
    setP (p, "earOn", 0); setP (p, "lfLift", 0); setP (p, "hfTilt", 0);
    juce::AudioBuffer<float> buf (5, bs); juce::MidiBuffer midi;
    double maxCpu = 0, sumCpu = 0; int blocks = 0; bool nan = false; float outPeak = 0;
    auto runSeconds = [&] (double sec, float progGain) {
        for (int b = 0; b < (int) (sec * fs / bs); ++b)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float w = nd (rng) * 0.05f; const float x = (float) (0.6 * w + 0.8 * pinkish.process (w) * 3) * progGain;
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            // mic signal = what came out of the processor last block is a better model, but use program pre-processing for simplicity of "room"
            auto t0 = juce::Time::getHighResolutionTicks();
            juce::AudioBuffer<float> in (buf);
            p.processBlock (buf, midi);
            const double el = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / (bs / fs);
            maxCpu = std::max (maxCpu, el); sumCpu += el; ++blocks;
            // mics hear the processed output (mains+sub) through the room, fed back next block
            static std::vector<float> micSig (bs);
            for (int i = 0; i < bs; ++i)
            {
                const float o = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i)) + buf.getSample (2, i);
                if (! std::isfinite (o)) nan = true;
                outPeak = std::max (outPeak, std::abs (o));
                micSig[(size_t) i] = md.process ((float) room2.process (room1.process (o)));
            }
            juce::Thread::sleep (0);
            // stash for next block's mic inputs
            for (int m = 0; m < 3; ++m) for (int i = 0; i < bs; ++i) buf.setSample (2 + m, i, micSig[(size_t) i] * (1.0f + 0.1f * m) + nd (rng) * 1e-4f);
            // keep wall-clock roughly real time so the analysis thread keeps up
            juce::Thread::sleep (2);
        }
    };
    runSeconds (40, 1.0f);
    auto d = p.getDisplay();
    printf("CPU per block: mean %.1f%%  max %.1f%%   NaN: %s  out peak %.2f\n", 100 * sumCpu / blocks, 100 * maxCpu, nan ? "YES" : "no", outPeak);
    printf("live valid: %d   listen %.1f dBC   prog offset %.1f   loudness %.1f sone\n", d.hasLive, d.listenLevel, d.progOffset, d.loudnessSone);
    printf("band   live   total EQ\n");
    for (int b : { 3, 4, 5, 6, 8, 12, 17, 21, 23, 24, 26 }) printf("%6s %6.1f %6.1f\n", bandLabel (b), d.live[b], d.total[b]);
    // capture a setup trace
    setP (p, "liveTrim", 0);
    p.startCapture (5);
    runSeconds (8, 1.0f);
    auto tr = p.getTraces();
    printf("traces after capture: %zu\n", tr.size());
    if (! tr.empty()) { printf("trace '%s': 63 Hz %.1f, 4k %.1f, 1k %.1f\n", tr[0].name.toRawUTF8(), tr[0].resp[5], tr[0].resp[23], tr[0].resp[17]); }
    d = p.getDisplay();
    printf("EQ from setup traces: 63 Hz %.1f  4k %.1f\n", d.total[5], d.total[23]);
    // loudness comp dynamics: quiet passage
    setP (p, "earOn", 1); setP (p, "roomOn", 0); setP (p, "autoOffset", 0); setP (p, "progOffset", 120); setP (p, "refLevel", 95);
    runSeconds (6, 1.0f); d = p.getDisplay(); printf("loud: listen %.1f  ear@63 %.1f\n", d.listenLevel, d.ear[5]);
    runSeconds (10, 0.1f); d = p.getDisplay(); printf("quiet(-20dB): listen %.1f  ear@63 %.1f  @31.5 %.1f  EQ@63 %.1f\n", d.listenLevel, d.ear[5], d.ear[2], d.total[5]);
    p.releaseResources();
    return 0;
}
