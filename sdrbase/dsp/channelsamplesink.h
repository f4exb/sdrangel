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

#ifndef SDRBASE_DSP_CHANNELSAMPLESINK_H_
#define SDRBASE_DSP_CHANNELSAMPLESINK_H_

#include "export.h"
#include "dsptypes.h"

class Message;

class SDRBASE_API ChannelSampleSink {
public:
	ChannelSampleSink();
	virtual ~ChannelSampleSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end) = 0;
};

#endif // SDRBASE_DSP_CHANNELSAMPLESINK_H_
