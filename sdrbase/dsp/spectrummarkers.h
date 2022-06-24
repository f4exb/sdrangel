///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SPECTRUMMARKERS_H
#define INCLUDE_SPECTRUMMARKERS_H

#include <QString>
#include <QPointF>
#include <QColor>

#include "export.h"

struct SDRBASE_API SpectrumHistogramMarker
{
    enum SpectrumMarkerType
    {
        SpectrumMarkerTypeManual,
        SpectrumMarkerTypePower,
        SpectrumMarkerTypePowerMax
    };

    QPointF m_point;
    float m_frequency;
    int m_fftBin;
    float m_power;
    bool m_holdReset;
    float m_powerMax;
    SpectrumMarkerType m_markerType;
    QColor m_markerColor;
    bool m_show;
    QString m_frequencyStr;
    QString m_powerStr;
    QString m_deltaFrequencyStr;
    QString m_deltaPowerStr;
    static const int m_maxNbOfMarkers = 4;

    SpectrumHistogramMarker() :
        m_point(0, 0),
        m_frequency(0),
        m_fftBin(0),
        m_power(0),
        m_holdReset(true),
        m_powerMax(0),
        m_markerType(SpectrumMarkerTypeManual),
        m_markerColor("white"),
        m_show(true),
        m_frequencyStr(),
        m_powerStr(),
        m_deltaFrequencyStr(),
        m_deltaPowerStr()
    {}

    SpectrumHistogramMarker(
        const QPointF& point,
        float frequency,
        int   fftBin,
        float power,
        bool  holdReset,
        float powerMax,
        SpectrumMarkerType markerType,
        QColor markerColor,
        bool show,
        const QString& frequencyStr,
        const QString& powerStr,
        const QString& deltaFrequencyStr,
        const QString& deltaPowerStr
    ) :
        m_point(point),
        m_frequency(frequency),
        m_fftBin(fftBin),
        m_power(power),
        m_holdReset(holdReset),
        m_powerMax(powerMax),
        m_markerType(markerType),
        m_markerColor(markerColor),
        m_show(show),
        m_frequencyStr(frequencyStr),
        m_powerStr(powerStr),
        m_deltaFrequencyStr(deltaFrequencyStr),
        m_deltaPowerStr(deltaPowerStr)
    {}

    SpectrumHistogramMarker(const SpectrumHistogramMarker& other) = default;
    SpectrumHistogramMarker& operator=(const SpectrumHistogramMarker&) = default;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

struct SDRBASE_API SpectrumWaterfallMarker
{
    QPointF m_point;
    float m_frequency;
    float m_time;
    QColor m_markerColor;
    bool m_show;
    QString m_frequencyStr;
    QString m_timeStr;
    QString m_deltaFrequencyStr;
    QString m_deltaTimeStr;
    static const int m_maxNbOfMarkers = 4;

    SpectrumWaterfallMarker() :
        m_point(0, 0),
        m_frequency(0),
        m_time(0),
        m_markerColor("white"),
        m_show(true),
        m_frequencyStr(),
        m_timeStr(),
        m_deltaFrequencyStr(),
        m_deltaTimeStr()
    {}

    SpectrumWaterfallMarker(
        const QPointF& point,
        float frequency,
        float time,
        QColor markerColor,
        bool show,
        const QString& frequencyStr,
        const QString& timeStr,
        const QString& deltaFrequencyStr,
        const QString& deltaTimeStr
    ) :
        m_point(point),
        m_frequency(frequency),
        m_time(time),
        m_markerColor(markerColor),
        m_show(show),
        m_frequencyStr(frequencyStr),
        m_timeStr(timeStr),
        m_deltaFrequencyStr(deltaFrequencyStr),
        m_deltaTimeStr(deltaTimeStr)
    {}

    SpectrumWaterfallMarker(const SpectrumWaterfallMarker& other) = default;
    SpectrumWaterfallMarker& operator=(const SpectrumWaterfallMarker&) = default;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

struct SDRBASE_API SpectrumAnnotationMarker
{
    enum ShowState
    {
        Hidden,
        ShowTop,
        ShowFull,
        ShowText
    };

    qint64 m_startFrequency;
    uint32_t m_bandwidth;
    QColor m_markerColor;
    ShowState m_show;
    QString m_text;
    float m_startPos;
    float m_stopPos;

    SpectrumAnnotationMarker() :
        m_startFrequency(0),
        m_bandwidth(0),
        m_markerColor("white"),
        m_show(ShowTop),
        m_text("Text"),
        m_startPos(0.0f),
        m_stopPos(1.0f)
    {}

    SpectrumAnnotationMarker(
        qint64 startFrequency,
        uint32_t bandwidth,
        QColor markerColor,
        ShowState show,
        const QString& text
    ) :
        m_startFrequency(startFrequency),
        m_bandwidth(bandwidth),
        m_markerColor(markerColor),
        m_show(show),
        m_text(text),
        m_startPos(0.0f),
        m_stopPos(1.0f)
    {}

    SpectrumAnnotationMarker(const SpectrumAnnotationMarker& other) = default;
    SpectrumAnnotationMarker& operator=(const SpectrumAnnotationMarker&) = default;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif // INCLUDE_SPECTRUMMARKERS_H
