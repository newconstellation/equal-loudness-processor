#pragma once
// Equal-Loudness Processor — DSP building blocks.
// ISO 226:2003 equal-loudness contours, ISO 532-1 (Zwicker) stationary loudness,
// a 31-band minimum-phase graphic EQ with interaction-corrected band gains,
// Linkwitz-Riley crossover, delay, peak limiter and an exact pink-noise loop.

#include <array>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <random>
#include <string>
#include <cctype>
#include <cstdlib>

namespace elp
{
constexpr int NB = 31;
constexpr double PI = 3.14159265358979323846;

inline const std::array<double, NB>& bandCentres()
{
    static const std::array<double, NB> fc = [] {
        std::array<double, NB> a {};
        for (int i = 0; i < NB; ++i) a[(size_t) i] = 1000.0 * std::pow (10.0, (i - 17) / 10.0);
        return a;
    }();
    return fc;
}
inline const char* bandLabel (int i)
{
    static const char* L[NB] = { "20","25","31.5","40","50","63","80","100","125","160","200","250","315","400","500","630","800",
                                 "1k","1.25k","1.6k","2k","2.5k","3.15k","4k","5k","6.3k","8k","10k","12.5k","16k","20k" };
    return L[i];
}
inline double bandLo (int i) { return bandCentres()[(size_t) i] * std::pow (10.0, -0.05); }
inline double bandHi (int i) { return bandCentres()[(size_t) i] * std::pow (10.0,  0.05); }
inline double clampd (double v, double a, double b) { return std::min (b, std::max (a, v)); }
inline double powToDb (double p) { return p > 1e-20 ? 10.0 * std::log10 (p) : -200.0; }
inline double dbToPow (double d) { return std::pow (10.0, d / 10.0); }

//==============================================================================
// ISO 226:2003
namespace iso226
{
    inline const double AF[29] = {0.532,0.506,0.480,0.455,0.432,0.409,0.387,0.367,0.349,0.330,0.315,0.301,0.288,0.276,0.267,0.259,0.253,0.250,0.246,0.244,0.243,0.243,0.243,0.242,0.242,0.245,0.254,0.271,0.301};
    inline const double LU[29] = {-31.6,-27.2,-23.0,-19.1,-15.9,-13.0,-10.3,-8.1,-6.2,-4.5,-3.1,-2.0,-1.1,-0.4,0.0,0.3,0.5,0.0,-2.7,-4.1,-1.0,1.7,2.5,1.2,-2.1,-7.1,-11.2,-10.7,-3.1};
    inline const double TF[29] = {78.5,68.7,59.5,51.1,44.0,37.5,31.5,26.5,22.1,17.9,14.4,11.4,8.6,6.2,4.4,3.0,2.2,2.4,3.5,1.7,-1.3,-4.2,-6.0,-5.4,-1.5,6.0,12.6,13.9,12.3};

