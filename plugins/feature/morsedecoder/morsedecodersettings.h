///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_MORSEDECODERSETTINGS_H_
#define INCLUDE_FEATURE_MORSEDECODERSETTINGS_H_

#include <QString>
#include <QByteArray>

class Serializable;

struct MorseDecoderSettings
{
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    QString m_logFilename;
    bool m_logEnabled;
    bool m_auto; //!< Auto pitch and speed
    bool m_showThreshold; //!< Show decoder threshold on scope imaginary trace

    MorseDecoderSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const MorseDecoderSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static QString formatText(const QString& text);

    static const QStringList m_channelURIs;
};

#endif // INCLUDE_FEATURE_MORSEDECODERSETTINGS_H_
