///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

// This is a lightweight channel sample source interface

#ifndef SDRBASE_DSP_CHANNELSAMPLESOURCE_H_
#define SDRBASE_DSP_CHANNELSAMPLESOURCE_H_

#include "export.h"
#include "dsptypes.h"

class SDRBASE_API ChannelSampleSource {
public:
	ChannelSampleSource();
	virtual ~ChannelSampleSource();

	virtual void pull(SampleVector::iterator begin, unsigned int nbSamples) = 0; //!< pull nbSamples from the source and write them starting at begin
    virtual void pullOne(Sample& sample) = 0; //!< pull a single sample from the source
	virtual void prefetch(unsigned int nbSamples) = 0; //!< Do operation(s) before pulling nbSamples
};

#endif // SDRBASE_DSP_CHANNELSAMPLESOURCE_H_
