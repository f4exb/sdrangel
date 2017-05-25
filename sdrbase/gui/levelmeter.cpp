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

#include "gui/levelmeter.h"

#include <math.h>

#include <QPainter>
#include <QTimer>
#include <QDebug>

// Constants
const int RedrawInterval = 100; // ms
const qreal PeakDecayRate = 0.001;
const int PeakHoldLevelDuration = 2000; // ms

LevelMeter::LevelMeter(QWidget *parent)
    :   QWidget(parent)
    ,   m_avgLevel(0.0)
    ,   m_peakLevel(0.0)
    ,   m_decayedPeakLevel(0.0)
    ,   m_peakDecayRate(PeakDecayRate)
    ,   m_peakHoldLevel(0.0)
    ,   m_redrawTimer(new QTimer(this))
    ,   m_avgColor(0xff, 0x8b, 0x00, 128)          // color mapper foreground
    ,   m_peakColor(Qt::red)                       // just red 100% opaque
    ,   m_decayedPeakColor(0x97, 0x54, 0x00, 128)  // color mapper 59%
    ,   m_backgroundPixmap(0)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    setMinimumWidth(30);

    connect(m_redrawTimer, SIGNAL(timeout()), this, SLOT(redrawTimerExpired()));
    m_redrawTimer->start(RedrawInterval);
}

LevelMeter::~LevelMeter()
{
    if (m_backgroundPixmap) {
        delete m_backgroundPixmap;
    }
}

void LevelMeter::reset()
{
    m_avgLevel = 0.0;
    m_peakLevel = 0.0;
    update();
}

void LevelMeter::levelChanged(qreal avgLevel, qreal peakLevel, int numSamples)
{
    // Smooth the RMS signal
    const qreal smooth = pow(qreal(0.9), static_cast<qreal>(numSamples) / 256); // TODO: remove this magic number
    m_avgLevel = (m_avgLevel * smooth) + (avgLevel * (1.0 - smooth));

    if (peakLevel > m_decayedPeakLevel) {
        m_peakLevel = peakLevel;
        m_decayedPeakLevel = peakLevel;
        m_peakLevelChanged.start();
    }

    if (peakLevel > m_peakHoldLevel) {
        m_peakHoldLevel = peakLevel;
        m_peakHoldLevelChanged.start();
    }

    update();
}

void LevelMeter::redrawTimerExpired()
{
    // Decay the peak signal
    const int elapsedMs = m_peakLevelChanged.elapsed();
    const qreal decayAmount = m_peakDecayRate * elapsedMs;
    if (decayAmount < m_peakLevel)
        m_decayedPeakLevel = m_peakLevel - decayAmount;
    else
        m_decayedPeakLevel = 0.0;

    // Check whether to clear the peak hold level
    if (m_peakHoldLevelChanged.elapsed() > PeakHoldLevelDuration)
        m_peakHoldLevel = 0.0;

    update();
}

void LevelMeter::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    render(&painter);
}

void LevelMeter::resizeEvent(QResizeEvent * event)
{
    Q_UNUSED(event)

    resized();
}

// ====================================================================

LevelMeterVU::LevelMeterVU(QWidget *parent) :
        LevelMeter(parent)
{
    m_scaleEngine.setFont(font());
    m_scaleEngine.setOrientation(Qt::Horizontal);
    m_scaleEngine.setRange(Unit::Percent, 0, 100);

    resized();
}

LevelMeterVU::~LevelMeterVU()
{
}

void LevelMeterVU::resized()
{
    if (m_backgroundPixmap)
    {
        delete m_backgroundPixmap;
    }

    m_backgroundPixmap = new QPixmap(rect().width(), rect().height());
    m_backgroundPixmap->fill(QColor(42, 42, 42, 255));

    QPainter painter(m_backgroundPixmap);
    QRect barTop = m_backgroundPixmap->rect();
    barTop.setBottom(0.5 * rect().height() - 2);
    barTop.setTop(2);
    barTop.setLeft(0.75* rect().width());
    painter.fillRect(barTop, Qt::red);

    QRect bar = m_backgroundPixmap->rect();

    // 100% full height white line
    painter.setPen(Qt::white);
//    painter.drawLine(0.75*bar.width(), 0, 0.75*bar.width(), bar.height());

    m_scaleEngine.setSize(0.75*bar.width());
    const ScaleEngine::TickList& scaleTickList = m_scaleEngine.getTickList();


    for (int i = 0; i < scaleTickList.count(); i++)
    {
//        qDebug() << "LevelMeterVU::resized: tick #" << i
//                << " major: " << scaleTickList[i].major
//                << " pos: " << scaleTickList[i].pos
//                << " text: " << scaleTickList[i].text
//                << " textPos: " << scaleTickList[i].textPos
//                << " textSize: " << scaleTickList[i].textSize;
        const ScaleEngine::Tick tick = scaleTickList[i];

        if(tick.major)
        {
            if ((tick.textSize > 0) && (tick.textPos > 0))
            {
                painter.drawText(QPointF(tick.textPos - (tick.textSize/2) - 2, bar.height()/2), tick.text);
            }

            painter.drawLine(shiftx(tick.pos, bar.width()), 0, shiftx(scaleTickList[i].pos, bar.width()), bar.height());
        }
        else
        {
            painter.drawLine(tick.pos, bar.height()/4, scaleTickList[i].pos, bar.height()/2);
        }
    }
}

