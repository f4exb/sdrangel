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

#ifndef SDRGUI_GUI_GLVIDEOWIDGET_H_
#define SDRGUI_GUI_GLVIDEOWIDGET_H_

#include <QImage>
#include <QMutex>
#include <QOpenGLWidget>
#include <QTimer>

#include "gui/glvideoframe.h"
#include "gui/glvideorenderer.h"
#include "export.h"

// Displays video: frames of YUV in memory, or on the GPU (D3D11, CUDA, DMA-BUF, IOSurface) without copying them
// through the CPU, converted to RGB by the GPU, with the frame's aspect ratio. Also a message in place of video,
// text overlaid on it for a while, as a channel's name, and an image over the whole widget, as a programme guide.
// Frames can be given from any thread, as a decoder's: the latest is shown at the next paint
class SDRGUI_API GLVideoWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit GLVideoWidget(QWidget *parent = nullptr);
    ~GLVideoWidget();

    //! From any thread. The widget keeps the frame until a newer one has been shown, but not GPU frames while hidden
    void setFrame(GLVideoFramePtr frame);
    //! From any thread: if frames of the type can be shown. GPU types can't until the widget's OpenGL context
    //! exists, or after one has failed to be shown, so the decoder should copy them to memory instead
    bool canShow(GLVideoFrame::Type type) const { return m_renderer.canShow(type); }

    void clear();                                   //!< No video, letting go of all frames given
    void setMessage(const QString& message);        //!< Shown in place of video, until set empty
    void setOverlay(const QString& text, double seconds);   //!< Over the video, bottom right, lines centred
    //! Over everything, blended, as large as pixelSize() (scaled to fit if not), until set null
    void setGuide(const QImage& image);
    QSize pixelSize() const;                        //!< Of the widget, in device pixels
    void setKeepAspectRatio(bool keep);
    void setDeinterlace(bool deinterlace);          //!< Of GPU frames the renderer deinterlaces (D3D11)
    QSize getFrameSize() const;                     //!< Of the last frame, empty if none

protected:
    void initializeGL() override;
    void paintGL() override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void cleanup();
    QImage messageImage(int width, int height);
    QImage overlayImage(int videoHeight);

    GLVideoRenderer m_renderer;
    mutable QMutex m_mutex;         //!< For m_pending and m_hidden
    GLVideoFramePtr m_pending;      //!< Given, to be shown
    GLVideoFramePtr m_current;      //!< Being shown
    bool m_hidden;                  //!< So GPU frames given aren't kept
    QString m_message;
    QImage m_messageImage;
    QString m_overlay;
    QImage m_overlayImage;
    int m_overlayHeight;            //!< Video height the overlay image was made for
    QTimer m_overlayTimer;
    QImage m_guide;
    bool m_keepAspect;
    bool m_deinterlace;
    QSize m_frameSize;
};

#endif // SDRGUI_GUI_GLVIDEOWIDGET_H_
