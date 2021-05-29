///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB.                                  //
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

#ifndef SDRBASE_DSP_GLSCOPEINTERFACE_H_
#define SDRBASE_DSP_GLSCOPEINTERFACE_H_

#include "dsptypes.h"
#include "physicalunit.h"
#include "glscopesettings.h"

class GLScopeInterface
{
public:
    GLScopeInterface() {}
    virtual ~GLScopeInterface() {}
    virtual void setTraces(std::vector<GLScopeSettings::TraceData>* tracesData, std::vector<float *>* traces) = 0;
    virtual void newTraces(std::vector<float *>* traces, int traceIndex, std::vector<Projector::ProjectionType>* projectionTypes) = 0;
    virtual void setSampleRate(int sampleRate) = 0;
    virtual void setTraceSize(int trceSize, bool emitSignal = false) = 0;
    virtual void setTriggerPre(uint32_t triggerPre, bool emitSignal = false) = 0;
    virtual const QAtomicInt& getProcessingTraceIndex() const = 0;
    virtual void setTimeBase(int timeBase) = 0;
    virtual void setTimeOfsProMill(int timeOfsProMill) = 0;
    virtual void setFocusedTriggerData(GLScopeSettings::TriggerData& triggerData) = 0;
    virtual void setFocusedTraceIndex(uint32_t traceIndex) = 0;
    virtual void setConfigChanged() = 0;
    virtual void updateDisplay() = 0;
};

#endif // SDRBASE_DSP_GLSCOPEINTERFACE_H_
