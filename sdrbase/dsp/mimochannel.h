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

#ifndef SDRBASE_MIMOCHANNEL_H
#define SDRBASE_MIMOCHANNEL_H

#include "export.h"
#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "util/message.h"


class SDRBASE_API MIMOChannel {
public:
	MIMOChannel();
	virtual ~MIMOChannel();

	virtual void startSinks() = 0;
	virtual void stopSinks() = 0;
	virtual void startSources() = 0;
	virtual void stopSources() = 0;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex) = 0;
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex) = 0;
	virtual void pushMessage(Message *msg) = 0;
	virtual QString getMIMOName() = 0;
};

#endif // SDRBASE_MIMOCHANNEL_H
