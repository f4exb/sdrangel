///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef SDRBASE_DSP_GLSPECTRUMINTERFACE_H_
#define SDRBASE_DSP_GLSPECTRUMINTERFACE_H_

#include <vector>
#include <functional>
#include <QDateTime>
#include "dsp/dsptypes.h"

class SpectrumVis;

class GLSpectrumInterface
{
public:
    GLSpectrumInterface() {}
    virtual ~GLSpectrumInterface() {}
    virtual void newSpectrum(const Real* spectrum, int fftSize)
    {
        (void) spectrum;
        (void) fftSize;
    }

    // Called by the SpectrumVis this is attached to: set to null before being destroyed
    virtual void setSpectrumVis(SpectrumVis *spectrumVis) { (void) spectrumVis; }

    //!< One row of the spectrum history
    typedef std::function<void(const Real *spectrum, int fftSize, quint32 sampleRate, qint64 centerFrequency, const QDateTime& dateTime)> HistoryRowCallback;

    //!< Get spectrum history from scroll buffe, that are no older than since
    //!< folded together by maximum so that at most maxRows are delivered over
    //!< the whole period. Returns false when there is no history at all
    virtual bool getSpectrumHistory(const QDateTime& since, int maxRows, const HistoryRowCallback& row)
    {
        (void) since;
        (void) maxRows;
        (void) row;
        return false;
    }

    // Actions on what is displayed. The default does nothing, so a spectrum with no GUI simply
    // ignores them rather than needing every caller to know whether one is attached.
    virtual void spectrumAutoscale() {}
    virtual void spectrumClear() {}
    virtual void spectrumResetMeasurements() {}
    virtual void spectrumGotoMarker(int markerIndex) { (void) markerIndex; }

};

#endif // SDRBASE_DSP_GLSPECTRUMINTERFACE_H_
