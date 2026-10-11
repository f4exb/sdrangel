///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QFontMetrics>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPainterPath>

#include "gui/glvideowidget.h"

GLVideoWidget::GLVideoWidget(QWidget *parent) :
    QOpenGLWidget(parent),
    m_hidden(false),
    m_overlayHeight(0),
    m_keepAspect(true),
    m_deinterlace(true)
{
    m_overlayTimer.setSingleShot(true);
    connect(&m_overlayTimer, &QTimer::timeout, this, [this]() {
        m_overlay.clear();
        update();
    });
}

GLVideoWidget::~GLVideoWidget()
{
    cleanup();
}

void GLVideoWidget::initializeGL()
{
    m_renderer.initialize();
    // The context is destroyed when the widget is, or recreated when it's reparented: GL objects go first
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLVideoWidget::cleanup, Qt::DirectConnection);
}

void GLVideoWidget::cleanup()
{
    if (!context()) {
        return;
    }

    makeCurrent();
    m_current.reset(); // GPU frames may hold the decoder's surfaces
    m_renderer.cleanup();
    doneCurrent();
    disconnect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLVideoWidget::cleanup);
}

void GLVideoWidget::setFrame(GLVideoFramePtr frame)
{
    {
        QMutexLocker locker(&m_mutex);

        // GPU frames hold the decoder's surfaces, so aren't kept while hidden. The next given after shown is shown
        if (m_hidden && frame && frame->isGPU()) {
            return;
        }

        m_pending = frame;
    }

    QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
}

void GLVideoWidget::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_pending.reset();
    }

    m_current.reset();

    // The renderer's, as for deinterlacing, may hold the decoder's GPU surfaces too
    if (context())
    {
        makeCurrent();
        m_renderer.releaseFrames();
        doneCurrent();
    }

    update();
}

// GPU frames, and the renderer's (for deinterlacing, and the interops' views of them), hold the decoder's surfaces,
// which a hidden widget has no use for. Frames in memory are kept, so a paused picture is shown again
void GLVideoWidget::hideEvent(QHideEvent *event)
{
    {
        QMutexLocker locker(&m_mutex);
        m_hidden = true;

        if (m_pending && m_pending->isGPU()) {
            m_pending.reset();
        }
    }

    if (m_current && m_current->isGPU()) {
        m_current.reset();
    }

    if (context())
    {
        makeCurrent();
        m_renderer.releaseFrames();
        doneCurrent();
    }

    QOpenGLWidget::hideEvent(event);
}

void GLVideoWidget::showEvent(QShowEvent *event)
{
    {
        QMutexLocker locker(&m_mutex);
        m_hidden = false;
    }

    QOpenGLWidget::showEvent(event);
}

void GLVideoWidget::setMessage(const QString& message)
{
    if (message != m_message)
    {
        m_message = message;
        m_messageImage = QImage();
        update();
    }
}

void GLVideoWidget::setOverlay(const QString& text, double seconds)
{
    m_overlay = text;
    m_overlayImage = QImage();
    m_overlayTimer.start((int) (seconds * 1000.0));
    update();
}

void GLVideoWidget::setGuide(const QImage& image)
{
    m_guide = image;
    update();
}

// As QOpenGLWidget sizes its framebuffer: rounded, so there's no edge left at fractional scales (125%, 150%)
QSize GLVideoWidget::pixelSize() const
{
    return size() * devicePixelRatioF();
}

void GLVideoWidget::setKeepAspectRatio(bool keep)
{
    m_keepAspect = keep;
    update();
}

void GLVideoWidget::setDeinterlace(bool deinterlace)
{
    m_deinterlace = deinterlace;
}

QSize GLVideoWidget::getFrameSize() const
{
    return m_frameSize;
}

// White text, centred, on black, as large as the widget allows
QImage GLVideoWidget::messageImage(int width, int height)
{
    if (!m_messageImage.isNull() && (m_messageImage.width() == width) && (m_messageImage.height() == height)) {
        return m_messageImage;
    }

    QImage image(width, height, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font = painter.font();
    font.setPixelSize(std::max(12, height / 20));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(image.rect().adjusted(width / 10, 0, -width / 10, 0), Qt::AlignCenter | Qt::TextWordWrap, m_message);
    m_messageImage = image;
    return m_messageImage;
}

// White text in a translucent rounded box, sized for the video
QImage GLVideoWidget::overlayImage(int videoHeight)
{
    if (!m_overlayImage.isNull() && (m_overlayHeight == videoHeight)) {
        return m_overlayImage;
    }

    QFont font = this->font();
    font.setPixelSize(std::max(12, videoHeight / 16));
    const QFontMetrics metrics(font);
    const int padding = metrics.height() / 3;
    const QRect text = metrics.boundingRect(QRect(0, 0, 4096, 4096), Qt::AlignCenter, m_overlay);
    QImage image(text.width() + 2 * padding, text.height() + 2 * padding, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    QPainterPath path;
    path.addRoundedRect(image.rect(), padding, padding);
    painter.fillPath(path, QColor(0, 0, 0, 160));
    painter.setPen(Qt::white);
    painter.drawText(image.rect(), Qt::AlignCenter, m_overlay);
    m_overlayImage = image;
    m_overlayHeight = videoHeight;
    return m_overlayImage;
}

void GLVideoWidget::paintGL()
{
    {
        QMutexLocker locker(&m_mutex);

        if (m_pending) {
            m_current = std::move(m_pending);
        }
    }

    const QSize pixels = pixelSize();
    const int width = pixels.width();
    const int height = pixels.height();
    QOpenGLFunctions *gl = context()->functions();
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (!m_message.isEmpty())
    {
        m_renderer.drawImage(messageImage(width, height), QRect(0, 0, width, height), width, height);
    }
    else if (m_current)
    {
        m_frameSize = QSize(m_current->m_width, m_current->m_height);
        m_renderer.drawFrame(m_current, width, height, m_keepAspect, m_deinterlace);

        // The guide shows what the overlay would
        if (!m_overlay.isEmpty() && m_guide.isNull())
        {
            const QRect video = GLVideoRenderer::videoRect(*m_current, width, height, m_keepAspect);
            const QImage image = overlayImage(video.height());
            const int margin = video.height() / 20;
            const QRect rect(video.right() - margin - image.width(), video.bottom() - margin - image.height(), image.width(), image.height());
            m_renderer.drawImage(image, rect, width, height);
        }
    }

    if (!m_guide.isNull()) {
        m_renderer.drawImage(m_guide, QRect(0, 0, width, height), width, height);
    }
}
