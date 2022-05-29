///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#ifndef SDRBASE_GUI_GLSCOPENG_H_
#define SDRBASE_GUI_GLSCOPENG_H_

#include <QOpenGLWidget>
#include <QPen>
#include <QTimer>
#include <QMutex>
#include <QFont>
#include <QMatrix4x4>
#include <QAtomicInt>

#include "dsp/glscopeinterface.h"
#include "dsp/dsptypes.h"
#include "dsp/scopevis.h"
#include "gui/scaleengine.h"
#include "gui/glshadercolors.h"
#include "gui/glshadersimple.h"
#include "gui/glshadertextured.h"
#include "export.h"
#include "util/bitfieldindex.h"
#include "util/incrementalarray.h"

class QPainter;

class SDRGUI_API GLScope: public QOpenGLWidget, public GLScopeInterface
{
    Q_OBJECT

public:
    enum DisplayMode {
        DisplayXYH,
        DisplayXYV,
        DisplayX,
        DisplayY,
        DisplayPol
    };

    GLScope(QWidget* parent = 0);
    virtual ~GLScope();

    void connectTimer(const QTimer& timer);
    void disconnectTimer();

    virtual void setTraces(std::vector<GLScopeSettings::TraceData>* tracesData, std::vector<float *>* traces);
    virtual void newTraces(std::vector<float *>* traces, int traceIndex, std::vector<Projector::ProjectionType>* projectionTypes);

    int getSampleRate() const { return m_sampleRate; }
    int getTraceSize() const { return m_traceSize; }

    virtual void setTriggerPre(uint32_t triggerPre, bool emitSignal = false); //!< number of samples
    virtual void setTimeOfsProMill(int timeOfsProMill);
    virtual void setSampleRate(int sampleRate);
    virtual void setTimeBase(int timeBase);
    virtual void setFocusedTraceIndex(uint32_t traceIndex);
    void setDisplayMode(DisplayMode displayMode);
    virtual void setTraceSize(int trceSize, bool emitSignal = false);
    virtual void updateDisplay();
    void setDisplayGridIntensity(int intensity);
    void setDisplayTraceIntensity(int intensity);
    virtual void setFocusedTriggerData(GLScopeSettings::TriggerData& triggerData) { m_focusedTriggerData = triggerData; }
    virtual void setConfigChanged() { m_configChanged = true; }
    //void incrementTraceCounter() { m_traceCounter++; }

    bool getDataChanged() const { return m_dataChanged; }
    DisplayMode getDisplayMode() const { return m_displayMode; }
    void setDisplayXYPoints(bool value) { m_displayXYPoints = value; }
    void setDisplayXYPolarGrid(bool value) { m_displayPolGrid = value; }
    virtual const QAtomicInt& getProcessingTraceIndex() const { return m_processingTraceIndex; }
    void setTraceModulo(int modulo) { m_traceModulo = modulo; }

    void setXScaleFreq(bool set) { m_xScaleFreq = set; m_configChanged = true; }
    bool isXScaleFreq() const { return m_xScaleFreq; }
    void setXScaleCenterFrequency(qint64 cf) { m_xScaleCenterFrequency = cf; m_configChanged = true; }
    void setXScaleFrequencySpan(int span) { m_xScaleFrequencySpan = span; m_configChanged = true; }

signals:
    void sampleRateChanged(int);
    void traceSizeChanged(uint32_t);
    void preTriggerChanged(uint32_t); //!< number of samples

private:
    struct ScopeMarker {
        QPointF m_point;
        float m_time;
        float m_value;
        QString m_timeStr;
        QString m_valueStr;
        QString m_timeDeltaStr;
        QString m_valueDeltaStr;
        ScopeMarker() :
            m_point(0, 0),
            m_time(0),
            m_value(0),
            m_timeStr(),
            m_valueStr(),
            m_timeDeltaStr(),
            m_valueDeltaStr()
        {}
        ScopeMarker(
            const QPointF& point,
            float time,
            float value,
            const QString timeStr,
            const QString& valueStr,
            const QString& timeDeltaStr,
            const QString& valueDeltaStr
        ) :
            m_point(point),
            m_time(time),
            m_value(value),
            m_timeStr(timeStr),
            m_valueStr(valueStr),
            m_timeDeltaStr(timeDeltaStr),
            m_valueDeltaStr(valueDeltaStr)
        {}
        ScopeMarker(const ScopeMarker& other) :
            m_point(other.m_point),
            m_time(other.m_time),
            m_timeStr(other.m_timeStr),
            m_valueStr(other.m_valueStr),
            m_timeDeltaStr(other.m_timeDeltaStr),
            m_valueDeltaStr(other.m_valueDeltaStr)
        {}
    };
    QList<ScopeMarker> m_markers1;
    QList<ScopeMarker> m_markers2;

