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

#include <algorithm>
#include "audiocompressorsnd.h"


AudioCompressorSnd::AudioCompressorSnd()
{
    m_sampleIndex = 0;
    std::fill(m_processedBuffer, m_processedBuffer+AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE, 0.0f);
}

AudioCompressorSnd::~AudioCompressorSnd()
{}

void AudioCompressorSnd::initState()
{
    m_compressorState.sf_advancecomp(
        m_rate,
        m_pregain,
        m_threshold,
        m_knee,
        m_ratio,
        m_attack,
        m_release,
        m_predelay,
        m_releasezone1,
        m_releasezone2,
        m_releasezone3,
        m_releasezone4,
        m_postgain,
        m_wet
    );
}

float AudioCompressorSnd::compress(float sample)
{
    float compressedSample;

    if (m_sampleIndex >= AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE)
    {
        sf_compressor_process(&m_compressorState, AUDIOCOMPRESSORSND_SF_COMPRESSOR_CHUNKSIZE, m_storageBuffer, m_processedBuffer);
        m_sampleIndex = 0;
    }

    compressedSample = m_processedBuffer[m_sampleIndex];
    m_storageBuffer[m_sampleIndex] = sample;
    m_sampleIndex++;

    return compressedSample;
}

// populate the compressor state with advanced parameters
void AudioCompressorSnd::CompressorState::sf_advancecomp(
    // these parameters are the same as the simple version above:
    int rate, float pregain, float threshold, float knee, float ratio, float attack, float release,
    // these are the advanced parameters:
    float predelay,     // seconds, length of the predelay buffer [0 to 1]
    float releasezone1, // release zones should be increasing between 0 and 1, and are a fraction
    float releasezone2, //  of the release time depending on the input dB -- these parameters define
    float releasezone3, //  the adaptive release curve, which is discussed in further detail in the
    float releasezone4, //  demo: adaptive-release-curve.html
    float postgain,     // dB, amount of gain to apply after compression [0 to 100]
    float wet)          // amount to apply the effect [0 completely dry to 1 completely wet]
{
	// setup the predelay buffer
	int delaybufsize = rate * predelay;

    if (delaybufsize < 1)
    {
		delaybufsize = 1;
    }
    else if (delaybufsize > AUDIOCOMPRESSORSND_SF_COMPRESSOR_MAXDELAY)
    {
		delaybufsize = AUDIOCOMPRESSORSND_SF_COMPRESSOR_MAXDELAY;
        std::fill(delaybuf, delaybuf+delaybufsize, 0.0f);
    }

	// useful values
	float linearpregain = db2lin(pregain);
	float linearthreshold = db2lin(threshold);
	float slope = 1.0f / ratio;
	float attacksamples = rate * attack;
	float attacksamplesinv = 1.0f / attacksamples;
	float releasesamples = rate * release;
	float satrelease = 0.0025f; // seconds
	float satreleasesamplesinv = 1.0f / ((float)rate * satrelease);
	float dry = 1.0f - wet;

	// metering values (not used in core algorithm, but used to output a meter if desired)
	float meterfalloff = 0.325f; // seconds
	float meterrelease = 1.0f - expf(-1.0f / ((float)rate * meterfalloff));

	// calculate knee curve parameters
	float k = 5.0f; // initial guess
	float kneedboffset = 0.0f;
	float linearthresholdknee = 0.0f;

	if (knee > 0.0f) // if a knee exists, search for a good k value
    {
		float xknee = db2lin(threshold + knee);
		float mink = 0.1f;
		float maxk = 10000.0f;

		// search by comparing the knee slope at the current k guess, to the ideal slope
		for (int i = 0; i < 15; i++)
        {
			if (kneeslope(xknee, k, linearthreshold) < slope) {
				maxk = k;
            } else {
				mink = k;
            }

			k = sqrtf(mink * maxk);
		}

		kneedboffset = lin2db(kneecurve(xknee, k, linearthreshold));
		linearthresholdknee = db2lin(threshold + knee);
	}

	// calculate a master gain based on what sounds good
	float fulllevel = compcurve(1.0f, k, slope, linearthreshold, linearthresholdknee, threshold, knee, kneedboffset);
	float mastergain = db2lin(postgain) * powf(1.0f / fulllevel, 0.6f);

	// calculate the adaptive release curve parameters
	// solve a,b,c,d in `y = a*x^3 + b*x^2 + c*x + d`
	// interescting points (0, y1), (1, y2), (2, y3), (3, y4)
	float y1 = releasesamples * releasezone1;
	float y2 = releasesamples * releasezone2;
	float y3 = releasesamples * releasezone3;
	float y4 = releasesamples * releasezone4;
	float a = (-y1 + 3.0f * y2 - 3.0f * y3 + y4) / 6.0f;
	float b = y1 - 2.5f * y2 + 2.0f * y3 - 0.5f * y4;
	float c = (-11.0f * y1 + 18.0f * y2 - 9.0f * y3 + 2.0f * y4) / 6.0f;
	float d = y1;

	// save everything
	this->metergain            = 1.0f; // large value overwritten immediately since it's always < 0
	this->meterrelease         = meterrelease;
	this->threshold            = threshold;
	this->knee                 = knee;
	this->wet                  = wet;
	this->linearpregain        = linearpregain;
	this->linearthreshold      = linearthreshold;
	this->slope                = slope;
	this->attacksamplesinv     = attacksamplesinv;
	this->satreleasesamplesinv = satreleasesamplesinv;
	this->dry                  = dry;
	this->k                    = k;
	this->kneedboffset         = kneedboffset;
	this->linearthresholdknee  = linearthresholdknee;
	this->mastergain           = mastergain;
	this->a                    = a;
	this->b                    = b;
	this->c                    = c;
	this->d                    = d;
	this->detectoravg          = 0.0f;
	this->compgain             = 1.0f;
	this->maxcompdiffdb        = -1.0f;
	this->delaybufsize         = delaybufsize;
	this->delaywritepos        = 0;
	this->delayreadpos         = delaybufsize > 1 ? 1 : 0;
}

