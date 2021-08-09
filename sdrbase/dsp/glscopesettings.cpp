///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "util/simpleserializer.h"
#include "glscopesettings.h"

const double GLScopeSettings::AMPS[27] = {
        2e-1, 1e-1, 5e-2,
        2e-2, 1e-2, 5e-3,
        2e-3, 1e-3, 5e-4,
        2e-4, 1e-4, 5e-5,
        2e-5, 1e-5, 5e-6,
        2e-6, 1e-6, 5e-7,
        2e-7, 1e-7, 5e-8,
        2e-8, 1e-8, 5e-9,
        2e-9, 1e-9, 5e-10,
};

GLScopeSettings::GLScopeSettings()
{
    resetToDefaults();
}

GLScopeSettings::GLScopeSettings(const GLScopeSettings& t)
{
    resetToDefaults();

    for (unsigned int i = 0; i < m_maxNbTraces; i++) {
        m_tracesData[i] = t.m_tracesData[i];
    }

    for (unsigned int i = 0; i < m_maxNbTriggers; i++) {
        m_triggersData[i] = t.m_triggersData[i];
    }

    m_displayMode = t.m_displayMode;
    m_traceIntensity = t.m_traceIntensity;
    m_gridIntensity = t.m_gridIntensity;
    m_time = t.m_time;
    m_timeOfs = t.m_timeOfs;
    m_traceLen = t.m_traceLen;
    m_trigPre = t.m_trigPre;
}

GLScopeSettings::~GLScopeSettings()
{}

void GLScopeSettings::resetToDefaults()
{
    m_displayMode = DisplayX;
    m_traceIntensity = 50;
    m_gridIntensity = 10;
    m_time = 1;
    m_timeOfs = 0;
    m_traceLen = 1;
    m_trigPre = 0;
}

QByteArray GLScopeSettings::serialize() const
{
    SimpleSerializer s(1);

    // first row
    s.writeS32(1, (int) m_displayMode);
    s.writeS32(2, m_traceIntensity);
    s.writeS32(3, m_gridIntensity);
    s.writeS32(4, m_time);
    // s.writeS32(5, m_timeOfs);
    s.writeS32(6, m_traceLen);

    std::vector<TraceData>::const_iterator traceDataIt = m_tracesData.begin();
    unsigned int i = 0;

    for (; traceDataIt != m_tracesData.end(); ++traceDataIt, i++)
    {
        if (20 + 16*i > 200) {
            break;
        }

        s.writeS32(20 + 16*i, (int) traceDataIt->m_projectionType);
        s.writeFloat(21 + 16*i, traceDataIt->m_amp);
        s.writeFloat(22 + 16*i, traceDataIt->m_ofs);
        s.writeS32(24 + 16*i, traceDataIt->m_traceDelayCoarse);
        s.writeS32(25 + 16*i, traceDataIt->m_traceDelayFine);
        s.writeFloat(26 + 16*i, traceDataIt->m_traceColorR);
        s.writeFloat(27 + 16*i, traceDataIt->m_traceColorG);
        s.writeFloat(28 + 16*i, traceDataIt->m_traceColorB);
        s.writeU32(29 + 16*i, traceDataIt->m_streamIndex);
    }

    s.writeU32(10, i);
    s.writeU32(200, m_triggersData.size());
    s.writeS32(201, m_trigPre);
    std::vector<TriggerData>::const_iterator triggerDataIt = m_triggersData.begin();
    i = 0;

    for (; triggerDataIt != m_triggersData.end(); ++triggerDataIt, i++)
    {
        s.writeS32(210 + 16*i, (int) triggerDataIt->m_projectionType);
        s.writeS32(211 + 16*i, triggerDataIt->m_triggerRepeat);
        s.writeBool(212 + 16*i, triggerDataIt->m_triggerPositiveEdge);
        s.writeBool(213 + 16*i, triggerDataIt->m_triggerBothEdges);
        s.writeS32(214 + 16*i, triggerDataIt->m_triggerLevelCoarse);
        s.writeS32(215 + 16*i, triggerDataIt->m_triggerLevelFine);
        s.writeS32(216 + 16*i, triggerDataIt->m_triggerDelayCoarse);
        s.writeS32(217 + 16*i, triggerDataIt->m_triggerDelayFine);
        s.writeFloat(218 + 16*i, triggerDataIt->m_triggerColorR);
        s.writeFloat(219 + 16*i, triggerDataIt->m_triggerColorG);
        s.writeFloat(220 + 16*i, triggerDataIt->m_triggerColorB);
        s.writeU32(221 + 16*i, triggerDataIt->m_triggerHoldoff);
        s.writeU32(222 + 16*i, triggerDataIt->m_streamIndex);
    }

    return s.final();
}

