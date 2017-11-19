#include "lorademodsettings.h"

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "lorademodsettings.h"

const int LoRaDemodSettings::bandwidths[] = {7813,15625,20833,31250,62500};
const int LoRaDemodSettings::nb_bandwidths = 5;

LoRaDemodSettings::LoRaDemodSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void LoRaDemodSettings::resetToDefaults()
{
    m_bandwidthIndex = 0;
    m_spread = 0;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "LoRa Demodulator";
}

QByteArray LoRaDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_centerFrequency);
    s.writeS32(2, m_bandwidthIndex);
    s.writeS32(3, m_spread);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(5, m_channelMarker->serialize());
    }

    s.writeString(6, m_title);

    return s.final();
}

bool LoRaDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;

        d.readS32(1, &m_centerFrequency, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spread, 0);

        if (m_spectrumGUI) {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_channelMarker) {
            d.readBlob(5, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(6, &m_title, "LoRa Demodulator");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
