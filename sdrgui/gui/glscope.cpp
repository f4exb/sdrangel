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

#include <QtGlobal>
#include <QDebug>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QSurface>
#include <algorithm>

#include "glscope.h"

const GLfloat GLScope::m_q3RadiiConst[] = {
    0.0f,  0.5f,  1.0f,  0.5f,  // 0
    0.0f,  0.75f, 1.0f,  0.25f, // 30
    0.0f,  1.0f,  1.0f,  0.0f,  // 45
    0.25f, 1.0f,  0.75f, 0.0f,  // 60
    0.5f,  1.0f,  0.5f,  0.0f,  // 90
    0.75f, 1.0f,  0.25f, 0.0f,  // 120
    1.0f,  1.0f,  0.0f,  0.0f,  // 135
    1.0f,  0.75f, 0.0f,  0.25f  // 150
};

GLScope::GLScope(QWidget *parent) :
    QOpenGLWidget(parent),
    m_tracesData(nullptr),
    m_traces(nullptr),
    m_projectionTypes(nullptr),
    m_processingTraceIndex(-1),
    m_bufferIndex(0),
    m_displayMode(DisplayX),
    m_displayPolGrid(false),
    m_masterTimer(nullptr),
    m_dataChanged(0),
    m_configChanged(false),
    m_sampleRate(0),
    m_timeOfsProMill(0),
    m_triggerPre(0),
    m_traceSize(0),
    m_traceModulo(0),
    m_timeBase(1),
    m_timeOffset(0),
    m_focusedTraceIndex(0),
    m_displayGridIntensity(10),
    m_displayTraceIntensity(50),
    m_displayXYPoints(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.start(50);

    m_y1Scale.setFont(font());
    m_y1Scale.setOrientation(Qt::Vertical);
    m_y2Scale.setFont(font());
    m_y2Scale.setOrientation(Qt::Vertical);
    m_x1Scale.setFont(font());
    m_x1Scale.setOrientation(Qt::Horizontal);
    m_x2Scale.setFont(font());
    m_x2Scale.setOrientation(Qt::Horizontal);
    m_xScaleFreq = false;
    m_xScaleCenterFrequency = 0;
    m_xScaleFrequencySpan = 48000;

    m_channelOverlayFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_channelOverlayFont.setBold(true);
    m_channelOverlayFont.setPointSize(font().pointSize() + 1);

    m_textOverlayFont = font(); // QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_textOverlayFont.setBold(true);
    // m_textOverlayFont.setPointSize(font().pointSize() - 1);

    m_q3Radii.allocate(4*8);
    std::copy(m_q3RadiiConst, m_q3RadiiConst + 4*8, m_q3Radii.m_array);
    m_q3Circle.allocate(4*96); // 96 segments = 4*24 with 1/24 being 15 degrees
    drawCircle(0.5f, 0.5f, 0.5f, 96, false, m_q3Circle.m_array);
    //m_traceCounter = 0;
}

GLScope::~GLScope()
{
}

void GLScope::setDisplayGridIntensity(int intensity)
{
    m_displayGridIntensity = intensity;

    if (m_displayGridIntensity > 100) {
        m_displayGridIntensity = 100;
    } else if (m_displayGridIntensity < 0) {
        m_displayGridIntensity = 0;
    }

    update();
}

void GLScope::setDisplayTraceIntensity(int intensity)
{
    m_displayTraceIntensity = intensity;

    if (m_displayTraceIntensity > 100) {
        m_displayTraceIntensity = 100;
    } else if (m_displayTraceIntensity < 0) {
        m_displayTraceIntensity = 0;
    }

    update();
}

void GLScope::setTraces(std::vector<GLScopeSettings::TraceData> *tracesData, std::vector<float *> *traces)
{
    m_tracesData = tracesData;
    m_traces = traces;
}

void GLScope::newTraces(std::vector<float *> *traces, int traceIndex, std::vector<Projector::ProjectionType> *projectionTypes)
{
    if (traces->size() > 0)
    {
        if (!m_mutex.tryLock(0)) {
            return;
        }

        if (m_dataChanged.testAndSetOrdered(0, 1))
        {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            m_processingTraceIndex.storeRelaxed(traceIndex);
#else
            m_processingTraceIndex.store(traceIndex);
#endif
            m_traces = &traces[traceIndex];
            m_projectionTypes = projectionTypes;
        }

        m_mutex.unlock();
    }
}

void GLScope::initializeGL()
{
    QOpenGLContext *glCurrentContext = QOpenGLContext::currentContext();
    int majorVersion = 0;
    int minorVersion = 0;

    if (glCurrentContext)
    {
        if (QOpenGLContext::currentContext()->isValid())
        {
            qDebug() << "GLScope::initializeGL: context:"
                << " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
                << " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
                << " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
            majorVersion = (QOpenGLContext::currentContext()->format()).majorVersion();
            minorVersion = (QOpenGLContext::currentContext()->format()).minorVersion();
        }
        else
        {
            qDebug() << "GLScope::initializeGL: current context is invalid";
        }
    }
    else
    {
        qCritical() << "GLScope::initializeGL: no current context";
        return;
    }

    QSurface *surface = glCurrentContext->surface();

    if (surface == 0)
    {
        qCritical() << "GLScope::initializeGL: no surface attached";
        return;
    }
    else
    {
        if (surface->surfaceType() != QSurface::OpenGLSurface)
        {
            qCritical() << "GLScope::initializeGL: surface is not an OpenGLSurface: " << surface->surfaceType()
                << " cannot use an OpenGL context";
            return;
        }
        else
        {
            qDebug() << "GLScope::initializeGL: OpenGL surface:"
                << " class: " << (surface->surfaceClass() == QSurface::Window ? "Window" : "Offscreen");
        }
    }

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->initializeOpenGLFunctions();

    //glDisable(GL_DEPTH_TEST);
    m_glShaderSimple.initializeGL(majorVersion, minorVersion);
    m_glShaderColors.initializeGL(majorVersion, minorVersion);
    m_glShaderLeft1Scale.initializeGL(majorVersion, minorVersion);
    m_glShaderBottom1Scale.initializeGL(majorVersion, minorVersion);
    m_glShaderLeft2Scale.initializeGL(majorVersion, minorVersion);
    m_glShaderBottom2Scale.initializeGL(majorVersion, minorVersion);
    m_glShaderPowerOverlay.initializeGL(majorVersion, minorVersion);
    m_glShaderTextOverlay.initializeGL(majorVersion, minorVersion);
}

void GLScope::resizeGL(int width, int height)
{
    QMutexLocker mutexLocker(&m_mutex);
    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glViewport(0, 0, width, height);
    m_configChanged = true;
}