bool GLScopeSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int intValue;
        uint32_t uintValue;
        bool boolValue;

        d.readS32(1, &intValue, (int) DisplayX);
        m_displayMode = (DisplayMode) intValue;
        d.readS32(2, &m_traceIntensity, 50);
        d.readS32(3, &m_gridIntensity, 10);
        d.readS32(4, &m_time, 1);
        // d.readS32(5, &m_timeOfs, 0);
        d.readS32(6, &m_traceLen, 1);
        d.readS32(201, &m_trigPre, 0);

        uint32_t nbTracesSaved;
        d.readU32(10, &nbTracesSaved, 1);
        m_tracesData.clear();
        float r, g, b;

        for (unsigned int iTrace = 0; iTrace < nbTracesSaved; iTrace++)
        {
            if (20 + 16*iTrace > 200) {
                break;
            }

            m_tracesData.push_back(TraceData());

            d.readS32(20 + 16*iTrace, &intValue, 0);
            m_tracesData.back().m_projectionType = (Projector::ProjectionType) intValue;
            d.readFloat(21 + 16*iTrace, &m_tracesData.back().m_amp, 1.0f);
            d.readFloat(22 + 16*iTrace, &m_tracesData.back().m_ofs, 0.0f);
            d.readS32(24 + 16*iTrace, &intValue, 0);
            m_tracesData.back().m_traceDelayCoarse = intValue;
            d.readS32(25 + 16*iTrace, &intValue, 0);
            m_tracesData.back().m_traceDelayFine = intValue;
            d.readFloat(26 + 16*iTrace, &r, 1.0f);
            d.readFloat(27 + 16*iTrace, &g, 1.0f);
            d.readFloat(28 + 16*iTrace, &b, 1.0f);
            m_tracesData.back().m_traceColorR = r;
            m_tracesData.back().m_traceColorG = g;
            m_tracesData.back().m_traceColorB = b;
            m_tracesData.back().m_traceColor.setRedF(r);
            m_tracesData.back().m_traceColor.setGreenF(g);
            m_tracesData.back().m_traceColor.setBlueF(b);
            d.readU32(29 + 16*iTrace, &uintValue, 0);
            m_tracesData.back().m_streamIndex = uintValue;
        }

        uint32_t nbTriggersSaved;
        d.readU32(200, &nbTriggersSaved, 1);
        m_triggersData.clear();

        for (unsigned int iTrigger = 0; iTrigger < nbTriggersSaved; iTrigger++)
        {
            m_triggersData.push_back(TriggerData());

            d.readS32(210 + 16*iTrigger, &intValue, 0);
            m_triggersData.back().m_projectionType = (Projector::ProjectionType) intValue;
            d.readS32(211 + 16*iTrigger, &intValue, 1);
            m_triggersData.back().m_triggerRepeat = intValue;
            d.readBool(212 + 16*iTrigger, &boolValue, true);
            m_triggersData.back().m_triggerPositiveEdge = boolValue;
            d.readBool(213 + 16*iTrigger, &boolValue, false);
            m_triggersData.back().m_triggerBothEdges = boolValue;
            d.readS32(214 + 16*iTrigger, &intValue, 1);
            m_triggersData.back().m_triggerLevelCoarse = intValue;
            d.readS32(215 + 16*iTrigger, &intValue, 1);
            m_triggersData.back().m_triggerLevelFine = intValue;
            d.readS32(216 + 16*iTrigger, &intValue, 1);
            m_triggersData.back().m_triggerDelayCoarse = intValue;
            d.readS32(217 + 16*iTrigger, &intValue, 1);
            m_triggersData.back().m_triggerDelayFine = intValue;
            d.readFloat(218 + 16*iTrigger, &r, 1.0f);
            d.readFloat(219 + 16*iTrigger, &g, 1.0f);
            d.readFloat(220 + 16*iTrigger, &b, 1.0f);
            m_triggersData.back().m_triggerColorR = r;
            m_triggersData.back().m_triggerColorG = g;
            m_triggersData.back().m_triggerColorB = b;
            m_triggersData.back().m_triggerColor.setRedF(r);
            m_triggersData.back().m_triggerColor.setGreenF(g);
            m_triggersData.back().m_triggerColor.setBlueF(b);
            d.readU32(221 + 16*iTrigger, &uintValue, 1);
            m_triggersData.back().m_triggerHoldoff = uintValue;
            d.readU32(222 + 16*iTrigger, &uintValue, 0);
            m_triggersData.back().m_streamIndex = uintValue;
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

GLScopeSettings& GLScopeSettings::operator=(const GLScopeSettings& t)
{
    // Check for self assignment
    if (this != &t)
    {
        for (unsigned int i = 0; i < m_maxNbTraces; i++) {
            m_tracesData[i] = t.m_tracesData[i];
        }

        for (unsigned int i = 0; i < m_maxNbTriggers; i++) {
            m_triggersData[i] = t.m_triggersData[i];
        }

        m_displayMode = t.m_displayMode;
        m_traceIntensity = t.m_traceIntensity;
        m_gridIntensity = t.m_gridIntensity;
        m_time = t.m_time;
        m_timeOfs = t.m_timeOfs;
        m_traceLen = t.m_traceLen;
        m_trigPre = t.m_trigPre;
    }

    return *this;
}
