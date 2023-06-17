///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AUDIOCATSISO_AUDIOCATSISOSETTINGS_H_
#define _AUDIOCATSISO_AUDIOCATSISOSETTINGS_H_

#include <QString>
#include "audio/audiodeviceinfo.h"
#include "util/message.h"

struct AudioCATSISOSettings {

    class MsgPTT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getPTT() const { return m_ptt; }

        static MsgPTT* create(bool ptt) {
            return new MsgPTT(ptt);
        }

    protected:
        bool m_ptt;

        MsgPTT(bool ptt) :
            Message(),
            m_ptt(ptt)
        { }
    };

    class MsgCATConnect : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getConnect() const { return m_connect; }

        static MsgCATConnect* create(bool connect) {
            return new MsgCATConnect(connect);
        }

    protected:
        bool m_connect;

        MsgCATConnect(bool connect) :
            Message(),
            m_connect(connect)
        { }
    };

    class MsgCATReportStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        enum Status {
            StatusNone,
            StatusConnected,
            StatusError
        };

        Status getStatus() const { return m_status; }

        static MsgCATReportStatus* create(Status status) {
            return new MsgCATReportStatus(status);
        }

    protected:
        Status m_status;

        MsgCATReportStatus(Status status) :
            Message(),
            m_status(status)
        { }
    };

	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER
	} fcPos_t;

    enum IQMapping {
        LR,
        RL,
        L,
        R
    };

    quint64 m_rxCenterFrequency;
    quint64 m_txCenterFrequency;
    bool    m_transverterMode;
    qint64  m_transverterDeltaFrequency;
    bool    m_iqOrder;
    bool    m_txEnable;
    bool    m_pttSpectrumLink;

    QString      m_rxDeviceName;       // Including realm, as from getFullDeviceName below
    IQMapping    m_rxIQMapping;
    unsigned int m_log2Decim;
    fcPos_t      m_fcPosRx;
    bool         m_dcBlock;
    bool         m_iqCorrection;
    float        m_rxVolume;

    QString      m_txDeviceName;       // Including realm, as from getFullDeviceName below
    IQMapping    m_txIQMapping;
    int          m_txVolume; //!< dB

    QString      m_catDevicePath;
    uint32_t     m_hamlibModel; //!< Hamlib model number
    int          m_catSpeedIndex;
    int          m_catDataBitsIndex;
    int          m_catStopBitsIndex;
    int          m_catHandshakeIndex;
    int          m_catPTTMethodIndex;
    bool         m_catDTRHigh;
    bool         m_catRTSHigh;
    uint32_t     m_catPollingMs; //!< CAT polling interval in ms

    static const int m_catSpeeds[];
    static const int m_catDataBits[];
    static const int m_catStopBits[];
    static const int m_catHandshakes[];
    static const int m_catPTTMethods[];

    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	AudioCATSISOSettings();
    AudioCATSISOSettings(const AudioCATSISOSettings& other);
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    AudioCATSISOSettings& operator=(const AudioCATSISOSettings&) = default;
    void applySettings(const QStringList& settingsKeys, const AudioCATSISOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    // Append realm to device names, because there may be multiple devices with the same name on Windows
    static QString getFullDeviceName(const AudioDeviceInfo &deviceInfo)
    {
        QString realm = deviceInfo.realm();
        if (realm != "" && realm != "default" && realm != "alsa")
            return deviceInfo.deviceName() + " " + realm;
        else
            return deviceInfo.deviceName();
    }

    static int getSampleRateFromIndex(unsigned int index);
};

#endif /* _AUDIOCATSISO_AUDIOCATSISOSETTINGS_H_ */