    // SPL (dB) of band i that sounds as loud as P phon. Bands above 12.5 kHz reuse the 12.5 kHz contour.
    inline double phonToSpl (int i, double P)
    {
        const int k = std::min (i, 28);
        const double af = AF[k], Lu = LU[k], Tf = TF[k];
        const double Af = 4.47e-3 * (std::pow (10.0, 0.025 * P) - 1.15) + std::pow (0.4 * std::pow (10.0, (Tf + Lu) / 10.0 - 9.0), af);
        return (10.0 / af) * std::log10 (Af) - Lu + 94.0;
    }
}

inline double cWeightDb (double f)
{
    const double f2 = f * f;
    const double r = (12194.0 * 12194.0 * f2) / ((f2 + 20.6 * 20.6) * (f2 + 12194.0 * 12194.0));
    return 20.0 * std::log10 (r) + 0.06;
}

//==============================================================================
// ISO 532-1 stationary loudness (Zwicker), from 1/3-octave levels 25 Hz..12.5 kHz.
struct ZwickerResult { double N = 0, LN = 0; std::array<double, 240> NS {}; };

inline ZwickerResult zwicker (const std::array<double, NB>& bands31, bool diffuse)
{
    static const double RAP[8] = {45,55,65,71,80,90,100,120};
    static const double DLL[8][11] = {{-32,-24,-16,-10,-5,0,-7,-3,0,-2,0},{-29,-22,-15,-10,-4,0,-7,-2,0,-2,0},{-27,-19,-14,-9,-4,0,-6,-2,0,-2,0},{-25,-17,-12,-9,-3,0,-5,-2,0,-2,0},{-23,-16,-11,-7,-3,0,-4,-1,0,-1,0},{-20,-14,-10,-6,-3,0,-4,-1,0,-1,0},{-18,-12,-9,-6,-2,0,-3,-1,0,-1,0},{-15,-10,-8,-4,-2,0,-3,-1,0,-1,0}};
    static const double LTQ[20] = {30,18,12,8,7,6,5,4,3,3,3,3,3,3,3,3,3,3,3,3};
    static const double A0[20] = {0,0,0,0,0,0,0,0,0,0,-0.5,-1.6,-3.2,-5.4,-5.6,-4,-1.5,2,5,12};
    static const double DDF[20] = {0,0,0.5,0.9,1.2,1.6,2.3,2.8,3,2,0,-1.4,-2,-1.9,-1,0.5,3,4,4.3,4};
    static const double DCB[20] = {-0.25,-0.6,-0.8,-0.8,-0.5,0,0.5,1.1,1.5,1.7,1.8,1.8,1.7,1.6,1.4,1.2,0.8,0.5,0,-0.5};
    static const double ZUP[21] = {0.9,1.8,2.8,3.5,4.4,5.4,6.6,7.9,9.2,10.6,12.3,13.8,15.2,16.7,18.1,19.3,20.6,21.8,22.7,23.6,24};
    static const double RNS[18] = {21.5,18,15.1,11.5,9,6.1,4.4,3.1,2.13,1.36,0.82,0.42,0.30,0.22,0.15,0.10,0.035,0};
    static const double USL[18][8] = {{13,8.2,6.3,5.5,5.5,5.5,5.5,5.5},{9,7.5,6,5.1,4.5,4.5,4.5,4.5},{7.8,6.7,5.6,4.9,4.4,3.9,3.9,3.9},{6.2,5.4,4.6,4.0,3.5,3.2,3.2,3.2},{4.5,3.8,3.6,3.2,2.9,2.7,2.7,2.7},{3.7,3.0,2.8,2.35,2.2,2.2,2.2,2.2},{2.9,2.3,2.1,1.9,1.8,1.7,1.7,1.7},{2.4,1.7,1.5,1.35,1.3,1.3,1.3,1.3},{1.95,1.45,1.3,1.15,1.1,1.1,1.1,1.1},{1.5,1.2,0.94,0.86,0.82,0.82,0.82,0.82},{0.72,0.67,0.64,0.63,0.62,0.62,0.62,0.62},{0.59,0.53,0.51,0.50,0.42,0.42,0.42,0.42},{0.40,0.33,0.26,0.24,0.24,0.22,0.22,0.22},{0.27,0.21,0.20,0.18,0.17,0.17,0.17,0.17},{0.16,0.15,0.14,0.12,0.11,0.11,0.11,0.11},{0.12,0.11,0.10,0.08,0.08,0.08,0.08,0.08},{0.09,0.08,0.07,0.06,0.06,0.06,0.06,0.05},{0.06,0.05,0.03,0.02,0.02,0.02,0.02,0.02}};

    double LT[28];
    for (int i = 0; i < 28; ++i) LT[i] = std::min (bands31[(size_t) i + 1], 120.0);

    double TI[11];
    for (int i = 0; i < 11; ++i)
    {
        const double L = LT[i];
        if (L < -150) { TI[i] = 0; continue; }
        int j = 0;
        while (j < 7 && L > RAP[j] - DLL[j][i]) ++j;
        TI[i] = std::pow (10.0, (L + DLL[j][i]) / 10.0);
    }
    double g[3] = { 0, 0, 0 };
    for (int i = 0; i < 6; ++i) g[0] += TI[i];
    for (int i = 6; i < 9; ++i) g[1] += TI[i];
    for (int i = 9; i < 11; ++i) g[2] += TI[i];

    double NM[21] = {};
    for (int i = 0; i < 20; ++i)
    {
        double LE = i < 3 ? (g[i] > 0 ? 10.0 * std::log10 (g[i]) : -200.0) : LT[i + 8];
        if (LE < -150) continue;
        LE -= A0[i];
        if (diffuse) LE += DDF[i];
        if (LE > LTQ[i])
        {
            LE -= DCB[i];
            const double MP1 = 0.0635 * std::pow (10.0, 0.025 * LTQ[i]);
            const double MP2 = std::pow (0.75 + 0.25 * std::pow (10.0, (LE - LTQ[i]) / 10.0), 0.25) - 1.0;
            NM[i] = std::max (0.0, MP1 * MP2);
        }
    }
    const double k = 0.4 + 0.32 * std::pow (NM[0], 0.2);
    if (k <= 1) NM[0] *= k;

    ZwickerResult r;
    double N = 0, Z1 = 0, N1 = 0, Z = 0.1; int IZ = 0, J = 0;
    for (int i = 0; i < 21; ++i)
    {
        const double zup = ZUP[i] + 0.0001;
        const int IG = i == 0 ? 7 : std::min (i - 1, 7);
        int guard = 0;
        while (Z1 < zup && guard++ < 1000)
        {
            double N2, Z2;
            if (N1 > NM[i])
            {
                N2 = std::max (RNS[J], NM[i]);
                double DZ = (N1 - N2) / USL[J][IG];
                Z2 = Z1 + DZ;
                if (Z2 > zup) { Z2 = zup; DZ = Z2 - Z1; N2 = N1 - DZ * USL[J][IG]; }
                N += DZ * (N1 + N2) / 2;
                for (; Z <= Z2 && IZ < 240; Z += 0.1) r.NS[(size_t) IZ++] = N1 - (Z - Z1) * USL[J][IG];
            }
            else
            {
                if (N1 < NM[i]) { J = 0; while (J < 17 && RNS[J] >= NM[i]) ++J; }
                Z2 = zup; N2 = NM[i];
                N += N2 * (Z2 - Z1);
                for (; Z <= Z2 && IZ < 240; Z += 0.1) r.NS[(size_t) IZ++] = N2;
            }
            if (N2 <= RNS[J]) ++J;
            if (J > 17) J = 17;
            Z1 = Z2; N1 = N2;
        }
    }
    r.N = std::max (0.0, N);
    r.LN = r.N <= 0 ? 0 : r.N < 1 ? 40.0 * std::pow (r.N + 0.0005, 0.35) : 10.0 * std::log2 (r.N) + 40.0;
    return r;
}

//==============================================================================
// RBJ biquad
struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline BiquadCoeffs peaking (double fs, double f0, double Q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * PI * std::min (f0, 0.49 * fs) / fs;
    const double alpha = std::sin (w) / (2.0 * Q), c = std::cos (w);
    const double a0 = 1 + alpha / A;
    return { (1 + alpha * A) / a0, (-2 * c) / a0, (1 - alpha * A) / a0, (-2 * c) / a0, (1 - alpha / A) / a0 };
}
inline BiquadCoeffs butter2 (double fs, double f0, bool highpass)
{
    const double w = 2.0 * PI * f0 / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071067811865476);
    const double a0 = 1 + alpha;
    if (highpass) return { (1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - alpha) / a0 };
    return { (1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0, -2 * c / a0, (1 - alpha) / a0 };
}
inline double magDb (const BiquadCoeffs& k, double fs, double f)
{
    const std::complex<double> z1 = std::polar (1.0, -2.0 * PI * f / fs), z2 = z1 * z1;
    const auto h = (k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2);
    return 20.0 * std::log10 (std::max (1e-12, std::abs (h)));
}

struct Biquad
{
    BiquadCoeffs k; double s1 = 0, s2 = 0;
    inline double process (double x) noexcept
    {
        const double y = k.b0 * x + s1;
        s1 = k.b1 * x - k.a1 * y + s2;
        s2 = k.b2 * x - k.a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0; }
};

//==============================================================================
// 31-band graphic EQ. Band filter gains are solved so the cascade hits the
// requested curve at every band centre (interaction-corrected, cf. Välimäki & Liski 2017).
class GraphicEqDesigner
{
public:
    static constexpr double Q = 2.4;   // wider than a 1/3-octave so adjacent bands blend without ripple

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const auto& fc = bandCentres();
        const double probe = 12.0;
        for (int i = 0; i < NB; ++i)
        {
            const auto k = peaking (fs, fc[(size_t) i], Q, probe);
            for (int j = 0; j < NB; ++j) B[(size_t) j][(size_t) i] = magDb (k, fs, fc[(size_t) j]) / probe;
        }
        valid.fill (true);
        for (int i = 0; i < NB; ++i) valid[(size_t) i] = fc[(size_t) i] < 0.45 * fs;
    }

