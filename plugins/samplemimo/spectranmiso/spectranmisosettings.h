///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef _SPECTRANMISO_SPECTRANMISOSETTINGS_H_
#define _SPECTRANMISO_SPECTRANMISOSETTINGS_H_

#include <QString>
#include <QByteArray>

#include "util/message.h"

enum SpectranModel {
    SPECTRAN_V6,
    SPECTRAN_V6Eco
};

enum SpectranRxChannel {
    SPECTRAN_RX_CHANNEL_1 = 0,
    SPECTRAN_RX_CHANNEL_2,
    SPECTRAN_RX_CHANNEL_END
};

enum SpectranMISOMode {
    SPECTRANMISO_MODE_RX_IQ = 0,
    SPECTRANMISO_MODE_TX_IQ,
    SPECTRANMISO_MODE_RXTX_IQ,
    SPECTRANMISO_MODE_RX_RAW,
    SPECTRANMISO_MODE_2RX_RAW_INTL, // 2 Rx interleaved
    SPECTRANMISO_MODE_2RX_RAW,      // 2 Rx non interleaved
    SPECTRANMISO_MODE_END
};

enum SpectranMISOClockRate {
    SPECTRANMISO_CLOCK_92M = 0,
    SPECTRANMISO_CLOCK_122M,
    SPECTRANMISO_CLOCK_245M,
    SPECTRANMISO_CLOCK_46M,
    SPECTRANMISO_CLOCK_61M,
    SPECTRANMISO_CLOCK_76M,
    SPECTRANMISO_CLOCK_END
};

struct SpectranMISOSettings {
    SpectranMISOMode m_mode;
    SpectranRxChannel m_rxChannel;
    quint64 m_rxCenterFrequency;
    quint64 m_txCenterFrequency;
    int m_sampleRate; //!< sample rate in Hz (IQ mode)
    SpectranMISOClockRate m_clockRate; //!< clock rate (raw mode)
    int m_logDecimation; //!< log decimation factor (raw mode) 0 to 9
    int m_streamIndex;
    int m_spectrumStreamIndex; //!< spectrum source
    bool m_streamLock; //!< Lock stream control and spectrum to same Rx
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    int m_txDrive;
    static const int m_maxTxSampleRate = 20000000; // 20 MHz

    SpectranMISOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    SpectranMISOSettings& operator=(const SpectranMISOSettings&) = default;
    void applySettings(const QStringList& settingsKeys, const SpectranMISOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    static bool isRawMode(const SpectranMISOMode& mode);
    static bool isDualRx(const SpectranMISOMode& mode);
    static bool isRxModeSingle(const SpectranMISOMode& mode);
    static bool isTxMode(const SpectranMISOMode& mode);
    static bool isDecimationEnabled(const SpectranMISOMode& mode);

    static const QMap<SpectranMISOMode, QString> m_modeDisplayNames;
};

class MsgConfigureSpectranMISO : public Message {
    MESSAGE_CLASS_DECLARATION

public:
    const SpectranMISOSettings& getSettings() const { return m_settings; }
    const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
    bool getForce() const { return m_force; }

    static MsgConfigureSpectranMISO* create(const SpectranMISOSettings& settings, const QList<QString>& settingsKeys, bool force) {
        return new MsgConfigureSpectranMISO(settings, settingsKeys, force);
    }

private:
    SpectranMISOSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_force;

    MsgConfigureSpectranMISO(const SpectranMISOSettings& settings, const QList<QString>& settingsKeys, bool force) :
        Message(),
        m_settings(settings),
        m_settingsKeys(settingsKeys),
        m_force(force)
    { }
};

#endif // _SPECTRANMISO_SPECTRANMISOSETTINGS_H_