void GLScope::paintGL()
{
    if (!m_mutex.tryLock(2)) {
        return;
    }

    if (m_configChanged)
    {
        applyConfig();
        m_configChanged = false;
    }

    //    qDebug("GLScope::paintGL: m_traceCounter: %d", m_traceCounter);
    //    m_traceCounter = 0;

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    if ((m_displayMode == DisplayX) || (m_displayMode == DisplayXYV) || (m_displayMode == DisplayXYH)) // display trace #0
    {
        // draw rect around
        {
            GLfloat q3[]{
                1, 1,
                0, 1,
                0, 0,
                1, 0};

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix1, color, q3, 4);
        }

        // paint grid
        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        // Y1 (X trace or trace #0)
        {
            tickList = &m_y1Scale.getTickList();

            //GLfloat q3[4*tickList->count()];
            GLfloat *q3 = m_q3TickY1.m_array;
            int effectiveTicks = 0;

            for (int i = 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if ((tick->major) && (tick->textSize > 0))
                {
                    float y = 1 - (tick->pos / m_y1Scale.getSize());
                    q3[4 * effectiveTicks] = 0;
                    q3[4 * effectiveTicks + 1] = y;
                    q3[4 * effectiveTicks + 2] = 1;
                    q3[4 * effectiveTicks + 3] = y;
                    effectiveTicks++;
                }
            }

            float blue = 1.0f;
            QVector4D color(1.0f, 1.0f, blue, (float)m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2 * effectiveTicks);
        }

        // X1 (time)
        {
            tickList = &m_x1Scale.getTickList();

            //GLfloat q3[4*tickList->count()];
            GLfloat *q3 = m_q3TickX1.m_array;
            int effectiveTicks = 0;

            for (int i = 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if ((tick->major) && (tick->textSize > 0))
                {
                    float x = tick->pos / m_x1Scale.getSize();
                    q3[4 * effectiveTicks] = x;
                    q3[4 * effectiveTicks + 1] = 0;
                    q3[4 * effectiveTicks + 2] = x;
                    q3[4 * effectiveTicks + 3] = 1;
                    effectiveTicks++;
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float)m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2 * effectiveTicks);
        }

        // paint left #1 scale
        {
            GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            m_glShaderLeft1Scale.drawSurface(m_glLeft1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #1 scale
        {
            GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            m_glShaderBottom1Scale.drawSurface(m_glBot1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint trace #1
        if (m_traceSize > 0)
        {
            const float *trace = (*m_traces)[0];
            const GLScopeSettings::TraceData &traceData = (*m_tracesData)[0];

            if (traceData.m_viewTrace)
            {
                int start = (m_timeOfsProMill / 1000.0) * m_traceSize;
                int end = std::min(start + m_traceSize / m_timeBase, m_traceSize);

                if (end - start < 2) {
                    start--;
                }

                float rectX = m_glScopeRect1.x();
                float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
                float rectW = m_glScopeRect1.width() * (float)m_timeBase / (float)(m_traceSize - 1);
                //float rectH = -(m_glScopeRect1.height() / 2.0f) * traceData.m_amp;
                float rectH = -m_glScopeRect1.height() / 2.0f;

                //QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
                QVector4D color(traceData.m_traceColorR, traceData.m_traceColorG, traceData.m_traceColorB, m_displayTraceIntensity / 100.0f);
                QMatrix4x4 mat;
                mat.setToIdentity();
                mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                mat.scale(2.0f * rectW, -2.0f * rectH);
                m_glShaderSimple.drawPolyline(mat, color, (GLfloat *)&trace[2 * start], end - start);

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[]{
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel};

                    float rectX = m_glScopeRect1.x();
                    float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
                    float rectW = m_glScopeRect1.width();
                    float rectH = -m_glScopeRect1.height() / 2.0f;

                    QVector4D color(
                        m_focusedTriggerData.m_triggerColorR,
                        m_focusedTriggerData.m_triggerColorG,
                        m_focusedTriggerData.m_triggerColorB,
                        0.4f);
                    QMatrix4x4 mat;
                    mat.setToIdentity();
                    mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                    mat.scale(2.0f * rectW, -2.0f * rectH);
                    m_glShaderSimple.drawSegments(mat, color, q3, 2);
                } // display trigger

                // Paint overlay if any
                if ((m_focusedTraceIndex == 0) && (traceData.m_hasTextOverlay))
                {
                    drawChannelOverlay(
                        traceData.m_textOverlay,
                        traceData.m_traceColor,
                        m_channelOverlayPixmap1,
                        m_glScopeRect1);
                } // display overlay
            } // displayable trace
        } // trace length > 0
    } // Display X

    if ((m_displayMode == DisplayY) || (m_displayMode == DisplayXYV) || (m_displayMode == DisplayXYH)) // display traces #1..n
    {
        // draw rect around
        {
            GLfloat q3[]{
                1, 1,
                0, 1,
                0, 0,
                1, 0};

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix2, color, q3, 4);
        }

        // paint grid

        drawRectGrid2();

        // paint traces #1..n
        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill / 1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize / m_timeBase, m_traceSize);

            if (end - start < 2) {
                start--;
            }

            for (unsigned int i = 1; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const GLScopeSettings::TraceData &traceData = (*m_tracesData)[i];

                if (!traceData.m_viewTrace) {
                    continue;
                }

                float rectX = m_glScopeRect2.x();
                float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
                float rectW = m_glScopeRect2.width() * (float)m_timeBase / (float)(m_traceSize - 1);
                //float rectH = -(m_glScopeRect1.height() / 2.0f) * traceData.m_amp;
                float rectH = -m_glScopeRect2.height() / 2.0f;

                //QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
                QVector4D color(traceData.m_traceColorR, traceData.m_traceColorG, traceData.m_traceColorB, m_displayTraceIntensity / 100.0f);
                QMatrix4x4 mat;
                mat.setToIdentity();
                mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                mat.scale(2.0f * rectW, -2.0f * rectH);
                m_glShaderSimple.drawPolyline(mat, color, (GLfloat *)&trace[2 * start], end - start);

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[]{
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel};

                    float rectX = m_glScopeRect2.x();
                    float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
                    float rectW = m_glScopeRect2.width();
                    float rectH = -m_glScopeRect2.height() / 2.0f;

                    QVector4D color(
                        m_focusedTriggerData.m_triggerColorR,
                        m_focusedTriggerData.m_triggerColorG,
                        m_focusedTriggerData.m_triggerColorB,
                        0.4f);
                    QMatrix4x4 mat;
                    mat.setToIdentity();
                    mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                    mat.scale(2.0f * rectW, -2.0f * rectH);
                    m_glShaderSimple.drawSegments(mat, color, q3, 2);
                }

                // Paint overlay if any
                if ((i == m_focusedTraceIndex) && (traceData.m_hasTextOverlay))
                {
                    drawChannelOverlay(
                        traceData.m_textOverlay,
                        traceData.m_traceColor,
                        m_channelOverlayPixmap2,
                        m_glScopeRect2);
                }

            } // one trace display
        } // trace length > 0
    } // Display Y

    if (m_displayMode == DisplayPol)
    {
        // paint left display: mixed XY

        // draw rect around
        {
            GLfloat q3[]{
                1, 1,
                0, 1,
                0, 0,
                1, 0};

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix1, color, q3, 4);
        }

        // paint grid
        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        // Horizontal grid Y1
        tickList = &m_y1Scale.getTickList();
        {
            //GLfloat q3[4*tickList->count()];
            GLfloat *q3 = m_q3TickY1.m_array;
            int effectiveTicks = 0;

            for (int i = 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if ((tick->major) && (tick->textSize > 0))
                {
                    float y = 1 - (tick->pos / m_y1Scale.getSize());
                    q3[4 * effectiveTicks] = 0;
                    q3[4 * effectiveTicks + 1] = y;
                    q3[4 * effectiveTicks + 2] = 1;
                    q3[4 * effectiveTicks + 3] = y;
                    effectiveTicks++;
                }
            }

            QVector4D color(1.0f, 1.0f, 0.25f, (float)m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2 * effectiveTicks);
        }

        // Vertical grid X1
        tickList = &m_x1Scale.getTickList();
        {
            //GLfloat q3[4*tickList->count()];
            GLfloat *q3 = m_q3TickX1.m_array;
            int effectiveTicks = 0;

            for (int i = 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if ((tick->major) && (tick->textSize > 0))
                {
                    float x = tick->pos / m_x1Scale.getSize();
                    q3[4 * effectiveTicks] = x;
                    q3[4 * effectiveTicks + 1] = 0;
                    q3[4 * effectiveTicks + 2] = x;
                    q3[4 * effectiveTicks + 3] = 1;
                    effectiveTicks++;
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float)m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2 * effectiveTicks);
        }

        // paint left #1 scale
        {
            GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};

            m_glShaderLeft1Scale.drawSurface(m_glLeft1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #1 scale
        {
            GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};

            m_glShaderBottom1Scale.drawSurface(m_glBot1ScaleMatrix, tex1, vtx1, 4);
        }

        // Paint secondary grid

        // Horizontal secondary grid Y2
        tickList = &m_y2Scale.getTickList();
        {
            //GLfloat q3[4*tickList->count()];
            GLfloat *q3 = m_q3TickY2.m_array;
            int effectiveTicks = 0;

            for (int i = 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if ((tick->major) && (tick->textSize > 0))
                {
                    float y = 1 - (tick->pos / m_y2Scale.getSize());
                    q3[4 * effectiveTicks] = 0;
                    q3[4 * effectiveTicks + 1] = y;
                    q3[4 * effectiveTicks + 2] = 1;
                    q3[4 * effectiveTicks + 3] = y;
                    effectiveTicks++;
                }
            }

            QVector4D color(0.25f, 1.0f, 1.0f, (float)m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2 * effectiveTicks);
        }

        // Paint secondary scale
        {
            GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};
            GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0};

            m_glShaderLeft2Scale.drawSurface(m_glRight1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint all traces
        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill / 1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize / m_timeBase, m_traceSize);

            if (end - start < 2) {
                start--;
            }

            for (unsigned int i = 0; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const GLScopeSettings::TraceData &traceData = (*m_tracesData)[i];

                if (!traceData.m_viewTrace) {
                    continue;
                }

                float rectX = m_glScopeRect1.x();
                float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
                float rectW = m_glScopeRect1.width() * (float)m_timeBase / (float)(m_traceSize - 1);
                //float rectH = -(m_glScopeRect1.height() / 2.0f) * traceData.m_amp;
                float rectH = -m_glScopeRect1.height() / 2.0f;

                //QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
                QVector4D color(traceData.m_traceColorR, traceData.m_traceColorG, traceData.m_traceColorB, m_displayTraceIntensity / 100.0f);
                QMatrix4x4 mat;
                mat.setToIdentity();
                mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                mat.scale(2.0f * rectW, -2.0f * rectH);

                if (i == 1) { // Y1 in rainbow color
                    m_glShaderColors.drawPolyline(mat, (GLfloat *)&trace[2 * start], m_q3Colors.m_array, m_displayTraceIntensity / 100.0f, end - start);
                } else {
                    m_glShaderSimple.drawPolyline(mat, color, (GLfloat *)&trace[2 * start], end - start);
                }

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[]{
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel};

                    float rectX = m_glScopeRect1.x();
                    float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
                    float rectW = m_glScopeRect1.width();
                    float rectH = -m_glScopeRect1.height() / 2.0f;

                    QVector4D color(
                        m_focusedTriggerData.m_triggerColorR,
                        m_focusedTriggerData.m_triggerColorG,
                        m_focusedTriggerData.m_triggerColorB,
                        0.4f);
                    QMatrix4x4 mat;
                    mat.setToIdentity();
                    mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                    mat.scale(2.0f * rectW, -2.0f * rectH);
                    m_glShaderSimple.drawSegments(mat, color, q3, 2);
                }

                // Paint overlay if any
                if ((i == m_focusedTraceIndex) && (traceData.m_hasTextOverlay))
                {
                    drawChannelOverlay(
                        traceData.m_textOverlay,
                        traceData.m_traceColor,
                        m_channelOverlayPixmap1,
                        m_glScopeRect1);
                }
            } // all traces display
        } // trace length > 0

        // paint right display: polar XY

        // draw rect around
        {
            GLfloat q3[]{
                1, 1,
                0, 1,
                0, 0,
                1, 0};

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix2, color, q3, 4);
        }

        // paint grid

        if (m_displayPolGrid) {
            drawPolarGrid2();
        } else {
            drawRectGrid2();
        }

        // paint polar traces

        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill / 1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize / m_timeBase, m_traceSize);

            if (end - start < 2)
                start--;

            //GLfloat q3[2*(end - start)];
            GLfloat *q3 = m_q3Polar.m_array;
            const float *trace0 = (*m_traces)[0];

            // If X is an angle and XY display is in polar grid we will perform polar conversion of traces
            bool polarConversion = m_projectionTypes ?
                (*m_projectionTypes).size() > 0 ?
                    ((*m_projectionTypes)[0] == Projector::ProjectionPhase)
                    || ((*m_projectionTypes)[0] == Projector::ProjectionDOAP)
                    || ((*m_projectionTypes)[0] == Projector::ProjectionDOAN)
                    : false
                : false;
            polarConversion &= m_displayPolGrid;

            if (!polarConversion) { // When there is no polar conversion X values are fixed
                memcpy(q3, &(trace0[2*start + 1]), (2*(end-start) - 1)*sizeof(float)); // copy X values
            } // TODO: with polar conversion X can be converted to fixed sin(theta) and cos(theta)

            for (unsigned int i = 1; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const GLScopeSettings::TraceData &traceData = (*m_tracesData)[i];

                if (!traceData.m_viewTrace) {
                    continue;
                }

                if (polarConversion)
                {
                    bool positiveProjection = m_projectionTypes && (i < m_projectionTypes->size()) ?
                        isPositiveProjection((*m_projectionTypes)[i]) : false;

                    for (int j = start; j < end; j++)
                    {
                        float r;
                        if (positiveProjection) {
                            r = 0.5f*trace[2*j + 1] + 0.5f;
                        } else {
                            r = trace[2*j + 1];
                        }
                        float theta = M_PI*trace0[2*j + 1]; // TODO: fixed X to theta conversion (see above)
                        float x = r*cos(theta);
                        float y = r*sin(theta);
                        q3[2*(j-start)] = x;
                        q3[2*(j-start) + 1] = y;
                    }
                }
                else
                {
                    for (int j = start; j < end; j++)
                    {
                        float y = trace[2*j + 1];
                        q3[2*(j-start) + 1] = y;
                    }
                }

                float rectX = m_glScopeRect2.x() + m_glScopeRect2.width() / 2.0f;
                float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
                float rectW = m_glScopeRect2.width() / 2.0f;
                float rectH = -(m_glScopeRect2.height() / 2.0f);

                QVector4D color(traceData.m_traceColorR, traceData.m_traceColorG, traceData.m_traceColorB, m_displayTraceIntensity / 100.0f);
                QMatrix4x4 mat;
                mat.setToIdentity();
                mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
                mat.scale(2.0f * rectW, -2.0f * rectH);

                if (i == 1) // Y1 in rainbow color
                {
                    if (m_displayXYPoints) {
                        m_glShaderColors.drawPoints(mat, q3, m_q3Colors.m_array, m_displayTraceIntensity / 100.0f, end - start);
                    } else {
                        m_glShaderColors.drawPolyline(mat, q3, m_q3Colors.m_array, m_displayTraceIntensity / 100.0f, end - start);
                    }
                }
                else
                {
                    if (m_displayXYPoints) {
                        m_glShaderSimple.drawPoints(mat, color, q3, end - start);
                    } else {
                        m_glShaderSimple.drawPolyline(mat, color, q3, end - start);
                    }
                }
            } // XY polar display
        } // trace length > 0
    } // XY mixed + polar display

    drawMarkers();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    m_dataChanged.storeRelaxed(0);
    m_processingTraceIndex.storeRelaxed(-1);