    // target: desired response (dB) at band centres. Returns per-filter gains.
    std::array<double, NB> solve (const std::array<double, NB>& target) const
    {
        std::array<double, NB> g = lsq (target);
        for (int it = 0; it < 4; ++it)
        {
            const auto r = response (g);
            std::array<double, NB> e {};
            for (int j = 0; j < NB; ++j) e[(size_t) j] = valid[(size_t) j] ? target[(size_t) j] - r[(size_t) j] : 0.0;
            const auto d = lsq (e);
            for (int i = 0; i < NB; ++i) g[(size_t) i] = clampd (g[(size_t) i] + d[(size_t) i], -24.0, 24.0);
        }
        return g;
    }

    std::array<double, NB> response (const std::array<double, NB>& g) const
    {
        const auto& fc = bandCentres();
        std::array<double, NB> r {};
        for (int i = 0; i < NB; ++i)
        {
            if (std::abs (g[(size_t) i]) < 1e-6) continue;
            const auto k = peaking (fs, fc[(size_t) i], Q, g[(size_t) i]);
            for (int j = 0; j < NB; ++j) r[(size_t) j] += magDb (k, fs, fc[(size_t) j]);
        }
        return r;
    }
    double responseAt (const std::array<double, NB>& g, double f) const
    {
        double r = 0; const auto& fc = bandCentres();
        for (int i = 0; i < NB; ++i) if (std::abs (g[(size_t) i]) > 1e-6) r += magDb (peaking (fs, fc[(size_t) i], Q, g[(size_t) i]), fs, f);
        return r;
    }

private:
    // Solve (B^T B + λI) x = B^T t by Gaussian elimination.
    std::array<double, NB> lsq (const std::array<double, NB>& t) const
    {
        double M[NB][NB + 1];
        for (int a = 0; a < NB; ++a)
        {
            if (! valid[(size_t) a])
            {
                for (int b = 0; b <= NB; ++b) M[a][b] = (a == b ? 1.0 : 0.0);
                continue;
            }
            for (int b = 0; b < NB; ++b)
            {
                if (! valid[(size_t) b]) { M[a][b] = 0; continue; }
                double s = 0;
                for (int j = 0; j < NB; ++j) if (valid[(size_t) j]) s += B[(size_t) j][(size_t) a] * B[(size_t) j][(size_t) b];
                M[a][b] = s + (a == b ? 1e-3 : 0.0);
            }
            double s = 0;
            for (int j = 0; j < NB; ++j) if (valid[(size_t) j]) s += B[(size_t) j][(size_t) a] * t[(size_t) j];
            M[a][NB] = s;
        }
        for (int c = 0; c < NB; ++c)
        {
            int p = c;
            for (int r = c + 1; r < NB; ++r) if (std::abs (M[r][c]) > std::abs (M[p][c])) p = r;
            if (p != c) for (int k = 0; k <= NB; ++k) std::swap (M[c][k], M[p][k]);
            const double d = M[c][c];
            if (std::abs (d) < 1e-15) continue;
            for (int r = 0; r < NB; ++r)
            {
                if (r == c) continue;
                const double f = M[r][c] / d;
                if (f == 0) continue;
                for (int k = c; k <= NB; ++k) M[r][k] -= f * M[c][k];
            }
        }
        std::array<double, NB> x {};
        for (int i = 0; i < NB; ++i) x[(size_t) i] = std::abs (M[i][i]) > 1e-15 ? M[i][NB] / M[i][i] : 0.0;
        return x;
    }

