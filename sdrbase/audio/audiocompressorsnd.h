///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Audio compressor based on sndfilter by Sean Connelly (@voidqk)                //
// https://github.com/voidqk/sndfilter                                           //
//                                                                               //
// Sample by sample interface to facilitate integration in SDRangel modulators.  //
// Uses mono samples (just floats)                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_AUDIO_AUDIOCOMPRESSORSND_H_
#define SDRBASE_AUDIO_AUDIOCOMPRESSORSND_H_

#include <cmath>

// maximum number of samples in the delay buffer
#define AUDIOCOMPRESSORSND_SF_COMPRESSOR_MAXDELAY   1024

// samples per update; the compressor works by dividing the input chunks into even smaller sizes,
// and performs heavier calculations after each mini-chunk to adjust the final envelope
#define AUDIOCOMPRESSORSND_SF_COMPRESSOR_SPU        32

// not sure what this does exactly, but it is part of the release curve
#define AUDIOCOMPRESSORSND_SF_COMPRESSOR_SPACINGDB  5.0f

// the "chunk" size as defined in original sndfilter library
#define AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE  128

#include "export.h"

class SDRBASE_API AudioCompressorSnd
{
public:
    AudioCompressorSnd();
    ~AudioCompressorSnd();

    void initDefault(int rate)
    {
        m_rate = rate;
        m_pregain = 0.000f;
        m_threshold = -24.000f;
        m_knee = 30.000f;
        m_ratio = 12.000f;
        m_attack = 0.003f;
        m_release = 0.250f;
		m_predelay = 0.006f;
		m_releasezone1 = 0.090f;
		m_releasezone2 = 0.160f;
		m_releasezone3 = 0.420f;
		m_releasezone4 = 0.980f;
		m_postgain = 0.000f;
		m_wet = 1.000f;
        initState();
    }

    void initSimple(
        int rate,        // input sample rate (samples per second)
        float pregain,   // dB, amount to boost the signal before applying compression [0 to 100]
        float threshold, // dB, level where compression kicks in [-100 to 0]
        float knee,      // dB, width of the knee [0 to 40]
        float ratio,     // unitless, amount to inversely scale the output when applying comp [1 to 20]
        float attack,    // seconds, length of the attack phase [0 to 1]
        float release    // seconds, length of the release phase [0 to 1]
    )
    {
        m_rate = rate;
        m_pregain = pregain;
        m_threshold = threshold;
        m_knee = knee;
        m_ratio = ratio;
        m_attack = attack;
        m_release = release;
		m_predelay = 0.006f;
		m_releasezone1 = 0.090f;
		m_releasezone2 = 0.160f;
		m_releasezone3 = 0.420f;
		m_releasezone4 = 0.980f;
		m_postgain = 0.000f;
		m_wet = 1.000f;
        initState();
    }

    void initState();
    float compress(float sample);

    float m_rate;
    float m_pregain;
    float m_threshold;
    float m_knee;
    float m_ratio;
    float m_attack;
    float m_release;
    float m_predelay;
    float m_releasezone1;
    float m_releasezone2;
    float m_releasezone3;
    float m_releasezone4;
    float m_postgain;
    float m_wet;

private:
    static inline float db2lin(float db){ // dB to linear
        return powf(10.0f, 0.05f * db);
    }

    static inline float lin2db(float lin){ // linear to dB
        return 20.0f * log10f(lin);
    }

    // for more information on the knee curve, check out the compressor-curve.html demo + source code
    // included in this repo
    static inline float kneecurve(float x, float k, float linearthreshold){
        return linearthreshold + (1.0f - expf(-k * (x - linearthreshold))) / k;
    }

    static inline float kneeslope(float x, float k, float linearthreshold){
        return k * x / ((k * linearthreshold + 1.0f) * expf(k * (x - linearthreshold)) - 1);
    }

    static inline float compcurve(float x, float k, float slope, float linearthreshold,
        float linearthresholdknee, float threshold, float knee, float kneedboffset){
        if (x < linearthreshold)
            return x;
        if (knee <= 0.0f) // no knee in curve
            return db2lin(threshold + slope * (lin2db(x) - threshold));
        if (x < linearthresholdknee)
            return kneecurve(x, k, linearthreshold);
        return db2lin(kneedboffset + slope * (lin2db(x) - threshold - knee));
    }

    // for more information on the adaptive release curve, check out adaptive-release-curve.html demo +
    // source code included in this repo
    static inline float adaptivereleasecurve(float x, float a, float b, float c, float d){
        // a*x^3 + b*x^2 + c*x + d
        float x2 = x * x;
        return a * x2 * x + b * x2 + c * x + d;
    }

    static inline float clampf(float v, float min, float max){
        return v < min ? min : (v > max ? max : v);
    }

    static inline float absf(float v){
        return v < 0.0f ? -v : v;
    }

    static inline float fixf(float v, float def){
        // fix NaN and infinity values that sneak in... not sure why this is needed, but it is
        if (std::isnan(v) || std::isinf(v))
            return def;
        return v;
    }

    struct CompressorState
    { // sf_compressor_state_st
        // user can read the metergain state variable after processing a chunk to see how much dB the
        // compressor would have liked to compress the sample; the meter values aren't used to shape the
        // sound in any way, only used for output if desired
        float metergain;

        // everything else shouldn't really be mucked with unless you read the algorithm and feel
        // comfortable
        float meterrelease;
        float threshold;
        float knee;
        float linearpregain;
        float linearthreshold;
        float slope;
        float attacksamplesinv;
        float satreleasesamplesinv;
        float wet;
        float dry;
        float k;
        float kneedboffset;
        float linearthresholdknee;
        float mastergain;
        float a; // adaptive release polynomial coefficients
        float b;
        float c;
        float d;
        float detectoravg;
        float compgain;
        float maxcompdiffdb;
        int delaybufsize;
        int delaywritepos;
        int delayreadpos;
        float delaybuf[AUDIOCOMPRESSORSND_SF_COMPRESSOR_MAXDELAY]; // predelay buffer

        // populate the compressor state with advanced parameters
        void sf_advancecomp(
            // these parameters are the same as the simple version above:
            int rate, float pregain, float threshold, float knee, float ratio, float attack, float release,
            // these are the advanced parameters:
            float predelay,     // seconds, length of the predelay buffer [0 to 1]
            float releasezone1, // release zones should be increasing between 0 and 1, and are a fraction
            float releasezone2, //  of the release time depending on the input dB -- these parameters define
            float releasezone3, //  the adaptive release curve, which is discussed in further detail in the
            float releasezone4, //  demo: adaptive-release-curve.html
            float postgain,     // dB, amount of gain to apply after compression [0 to 100]
            float wet           // amount to apply the effect [0 completely dry to 1 completely wet]
        );
    };

    static void sf_compressor_process(CompressorState *state, int size, float *input, float *output);

    CompressorState m_compressorState;
    float m_storageBuffer[AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE];
    float m_processedBuffer[AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE];
    int m_sampleIndex;
};




#endif // SDRBASE_AUDIO_AUDIOCOMPRESSORSND_H_