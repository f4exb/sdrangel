///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QPainter>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurface>
#include <QFontDatabase>
#include <QDebug>
#include <algorithm>

#include "glscopeng.h"

GLScopeNG::GLScopeNG(QWidget* parent) :
    QGLWidget(parent),
    m_tracesData(0),
    m_traces(0),
    m_bufferIndex(0),
    m_displayMode(DisplayX),
    m_dataChanged(false),
    m_configChanged(false),
    m_sampleRate(0),
    m_timeOfsProMill(0),
    m_triggerPre(0),
    m_traceSize(0),
    m_timeBase(1),
    m_timeOffset(0),
    m_focusedTraceIndex(0),
    m_displayGridIntensity(10),
    m_displayTraceIntensity(50)
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

    m_channelOverlayFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_channelOverlayFont.setBold(true);
    m_channelOverlayFont.setPointSize(font().pointSize()+1);

    //m_traceCounter = 0;
}

GLScopeNG::~GLScopeNG()
{
    cleanup();
}

void GLScopeNG::setDisplayGridIntensity(int intensity)
{
    m_displayGridIntensity = intensity;
    if (m_displayGridIntensity > 100) {
        m_displayGridIntensity = 100;
    } else if (m_displayGridIntensity < 0) {
        m_displayGridIntensity = 0;
    }
    update();
}

void GLScopeNG::setDisplayTraceIntensity(int intensity)
{
    m_displayTraceIntensity = intensity;
    if (m_displayTraceIntensity > 100) {
        m_displayTraceIntensity = 100;
    } else if (m_displayTraceIntensity < 0) {
        m_displayTraceIntensity = 0;
    }
    update();
}

void GLScopeNG::setTraces(std::vector<ScopeVisNG::TraceData>* tracesData, std::vector<float *>* traces)
{
    m_tracesData = tracesData;
    m_traces = traces;
}

void GLScopeNG::newTraces(std::vector<float *>* traces)
{
    if (traces->size() > 0)
    {
        if(!m_mutex.tryLock(2))
            return;

        m_traces = traces;
        m_dataChanged = true;

        m_mutex.unlock();
    }
}

void GLScopeNG::initializeGL()
{
    QOpenGLContext *glCurrentContext =  QOpenGLContext::currentContext();

    if (glCurrentContext) {
        if (QOpenGLContext::currentContext()->isValid()) {
            qDebug() << "GLScopeNG::initializeGL: context:"
                << " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
                << " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
                << " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
        }
        else {
            qDebug() << "GLScopeNG::initializeGL: current context is invalid";
        }
    } else {
        qCritical() << "GLScopeNG::initializeGL: no current context";
        return;
    }

    QSurface *surface = glCurrentContext->surface();

    if (surface == 0)
    {
        qCritical() << "GLScopeNG::initializeGL: no surface attached";
        return;
    }
    else
    {
        if (surface->surfaceType() != QSurface::OpenGLSurface)
        {
            qCritical() << "GLScopeNG::initializeGL: surface is not an OpenGLSurface: " << surface->surfaceType()
                << " cannot use an OpenGL context";
            return;
        }
        else
        {
            qDebug() << "GLScopeNG::initializeGL: OpenGL surface:"
                << " class: " << (surface->surfaceClass() == QSurface::Window ? "Window" : "Offscreen");
        }
    }

    connect(glCurrentContext, &QOpenGLContext::aboutToBeDestroyed, this, &GLScopeNG::cleanup); // TODO: when migrating to QOpenGLWidget

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->initializeOpenGLFunctions();

    //glDisable(GL_DEPTH_TEST);
    m_glShaderSimple.initializeGL();
    m_glShaderLeft1Scale.initializeGL();
    m_glShaderBottom1Scale.initializeGL();
    m_glShaderLeft2Scale.initializeGL();
    m_glShaderBottom2Scale.initializeGL();
    m_glShaderPowerOverlay.initializeGL();
}

void GLScopeNG::resizeGL(int width, int height)
{
    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glViewport(0, 0, width, height);
    m_configChanged = true;
}