    double fs = 48000;
    std::array<std::array<double, NB>, NB> B {};
    std::array<bool, NB> valid {};
};

//==============================================================================
// Simple delay line (whole samples)
struct Delay
{
    std::vector<float> buf; int w = 0, d = 0;
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 1, 0.0f); w = 0; }
    void setDelay (int samples) { d = std::max (0, std::min (samples, (int) buf.size() - 1)); }
    inline float process (float x) noexcept
    {
        buf[(size_t) w] = x;
        int r = w - d; if (r < 0) r += (int) buf.size();
        const float y = buf[(size_t) r];
        if (++w >= (int) buf.size()) w = 0;
        return y;
    }
};

// Peak limiter with 1.5 ms lookahead, linked across the channels it processes.
struct Limiter
{
    std::vector<Delay> dl; double env = 1.0, att = 0, rel = 0; int look = 0;
    void prepare (double fs, int channels)
    {
        look = (int) std::round (0.0015 * fs);
        dl.assign ((size_t) channels, {});
        for (auto& d : dl) { d.prepare (look + 4); d.setDelay (look); }
        att = std::exp (-1.0 / (0.0008 * fs));
        rel = std::exp (-1.0 / (0.120 * fs));
        env = 1.0;
    }
    // Returns current gain reduction in dB (<= 0).
    double process (float* const* ch, int numCh, int n, double ceilingLin, bool enabled) noexcept
    {
        double minG = 1.0;
        for (int i = 0; i < n; ++i)
        {
            double peak = 0;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, (double) std::abs (ch[c][i]));
            const double want = (enabled && peak > ceilingLin) ? ceilingLin / peak : 1.0;
            env = want < env ? att * env + (1 - att) * want : rel * env + (1 - rel) * want;
            const double gNow = std::min (env, want);
            for (int c = 0; c < numCh; ++c)
            {
                double y = dl[(size_t) c].process (ch[c][i]) * gNow;
                if (enabled) y = clampd (y, -ceilingLin, ceilingLin);
                ch[c][i] = (float) y;
            }
            minG = std::min (minG, gNow);
        }
        return 20.0 * std::log10 (minG);
    }
};

