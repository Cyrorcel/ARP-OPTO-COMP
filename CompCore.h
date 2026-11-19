#pragma once
// Coeur DSP du compresseur (aucune dépendance JUCE : testable en natif).
//
// Chaîne de traitement :
//   entrée -> gain d'entrée -> [FILTRE PASSE-BAS 10 kHz, 12 dB/oct, fixe] -> étage tube
//          -> détecteur (précédé du [FILTRE PASSE-HAUT 50 Hz, 12 dB/oct, fixe] : sidechain uniquement)
//          -> gain computer / lissage opto -> gain appliqué au signal (non filtré par le sidechain)
//   sortie = sec * (1 - mix) + traité * gain * mix
//          -> [EQ DE SORTIE fixe : cloche +1,2 dB à 2300 Hz, Q = 2] (après le compresseur)
#include <algorithm>
#include <cmath>

namespace arp
{
constexpr int    kMaxChannels = 8;
constexpr double kPi = 3.14159265358979323846;

// Filtres fixes : volontairement non réglables par l'utilisateur (signature sonore du compresseur)
constexpr double kInputLowpassHz      = 10000.0;   // 12 dB/oct, avant la compression
constexpr double kSidechainHighpassHz = 50.0;      // 12 dB/oct, sur le détecteur uniquement
constexpr double kOutputEqHz          = 2300.0;    // cloche fixe après la compression
constexpr double kOutputEqGainDb      = 1.2;
constexpr double kOutputEqQ           = 2.0;

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    // Passe-bas Butterworth du 2e ordre (Q = 1/racine(2)) : 12 dB/octave.
    // Conception "adaptée" (M. Vicanek, Matched Second Order Digital Filters) : la courbe suit celle d'un
    // filtre analogique à toutes les fréquences d'échantillonnage. Un bilinéaire classique tomberait bien
    // plus vite que 12 dB/oct au-dessus de 10 kHz à 44,1 ou 48 kHz.
    void setLowpass (double fs, double f, double Q = 0.70710678118654752)
    {
        f = std::min (f, fs * 0.45);
        const double w0 = 2.0 * kPi * f / fs, q = 1.0 / (2.0 * Q);
        const double A1c = -2.0 * std::exp (-q * w0) * std::cos (std::sqrt (1.0 - q * q) * w0);
        const double A2c = std::exp (-2.0 * q * w0);

        const double pA0 = (1.0 + A1c + A2c) * (1.0 + A1c + A2c);
        const double pA1 = (1.0 - A1c + A2c) * (1.0 - A1c + A2c);
        const double pA2 = -4.0 * A2c;
        const double s = std::sin (w0 * 0.5), phi1 = s * s, phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
        const double R1 = (pA0 * phi0 + pA1 * phi1 + pA2 * phi2) * Q * Q;
        const double B0 = pA0, B1 = (R1 - B0 * phi0) / phi1;

        b0 = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
        b1 = std::sqrt (B0) - b0;
        b2 = 0.0;
        a1 = A1c;
        a2 = A2c;
    }

    // Filtre en cloche (RBJ) : gain en dB à la fréquence centrale, largeur donnée par Q
    void setPeaking (double fs, double f, double gainDb, double Q)
    {
        f = std::min (f, fs * 0.45);
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * kPi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * Q);
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;  b1 = (-2.0 * cw) / a0;  b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cw) / a0;        a2 = (1.0 - alpha / A) / a0;
    }

    void setHighpass (double fs, double f, double q = 0.70710678118654752)
    {
        f = std::min (f, fs * 0.45);
        const double w0 = 2.0 * kPi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) * 0.5) / a0;  b1 = -(1.0 + cw) / a0;  b2 = b0;
        a1 = (-2.0 * cw) / a0;         a2 = (1.0 - alpha) / a0;
    }

    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

inline float gainToDb (float g, float floorDb)
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

