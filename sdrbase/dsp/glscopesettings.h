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

#ifndef SDRBASE_DSP_GLSCOPESETTINGS_H
#define SDRBASE_DSP_GLSCOPESETTINGS_H

#include <vector>

#include <QByteArray>
#include <QColor>

#include "export.h"
#include "dsp/dsptypes.h"
#include "dsp/projector.h"
#include "settings/serializable.h"

class SDRBASE_API GLScopeSettings : public Serializable
{
public:
    enum DisplayMode // TODO: copy of GLScope::DisplayMode => unify
    {
        DisplayXYH,
        DisplayXYV,
        DisplayX,
        DisplayY,
        DisplayPol
    };

    struct TraceData // TODO: copy of ScopeVis::TraceData => unify
    {
        uint32_t m_streamIndex;          //!< I/Q stream index
        Projector::ProjectionType m_projectionType; //!< Complex to real projection type
        float m_amp;                     //!< Amplification factor
        float m_ofs;                     //!< Offset factor
        int m_traceDelay;                //!< Trace delay in number of samples
        int m_traceDelayCoarse;          //!< Coarse delay slider value
        int m_traceDelayFine;            //!< Fine delay slider value
        float m_triggerDisplayLevel;     //!< Displayable trigger display level in -1:+1 scale. Off scale if not displayable.
        QColor m_traceColor;             //!< Trace display color
        float m_traceColorR;             //!< Trace display color - red shortcut
        float m_traceColorG;             //!< Trace display color - green shortcut
        float m_traceColorB;             //!< Trace display color - blue shortcut
        bool m_hasTextOverlay;           //!< True if a text overlay has to be displayed
        QString m_textOverlay;           //!< Text overlay to display
        bool m_viewTrace;                //!< Trace visibility

        TraceData()
        {
            resetToDefaults();
        }

        void setColor(QColor color)
        {
            m_traceColor = color;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            float r,g,b,a;
#else
            qreal r,g,b,a;
#endif
            m_traceColor.getRgbF(&r, &g, &b, &a);
            m_traceColorR = r;
            m_traceColorG = g;
            m_traceColorB = b;
        }

        void resetToDefaults()
        {
            m_streamIndex = 0;
            m_projectionType = Projector::ProjectionReal;
            m_amp = 1.0f;
            m_ofs = 0.0f;
            m_traceDelay = 0;
            m_traceDelayCoarse = 0;
            m_traceDelayFine = 0;
            m_triggerDisplayLevel = 2.0; // Over scale by default (2.0)
            m_traceColor = QColor(255,255,64);
            m_hasTextOverlay = false;
            m_viewTrace = true;
            setColor(m_traceColor);
        }
    };

    struct TriggerData // TODO: copy of ScopeVis::TriggerData => unify
    {
        uint32_t m_streamIndex;          //!< I/Q stream index
        Projector::ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trigger is associated with
        Real m_triggerLevel;             //!< Level in real units
        int  m_triggerLevelCoarse;
        int  m_triggerLevelFine;
        bool m_triggerPositiveEdge;      //!< Trigger on the positive edge (else negative)
        bool m_triggerBothEdges;         //!< Trigger on both edges (else only one)
        uint32_t m_triggerHoldoff;       //!< Trigger holdoff in number of samples
        uint32_t m_triggerDelay;         //!< Delay before the trigger is kicked off in number of samples (trigger delay)
        double m_triggerDelayMult;       //!< Trigger delay as a multiplier of trace length
        int m_triggerDelayCoarse;
        int m_triggerDelayFine;
        uint32_t m_triggerRepeat;        //!< Number of trigger conditions before the final decisive trigger
        QColor m_triggerColor;           //!< Trigger line display color
        float m_triggerColorR;           //!< Trigger line display color - red shortcut
        float m_triggerColorG;           //!< Trigger line display color - green shortcut
        float m_triggerColorB;           //!< Trigger line display color - blue shortcut

        TriggerData()
        {
            resetToDefaults();
        }

        void setColor(QColor color)
        {
            m_triggerColor = color;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            float r,g,b,a;
#else
            qreal r,g,b,a;
#endif
            m_triggerColor.getRgbF(&r, &g, &b, &a);
            m_triggerColorR = r;
            m_triggerColorG = g;
            m_triggerColorB = b;
        }

        void resetToDefaults()
        {
            m_streamIndex = 0;
            m_projectionType = Projector::ProjectionReal;
            m_inputIndex = 0;
            m_triggerLevel = 0.0f;
            m_triggerLevelCoarse = 0;
            m_triggerLevelFine = 0;
            m_triggerPositiveEdge = true;
            m_triggerBothEdges = false;
            m_triggerHoldoff = 1;
            m_triggerDelay = 0;
            m_triggerDelayMult = 0.0;
            m_triggerDelayCoarse = 0;
            m_triggerDelayFine = 0;
            m_triggerRepeat = 0;
            m_triggerColor = QColor(0,255,0);
            setColor(m_triggerColor);
        }
    };

    DisplayMode m_displayMode;
    int m_traceIntensity;
    int m_gridIntensity;
    int m_time;
    int m_timeOfs;
    int m_traceLenMult;
    int m_trigPre;
    std::vector<TraceData> m_tracesData;
    std::vector<TriggerData> m_triggersData;
    static const double AMPS[27];
    static const uint32_t m_traceChunkDefaultSize = 4800;
    static const uint32_t m_maxNbTriggers = 10;
    static const uint32_t m_maxNbTraces = 10;
    static const uint32_t m_nbTraceMemories = 50;
    static const uint32_t m_nbTraceBuffers = 2;

    GLScopeSettings();
    GLScopeSettings(const GLScopeSettings& t);
    virtual ~GLScopeSettings();

    void resetToDefaults();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);
    GLScopeSettings& operator=(const GLScopeSettings& t);
    static int qColorToInt(const QColor& color);
    static QColor intToQColor(int intColor);
};

#endif // SDRBASE_DSP_GLSCOPESETTINGS_H
