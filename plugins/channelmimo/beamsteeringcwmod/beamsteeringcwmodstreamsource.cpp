///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <cmath>

#include "dsp/dsptypes.h"
#include "beamsteeringcwmodstreamsource.h"

BeamSteeringCWModStreamSource::BeamSteeringCWModStreamSource() :
    m_amp(SDR_TX_SCALEF/sqrt(2.0f)),
    m_phase(0)
{
    m_real = m_amp;
    m_imag = 0.0f;
}

BeamSteeringCWModStreamSource::~BeamSteeringCWModStreamSource()
{}

void BeamSteeringCWModStreamSource::muteChannel(bool mute)
{
    if (mute)
    {
        m_real = 0;
        m_imag = 0;
    }
    else
    {
        setPhase(m_phase);
    }
}

void BeamSteeringCWModStreamSource::setPhase(float phase)
{
    float normPhase = phase < -M_PI ? -M_PI : phase > M_PI ? M_PI : phase;
    m_real = m_amp * cos(normPhase);
    m_imag = m_amp * sin(normPhase);
    m_phase = phase;
}

void BeamSteeringCWModStreamSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::fill(begin, begin + nbSamples, Sample{m_real, m_imag});
}

void BeamSteeringCWModStreamSource::pullOne(Sample& sample)
{
    sample.setReal(m_real);
    sample.setImag(m_imag);
}

void BeamSteeringCWModStreamSource::reset()
{}
