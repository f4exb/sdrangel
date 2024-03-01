#ifndef SPY_SERVER_H
#define SPY_SERVER_H

#include <QtCore>

class SpyServerProtocol {

public:

    static constexpr int ProtocolID = (2<<24) | 1700;

    enum Command {
        setStreamingMode = 0,
        setStreamingEnabled = 1,
        setGain = 2,
        setIQFormat = 100,
        setCenterFrequency = 101,
        setIQDecimation = 102,
    };

    enum Message {
        DeviceMessage = 0,
        StateMessage = 1,
        IQ8MMessage = 100,
        IQ16Message = 101,
        IQ24Message = 102,
        IQ32Message = 103
    };

    struct Header {
        quint32 m_id;
        quint32 m_message;
        quint32 m_unused1;
        quint32 m_unused2;
        quint32 m_size;
    };

    struct Device {
        quint32 m_device;
        quint32 m_serial;
        quint32 m_sampleRate;
        quint32 m_unused1;
        quint32 m_decimationStages;         // 8 for Airspy HF, 11 for Airspy, 9 for E4000/R828D/R820
        quint32 m_unused2;
        quint32 m_maxGainIndex;             // 8 for Airspy HF, 21 for Airspy, 14 for E4000, 29 for R828D/R820
        quint32 m_minFrequency;
        quint32 m_maxFrequency;
        quint32 m_sampleBits;
        quint32 m_minDecimation; // Set when maximum_bandwidth is set in spyserver.config
        quint32 m_unused3;
    };

    struct State {
        quint32 m_controllable;
        quint32 m_gain;
        quint32 m_deviceCenterFrequency;
        quint32 m_iqCenterFrequency;
        quint32 m_unused1;
        quint32 m_unused2;
        quint32 m_unused3;
        quint32 m_unused4;
        quint32 m_unused5;
    };

    static void encodeUInt32(quint8 *p, quint32 data)
    {
        p[3] = (data >> 24) & 0xff;
        p[2] = (data >> 16) & 0xff;
        p[1] = (data >> 8) & 0xff;
        p[0] = data & 0xff;
    }

    static quint32 extractUInt32(quint8 *p)
    {
        quint32 data;
        data = (p[0] & 0xff)
            | ((p[1] & 0xff) << 8)
            | ((p[2] & 0xff) << 16)
            | ((p[3] & 0xff) << 24);
        return data;
    }

};

#endif /* SPY_SERVER_H */