    std::vector<GLScopeSettings::TraceData> *m_tracesData;
    std::vector<float *> *m_traces;
    std::vector<Projector::ProjectionType> *m_projectionTypes;
    QAtomicInt m_processingTraceIndex;
    GLScopeSettings::TriggerData m_focusedTriggerData;
    //int m_traceCounter;
    uint32_t m_bufferIndex;
    DisplayMode m_displayMode;
    bool m_displayPolGrid;
    QTimer m_timer;
    const QTimer *m_masterTimer;
    QMutex m_mutex;
    QAtomicInt m_dataChanged;
    bool m_configChanged;
    int m_sampleRate;
    int m_timeOfsProMill;
    uint32_t m_triggerPre;
    int m_traceSize;
    int m_traceModulo; //!< ineffective if <2
    int m_timeBase;
    int m_timeOffset;
    uint32_t m_focusedTraceIndex;

    // graphics stuff
    QRectF m_glScopeRect1;
    QRectF m_glScopeRect2;
    QMatrix4x4 m_glScopeMatrix1;
    QMatrix4x4 m_glScopeMatrix2;
    QMatrix4x4 m_glLeft1ScaleMatrix;
    QMatrix4x4 m_glRight1ScaleMatrix;
    QMatrix4x4 m_glLeft2ScaleMatrix;
    QMatrix4x4 m_glBot1ScaleMatrix;
    QMatrix4x4 m_glBot2ScaleMatrix;

    QPixmap m_left1ScalePixmap;
    QPixmap m_left2ScalePixmap;
    QPixmap m_bot1ScalePixmap;
    QPixmap m_bot2ScalePixmap;
    QPixmap m_channelOverlayPixmap1;
    QPixmap m_channelOverlayPixmap2;

    int m_displayGridIntensity;
    int m_displayTraceIntensity;
    bool m_displayXYPoints;

    ScaleEngine m_x1Scale; //!< Display #1 X scale. Time scale
    ScaleEngine m_x2Scale; //!< Display #2 X scale. Time scale
    ScaleEngine m_y1Scale; //!< Display #1 Y scale. Always connected to trace #0 (X trace)
    ScaleEngine m_y2Scale; //!< Display #2 Y scale. Connected to highlighted Y trace (#1..n)
    bool m_xScaleFreq;              //!< Force frequency display on time line for correlation modes
    qint64 m_xScaleCenterFrequency; //!< Frequency time line mode center frequency
    int m_xScaleFrequencySpan;      //!< Frequency time line mode frequency span

    QFont m_channelOverlayFont;
    QFont m_textOverlayFont;

    GLShaderSimple m_glShaderSimple;
    GLShaderColors m_glShaderColors;
    GLShaderTextured m_glShaderLeft1Scale;
    GLShaderTextured m_glShaderBottom1Scale;
    GLShaderTextured m_glShaderLeft2Scale;
    GLShaderTextured m_glShaderBottom2Scale;
    GLShaderTextured m_glShaderPowerOverlay;
    GLShaderTextured m_glShaderTextOverlay;

    IncrementalArray<GLfloat> m_q3Polar;
    IncrementalArray<GLfloat> m_q3TickY1;
    IncrementalArray<GLfloat> m_q3TickY2;
    IncrementalArray<GLfloat> m_q3TickX1;
    IncrementalArray<GLfloat> m_q3TickX2;
    IncrementalArray<GLfloat> m_q3Radii;  //!< Polar grid radii
    IncrementalArray<GLfloat> m_q3Circle; //!< Polar grid unit circle
    IncrementalArray<GLfloat> m_q3Colors; //!< Colors for trace rainbow palette

    static const int m_topMargin = 5;
    static const int m_botMargin = 20;
    static const int m_leftMargin = 35;
    static const int m_rightMargin = 5;

    static const GLfloat m_q3RadiiConst[];

    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();
    void drawMarkers();

    void applyConfig();
    void setYScale(ScaleEngine& scale, uint32_t highlightedTraceIndex);
    void setUniqueDisplays();     //!< Arrange displays when X and Y are unique on screen
    void setVerticalDisplays();   //!< Arrange displays when X and Y are stacked vertically
    void setHorizontalDisplays(); //!< Arrange displays when X and Y are stacked horizontally
    void setPolarDisplays();      //!< Arrange displays when X and Y are stacked over on the left and polar display is on the right

    void mousePressEvent(QMouseEvent* event);

    void drawChannelOverlay(      //!< Draws a text overlay
            const QString& text,
            const QColor& color,
            QPixmap& channelOverlayPixmap,
            const QRectF& glScopeRect);
    void drawTextOverlay(      //!< Draws a text overlay
            const QString& text,
            const QColor& color,
            const QFont& font,
            float shiftX,
            float shiftY,
            bool leftHalf,
            bool topHalf,
            const QRectF& glRect);

    static bool isPositiveProjection(Projector::ProjectionType& projectionType)
    {
        return (projectionType == Projector::ProjectionMagLin)
            || (projectionType == Projector::ProjectionMagDB)
            || (projectionType == Projector::ProjectionMagSq);
    }

    void drawRectGrid2();
    void drawPolarGrid2();
    QString displayScaled(float value, char type, int precision);

    static void drawCircle(float cx, float cy, float r, int num_segments, bool dotted, GLfloat *vertices);
    static void setColorPalette(int nbVertices, int modulo, GLfloat *colors);

protected slots:
    void cleanup();
    void tick();

};

#endif /* SDRBASE_GUI_GLSCOPENG_H_ */