void GLScopeNG::paintGL()
{
    if(!m_mutex.tryLock(2))
        return;

    if(m_configChanged)
        applyConfig();

//    qDebug("GLScopeNG::paintGL: m_traceCounter: %d", m_traceCounter);
//    m_traceCounter = 0;

    m_dataChanged = false;

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    if ((m_displayMode == DisplayX) || (m_displayMode == DisplayXYV) || (m_displayMode == DisplayXYH)) // display trace #0
    {
        // draw rect around
        {
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix1, color, q3, 4);
        }

        // paint grid
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        // Y1 (X trace or trace #0)
        {
            tickList = &m_y1Scale.getTickList();

            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = 1 - (tick->pos / m_y1Scale.getSize());
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            float blue = 1.0f;
            QVector4D color(1.0f, 1.0f, blue, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
        }

        // X1 (time)
        {
            tickList = &m_x1Scale.getTickList();

            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float x = tick->pos / m_x1Scale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
        }

        // paint left #1 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            m_glShaderLeft1Scale.drawSurface(m_glLeft1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #1 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            m_glShaderBottom1Scale.drawSurface(m_glBot1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint trace #1
        if (m_traceSize > 0)
        {
            const float *trace = (*m_traces)[0];
            const ScopeVisNG::TraceData& traceData = (*m_tracesData)[0];

            if (traceData.m_viewTrace)
            {
                int start = (m_timeOfsProMill/1000.0) * m_traceSize;
                int end = std::min(start + m_traceSize/m_timeBase, m_traceSize);

                if(end - start < 2)
                    start--;

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
                m_glShaderSimple.drawPolyline(mat, color, (GLfloat *) &trace[2*start], end - start);

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[] {
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel
                    };

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
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix2, color, q3, 4);
        }

        // paint grid
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        // Y2 (Focused Y trace)
        {
            tickList = &m_y2Scale.getTickList();

            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = 1 - (tick->pos / m_y2Scale.getSize());
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            float blue = 1.0f;
            QVector4D color(1.0f, 1.0f, blue, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
        }

        // X2 (time)
        {
            tickList = &m_x2Scale.getTickList();

            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float x = tick->pos / m_x2Scale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
        }

        // paint left #2 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            m_glShaderLeft2Scale.drawSurface(m_glLeft2ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #2 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            m_glShaderBottom2Scale.drawSurface(m_glBot2ScaleMatrix, tex1, vtx1, 4);
        }

        // paint traces #1..n
        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill/1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize/m_timeBase, m_traceSize);

            if(end - start < 2)
                start--;

            for (unsigned int i = 1; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const ScopeVisNG::TraceData& traceData = (*m_tracesData)[i];

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
                m_glShaderSimple.drawPolyline(mat, color, (GLfloat *) &trace[2*start], end - start);

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[] {
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel
                    };

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
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix1, color, q3, 4);

        }

        // paint grid
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        // Horizontal Y1
        tickList = &m_y1Scale.getTickList();
        {
            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = 1 - (tick->pos / m_y1Scale.getSize());
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 0.25f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
        }

        // Vertical X1
        tickList = &m_x1Scale.getTickList();
        {
            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float x = tick->pos / m_x1Scale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
        }

        // paint left #1 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderLeft1Scale.drawSurface(m_glLeft1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #1 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderBottom1Scale.drawSurface(m_glBot1ScaleMatrix, tex1, vtx1, 4);
        }

        // Paint secondary grid

        // Horizontal Y2
        tickList = &m_y2Scale.getTickList();
        {
            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float y = 1 - (tick->pos / m_y2Scale.getSize());
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(0.25f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
        }

        // Paint secondary scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderLeft2Scale.drawSurface(m_glRight1ScaleMatrix, tex1, vtx1, 4);
        }

        // paint all traces
        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill/1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize/m_timeBase, m_traceSize);

            if(end - start < 2)
                start--;

            for (unsigned int i = 0; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const ScopeVisNG::TraceData& traceData = (*m_tracesData)[i];

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
                m_glShaderSimple.drawPolyline(mat, color, (GLfloat *) &trace[2*start], end - start);

                // Paint trigger level if any
                if ((traceData.m_triggerDisplayLevel > -1.0f) && (traceData.m_triggerDisplayLevel < 1.0f))
                {
                    GLfloat q3[] {
                        0, traceData.m_triggerDisplayLevel,
                        1, traceData.m_triggerDisplayLevel
                    };

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
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glScopeMatrix2, color, q3, 4);
        }

        // paint grid

        // Horizontal Y2
        tickList = &m_y2Scale.getTickList();
        {
            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float y = 1 - (tick->pos / m_y2Scale.getSize());
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
        }

        // Vertical X2
        tickList = &m_x2Scale.getTickList();
        {
            GLfloat q3[4*tickList->count()];
            int effectiveTicks = 0;
            for(int i= 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if(tick->major) {
                    if(tick->textSize > 0) {
                        float x = tick->pos / m_x2Scale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
        }

        // paint left #2 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderLeft2Scale.drawSurface(m_glLeft2ScaleMatrix, tex1, vtx1, 4);
        }

        // paint bottom #2 scale
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderBottom2Scale.drawSurface(m_glBot2ScaleMatrix, tex1, vtx1, 4);
        }

        // paint polar traces

        if (m_traceSize > 0)
        {
            int start = (m_timeOfsProMill/1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize/m_timeBase, m_traceSize);

            if(end - start < 2)
                start--;

            GLfloat q3[2*(end - start)];
            const float *trace0 = (*m_traces)[0];
            memcpy(q3, &(trace0[2*start+1]), (2*(end - start) - 1)*sizeof(float)); // copy X values

            for (unsigned int i = 1; i < m_traces->size(); i++)
            {
                const float *trace = (*m_traces)[i];
                const ScopeVisNG::TraceData& traceData = (*m_tracesData)[i];

                if (!traceData.m_viewTrace) {
                    continue;
                }

                for(int i = start; i < end; i++)
                {
                    float y = trace[2*i+1];
                    q3[2*(i-start)+1] = y;
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
                m_glShaderSimple.drawPolyline(mat, color, q3, end -start);
            } // XY polar display
        } // trace length > 0
    } // XY mixed + polar display

    m_mutex.unlock();
}