#else
    m_dataChanged.store(0);
    m_processingTraceIndex.store(-1);
#endif
    m_mutex.unlock();
}

void GLScope::drawMarkers()
{
    QVector4D markerColor(1.0f, 1.0f, 1.0f, 0.3f);
    QVector4D markerTextColor(1.0f, 1.0f, 1.0f, 0.8f);

    if ((m_markers1.size() > 0) && ((m_displayMode == DisplayX) || (m_displayMode == DisplayXYH) || (m_displayMode == DisplayXYV))) // Draw markers1
    {
        // crosshairs
        for (int i = 0; i < m_markers1.size(); i++)
        {
            GLfloat h[] {
                (float) m_markers1.at(i).m_point.x(), 0,
                (float) m_markers1.at(i).m_point.x(), 1
            };
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, markerColor, h, 2);
            GLfloat v[] {
                0, (float) m_markers1.at(i).m_point.y(),
                1, (float) m_markers1.at(i).m_point.y()
            };
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, markerColor, v, 2);
        }

        // text
        for (int i = 0; i < m_markers1.size(); i++)
        {
            if (i == 0)
            {
                drawTextOverlay(
                    m_markers1.at(i).m_timeStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_markers1.at(i).m_point.x() * m_glScopeRect1.width(),
                    m_glScopeRect1.height(),
                    m_markers1.at(i).m_point.x() < 0.5f,
                    false,
                    m_glScopeRect1);
                drawTextOverlay(
                    m_markers1.at(i).m_valueStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    0,
                    m_markers1.at(i).m_point.y() * m_glScopeRect1.height(),
                    true,
                    m_markers1.at(i).m_point.y() < 0.5f,
                    m_glScopeRect1);
            }
            else
            {
                drawTextOverlay(
                    m_markers1.at(i).m_timeDeltaStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_markers1.at(i).m_point.x() * m_glScopeRect1.width(),
                    0,
                    m_markers1.at(i).m_point.x() < 0.5f,
                    true,
                    m_glScopeRect1);
                drawTextOverlay(
                    m_markers1.at(i).m_valueDeltaStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_glScopeRect1.width(),
                    m_markers1.at(i).m_point.y() * m_glScopeRect1.height(),
                    false,
                    m_markers1.at(i).m_point.y() < 0.5f,
                    m_glScopeRect1);
            }
        }
    }

    if ((m_markers2.size() > 0) && ((m_displayMode == DisplayY) || (m_displayMode == DisplayXYH) || (m_displayMode == DisplayXYV))) // Draw markers2
    {
        // crosshairs
        for (int i = 0; i < m_markers2.size(); i++)
        {
            GLfloat h[] {
                (float) m_markers2.at(i).m_point.x(), 0,
                (float) m_markers2.at(i).m_point.x(), 1
            };
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, markerColor, h, 2);
            GLfloat v[] {
                0, (float) m_markers2.at(i).m_point.y(),
                1, (float) m_markers2.at(i).m_point.y()
            };
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, markerColor, v, 2);
        }

        // text
        for (int i = 0; i < m_markers2.size(); i++)
        {
            if (i == 0)
            {
                drawTextOverlay(
                    m_markers2.at(i).m_timeStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_markers2.at(i).m_point.x() * m_glScopeRect2.width(),
                    m_glScopeRect2.height(),
                    m_markers2.at(i).m_point.x() < 0.5f,
                    false,
                    m_glScopeRect2);
                drawTextOverlay(
                    m_markers2.at(i).m_valueStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    0,
                    m_markers2.at(i).m_point.y() * m_glScopeRect2.height(),
                    true,
                    m_markers2.at(i).m_point.y() < 0.5f,
                    m_glScopeRect2);
            }
            else
            {
                drawTextOverlay(
                    m_markers2.at(i).m_timeDeltaStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_markers2.at(i).m_point.x() * m_glScopeRect2.width(),
                    0,
                    m_markers2.at(i).m_point.x() < 0.5f,
                    true,
                    m_glScopeRect2);
                drawTextOverlay(
                    m_markers2.at(i).m_valueDeltaStr,
                    QColor(255, 255, 255, 192),
                    m_textOverlayFont,
                    m_glScopeRect2.width(),
                    m_markers2.at(i).m_point.y() * m_glScopeRect2.height(),
                    false,
                    m_markers2.at(i).m_point.y() < 0.5f,
                    m_glScopeRect2);
            }
        }
    }
}

