///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
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

#include <algorithm>

#include <QPainter>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurface>
#include <QDebug>
#include <QMutexLocker>

#include "tvscreen.h"


// Note: When this object is created, QWidget* is converted to bool
TVScreen::TVScreen(bool color, QWidget* parent) :
    QOpenGLWidget(parent),
    m_mutex(QMutex::Recursive),
    m_glShaderArray(color)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.start(40); // capped at 25 FPS

    m_lastData = nullptr;
    m_dataChanged = false;
    m_glContextInitialized = false;

    //Par défaut
    m_askedCols = TV_COLS;
    m_askedRows = TV_ROWS;
    m_cols = TV_COLS;
    m_rows = TV_ROWS;
}

TVScreen::~TVScreen()
{
}

void TVScreen::setColor(bool color)
{
	m_glShaderArray.setColor(color);
}

QRgb* TVScreen::getRowBuffer(int row)
{
    if (!m_glContextInitialized) {
        return nullptr;
    }

    return m_glShaderArray.GetRowBuffer(row);
}

void TVScreen::renderImage(unsigned char * data)
{
    m_lastData = data;
    m_dataChanged = true;
}

void TVScreen::resetImage()
{
    m_glShaderArray.ResetPixels();
}

void TVScreen::resetImage(int alpha)
{
    m_glShaderArray.ResetPixels(alpha);
}

void TVScreen::resizeTVScreen(int cols, int intRows)
{
    qDebug("TVScreen::resizeTVScreen: cols: %d, rows: %d", cols, intRows);
    QMutexLocker mlock(&m_mutex);
    m_askedCols = cols;
    m_askedRows = intRows;
    m_cols = cols;
    m_rows = intRows;
}

void TVScreen::getSize(int& cols, int& intRows) const
{
    cols = m_cols;
    intRows = m_rows;
}

void TVScreen::initializeGL()
{
    QMutexLocker mlock(&m_mutex);

    QOpenGLContext *glCurrentContext = QOpenGLContext::currentContext();

    if (glCurrentContext)
    {
        if (QOpenGLContext::currentContext()->isValid())
        {
            qDebug() << "TVScreen::initializeGL: context:"
                << " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
                << " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
                << " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
        }
        else
        {
            qDebug() << "TVScreen::initializeGL: current context is invalid";
        }
    }
    else
    {
        qCritical() << "TVScreen::initializeGL: no current context";
        return;
    }

    QSurface *surface = glCurrentContext->surface();

    if (surface == nullptr)
    {
        qCritical() << "TVScreen::initializeGL: no surface attached";
        return;
    }
    else
    {
        if (surface->surfaceType() != QSurface::OpenGLSurface)
        {
            qCritical() << "TVScreen::initializeGL: surface is not an OpenGLSurface: "
                << surface->surfaceType()
                << " cannot use an OpenGL context";
            return;
        }
        else
        {
            qDebug() << "TVScreen::initializeGL: OpenGL surface:"
                << " class: " << (surface->surfaceClass() == QSurface::Window ? "Window" : "Offscreen");
        }
    }

    connect(
        glCurrentContext,
        &QOpenGLContext::aboutToBeDestroyed,
        this,
        &TVScreen::cleanup
    );

    m_glContextInitialized = true;
}

void TVScreen::resizeGL(int width, int height)
{
    QMutexLocker mlock(&m_mutex);
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glViewport(0, 0, width, height);
}

void TVScreen::paintGL()
{
    if (!m_mutex.tryLock(2)) {
        return;
    }

    m_dataChanged = false;

    if ((m_askedCols != 0) && (m_askedRows != 0))
    {
        int major = 0, minor = 0;
        if (QOpenGLContext::currentContext())
        {
            major = QOpenGLContext::currentContext()->format().majorVersion();
            minor = QOpenGLContext::currentContext()->format().minorVersion();
        }
        m_glShaderArray.initializeGL(major, minor, m_askedCols, m_askedRows);
        m_askedCols = 0;
        m_askedRows = 0;
    }

    m_glShaderArray.RenderPixels(m_lastData);
    m_mutex.unlock();
}

void TVScreen::mousePressEvent(QMouseEvent* event)
{
    (void) event;
}

void TVScreen::tick()
{
    if (m_dataChanged) {
        update();
    }
}

void TVScreen::connectTimer(const QTimer& timer)
{
     qDebug() << "TVScreen::connectTimer";
     disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
     connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
     m_timer.stop();
}

void TVScreen::cleanup()
{
    QMutexLocker mlock(&m_mutex);

    if (m_glContextInitialized) {
        m_glShaderArray.cleanup();
    }
}

bool TVScreen::selectRow(int line)
{
    if (m_glContextInitialized) {
        return m_glShaderArray.SelectRow(line);
    } else {
        return false;
    }
}

bool TVScreen::setDataColor(int col, int red, int green, int blue)
{
    if (m_glContextInitialized) {
        return m_glShaderArray.SetDataColor(col, qRgb(blue, green, red)); // FIXME: blue <> red inversion in shader
    } else {
        return false;
    }
}

bool TVScreen::setDataColor(int col, int red, int green, int blue, int alpha)
{
    if (m_glContextInitialized) {
        return m_glShaderArray.SetDataColor(col, qRgba(blue, green, red, alpha)); // FIXME: blue <> red inversion in shader
    } else {
        return false;
    }
}
