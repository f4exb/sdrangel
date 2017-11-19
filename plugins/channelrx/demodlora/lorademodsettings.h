
#ifndef PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include <stdint.h>

class Serializable;

struct LoRaDemodSettings
{
    int m_centerFrequency;
    int m_bandwidthIndex;
    int m_spread;
    uint32_t m_rgbColor;
    QString m_title;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;

    static const int bandwidths[];
    static const int nb_bandwidths;

    LoRaDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_DEMODLORA_LORADEMODSETTINGS_H_ */
