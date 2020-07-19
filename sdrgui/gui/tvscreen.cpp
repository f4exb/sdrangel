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

#include <QPainter>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurface>
#include "tvscreen.h"

#include <algorithm>
#include <QDebug>

// Note: When this object is created, QWidget* is converted to bool
TVScreen::TVScreen(bool blnColor, QWidget* parent) :
        QGLWidget(parent), m_objMutex(QMutex::NonRecursive), m_objGLShaderArray(blnColor)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&m_objTimer, SIGNAL(timeout()), this, SLOT(tick()));
    m_objTimer.start(40); // capped at 25 FPS

    m_chrLastData = NULL;
    m_subsampleShift = 0.0;
    m_blnConfigChanged = false;
    m_blnDataChanged = false;
    m_blnGLContextInitialized = false;

    //Par défaut
    m_intAskedCols = TV_COLS;
    m_intAskedRows = TV_ROWS;
    m_cols = TV_COLS;
    m_rows = TV_ROWS;
}

TVScreen::~TVScreen()
{
    cleanup();
}

void TVScreen::setColor(bool blnColor)
{
	m_objGLShaderArray.setColor(blnColor);
}

void TVScreen::setExtraColumns(bool blnExtraColumns)
{
    m_objGLShaderArray.setExtraColumns(blnExtraColumns);
}

QRgb* TVScreen::getRowBuffer(int intRow)
{
    if (!m_blnGLContextInitialized)
    {
        return NULL;
    }

    return m_objGLShaderArray.GetRowBuffer(intRow);
}

void TVScreen::renderImage(unsigned char * objData, float subsampleShift)
{
    m_chrLastData = objData;
    m_subsampleShift = subsampleShift;
    m_blnDataChanged = true;
}

void TVScreen::resetImage()
{
    m_objGLShaderArray.ResetPixels();
}

void TVScreen::resetImage(int alpha)
{
    m_objGLShaderArray.ResetPixels(alpha);
}

void TVScreen::resizeTVScreen(int intCols, int intRows)
{
    qDebug("TVScreen::resizeTVScreen: cols: %d, rows: %d", intCols, intRows);
    m_intAskedCols = intCols;
    m_intAskedRows = intRows;
    m_cols = intCols;
    m_rows = intRows;
}

void TVScreen::getSize(int& intCols, int& intRows) const
{
    intCols = m_cols;
    intRows = m_rows;
}

void TVScreen::initializeGL()
{
    m_objMutex.lock();

    QOpenGLContext *objGlCurrentContext = QOpenGLContext::currentContext();

    if (objGlCurrentContext)
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

    QSurface *objSurface = objGlCurrentContext->surface();

    if (objSurface == NULL)
    {
        qCritical() << "TVScreen::initializeGL: no surface attached";
        return;
    }
    else
    {
        if (objSurface->surfaceType() != QSurface::OpenGLSurface)
        {
            qCritical() << "TVScreen::initializeGL: surface is not an OpenGLSurface: "
                    << objSurface->surfaceType()
                    << " cannot use an OpenGL context";
            return;
        }
        else
        {
            qDebug() << "TVScreen::initializeGL: OpenGL surface:"
                    << " class: " << (objSurface->surfaceClass() == QSurface::Window ? "Window" : "Offscreen");
        }
    }

    connect(objGlCurrentContext, &QOpenGLContext::aboutToBeDestroyed, this,
            &TVScreen::cleanup); // TODO: when migrating to QOpenGLWidget

    m_blnGLContextInitialized = true;

    m_objMutex.unlock();
}

void TVScreen::resizeGL(int intWidth, int intHeight)
{
    QOpenGLFunctions *ptrF = QOpenGLContext::currentContext()->functions();
    ptrF->glViewport(0, 0, intWidth, intHeight);
    m_blnConfigChanged = true;
}

void TVScreen::paintGL()
{
    if (!m_objMutex.tryLock(2))
        return;

    m_blnDataChanged = false;

    if ((m_intAskedCols != 0) && (m_intAskedRows != 0))
    {
        m_objGLShaderArray.InitializeGL(m_intAskedCols, m_intAskedRows);
        m_intAskedCols = 0;
        m_intAskedRows = 0;
    }

    m_objGLShaderArray.setSubsampleShift(m_subsampleShift);
    m_objGLShaderArray.RenderPixels(m_chrLastData);

    m_objMutex.unlock();
}

void TVScreen::mousePressEvent(QMouseEvent* event)
{
    (void) event;
}

void TVScreen::tick()
{
    if (m_blnDataChanged) {
        update();
    }
}

void TVScreen::connectTimer(const QTimer& objTimer)
{
     qDebug() << "TVScreen::connectTimer";
     disconnect(&m_objTimer, SIGNAL(timeout()), this, SLOT(tick()));
     connect(&objTimer, SIGNAL(timeout()), this, SLOT(tick()));
     m_objTimer.stop();
}

void TVScreen::cleanup()
{
    if (m_blnGLContextInitialized)
    {
        m_objGLShaderArray.Cleanup();
    }
}

bool TVScreen::selectRow(int intLine)
{
    if (m_blnGLContextInitialized)
    {
        return m_objGLShaderArray.SelectRow(intLine);
    }
    else
    {
        return false;
    }
}

bool TVScreen::setDataColor(int intCol, int intRed, int intGreen, int intBlue)
{
    if (m_blnGLContextInitialized)
    {
        return m_objGLShaderArray.SetDataColor(intCol, qRgb(intBlue, intGreen, intRed)); // FIXME: blue <> red inversion in shader
    }
    else
    {
        return false;
    }
}

bool TVScreen::setDataColor(int intCol, int intRed, int intGreen, int intBlue, int intAlpha)
{
    if (m_blnGLContextInitialized)
    {
        return m_objGLShaderArray.SetDataColor(intCol, qRgba(intBlue, intGreen, intRed, intAlpha)); // FIXME: blue <> red inversion in shader
    }
    else
    {
        return false;
    }
}