void GLScope::setSampleRate(int sampleRate)
{
    m_mutex.lock();
    m_sampleRate = sampleRate;
    m_configChanged = true;
    m_mutex.unlock();
    update();
    emit sampleRateChanged(m_sampleRate);
}

void GLScope::setTimeBase(int timeBase)
{
    m_mutex.lock();
    m_timeBase = timeBase;
    m_configChanged = true;
    m_mutex.unlock();
    update();
}

void GLScope::setTriggerPre(uint32_t triggerPre, bool emitSignal)
{
    m_mutex.lock();
    m_triggerPre = triggerPre;
    m_configChanged = true;
    m_mutex.unlock();
    update();

    if (emitSignal) {
        emit preTriggerChanged(m_triggerPre);
    }
}

void GLScope::setTimeOfsProMill(int timeOfsProMill)
{
    m_mutex.lock();
    m_timeOfsProMill = timeOfsProMill;
    m_configChanged = true;
    m_mutex.unlock();
    update();
}

void GLScope::setFocusedTraceIndex(uint32_t traceIndex)
{
    m_mutex.lock();
    m_focusedTraceIndex = traceIndex;
    m_configChanged = true;
    m_mutex.unlock();
    update();
}

void GLScope::setDisplayMode(DisplayMode displayMode)
{
    m_mutex.lock();
    m_displayMode = displayMode;
    m_configChanged = true;
    m_mutex.unlock();
    update();
}

