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

#include "interferometersink.h"

InterferometerSink::InterferometerSink(InterferometerCorrelator *correlator) :
    m_processingUnit(false),
    m_correlator(correlator)
{}

InterferometerSink::~InterferometerSink()
{}

void InterferometerSink::start()
{}

void InterferometerSink::stop()
{}

void InterferometerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    (void) begin;
    (void) end;
}

bool InterferometerSink::handleMessage(const Message& cmd)
{
    (void) cmd;
    return false;
}