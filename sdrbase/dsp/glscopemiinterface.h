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
#include "scopesettings.h"
#include "physicalunit.h"

class GLScopeInterface
{
public:
    GLScopeInterface() {}
    virtual ~GLScopeInterface() {}
    virtual void setTracesData(std::vector<ScopeSettings::TraceData>* tracesData) = 0;
    virtual void setTraces(std::vector<std::vector<float>>* traces) = 0;
    virtual void newTraces(int traceIndex, int traceSize) = 0;
    virtual void setTimeScale(float min, float max) = 0; //!< Linear horizontal scales
    virtual void setXScale(Unit::Physical unit, float min, float max) = 0; //!< Set X Scale => X for polar, Y1 for linear
    virtual void setYScale(Unit::Physical unit, float min, float max) = 0; //!< Set Y Scale => Y for polar, Y2 for linear
};

#endif // SDRBASE_DSP_GLSPECTRUMINTERFACE_H_