void GLScope::setTraceSize(int traceSize, bool emitSignal)
{
    m_mutex.lock();
    m_traceSize = traceSize;
    m_q3Colors.allocate(3*traceSize);
    setColorPalette(traceSize, m_traceModulo, m_q3Colors.m_array);
    m_configChanged = true;
    m_mutex.unlock();
    update();

    if (emitSignal) {
        emit traceSizeChanged(m_traceSize);
    }
}

void GLScope::updateDisplay()
{
    m_mutex.lock();
    m_configChanged = true;
    m_mutex.unlock();
    update();
}

void GLScope::applyConfig()
{
    QFontMetrics fm(font());
    //float t_start = ((m_timeOfsProMill / 1000.0) * ((float) m_traceSize / m_sampleRate)) - ((float) m_triggerPre / m_sampleRate);
    float t_start = (((m_timeOfsProMill / 1000.0f) * (float)m_traceSize) / m_sampleRate) - ((float)m_triggerPre / m_sampleRate);
    float t_len = ((float)m_traceSize / m_sampleRate) / (float)m_timeBase;

    // scales

    if (m_xScaleFreq) {
        m_x1Scale.setRange(Unit::Frequency, m_xScaleCenterFrequency - (m_xScaleFrequencySpan/2), m_xScaleCenterFrequency + (m_xScaleFrequencySpan/2));
    } else {
        m_x1Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale
    }

    if (m_displayMode == DisplayPol)
    {
        setYScale(m_x2Scale, 0); // polar scale (X)
    }
    else
    {
        if (m_xScaleFreq) {
            m_x2Scale.setRange(Unit::Frequency, m_xScaleCenterFrequency - (m_xScaleFrequencySpan/2), m_xScaleCenterFrequency + (m_xScaleFrequencySpan/2));
        } else {
            m_x2Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale
        }
    }

    if (m_traces->size() > 0) {
        setYScale(m_y1Scale, 0); // This is always the X trace (trace #0)
    }

    if ((m_traces->size() > 1) && (m_focusedTraceIndex < m_traces->size())) {
        setYScale(m_y2Scale, m_focusedTraceIndex > 0 ? m_focusedTraceIndex : 1); // if Highlighted trace is #0 (X trace) set it to first Y trace (trace #1)
    } else {
        setYScale(m_y2Scale, 0); // Default to the X trace (trace #0) - If there is only one trace it should not get there (Y displays disabled in the UI)
    }

    // display arrangements

    if ((m_displayMode == DisplayX) || (m_displayMode == DisplayY)) { // unique displays
        setUniqueDisplays();
    } else if (m_displayMode == DisplayXYV) { // both displays vertically arranged
        setVerticalDisplays();
    } else if (m_displayMode == DisplayXYH) { // both displays horizontally arranged
        setHorizontalDisplays();
    } else if (m_displayMode == DisplayPol) { // horizontal arrangement: XY stacked on left and polar on right
        setPolarDisplays();
    }

    m_q3TickY1.allocate(4 * m_y1Scale.getTickList().count());
    m_q3TickY2.allocate(4 * m_y2Scale.getTickList().count());
    m_q3TickX1.allocate(4 * m_x1Scale.getTickList().count());
    m_q3TickX2.allocate(4 * m_x2Scale.getTickList().count());

    int start = (m_timeOfsProMill / 1000.0) * m_traceSize;
    int end = std::min(start + m_traceSize / m_timeBase, m_traceSize);

    if (end - start < 2)
        start--;

    m_q3Polar.allocate(2 * (end - start));
}

void GLScope::setUniqueDisplays()
{
    QFontMetrics fm(font());
    int M = fm.horizontalAdvance("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = width() - m_leftMargin - m_rightMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)scopeWidth / (float)width(),
        (float)scopeHeight / (float)height());

    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix1.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot1ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft1ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    // Y displays

    m_glScopeRect2 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)scopeWidth / (float)width(),
        (float)scopeHeight / (float)height());

    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix2.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot2ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft2ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // X horizontal scale

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);

        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale

    { //  X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } // X vertical scale

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale
}

void GLScope::setVerticalDisplays()
{
    QFontMetrics fm(font());
    int M = fm.horizontalAdvance("-");
    int scopeHeight = (height() - m_topMargin) / 2 - m_botMargin;
    int scopeWidth = width() - m_leftMargin - m_rightMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)scopeWidth / (float)width(),
        (float)scopeHeight / (float)height());

    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix1.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot1ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft1ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    // Y display

    m_glScopeRect2 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)(m_botMargin + m_topMargin + scopeHeight) / (float)height(),
        (float)scopeWidth / (float)width(),
        (float)scopeHeight / (float)height());

    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (m_botMargin + m_topMargin + scopeHeight) / (float)height()));
    m_glScopeMatrix2.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + scopeHeight + m_botMargin + 1) / (float)height()));
    m_glBot2ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * (m_topMargin + scopeHeight + m_botMargin) / (float)height()));
    m_glLeft2ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // X horizontal scale (X1)

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);
        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale (X2)

    { //  X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } //  X vertical scale (Y1)

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale (Y2)
}

