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

#ifndef INCLUDE_BEAMSTEERINGCWMODSTREAMSOURCE_H
#define INCLUDE_BEAMSTEERINGCWMODSTREAMSOURCE_H

#include "dsp/channelsamplesource.h"

class BeamSteeringCWModStreamSource : public ChannelSampleSource
{
public:
    BeamSteeringCWModStreamSource();
    virtual ~BeamSteeringCWModStreamSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; } // prefetching of samples is not implemented

    void reset();
    void setPhase(float phase);
    unsigned int getStreamIndex() const { return m_streamIndex; }
    void setStreamIndex(unsigned int streamIndex) { m_streamIndex = streamIndex; }
    void muteChannel(bool mute);

private:
    unsigned int m_streamIndex;
    float m_amp;
    float m_phase;
    FixReal m_real;
    FixReal m_imag;
};

#endif // INCLUDE_BEAMSTEERINGCWMODSTREAMSOURCE_H
