///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
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
#include "tvscreen.h"

#include <algorithm>
#include <QDebug>

TVScreen::TVScreen(bool blnColor, QWidget* parent) :
        QGLWidget(parent), m_objMutex(QMutex::NonRecursive), m_objGLShaderArray(blnColor)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&m_objTimer, SIGNAL(timeout()), this, SLOT(tick()));
    m_objTimer.start(40); // capped at 25 FPS

    m_chrLastData = NULL;
    m_blnConfigChanged = false;
    m_blnDataChanged = false;
    m_blnGLContextInitialized = false;

    //Par défaut
    m_intAskedCols = TV_COLS;
    m_intAskedRows = TV_ROWS;

}

TVScreen::~TVScreen()
{
    cleanup();
}

QRgb* TVScreen::getRowBuffer(int intRow)
{
    if (!m_blnGLContextInitialized)
    {
        return NULL;
    }

    return m_objGLShaderArray.GetRowBuffer(intRow);
}

void TVScreen::renderImage(unsigned char * objData)
{
    m_chrLastData = objData;
    m_blnDataChanged = true;
    //update();
}

void TVScreen::resetImage()
{
    m_objGLShaderArray.ResetPixels();
}

void TVScreen::resizeTVScreen(int intCols, int intRows)
{
    m_intAskedCols = intCols;
    m_intAskedRows = intRows;
}

void TVScreen::initializeGL()
{
    m_objMutex.lock();

    QOpenGLContext *objGlCurrentContext = QOpenGLContext::currentContext();

    if (objGlCurrentContext)
    {
        if (QOpenGLContext::currentContext()->isValid())
        {
            qDebug() << "DATVScreen::initializeGL: context:"
                    << " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
                    << " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
                    << " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
        }
        else
        {
            qDebug() << "DATVScreen::initializeGL: current context is invalid";
        }
    }
    else
    {
        qCritical() << "DATVScreen::initializeGL: no current context";
        return;
    }

    QSurface *objSurface = objGlCurrentContext->surface();

    if (objSurface == NULL)
    {
        qCritical() << "DATVScreen::initializeGL: no surface attached";
        return;
    }
    else
    {
        if (objSurface->surfaceType() != QSurface::OpenGLSurface)
        {
            qCritical() << "DATVScreen::initializeGL: surface is not an OpenGLSurface: "
                    << objSurface->surfaceType()
                    << " cannot use an OpenGL context";
            return;
        }
        else
        {
            qDebug() << "DATVScreen::initializeGL: OpenGL surface:"
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

    m_objGLShaderArray.RenderPixels(m_chrLastData);

    m_objMutex.unlock();
}

void TVScreen::mousePressEvent(QMouseEvent* event __attribute__((unused)))
{
}

void TVScreen::tick()
{
    if (m_blnDataChanged) {
        update();
    }
}

void TVScreen::connectTimer(const QTimer& objTimer)
{
     qDebug() << "DATVScreen::connectTimer";
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
        return m_objGLShaderArray.SetDataColor(intCol, qRgb(intRed, intGreen, intBlue));
    }
    else
    {
        return false;
    }
}