void GLScope::setHorizontalDisplays()
{
    QFontMetrics fm(font());
    int M = fm.horizontalAdvance("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = (width() - m_rightMargin) / 2 - m_leftMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)scopeWidth / (float)width(),
        (float)scopeHeight / (float)height());
    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix1.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot1ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft1ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    // Y display

    m_glScopeRect2 = QRectF(
        (float)(m_leftMargin + m_leftMargin + ((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2)) / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2) / (float)width(),
        (float)(height() - m_topMargin - m_botMargin) / (float)height());
    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate(
        -1.0f + ((float)2 * (m_leftMargin + m_leftMargin + ((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2)) / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix2.scale(
        (float)2 * ((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2) / (float)width(),
        (float)-2 * (height() - m_topMargin - m_botMargin) / (float)height());

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate(
        -1.0f + ((float)2 * (m_leftMargin + m_leftMargin + scopeWidth) / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot2ScaleMatrix.scale(
        (float)2 * scopeWidth / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate(
        -1.0f + (float)2 * (m_leftMargin + scopeWidth) / (float)width(),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft2ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());

    } // X horizontal scale (X1)

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);
        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale (X2)

    { // X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } // X vertical scale (Y1)

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale (Y2)
}

void GLScope::setPolarDisplays()
{
    QFontMetrics fm(font());
    int M = fm.horizontalAdvance("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = (width() - m_rightMargin) / 2 - m_leftMargin;
    int scopeDim = std::min(scopeWidth, scopeHeight);
    scopeWidth += scopeWidth - scopeDim;

    // Mixed XY display (left)

    m_glScopeRect1 = QRectF(
        (float)m_leftMargin / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)(scopeWidth - m_leftMargin) / (float)width(),
        (float)scopeHeight / (float)height());
    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix1.scale(
        (float)2 * (scopeWidth - m_leftMargin) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate(
        -1.0f + ((float)2 * m_leftMargin / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot1ScaleMatrix.scale(
        (float)2 * (scopeWidth - m_leftMargin) / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate(
        -1.0f,
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft1ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    m_glRight1ScaleMatrix.setToIdentity();
    m_glRight1ScaleMatrix.translate(
        -1.0f + ((float)2 * scopeWidth / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glRight1ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    // Polar XY display (right)

    m_glScopeRect2 = QRectF(
        (float)(m_leftMargin + scopeWidth + m_leftMargin) / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)scopeDim / (float)width(),
        (float)(height() - m_topMargin - m_botMargin) / (float)height());
    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate(
        -1.0f + ((float)2 * (m_leftMargin + scopeWidth + m_leftMargin) / (float)width()),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glScopeMatrix2.scale(
        (float)2 * scopeDim / (float)width(),
        (float)-2 * (height() - m_topMargin - m_botMargin) / (float)height());

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate(
        -1.0f + ((float)2 * (m_leftMargin + m_leftMargin + scopeWidth) / (float)width()),
        1.0f - ((float)2 * (scopeHeight + m_topMargin + 1) / (float)height()));
    m_glBot2ScaleMatrix.scale(
        (float)2 * scopeDim / (float)width(),
        (float)-2 * (m_botMargin - 1) / (float)height());

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate(
        -1.0f + (float)2 * (m_leftMargin + scopeWidth) / (float)width(),
        1.0f - ((float)2 * m_topMargin / (float)height()));
    m_glLeft2ScaleMatrix.scale(
        (float)2 * (m_leftMargin - 1) / (float)width(),
        (float)-2 * scopeHeight / (float)height());

    { // Mixed XY horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // Mixed XY horizontal scale (X1)

    { // Polar XY horizontal scale (X2)
        m_x2Scale.setSize(scopeDim);
        m_bot2ScalePixmap = QPixmap(
            scopeDim,
            m_botMargin - 1);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Polar XY horizontal scale (X2)

    { // Mixed XY vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());

    } // Mixed XY vertical scale (Y1)

    { // Polar XY vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight);

        const ScaleEngine::TickList *tickList;
        const ScaleEngine::Tick *tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0)) {
                painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent() / 2), tick->text);
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Polar XY vertical scale (Y2)
}

void GLScope::setYScale(ScaleEngine &scale, uint32_t highlightedTraceIndex)
{
    GLScopeSettings::TraceData &traceData = (*m_tracesData)[highlightedTraceIndex];
    double amp_range = 2.0 / traceData.m_amp;
    double amp_ofs = traceData.m_ofs;
    double pow_floor = -100.0 + traceData.m_ofs * 100.0;
    double pow_range = 100.0 / traceData.m_amp;

    switch (traceData.m_projectionType)
    {
    case Projector::ProjectionMagDB: // dB scale
        scale.setRange(Unit::Decibel, pow_floor, pow_floor + pow_range);
        break;
    case Projector::ProjectionMagLin:
    case Projector::ProjectionMagSq:
        if (amp_range < 1e-9) {
            scale.setRange(Unit::None, amp_ofs * 1e12, amp_range * 1e12 + amp_ofs * 1e12);
        } else if (amp_range < 1e-6) {
            scale.setRange(Unit::None, amp_ofs * 1e9, amp_range * 1e9 + amp_ofs * 1e9);
        } else if (amp_range < 1e-3) {
            scale.setRange(Unit::None, amp_ofs * 1e6, amp_range * 1e6 + amp_ofs * 1e6);
        } else if (amp_range < 1.0) {
            scale.setRange(Unit::None, amp_ofs * 1e3, amp_range * 1e3 + amp_ofs * 1e3);
        } else if (amp_range < 1e3) {
            scale.setRange(Unit::None, amp_ofs * 1e0, amp_range * 1e0 + amp_ofs * 1e0);
        } else if (amp_range < 1e6) {
            scale.setRange(Unit::None, amp_ofs * 1e3, amp_range * 1e3 + amp_ofs * 1e3);
        } else if (amp_range < 1e9) {
            scale.setRange(Unit::None, amp_ofs * 1e6, amp_range * 1e6 + amp_ofs * 1e6);
        } else {
            scale.setRange(Unit::None, amp_ofs * 1e9, amp_range * 1e9 + amp_ofs * 1e9);
        }
        break;
    case Projector::ProjectionPhase: // Phase or frequency (should be +/- 1)
    case Projector::ProjectionDOAP:
    case Projector::ProjectionDOAN:
    case Projector::ProjectionDPhase:
        // scale.setRange(Unit::None, -1.0 / traceData.m_amp + amp_ofs, 1.0 / traceData.m_amp + amp_ofs);
        // break;
    case Projector::ProjectionReal: // Linear generic
    case Projector::ProjectionImag:
    default:
        if (amp_range < 1e-9) {
            scale.setRange(Unit::None, -amp_range * 5e11  + amp_ofs * 1e12, amp_range * 5e11  + amp_ofs * 1e12);
        } else if (amp_range < 1e-6) {
            scale.setRange(Unit::None, -amp_range * 5e8  + amp_ofs * 1e9, amp_range * 5e8  + amp_ofs * 1e9);
        } else if (amp_range < 1e-3) {
            scale.setRange(Unit::None, -amp_range * 5e5  + amp_ofs * 1e6, amp_range * 5e5  + amp_ofs * 1e6);
        } else if (amp_range < 1.0) {
            scale.setRange(Unit::None, -amp_range * 5e2  + amp_ofs * 1e3, amp_range * 5e2  + amp_ofs * 1e3);
        } else if (amp_range < 1e3) {
            scale.setRange(Unit::None, -amp_range * 5e-1 + amp_ofs * 1e0, amp_range * 5e-1 + amp_ofs * 1e0);
        } else if (amp_range < 1e6) {
            scale.setRange(Unit::None, -amp_range * 5e-4 + amp_ofs * 1e3, amp_range * 5e-4 + amp_ofs * 1e3);
        } else if (amp_range < 1e9) {
            scale.setRange(Unit::None, -amp_range * 5e-7 + amp_ofs * 1e6, amp_range * 5e-7 + amp_ofs * 1e6);
        } else {
            scale.setRange(Unit::None, -amp_range * 5e-10 + amp_ofs * 1e9, amp_range * 5e-10 + amp_ofs * 1e9);
        }
        break;
    }
}

void GLScope::drawChannelOverlay(
    const QString &text,
    const QColor &color,
    QPixmap &channelOverlayPixmap,
    const QRectF &glScopeRect)
{
    if (text.isEmpty()) {
        return;
    }

    QFontMetricsF metrics(m_channelOverlayFont);
    QRectF textRect = metrics.boundingRect(text);
    QRectF overlayRect(0, 0, textRect.width() * 1.05f + 4.0f, textRect.height());
    channelOverlayPixmap = QPixmap(overlayRect.width(), overlayRect.height());
    channelOverlayPixmap.fill(Qt::transparent);
    QPainter painter(&channelOverlayPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    painter.fillRect(overlayRect, QColor(0, 0, 0, 0x80));
    QColor textColor(color);
    textColor.setAlpha(0xC0);
    painter.setPen(textColor);
    painter.setFont(m_channelOverlayFont);
    painter.drawText(QPointF(2.0f, overlayRect.height() - 4.0f), text);
    painter.end();

    m_glShaderPowerOverlay.initTexture(channelOverlayPixmap.toImage());

    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        float shiftX = glScopeRect.width() - ((overlayRect.width() + 4.0f) / width());
        float shiftY = 4.0f / height();
        float rectX = glScopeRect.x() + shiftX;
        float rectY = glScopeRect.y() + shiftY;
        float rectW = overlayRect.width() / (float)width();
        float rectH = overlayRect.height() / (float)height();

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
        mat.scale(2.0f * rectW, -2.0f * rectH);
        m_glShaderPowerOverlay.drawSurface(mat, tex1, vtx1, 4);
    }
}

void GLScope::drawTextOverlay(
    const QString &text,
    const QColor &color,
    const QFont& font,
    float shiftX,
    float shiftY,
    bool leftHalf,
    bool topHalf,
    const QRectF &glRect)
{
    if (text.isEmpty()) {
        return;
    }

    QFontMetricsF metrics(font);
    QRectF textRect = metrics.boundingRect(text);
    QRectF overlayRect(0, 0, textRect.width() * 1.05f + 4.0f, textRect.height());
    QPixmap channelOverlayPixmap = QPixmap(overlayRect.width(), overlayRect.height());
    channelOverlayPixmap.fill(Qt::transparent);
    QPainter painter(&channelOverlayPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    painter.fillRect(overlayRect, QColor(0, 0, 0, 0x80));
    QColor textColor(color);
    textColor.setAlpha(0xC0);
    painter.setPen(textColor);
    painter.setFont(font);
    painter.drawText(QPointF(2.0f, overlayRect.height() - 4.0f), text);
    painter.end();

    m_glShaderTextOverlay.initTexture(channelOverlayPixmap.toImage());

    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        // float shiftX = glRect.width() - ((overlayRect.width() + 4.0f) / width());
        // float shiftY = 4.0f / height();
        float rectX = glRect.x() + shiftX - (leftHalf ? 0 : (overlayRect.width()+1)/width());
        float rectY = glRect.y() + shiftY + (4.0f / height()) - (topHalf ? 0 : (overlayRect.height()+5)/height());
        float rectW = overlayRect.width() / (float) width();
        float rectH = overlayRect.height() / (float) height();

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
        mat.scale(2.0f * rectW, -2.0f * rectH);
        m_glShaderTextOverlay.drawSurface(mat, tex1, vtx1, 4);
    }
}

void GLScope::tick()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (m_dataChanged.loadRelaxed()) {
        update();
    }
#else
    if (m_dataChanged.load()) {
        update();
    }
#endif
}

void GLScope::connectTimer(const QTimer &timer)
{
    qDebug() << "GLScope::connectTimer";
    disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.stop();
    m_masterTimer = &timer;
}

void GLScope::disconnectTimer()
{
    qDebug() << "GLScope::disconnectTimer";

    if (m_masterTimer) {
        disconnect(m_masterTimer, SIGNAL(timeout()), this, SLOT(tick()));
    }

    m_masterTimer = nullptr;
}

void GLScope::cleanup()
{
    //makeCurrent();
    m_glShaderSimple.cleanup();
    m_glShaderColors.cleanup();
    m_glShaderBottom1Scale.cleanup();
    m_glShaderBottom2Scale.cleanup();
    m_glShaderLeft1Scale.cleanup();
    m_glShaderPowerOverlay.cleanup();
    m_glShaderTextOverlay.cleanup();
    //doneCurrent();
}

void GLScope::drawRectGrid2()
{
    const ScaleEngine::TickList *tickList;
    const ScaleEngine::Tick *tick;

    // Horizontal Y2
    tickList = &m_y2Scale.getTickList();
    {
        //GLfloat q3[4*tickList->count()];
        GLfloat *q3 = m_q3TickY2.m_array;
        int effectiveTicks = 0;

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0))
            {
                float y = 1 - (tick->pos / m_y2Scale.getSize());
                q3[4 * effectiveTicks] = 0;
                q3[4 * effectiveTicks + 1] = y;
                q3[4 * effectiveTicks + 2] = 1;
                q3[4 * effectiveTicks + 3] = y;
                effectiveTicks++;
            }
        }

        QVector4D color(1.0f, 1.0f, 1.0f, (float)m_displayGridIntensity / 100.0f);
        m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2 * effectiveTicks);
    }

    // Vertical X2
    tickList = &m_x2Scale.getTickList();
    {
        //GLfloat q3[4*tickList->count()];
        GLfloat *q3 = m_q3TickX2.m_array;
        int effectiveTicks = 0;

        for (int i = 0; i < tickList->count(); i++)
        {
            tick = &(*tickList)[i];

            if ((tick->major) && (tick->textSize > 0))
            {
                float x = tick->pos / m_x2Scale.getSize();
                q3[4 * effectiveTicks] = x;
                q3[4 * effectiveTicks + 1] = 0;
                q3[4 * effectiveTicks + 2] = x;
                q3[4 * effectiveTicks + 3] = 1;
                effectiveTicks++;
            }
        }

        QVector4D color(1.0f, 1.0f, 1.0f, (float)m_displayGridIntensity / 100.0f);
        m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2 * effectiveTicks);
    }

    // paint left #2 scale
    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        m_glShaderLeft2Scale.drawSurface(m_glLeft2ScaleMatrix, tex1, vtx1, 4);
    }

    // paint bottom #2 scale
    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        m_glShaderBottom2Scale.drawSurface(m_glBot2ScaleMatrix, tex1, vtx1, 4);
    }
}

void GLScope::drawPolarGrid2()
{
    QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
    m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, m_q3Radii.m_array, 2*8);  // Radii
    m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, m_q3Circle.m_array, 2*96); // Unit circle
}

// inspired by http://slabode.exofire.net/circle_draw.shtml
void GLScope::drawCircle(float cx, float cy, float r, int num_segments, bool dotted, GLfloat *vertices)
{
	float theta = 2*M_PI / float(num_segments);
	float tangential_factor = tanf(theta); //calculate the tangential factor
	float radial_factor = cosf(theta);     //calculate the radial factor

	float x = r; //we start at angle = 0
	float y = 0;

	for (int ii = 0; ii < num_segments; ii++)
	{
		//output vertex
        if (dotted)
        {
            vertices[2*ii]   = x + cx;
            vertices[2*ii+1] = y + cy;
        }
        else
        {
            vertices[4*ii]   = x + cx;
            vertices[4*ii+1] = y + cy;
        }

		// calculate the tangential vector
		// remember, the radial vector is (x, y)
		// to get the tangential vector we flip those coordinates and negate one of them
		float tx = -y;
		float ty = x;

		//add the tangential vector
		x += tx * tangential_factor;
		y += ty * tangential_factor;

		//correct using the radial factor
		x *= radial_factor;
		y *= radial_factor;

        if (!dotted)
        {
            vertices[4*ii+2] = x + cx;
            vertices[4*ii+3] = y + cy;
        }
	}
}

// https://stackoverflow.com/questions/19452530/how-to-render-a-rainbow-spectrum
void GLScope::setColorPalette(int nbVertices, int modulo, GLfloat *colors)
{
    for (int v = 0; v < nbVertices; v++)
    {
        int ci = modulo < 2 ? v : v % modulo;
        int nbColors = modulo < 2 ? nbVertices : modulo;
        float x = 0.8f*(((float) ci)/nbColors);
        QColor c = QColor::fromHslF(x, 0.8f, 0.6f);
        colors[3*v] = c.redF();
        colors[3*v+1] = c.greenF();
        colors[3*v+2] = c.blueF();
    }
}

void GLScope::mousePressEvent(QMouseEvent* event)
{
    if (m_displayMode == DisplayPol) { // Ignore mouse press on Polar displays
        return;
    }

    const QPointF& ep = event->localPos(); // x, y pixel position in whole scope window
    bool doUpdate = false;

    if (event->button() == Qt::RightButton)
    {
        QPointF p1 = ep; // relative position in graph #1
        p1.rx() = (ep.x()/width() -  m_glScopeRect1.left()) / m_glScopeRect1.width();
        p1.ry() = (ep.y()/height() - m_glScopeRect1.top()) / m_glScopeRect1.height();

        QPointF p2 = ep; // relative position in graph #2
        p2.rx() = (ep.x()/width() -  m_glScopeRect2.left()) / m_glScopeRect2.width();
        p2.ry() = (ep.y()/height() - m_glScopeRect2.top()) / m_glScopeRect2.height();

        if (event->modifiers() & Qt::ShiftModifier)
        {
            if ((p1.x() >= 0) && (p1.y() >= 0) && (p1.x() <= 1) && (p1.y() <= 1))
            {
                m_markers1.clear();
                doUpdate = true;
            }
            if ((p2.x() >= 0) && (p2.y() >= 0) && (p2.x() <= 1) && (p2.y() <= 1))
            {
                m_markers2.clear();
                doUpdate = true;
            }
        }
        else
        {
            if ((m_markers1.size() > 0) && (p1.x() >= 0) && (p1.y() >= 0) && (p1.x() <= 1) && (p1.y() <= 1))
            {
                m_markers1.pop_back();
                doUpdate = true;
            }
            if ((m_markers2.size() > 0) && (p2.x() >= 0) && (p2.y() >= 0) && (p2.x() <= 1) && (p2.y() <= 1))
            {
                m_markers2.pop_back();
                doUpdate = true;
            }
        }
    }
    else if (event->button() == Qt::LeftButton)
    {
        if (event->modifiers() & Qt::ShiftModifier)
        {
            QPointF p1 = ep; // relative position in graph #1
            p1.rx() = (ep.x()/width() -  m_glScopeRect1.left()) / m_glScopeRect1.width();
            p1.ry() = (ep.y()/height() - m_glScopeRect1.top()) / m_glScopeRect1.height();

            QPointF p2 = ep; // relative position in graph #2
            p2.rx() = (ep.x()/width() -  m_glScopeRect2.left()) / m_glScopeRect2.width();
            p2.ry() = (ep.y()/height() - m_glScopeRect2.top()) / m_glScopeRect2.height();

            if ((p1.x() >= 0) && (p1.y() >= 0) && (p1.x() <= 1) && (p1.y() <= 1) &&
                ((m_displayMode == DisplayX) || (m_displayMode == DisplayXYV) || (m_displayMode == DisplayXYH)))
            {
                if (m_markers1.size() < 2)
                {
                    m_markers1.push_back(ScopeMarker());
                    m_markers1.back().m_point = p1;
                    m_markers1.back().m_time = p1.x() * m_x1Scale.getRange() + m_x1Scale.getRangeMin();
                    m_markers1.back().m_value = (1.0f - p1.y()) * m_y1Scale.getRange() + m_y1Scale.getRangeMin();
                    m_markers1.back().m_timeStr = displayScaled(m_markers1.back().m_time, 'f', 1);
                    m_markers1.back().m_valueStr = displayScaled(m_markers1.back().m_value, 'f', 1);

                    if (m_markers1.size() > 1)
                    {
                        float deltaTime = m_markers1.back().m_time - m_markers1.at(0).m_time;
                        float deltaValue = m_markers1.back().m_value - m_markers1.at(0).m_value;
                        m_markers1.back().m_timeDeltaStr = displayScaled(deltaTime, 'f', 1);
                        m_markers1.back().m_valueDeltaStr = displayScaled(deltaValue, 'f', 1);
                    }

                    qDebug("GLScope::mousePressEvent: M1: t: %f v: %f", m_markers1.back().m_time, m_markers1.back().m_value);
                    doUpdate = true;
                }
            }

            if ((p2.x() >= 0) && (p2.y() >= 0) && (p2.x() <= 1) && (p2.y() <= 1) &&
                ((m_displayMode == DisplayY) || (m_displayMode == DisplayXYV) || (m_displayMode == DisplayXYH)))
            {
                if (m_markers2.size() < 2)
                {
                    m_markers2.push_back(ScopeMarker());
                    m_markers2.back().m_point = p2;
                    m_markers2.back().m_time = p2.x() * m_x2Scale.getRange() + m_x2Scale.getRangeMin();
                    m_markers2.back().m_value = (1.0f - p2.y()) * m_y2Scale.getRange() + m_y2Scale.getRangeMin();
                    m_markers2.back().m_timeStr = displayScaled(m_markers2.back().m_time, 'f', 1);
                    m_markers2.back().m_valueStr = displayScaled(m_markers2.back().m_value, 'f', 1);

                    if (m_markers2.size() > 1)
                    {
                        float deltaTime = m_markers2.back().m_time - m_markers2.at(0).m_time;
                        float deltaValue = m_markers2.back().m_value - m_markers2.at(0).m_value;
                        m_markers2.back().m_timeDeltaStr = displayScaled(deltaTime, 'f', 1);
                        m_markers2.back().m_valueDeltaStr = displayScaled(deltaValue, 'f', 1);
                    }

                    qDebug("GLScope::mousePressEvent: M2: t: %f v: %f", m_markers2.back().m_time, m_markers2.back().m_value);
                    doUpdate = true;
                }
            }
        }
    }

    if (doUpdate) {
        update();
    }
}

QString GLScope::displayScaled(float value, char type, int precision)
{
    float posValue = (value < 0) ? -value : value;

    if (posValue == 0)
    {
        return tr("%1").arg(QString::number(value, 'f', precision));
    }
    else if (posValue < 1)
    {
        if (posValue > 0.001) {
            return tr("%1m").arg(QString::number(value * 1000.0, type, precision));
        } else if (posValue > 0.000001) {
            return tr("%1u").arg(QString::number(value * 1000000.0, type, precision));
        } else if (posValue > 1e-9) {
            return tr("%1n").arg(QString::number(value * 1e9, type, precision));
        } else if (posValue > 1e-12) {
            return tr("%1p").arg(QString::number(value * 1e12, type, precision));
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
    else
    {
        if (posValue < 1000) {
            return tr("%1").arg(QString::number(value, type, precision));
        } else if (posValue < 1000000) {
            return tr("%1k").arg(QString::number(value / 1000.0, type, precision));
        } else if (posValue < 1000000000) {
            return tr("%1M").arg(QString::number(value / 1000000.0, type, precision));
        } else if (posValue < 1000000000000) {
            return tr("%1G").arg(QString::number(value / 1000000000.0, type, precision));
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
}