void GLScopeNG::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    m_configChanged = true;
    update();
    emit sampleRateChanged(m_sampleRate);
}

void GLScopeNG::setTimeBase(int timeBase)
{
    m_timeBase = timeBase;
    m_configChanged = true;
    update();
}

void GLScopeNG::setTriggerPre(uint32_t triggerPre)
{
    m_triggerPre = triggerPre;
    m_configChanged = true;
    update();
}

void GLScopeNG::setTimeOfsProMill(int timeOfsProMill)
{
    m_timeOfsProMill = timeOfsProMill;
    m_configChanged = true;
    update();
}

void GLScopeNG::setFocusedTraceIndex(uint32_t traceIndex)
{
    m_focusedTraceIndex = traceIndex;
    m_configChanged = true;
    update();
}

void GLScopeNG::setDisplayMode(DisplayMode displayMode)
{
    m_displayMode = displayMode;
    m_configChanged = true;
    update();
}

void GLScopeNG::setTraceSize(int traceSize)
{
    m_traceSize = traceSize;
    m_configChanged = true;
    update();
}

void GLScopeNG::updateDisplay()
{
    m_configChanged = true;
    update();
}

void GLScopeNG::applyConfig()
{
    m_configChanged = false;

    QFontMetrics fm(font());
    //float t_start = ((m_timeOfsProMill / 1000.0) * ((float) m_traceSize / m_sampleRate)) - ((float) m_triggerPre / m_sampleRate);
    float t_start = (((m_timeOfsProMill / 1000.0f) * (float) m_traceSize) / m_sampleRate) - ((float) m_triggerPre / m_sampleRate);
    float t_len = ((float) m_traceSize / m_sampleRate) / (float) m_timeBase;

    // scales

    m_x1Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale

    if (m_displayMode == DisplayPol)
    {
        setYScale(m_x2Scale, 0); // polar scale (X)
    }
    else
    {
        m_x2Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale
    }

    if (m_traces->size() > 0)
    {
        setYScale(m_y1Scale, 0); // This is always the X trace (trace #0)
    }

    if ((m_traces->size() > 1) && (m_focusedTraceIndex < m_traces->size()))
    {
        setYScale(m_y2Scale, m_focusedTraceIndex > 0 ? m_focusedTraceIndex : 1); // if Highlighted trace is #0 (X trace) set it to first Y trace (trace #1)
    }
    else
    {
        setYScale(m_y2Scale, 0); // Default to the X trace (trace #0) - If there is only one trace it should not get there (Y displays disabled in the UI)
    }

    // display arrangements

    if ((m_displayMode == DisplayX) || (m_displayMode == DisplayY)) // unique displays
    {
        setUniqueDisplays();
    }
    else if (m_displayMode == DisplayXYV) // both displays vertically arranged
    {
        setVerticalDisplays();
    }
    else if (m_displayMode == DisplayXYH) // both displays horizontally arranged
    {
        setHorizontalDisplays();
    }
    else if (m_displayMode == DisplayPol) // horizontal arrangement: XY stacked on left and polar on right
    {
        setPolarDisplays();
    }
}

