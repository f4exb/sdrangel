#ifndef TCPIQPLOTSETTINGS_H
#define TCPIQPLOTSETTINGS_H

#include "util/simpleserializer.h"
#include "settings/rollupstate.h"
#include <QColor>

class Serializable;

class Serializable;

struct TcpIqPlotSettings
{
    TcpIqPlotSettings() {
        resetToDefaults();
    }

    void resetToDefaults() {
        m_title = "TCP IQ Plot";
        m_rgbColor = QColor(100, 150, 255).rgb();
        m_port = 12345;
        m_useReverseAPI = false;
        m_reverseAPIAddress = "127.0.0.1";
        m_reverseAPIPort = 8888;
        m_reverseAPIDeviceIndex = 0;
        m_reverseAPIChannelIndex = 0;
        m_streamIndex = 0;
        m_rollupState = nullptr;
    }

    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }

    QByteArray serialize() const {
        SimpleSerializer s(1);
        s.writeString(1, m_title);
        s.writeU32(2, m_rgbColor);
        s.writeS32(3, m_port);
        s.writeBool(4, m_useReverseAPI);
        s.writeString(5, m_reverseAPIAddress);
        s.writeU32(6, m_reverseAPIPort);
        s.writeU32(7, m_reverseAPIDeviceIndex);
        s.writeU32(8, m_reverseAPIChannelIndex);
        s.writeS32(9, m_streamIndex);
        if (m_rollupState) {
            s.writeBlob(10, m_rollupState->serialize());
        }
        return s.final();
    }

    bool deserialize(const QByteArray& data) {
        SimpleDeserializer d(data);
        if (!d.isValid()) {
            resetToDefaults();
            return false;
        }
        if (d.getVersion() == 1) {
            uint32_t utmp;
            d.readString(1, &m_title, "TCP IQ Plot");
            d.readU32(2, &m_rgbColor, QColor(100, 150, 255).rgb());
            d.readS32(3, (qint32*)&m_port, 12345);
            d.readBool(4, &m_useReverseAPI, false);
            d.readString(5, &m_reverseAPIAddress, "127.0.0.1");
            d.readU32(6, &utmp, 8888);
            m_reverseAPIPort = utmp;
            d.readU32(7, &utmp, 0);
            m_reverseAPIDeviceIndex = utmp;
            d.readU32(8, &utmp, 0);
            m_reverseAPIChannelIndex = utmp;
            d.readS32(9, (qint32*)&m_streamIndex, 0);
            if (m_rollupState) {
                QByteArray bytetmp;
                d.readBlob(10, &bytetmp);
                m_rollupState->deserialize(bytetmp);
            }
            return true;
        } else {
            resetToDefaults();
            return false;
        }
    }

    QString m_title;
    QRgb m_rgbColor;
    int m_port;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_streamIndex;
    Serializable *m_rollupState;
};

#endif // TCPIQPLOTSETTINGS_H
