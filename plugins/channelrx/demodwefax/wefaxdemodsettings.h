///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_WEFAXDEMODSETTINGS_H
#define INCLUDE_WEFAXDEMODSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "dsp/dsptypes.h"

class Serializable;

struct WefaxDemodSettings
{
    static constexpr int MaxRowsLimit = 20000;

    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    int m_ioc;
    int m_linesPerMinute;
    bool m_autoMode;
    bool m_inverted;
    int m_minimumPhasingLines;
    double m_startConfirmSeconds;
    double m_stopConfirmSeconds;
    double m_manualClockCorrectionPpm;
    int m_maxRows;
    bool m_autoSave;
    QString m_autoSavePath;
    bool m_displayInverted;
    int m_displayContrast;
    int m_displayThreshold;
    int m_horizontalAlignment;
    double m_displaySlantCorrectionPpm;
    int m_displayRotation;
    int m_displayZoomPercent;
    bool m_autoScroll;
    bool m_autoSlant;           // Straighten the image from its vertical lines

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    WefaxDemodSettings();

    void resetToDefaults();
    void validate();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const WefaxDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }

    int internalSampleRate() const;
    // True when changing any of these keys invalidates the image in progress.
    static bool endsCapture(const QStringList& settingsKeys);
};

#endif // INCLUDE_WEFAXDEMODSETTINGS_H