void GLScopeNG::setUniqueDisplays()
{
    QFontMetrics fm(font());
    int M = fm.width("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = width() - m_leftMargin - m_rightMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float) m_leftMargin / (float) width(),
        (float) m_topMargin / (float) height(),
        (float) scopeWidth / (float) width(),
        (float) scopeHeight / (float) height()
    );

    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix1.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot1ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft1ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    // Y displays

    m_glScopeRect2 = QRectF(
        (float) m_leftMargin / (float) width(),
        (float) m_topMargin / (float) height(),
        (float) scopeWidth / (float) width(),
        (float) scopeHeight / (float) height()
    );

    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix2.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot2ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft2ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // X horizontal scale

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);

        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale

    { //  X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } // X vertical scale

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale
}

void GLScopeNG::setVerticalDisplays()
{
    QFontMetrics fm(font());
    int M = fm.width("-");
    int scopeHeight = (height() - m_topMargin) / 2 - m_botMargin;
    int scopeWidth = width() - m_leftMargin - m_rightMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float) m_leftMargin / (float) width(),
        (float) m_topMargin / (float) height(),
        (float) scopeWidth / (float) width(),
        (float) scopeHeight / (float) height()
    );

    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix1.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot1ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft1ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    // Y display

    m_glScopeRect2 = QRectF(
        (float) m_leftMargin / (float)width(),
        (float) (m_botMargin + m_topMargin + scopeHeight) / (float)height(),
        (float) scopeWidth / (float)width(),
        (float) scopeHeight / (float)height()
    );

    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(m_botMargin + m_topMargin + scopeHeight) / (float) height())
    );
    m_glScopeMatrix2.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + scopeHeight + m_botMargin + 1) / (float) height())
    );
    m_glBot2ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*(m_topMargin + scopeHeight + m_botMargin) / (float) height())
    );
    m_glLeft2ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // X horizontal scale (X1)

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);
        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale (X2)

    { //  X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } //  X vertical scale (Y1)

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale (Y2)
}

