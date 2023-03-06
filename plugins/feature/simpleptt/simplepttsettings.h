///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_SIMPLEPTTSETTINGS_H_
#define INCLUDE_FEATURE_SIMPLEPTTSETTINGS_H_

#include <QByteArray>
#include <QString>

class Serializable;

struct SimplePTTSettings
{
    enum GPIOControl
    {
        GPIONone,
        GPIORx,
        GPIOTx
    };

    QString m_title;
    quint32 m_rgbColor;
    int m_rxDeviceSetIndex;
    int m_txDeviceSetIndex;
    unsigned int m_rx2TxDelayMs;
    unsigned int m_tx2RxDelayMs;
    QString m_audioDeviceName; //!< for Vox
    int m_voxLevel; //!< Vox threshold level in dB
    int m_voxHold;  //!< Vox hold in milliseconds
    bool m_vox;
    bool m_voxEnable;
    GPIOControl m_gpioControl;

    bool m_rx2txGPIOEnable;
    int m_rx2txGPIOMask;
    int m_rx2txGPIOValues;
    bool m_rx2txCommandEnable;
    QString m_rx2txCommand;
    bool m_tx2rxGPIOEnable;
    int m_tx2rxGPIOMask;
    int m_tx2rxGPIOValues;
    bool m_tx2rxCommandEnable;
    QString m_tx2rxCommand;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    SimplePTTSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const SimplePTTSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif // INCLUDE_FEATURE_SIMPLEPTTSETTINGS_H_