void LevelMeterVU::render(QPainter *painter)
{
    painter->drawPixmap(rect(), *m_backgroundPixmap);

    QRect bar = rect();

    // Bottom moving gauge

    bar.setTop(0.5 * rect().height() + 2);
    bar.setBottom(rect().height() - 1);

    bar.setRight(rect().right() - (1.0 - 0.75*m_peakHoldLevel) * rect().width());
    bar.setLeft(bar.right() - 2);
    painter->fillRect(bar, m_peakColor);

    bar.setBottom(0.75*rect().height());

    bar.setRight(rect().right() - (1.0 - 0.75*m_avgLevel) * rect().width());
    bar.setLeft(1);
    painter->fillRect(bar, m_avgColor);

    bar.setTop(0.75 * rect().height() + 1);
    bar.setBottom(rect().height() - 1);

    bar.setRight(rect().right() - (1.0 - 0.75*m_decayedPeakLevel) * rect().width());
    painter->fillRect(bar, m_decayedPeakColor);

    // borders
    painter->setPen(QColor(0,0,0));
    painter->drawLine(0, 0, rect().width() - 2, 0);
    painter->drawLine(0, rect().height() - 1, 0, 0);
    painter->setPen(QColor(80,80,80));
    painter->drawLine(1, rect().height() - 1, rect().width() - 1, rect().height() - 1);
    painter->drawLine(rect().width() - 1, rect().height() - 1, rect().width() - 1, 0);
}

// ====================================================================

const QColor LevelMeterSignalDB::m_avgColor[3] = {
        QColor(0xff, 0x8b, 0x00, 128),
        QColor(0x8c, 0xff, 0x00, 128),
		QColor(0x8c, 0xff, 0x00, 128)
};

const QColor LevelMeterSignalDB::m_decayedPeakColor[3] = {
        QColor(0x97, 0x54, 0x00, 128),
        QColor(0x53, 0x96, 0x00, 128),
        QColor(0x00, 0x96, 0x53, 128)
};

const QColor LevelMeterSignalDB::m_peakColor[3] = {
        Qt::red,
        Qt::green,
		Qt::green
};

LevelMeterSignalDB::LevelMeterSignalDB(QWidget *parent) :
        LevelMeter(parent),
        m_colorTheme(ColorGold)
{
    m_scaleEngine.setFont(font());
    m_scaleEngine.setOrientation(Qt::Horizontal);
    m_scaleEngine.setRange(Unit::Decibel, -100, 0);

    resized();
}

LevelMeterSignalDB::~LevelMeterSignalDB()
{
}

void LevelMeterSignalDB::resized()
{
    if (m_backgroundPixmap)
    {
        delete m_backgroundPixmap;
    }

    m_backgroundPixmap = new QPixmap(rect().width(), rect().height());
    m_backgroundPixmap->fill(QColor(42, 42, 42, 255));

    QPainter painter(m_backgroundPixmap);
    QRect bar = m_backgroundPixmap->rect();

    // 100% full height white line
    painter.setPen(Qt::white);
    painter.setFont(font());

    m_scaleEngine.setSize(bar.width());
    const ScaleEngine::TickList& scaleTickList = m_scaleEngine.getTickList();


    for (int i = 0; i < scaleTickList.count(); i++)
    {
//        qDebug() << "LevelMeterVU::resized: tick #" << i
//                << " major: " << scaleTickList[i].major
//                << " pos: " << scaleTickList[i].pos
//                << " text: " << scaleTickList[i].text
//                << " textPos: " << scaleTickList[i].textPos
//                << " textSize: " << scaleTickList[i].textSize;
        const ScaleEngine::Tick tick = scaleTickList[i];

        if(tick.major)
        {
            if ((tick.textSize > 0) && (tick.textPos > 0))
            {
                painter.drawText(QPointF(tick.textPos - (tick.textSize/2) - 2, bar.height()/2 - 1), tick.text);
            }

            painter.drawLine(shiftx(tick.pos, bar.width()), 0, shiftx(scaleTickList[i].pos,bar.width()), bar.height());
        }
        else
        {
            painter.drawLine(tick.pos, bar.height()/4, scaleTickList[i].pos, bar.height()/2);
        }
    }
}

void LevelMeterSignalDB::render(QPainter *painter)
{
    painter->drawPixmap(rect(), *m_backgroundPixmap);

    QRect bar = rect();

    // Bottom moving gauge

    bar.setTop(0.5 * rect().height() + 2);
    bar.setBottom(0.75*rect().height());

    bar.setRight(rect().right() - (1.0 - m_avgLevel) * rect().width());
    bar.setLeft(1);
    painter->fillRect(bar, m_avgColor[m_colorTheme]);

    bar.setTop(0.75 * rect().height() + 1);
    bar.setBottom(rect().height() - 1);

    bar.setRight(rect().right() - (1.0 - m_decayedPeakLevel) * rect().width());
    painter->fillRect(bar, m_decayedPeakColor[m_colorTheme]);

    bar.setTop(0.5 * rect().height() + 2);
    bar.setBottom(rect().height() - 1);

    bar.setRight(rect().right() - (1.0 - m_peakHoldLevel) * rect().width());
    bar.setLeft(bar.right() - 2);
    painter->fillRect(bar, m_peakColor[m_colorTheme]);

    // borders
    painter->setPen(QColor(0,0,0));
    painter->drawLine(0, 0, rect().width() - 2, 0);
    painter->drawLine(0, rect().height() - 1, 0, 0);
    painter->setPen(QColor(80,80,80));
    painter->drawLine(1, rect().height() - 1, rect().width() - 1, rect().height() - 1);
    painter->drawLine(rect().width() - 1, rect().height() - 1, rect().width() - 1, 0);
}

