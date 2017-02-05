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
#include <QDebug>
#include <algorithm>

#include "glscopeng.h"

GLScopeNG::GLScopeNG(QWidget* parent) :
    QGLWidget(parent),
    m_displayMode(DisplayX),
    m_dataChanged(false),
    m_configChanged(false),
    m_traces(0),
    m_displayGridIntensity(10),
    m_displayTraceIntensity(50),
    m_timeBase(1),
    m_traceSize(0),
    m_sampleRate(0),
    m_triggerPre(0),
    m_timeOfsProMill(0),
    m_highlightedTraceIndex(0),
    m_timeOffset(0)
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

    m_powerOverlayFont.setBold(true);
    m_powerOverlayFont.setPointSize(font().pointSize()+1);
}

GLScopeNG::~GLScopeNG()
{
    cleanup();
}

void GLScopeNG::addTrace(ScopeVisNG::DisplayTrace *trace)
{
    m_traces.push_back(trace);
    m_configChanged = true;
    update();
}

void GLScopeNG::removeTrace(int index)
{
    if (index < m_traces.size()) {
        m_traces.erase(m_traces.begin() + index);
    }

    m_configChanged = true;
    update();
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

void GLScopeNG::newTraces()
{
    if (m_traces.size() > 0)
    {
        if(!m_mutex.tryLock(2))
            return;

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

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    if (m_displayMode == DisplayX) // display only trace #0
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

        // TODO: paint trigger level #1

        // paint trace #1
        if (m_traceSize > 0)
        {
            const ScopeVisNG::DisplayTrace* trace = m_traces[0];
            int start = (m_timeOfsProMill/1000.0) * m_traceSize;
            int end = std::min(start + m_traceSize/m_timeBase, m_traceSize);
            if(end - start < 2)
                start--;

            float rectX = m_glScopeRect1.x();
            float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
            float rectW = m_glScopeRect1.width() * (float)m_timeBase / (float)(m_traceSize - 1);
            float rectH = -(m_glScopeRect1.height() / 2.0f) * trace->m_traceData.m_amp;

            QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
            QMatrix4x4 mat;
            mat.setToIdentity();
            mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
            mat.scale(2.0f * rectW, -2.0f * rectH);
            m_glShaderSimple.drawPolyline(mat, color, (GLfloat *) &trace->m_trace[2*start], end - start);
        }
    }

    m_dataChanged = false;
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

void GLScopeNG::setTriggerPre(Real triggerPre)
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

void GLScopeNG::setHighlightedTraceIndex(uint32_t traceIndex)
{
    m_highlightedTraceIndex = traceIndex;
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
    int M = fm.width("-");
    float t_start = ((m_timeOfsProMill / 1000.0) - m_triggerPre) * ((float) m_traceSize / m_sampleRate);
    float t_len = ((float) m_traceSize / m_sampleRate) / (float) m_timeBase;

    m_x1Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale
    m_x2Scale.setRange(Unit::Time, t_start, t_start + t_len); // time scale

    if (m_traces.size() > 0)
    {
        setYScale(m_y1Scale, 0); // This is always the X trace (trace #0)
    }

    if ((m_traces.size() > 1) && (m_highlightedTraceIndex < m_traces.size()))
    {
        setYScale(m_y2Scale, m_highlightedTraceIndex > 0 ? m_highlightedTraceIndex : 1); // if Highlighted trace is #0 (X trace) set it to first Y trace (trace #1)
    }

    if ((m_displayMode == DisplayX) || (m_displayMode == DisplayY)) // unique display
    {
        int scopeHeight = height() - m_topMargin - m_botMargin;
        int scopeWidth = width() - m_leftMargin - m_rightMargin;

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

        { // X1 scale
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

        } // X1 scale

        if (m_displayMode == DisplayX) // use Y1 scale
        {
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
        }
        else if (m_displayMode == DisplayY) // use Y2 scale
        {
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
        } // Y scales
    } // single display
    else // dual display (X+Y or polar)
    {
        // left (first) display
        if ((m_displayMode == DisplayXYH) || (m_displayMode == DisplayPol)) // horizontal split of first display
        {
            int scopeHeight = height() - m_topMargin - m_botMargin;
            int scopeWidth = (width() - m_rightMargin)/2 - m_leftMargin;

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

            { // Y1 scale
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

            } // Y1 scale
            { // X1 scale
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

            } // X1 scale
        }
        else // vertical split of first display
        {
            int scopeHeight = (height() - m_topMargin) / 2 - m_botMargin;
            int scopeWidth = width() - m_leftMargin - m_rightMargin;

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

            { // Y1 scale
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

            } // Y1 scale
            { // X1 scale
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

            } // X1 scale
        } // hotizontal or vertical split of first display
        // right (second) display
        if (m_displayMode == DisplayPol) // in Polar mode second display is square and split is horizontal
        {
            int scopeHeight = height() - m_topMargin - m_botMargin;
            int scopeWidth = (width() - m_rightMargin)/2 - m_leftMargin;

            int scopeDim = std::min(scopeWidth, scopeHeight);

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

            m_glLeft2ScaleMatrix.setToIdentity();
            m_glLeft2ScaleMatrix.translate (
                -1.0f + (float) 2*(m_leftMargin + scopeWidth) / (float) width(),
                 1.0f - ((float) 2*m_topMargin / (float) height())
            );
            m_glLeft2ScaleMatrix.scale (
                (float) 2*(m_leftMargin-1) / (float) width(),
                (float) -2*scopeHeight / (float) height()
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
        }
        else // both displays are similar and share space equally
        {
            if (m_displayMode == DisplayXYH) // horizontal split of second display
            {
                int scopeHeight = height() - m_topMargin - m_botMargin;
                int scopeWidth = (width() - m_rightMargin)/2 - m_leftMargin;

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

                m_glLeft2ScaleMatrix.setToIdentity();
                m_glLeft2ScaleMatrix.translate (
                    -1.0f + (float) 2*(m_leftMargin + scopeWidth) / (float) width(),
                     1.0f - ((float) 2*m_topMargin / (float) height())
                );
                m_glLeft2ScaleMatrix.scale (
                    (float) 2*(m_leftMargin-1) / (float) width(),
                    (float) -2*scopeHeight / (float) height()
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

                { // Y2 scale
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

                } // Y2 scale
                { // X2 scale
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

                } // X2 scale
            }
            else // vertical split of second display
            {
                int scopeHeight = (height() - m_topMargin) / 2 - m_botMargin;
                int scopeWidth = width() - m_leftMargin - m_rightMargin;

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

                m_glLeft2ScaleMatrix.setToIdentity();
                m_glLeft2ScaleMatrix.translate (
                    -1.0f,
                     1.0f - ((float) 2*(m_topMargin + scopeHeight + m_botMargin) / (float) height())
                );
                m_glLeft2ScaleMatrix.scale (
                    (float) 2*(m_leftMargin-1) / (float) width(),
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

                { // Y2 scale
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

                } // Y2 scale
                { // X2 scale
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
                } // X2 scale
            } // vertical or horizontal split of second display
        } // second display square (polar mode) or half space
    } // single or dual display
}

void GLScopeNG::setYScale(ScaleEngine& scale, uint32_t highlightedTraceIndex)
{
    ScopeVisNG::DisplayTrace *trace = m_traces[highlightedTraceIndex];
    float amp_range = 2.0 / trace->m_traceData.m_amp;
    float amp_ofs = trace->m_traceData.m_ofs;
    float pow_floor = -100.0 + trace->m_traceData.m_ofs * 100.0;
    float pow_range = 100.0 / trace->m_traceData.m_amp;

    switch (trace->m_traceData.m_projectionType)
    {
    case ScopeVisNG::ProjectionMagDB: // dB scale
        scale.setRange(Unit::Decibel, pow_floor, pow_floor + pow_range);
        break;
    case ScopeVisNG::ProjectionPhase: // Phase or frequency
    case ScopeVisNG::ProjectionDPhase:
        scale.setRange(Unit::None, -1.0/trace->m_traceData.m_amp + amp_ofs, 1.0/trace->m_traceData.m_amp + amp_ofs);
        break;
    case ScopeVisNG::ProjectionReal: // Linear generic
    case ScopeVisNG::ProjectionImag:
    case ScopeVisNG::ProjectionMagLin:
    default:
        if (amp_range < 2.0) {
            scale.setRange(Unit::None, - amp_range * 500.0 + amp_ofs * 1000.0, amp_range * 500.0 + amp_ofs * 1000.0);
        } else {
            scale.setRange(Unit::None, - amp_range * 0.5 + amp_ofs, amp_range * 0.5 + amp_ofs);
        }
        break;
    }
}

void GLScopeNG::tick()
{
    if(m_dataChanged)
        update();
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