// Étage "tube" : saturation douce asymétrique (harmoniques paires),
// normalisée pour garder un gain unitaire sur les petits signaux.
inline float tubeStage (float x, float drive)
{
    if (drive < 0.001f)
        return x;

    const float k    = 1.0f + 5.0f * drive;
    const float bias = 0.18f * drive;
    const float t    = std::tanh (bias);
    const float y    = std::tanh (k * x + bias) - t;
    return y / (k * (1.0f - t * t));
}

struct CompParams
{
    float inputDb     = 0.0f;
    float thresholdDb = -12.0f;
    float ratio       = 3.0f;
    float drive       = 0.25f;
    float attackMs    = 10.0f;
    float releaseMs   = 200.0f;
    float makeupDb    = 3.0f;
    float mix         = 1.0f;    // 0..1
};

class CompCore
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            inputLowpass[c].setLowpass (sr, kInputLowpassHz);
            sidechainHighpass[c].setHighpass (sr, kSidechainHighpassHz);
            outputEq[c].setPeaking (sr, kOutputEqHz, kOutputEqGainDb, kOutputEqQ);
        }
        reset();
    }

    void reset()
    {
        grState = 0.0f;
        lastBlockPeak = 0.0f;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            inputLowpass[c].reset();
            sidechainHighpass[c].reset();
            outputEq[c].reset();
        }
    }

    float getGainReductionDb() const { return grState; }
    float getBlockPeak() const       { return lastBlockPeak; }

    void process (float* const* data, int numCh, int numSamples, const CompParams& p)
    {
        numCh = std::min (numCh, kMaxChannels);

        const float inGain  = dbToGain (p.inputDb);
        const float slope   = 1.0f / p.ratio - 1.0f;   // négatif
        const float attCoef = std::exp (-1.0f / (std::max (0.05f, p.attackMs) * 0.001f * (float) sr));
        constexpr float knee = 8.0f;

        float dry[kMaxChannels], wet[kMaxChannels];
        float blockPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                dry[c] = data[c][i];

                // 1. passe-bas fixe 10 kHz avant la compression, puis étage tube
                const float x = (float) inputLowpass[c].process ((double) (dry[c] * inGain));
                wet[c] = tubeStage (x, p.drive);

                // 2. sidechain : passe-haut fixe 50 Hz, le détecteur ne "voit" que ce qui est au-dessus
                const float det = (float) sidechainHighpass[c].process ((double) wet[c]);
                peak = std::max (peak, std::abs (det));
            }

            // détecteur lié stéréo + gain computer à genou doux
            const float lvl  = gainToDb (peak, -100.0f);
            const float over = lvl - p.thresholdDb;
            float target;
            if (2.0f * over < -knee)
                target = 0.0f;
            else if (2.0f * std::abs (over) <= knee)
            {
                const float t = over + knee * 0.5f;
                target = -slope * t * t / (2.0f * knee);
            }
            else
                target = -slope * over;

            // lissage façon opto : le relâchement ralentit quand la compression est profonde
            if (target > grState)
            {
                grState = attCoef * grState + (1.0f - attCoef) * target;
            }
            else
            {
                const float depth   = std::min (1.0f, std::max (0.0f, grState / 12.0f));
                const float tauMs   = p.releaseMs * (0.25f + 0.75f * depth);
                const float relCoef = std::exp (-1.0f / (tauMs * 0.001f * (float) sr));
                grState = relCoef * grState + (1.0f - relCoef) * target;
            }

            const float g = dbToGain (p.makeupDb - grState);

            for (int c = 0; c < numCh; ++c)
            {
                const float mixed = dry[c] * (1.0f - p.mix) + wet[c] * g * p.mix;
                const float out = (float) outputEq[c].process ((double) mixed);   // EQ fixe après le compresseur
                data[c][i] = out;
                blockPeak = std::max (blockPeak, std::abs (out));
            }
        }

        lastBlockPeak = blockPeak;
    }

private:
    double sr = 44100.0;
    float grState = 0.0f;
    float lastBlockPeak = 0.0f;
    Biquad inputLowpass[kMaxChannels];
    Biquad sidechainHighpass[kMaxChannels];
    Biquad outputEq[kMaxChannels];
};

} // namespace arp