// Exact 1/f noise loop built in the frequency domain; −20 dBFS RMS at unity gain.
inline std::vector<float> makePinkLoop (double fs, int log2n = 17)
{
    const int N = 1 << log2n;
    std::vector<std::complex<double>> X ((size_t) N);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<double> ph (0.0, 2.0 * PI);
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * fs / N;
        if (f < 12 || f > 21000) continue;
        X[(size_t) k] = std::polar (1.0 / std::sqrt (f), ph (rng));
        X[(size_t) (N - k)] = std::conj (X[(size_t) k]);
    }
    // inverse FFT (radix-2)
    for (int i = 1, j = 0; i < N; ++i)
    {
        int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) std::swap (X[(size_t) i], X[(size_t) j]);
    }
    for (int len = 2; len <= N; len <<= 1)
    {
        const auto wl = std::polar (1.0, 2.0 * PI / len);
        for (int i = 0; i < N; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k)
            {
                const auto u = X[(size_t) (i + k)], v = X[(size_t) (i + k + len / 2)] * w;
                X[(size_t) (i + k)] = u + v; X[(size_t) (i + k + len / 2)] = u - v; w *= wl;
            }
        }
    }
    double rms = 0; for (auto& c : X) rms += c.real() * c.real(); rms = std::sqrt (rms / N);
    std::vector<float> out ((size_t) N);
    for (int i = 0; i < N; ++i) out[(size_t) i] = (float) (X[(size_t) i].real() * 0.1 / rms);
    return out;
}

// Parse a text trace or mic correction file (frequency, dB per line) into 31 band values.
inline bool parseFreqDbText (const std::string& text, std::array<double, NB>& out)
{
    std::vector<std::pair<double, double>> pts;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t e = text.find ('\n', pos); if (e == std::string::npos) e = text.size();
        std::string line = text.substr (pos, e - pos); pos = e + 1;
        size_t s = line.find_first_not_of (" \t\r");
        if (s == std::string::npos) continue;
        const char c0 = line[s];
        if (! (std::isdigit ((unsigned char) c0) || c0 == '.' || c0 == '-' || c0 == '+')) continue;
        for (auto& ch : line) if (ch == ',' || ch == ';' || ch == '\t') ch = ' ';
        char* end = nullptr;
        const double f = std::strtod (line.c_str() + s, &end);
        if (end == line.c_str() + s) continue;
        char* end2 = nullptr;
        const double d = std::strtod (end, &end2);
        if (end2 == end || ! (f > 0) || ! std::isfinite (d)) continue;
        pts.emplace_back (f, d);
    }
    if (pts.size() < 5) return false;
    std::sort (pts.begin(), pts.end());
    auto at = [&] (double f) {
        if (f <= pts.front().first) return pts.front().second;
        if (f >= pts.back().first) return pts.back().second;
        auto it = std::lower_bound (pts.begin(), pts.end(), std::make_pair (f, -1e300));
        const auto hi = *it, lo = *(it - 1);
        const double t = std::log (f / lo.first) / std::log (hi.first / lo.first);
        return lo.second * (1 - t) + hi.second * t;
    };
    for (int i = 0; i < NB; ++i)
    {
        double p = 0;
        for (int k = 0; k < 8; ++k) p += dbToPow (at (bandLo (i) * std::pow (bandHi (i) / bandLo (i), (k + 0.5) / 8.0)));
        out[(size_t) i] = powToDb (p / 8.0);
    }
    return true;
}
} // namespace elp