void GLScopeNG::setHorizontalDisplays()
{
    QFontMetrics fm(font());
    int M = fm.width("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = (width() - m_rightMargin)/2 - m_leftMargin;

    // X display

    m_glScopeRect1 = QRectF(
        (float) m_leftMargin / (float) width(),
        (float) m_topMargin / (float) height(),
        (float) scopeWidth / (float) width(),
        (float) scopeHeight / (float) height()
    );
    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix1.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot1ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft1ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    // Y display

    m_glScopeRect2 = QRectF(
        (float)(m_leftMargin + m_leftMargin + ((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2)) / (float)width(),
        (float)m_topMargin / (float)height(),
        (float)((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2) / (float)width(),
        (float)(height() - m_topMargin - m_botMargin) / (float)height()
    );
    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate (
        -1.0f + ((float) 2*(m_leftMargin + m_leftMargin + ((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2)) / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix2.scale (
        (float) 2*((width() - m_leftMargin - m_leftMargin - m_rightMargin) / 2) / (float) width(),
        (float) -2*(height() - m_topMargin - m_botMargin) / (float) height()
    );

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate (
        -1.0f + ((float) 2*(m_leftMargin + m_leftMargin + scopeWidth) / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot2ScaleMatrix.scale (
        (float) 2*scopeWidth / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate (
        -1.0f + (float) 2*(m_leftMargin + scopeWidth) / (float) width(),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft2ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    { // X horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());

    } // X horizontal scale (X1)

    { // Y horizontal scale (X2)
        m_x2Scale.setSize(scopeWidth);
        m_bot2ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Y horizontal scale (X2)

    { // X vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());
    } // X vertical scale (Y1)

    { // Y vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Y vertical scale (Y2)
}

void GLScopeNG::setPolarDisplays()
{
    QFontMetrics fm(font());
    int M = fm.width("-");
    int scopeHeight = height() - m_topMargin - m_botMargin;
    int scopeWidth = (width() - m_rightMargin)/2 - m_leftMargin;
    int scopeDim = std::min(scopeWidth, scopeHeight);
    scopeWidth += scopeWidth - scopeDim;

    // Mixed XY display (left)

    m_glScopeRect1 = QRectF(
        (float) m_leftMargin / (float) width(),
        (float) m_topMargin / (float) height(),
        (float) (scopeWidth-m_leftMargin) / (float) width(),
        (float) scopeHeight / (float) height()
    );
    m_glScopeMatrix1.setToIdentity();
    m_glScopeMatrix1.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix1.scale (
        (float) 2*(scopeWidth-m_leftMargin) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glBot1ScaleMatrix.setToIdentity();
    m_glBot1ScaleMatrix.translate (
        -1.0f + ((float) 2*m_leftMargin / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot1ScaleMatrix.scale (
        (float) 2*(scopeWidth-m_leftMargin) / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft1ScaleMatrix.setToIdentity();
    m_glLeft1ScaleMatrix.translate (
        -1.0f,
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft1ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    m_glRight1ScaleMatrix.setToIdentity();
    m_glRight1ScaleMatrix.translate (
        -1.0f + ((float) 2*scopeWidth / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glRight1ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    // Polar XY display (right)

    m_glScopeRect2 = QRectF(
        (float)(m_leftMargin + scopeWidth + m_leftMargin) / (float)width(),
        (float)m_topMargin / (float)height(),
        (float) scopeDim / (float)width(),
        (float)(height() - m_topMargin - m_botMargin) / (float)height()
    );
    m_glScopeMatrix2.setToIdentity();
    m_glScopeMatrix2.translate (
        -1.0f + ((float) 2*(m_leftMargin + scopeWidth + m_leftMargin) / (float) width()),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glScopeMatrix2.scale (
        (float) 2*scopeDim / (float) width(),
        (float) -2*(height() - m_topMargin - m_botMargin) / (float) height()
    );

    m_glBot2ScaleMatrix.setToIdentity();
    m_glBot2ScaleMatrix.translate (
        -1.0f + ((float) 2*(m_leftMargin + m_leftMargin + scopeWidth) / (float) width()),
         1.0f - ((float) 2*(scopeHeight + m_topMargin + 1) / (float) height())
    );
    m_glBot2ScaleMatrix.scale (
        (float) 2*scopeDim / (float) width(),
        (float) -2*(m_botMargin - 1) / (float) height()
    );

    m_glLeft2ScaleMatrix.setToIdentity();
    m_glLeft2ScaleMatrix.translate (
        -1.0f + (float) 2*(m_leftMargin + scopeWidth) / (float) width(),
         1.0f - ((float) 2*m_topMargin / (float) height())
    );
    m_glLeft2ScaleMatrix.scale (
        (float) 2*(m_leftMargin-1) / (float) width(),
        (float) -2*scopeHeight / (float) height()
    );

    { // Mixed XY horizontal scale (X1)
        m_x1Scale.setSize(scopeWidth);

        m_bot1ScalePixmap = QPixmap(
            scopeWidth,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());
    } // Mixed XY horizontal scale (X1)

    { // Polar XY horizontal scale (X2)
        m_x2Scale.setSize(scopeDim);
        m_bot2ScalePixmap = QPixmap(
            scopeDim,
            m_botMargin - 1
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_bot2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_bot2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_x2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
                }
            }
        }

        m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());
    } // Polar XY horizontal scale (X2)

    { // Mixed XY vertical scale (Y1)
        m_y1Scale.setSize(scopeHeight);

        m_left1ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left1ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left1ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y1Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());

    } // Mixed XY vertical scale (Y1)

    { // Polar XY vertical scale (Y2)
        m_y2Scale.setSize(scopeHeight);

        m_left2ScalePixmap = QPixmap(
            m_leftMargin - 1,
            scopeHeight
        );

        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        m_left2ScalePixmap.fill(Qt::black);
        QPainter painter(&m_left2ScalePixmap);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        tickList = &m_y2Scale.getTickList();

        for(int i = 0; i < tickList->count(); i++) {
            tick = &(*tickList)[i];
            if(tick->major) {
                if(tick->textSize > 0) {
                    painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
                }
            }
        }

        m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());
    } // Polar XY vertical scale (Y2)
}

void GLScopeNG::setYScale(ScaleEngine& scale, uint32_t highlightedTraceIndex)
{
    ScopeVisNG::TraceData& traceData = (*m_tracesData)[highlightedTraceIndex];
    float amp_range = 2.0 / traceData.m_amp;
    float amp_ofs = traceData.m_ofs;
    float pow_floor = -100.0 + traceData.m_ofs * 100.0;
    float pow_range = 100.0 / traceData.m_amp;

    switch (traceData.m_projectionType)
    {
    case ScopeVisNG::ProjectionMagDB: // dB scale
        scale.setRange(Unit::Decibel, pow_floor, pow_floor + pow_range);
        break;
    case ScopeVisNG::ProjectionMagLin:
        if (amp_range < 2.0) {
            scale.setRange(Unit::None, amp_ofs * 1000.0, amp_range * 1000.0 + amp_ofs * 1000.0);
        } else {
            scale.setRange(Unit::None, amp_ofs, amp_range + amp_ofs);
        }
        break;
    case ScopeVisNG::ProjectionPhase: // Phase or frequency
    case ScopeVisNG::ProjectionDPhase:
        scale.setRange(Unit::None, -1.0/traceData.m_amp + amp_ofs, 1.0/traceData.m_amp + amp_ofs);
        break;
    case ScopeVisNG::ProjectionReal: // Linear generic
    case ScopeVisNG::ProjectionImag:
    default:
        if (amp_range < 2.0) {
            scale.setRange(Unit::None, - amp_range * 500.0 + amp_ofs * 1000.0, amp_range * 500.0 + amp_ofs * 1000.0);
        } else {
            scale.setRange(Unit::None, - amp_range * 0.5 + amp_ofs, amp_range * 0.5 + amp_ofs);
        }
        break;
    }
}

void GLScopeNG::drawChannelOverlay(
        const QString& text,
        const QColor& color,
        QPixmap& channelOverlayPixmap,
        const QRectF& glScopeRect)
{
    if (text.isEmpty()) {
        return;
    }

    QFontMetricsF metrics(m_channelOverlayFont);
    QRectF rect = metrics.boundingRect(text);
    channelOverlayPixmap = QPixmap(rect.width() + 4.0f, rect.height());
    channelOverlayPixmap.fill(Qt::transparent);
    QPainter painter(&channelOverlayPixmap);
    painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing, false);
    painter.fillRect(rect, QColor(0, 0, 0, 0x80));
    QColor textColor(color);
    textColor.setAlpha(0xC0);
    painter.setPen(textColor);
    painter.setFont(m_channelOverlayFont);
    painter.drawText(QPointF(0, rect.height() - 2.0f), text);
    painter.end();

    m_glShaderPowerOverlay.initTexture(channelOverlayPixmap.toImage());

    {
        GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0
        };
        GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0
        };

        float shiftX = glScopeRect.width() - ((rect.width() + 4.0f) / width());
        float rectX = glScopeRect.x() + shiftX;
        float rectY = glScopeRect.y();
        float rectW = rect.width() / (float) width();
        float rectH = rect.height() / (float) height();

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
        mat.scale(2.0f * rectW, -2.0f * rectH);
        m_glShaderPowerOverlay.drawSurface(mat, tex1, vtx1, 4);
    }
}

void GLScopeNG::tick()
{
    if(m_dataChanged) {
        update();
    }
}

void GLScopeNG::connectTimer(const QTimer& timer)
{
    qDebug() << "GLScopeNG::connectTimer";
    disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.stop();
}

void GLScopeNG::cleanup()
{
    //makeCurrent();
    m_glShaderSimple.cleanup();
    m_glShaderBottom1Scale.cleanup();
    m_glShaderBottom2Scale.cleanup();
    m_glShaderLeft1Scale.cleanup();
    m_glShaderPowerOverlay.cleanup();
    //doneCurrent();
}

