/****************************************************************************
 * Copyright (C) 2016 Edouard Griffiths, F4EXB
 * Modifications made to:
 * - use the widget horizontally
 * - differentiate each area with a different color
 * - allow overload by 25% with indication of 100% threshold and overload
 * - make it generic to fit many cases: VU, signal strength ...
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SDRBASE_GUI_LEVELMETER_H_
#define SDRBASE_GUI_LEVELMETER_H_

#include <QTime>
#include <QWidget>

#include "dsp/dsptypes.h"
#include "gui/scaleengine.h"

/**
 * Widget which displays a vertical audio level meter, indicating the
 * RMS and peak levels of the window of audio samples most recently analyzed
 * by the Engine.
 */
class LevelMeter : public QWidget
{
    Q_OBJECT

public:
    LevelMeter(QWidget *parent = 0);
    virtual ~LevelMeter();

    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent * event);

public slots:
    void reset();
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

protected slots:
    void redrawTimerExpired();

protected:
    /**
     * Height of RMS level bar.
     * Range 0.0 - 1.0.
     */
    qreal m_avgLevel;

    /**
     * Most recent peak level.
     * Range 0.0 - 1.0.
     */
    qreal m_peakLevel;

    /**
     * Height of peak level bar.
     * This is calculated by decaying m_peakLevel depending on the
     * elapsed time since m_peakLevelChanged, and the value of m_decayRate.
     */
    qreal m_decayedPeakLevel;

    /**
     * Time at which m_peakLevel was last changed.
     */
    QTime m_peakLevelChanged;

    /**
     * Rate at which peak level bar decays.
     * Expressed in level units / millisecond.
     */
    qreal m_peakDecayRate;

    /**
     * High watermark of peak level.
     * Range 0.0 - 1.0.
     */
    qreal m_peakHoldLevel;

    /**
     * Time at which m_peakHoldLevel was last changed.
     */
    QTime m_peakHoldLevelChanged;

    QTimer *m_redrawTimer;

    QColor m_avgColor;
    QColor m_peakColor;
    QColor m_decayedPeakColor;

    ScaleEngine m_scaleEngine;
    QPixmap *m_backgroundPixmap;

    virtual void render(QPainter *painter) = 0;
    virtual void resized() = 0;

    /** Shift the x coordinate so it does not fall right on 0 or width-1 */
    int shiftx(int val, int width)
    {
        return val == 0 ? 1 : val == width-1 ? width-2 : val;
    }
};

class LevelMeterVU : public LevelMeter
{
public:
    LevelMeterVU(QWidget *parent = 0);
    virtual ~LevelMeterVU();
protected:
    virtual void render(QPainter *painter);
    virtual void resized();
};

class LevelMeterSignalDB : public LevelMeter
{
public:
    typedef enum
    {
        ColorGold,
        ColorGreenYellow,
		ColorGreenAndBlue
    } ColorTheme;

    LevelMeterSignalDB(QWidget *parent = 0);
    virtual ~LevelMeterSignalDB();

    void setColorTheme(ColorTheme colorTheme) { m_colorTheme = colorTheme; }

    static const QColor m_avgColor[3];
    static const QColor m_decayedPeakColor[3];
    static const QColor m_peakColor[3];

protected:
    virtual void render(QPainter *painter);
    virtual void resized();

    ColorTheme m_colorTheme;
};

#endif /* SDRBASE_GUI_LEVELMETER_H_ */
