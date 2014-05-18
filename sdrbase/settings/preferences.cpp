#include "settings/preferences.h"
#include "util/simpleserializer.h"

Preferences::Preferences()
{
	resetToDefaults();
}

void Preferences::resetToDefaults()
{
	m_sourceType.clear();
	m_sourceDevice.clear();
	m_audioType.clear();
	m_audioDevice.clear();
}

QByteArray Preferences::serialize() const
{
	SimpleSerializer s(1);
	s.writeString(1, m_sourceType);
	s.writeString(2, m_sourceDevice);
	s.writeString(3, m_audioType);
	s.writeString(4, m_audioDevice);
	return s.final();
}

bool Preferences::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readString(1, &m_sourceType);
		d.readString(2, &m_sourceDevice);
		d.readString(3, &m_audioType);
		d.readString(4, &m_audioDevice);
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}
