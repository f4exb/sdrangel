
#ifndef PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_

#include <QByteArray>
#include <stdint.h>

class Serializable;

struct LoRaDemodSettings
{
    int m_centerFrequency;
    int m_bandwidth;
    int m_spread;
    uint32_t m_rgbColor;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    LoRaDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_ */