void AudioCompressorSnd::sf_compressor_process(AudioCompressorSnd::CompressorState *state, int size, float *input,	float *output)
{
	// pull out the state into local variables
	float metergain            = state->metergain;
	float meterrelease         = state->meterrelease;
	float threshold            = state->threshold;
	float knee                 = state->knee;
	float linearpregain        = state->linearpregain;
	float linearthreshold      = state->linearthreshold;
	float slope                = state->slope;
	float attacksamplesinv     = state->attacksamplesinv;
	float satreleasesamplesinv = state->satreleasesamplesinv;
	float wet                  = state->wet;
	float dry                  = state->dry;
	float k                    = state->k;
	float kneedboffset         = state->kneedboffset;
	float linearthresholdknee  = state->linearthresholdknee;
	float mastergain           = state->mastergain;
	float a                    = state->a;
	float b                    = state->b;
	float c                    = state->c;
	float d                    = state->d;
	float detectoravg          = state->detectoravg;
	float compgain             = state->compgain;
	float maxcompdiffdb        = state->maxcompdiffdb;
	int delaybufsize           = state->delaybufsize;
	int delaywritepos          = state->delaywritepos;
	int delayreadpos           = state->delayreadpos;
	float *delaybuf            = state->delaybuf;

	int samplesperchunk = AUDIOCOMPRESSORSND_SF_COMPRESSOR_SPU;
	int chunks = size / samplesperchunk;
	float ang90 = (float)M_PI * 0.5f;
	float ang90inv = 2.0f / (float)M_PI;
	int samplepos = 0;
	float spacingdb = AUDIOCOMPRESSORSND_SF_COMPRESSOR_SPACINGDB;

	for (int ch = 0; ch < chunks; ch++)
    {
		detectoravg = fixf(detectoravg, 1.0f);
		float desiredgain = detectoravg;
		float scaleddesiredgain = asinf(desiredgain) * ang90inv;
		float compdiffdb = lin2db(compgain / scaleddesiredgain);

		// calculate envelope rate based on whether we're attacking or releasing
		float enveloperate;
		if (compdiffdb < 0.0f)
        { // compgain < scaleddesiredgain, so we're releasing
			compdiffdb = fixf(compdiffdb, -1.0f);
			maxcompdiffdb = -1; // reset for a future attack mode
			// apply the adaptive release curve
			// scale compdiffdb between 0-3
			float x = (clampf(compdiffdb, -12.0f, 0.0f) + 12.0f) * 0.25f;
			float releasesamples = adaptivereleasecurve(x, a, b, c, d);
			enveloperate = db2lin(spacingdb / releasesamples);
		}
		else
        { // compresorgain > scaleddesiredgain, so we're attacking
			compdiffdb = fixf(compdiffdb, 1.0f);
			if (maxcompdiffdb == -1 || maxcompdiffdb < compdiffdb)
				maxcompdiffdb = compdiffdb;
			float attenuate = maxcompdiffdb;
			if (attenuate < 0.5f)
				attenuate = 0.5f;
			enveloperate = 1.0f - powf(0.25f / attenuate, attacksamplesinv);
		}

		// process the chunk
		for (int chi = 0; chi < samplesperchunk; chi++, samplepos++,
			delayreadpos = (delayreadpos + 1) % delaybufsize,
			delaywritepos = (delaywritepos + 1) % delaybufsize)
        {

			float inputL = input[samplepos] * linearpregain;
			delaybuf[delaywritepos] = inputL;

			inputL = absf(inputL);
			float inputmax = inputL;

			float attenuation;
			if (inputmax < 0.0001f)
				attenuation = 1.0f;
			else
            {
				float inputcomp = compcurve(inputmax, k, slope, linearthreshold,
					linearthresholdknee, threshold, knee, kneedboffset);
				attenuation = inputcomp / inputmax;
			}

			float rate;
			if (attenuation > detectoravg)
            { // if releasing
				float attenuationdb = -lin2db(attenuation);
				if (attenuationdb < 2.0f)
					attenuationdb = 2.0f;
				float dbpersample = attenuationdb * satreleasesamplesinv;
				rate = db2lin(dbpersample) - 1.0f;
			}
			else
				rate = 1.0f;

			detectoravg += (attenuation - detectoravg) * rate;
			if (detectoravg > 1.0f)
				detectoravg = 1.0f;
			detectoravg = fixf(detectoravg, 1.0f);

			if (enveloperate < 1) // attack, reduce gain
				compgain += (scaleddesiredgain - compgain) * enveloperate;
			else
            { // release, increase gain
				compgain *= enveloperate;
				if (compgain > 1.0f)
					compgain = 1.0f;
			}

			// the final gain value!
			float premixgain = sinf(ang90 * compgain);
			float gain = dry + wet * mastergain * premixgain;

			// calculate metering (not used in core algo, but used to output a meter if desired)
			float premixgaindb = lin2db(premixgain);
			if (premixgaindb < metergain)
				metergain = premixgaindb; // spike immediately
			else
				metergain += (premixgaindb - metergain) * meterrelease; // fall slowly

			// apply the gain
			output[samplepos] = delaybuf[delayreadpos] * gain;
		}
	}

	state->metergain     = metergain;
	state->detectoravg   = detectoravg;
	state->compgain      = compgain;
	state->maxcompdiffdb = maxcompdiffdb;
	state->delaywritepos = delaywritepos;
	state->delayreadpos  = delayreadpos;
}
