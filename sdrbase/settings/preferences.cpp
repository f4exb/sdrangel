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
	m_sourceIndex = 0;
	m_latitude = 0.0;
	m_longitude = 0.0;
	m_useLogFile = false;
	m_logFileName = "sdrangel.log";
	m_consoleMinLogLevel = QtDebugMsg;
    m_fileMinLogLevel = QtDebugMsg;
}

QByteArray Preferences::serialize() const
{
	SimpleSerializer s(1);
	s.writeString(1, m_sourceType);
	s.writeString(2, m_sourceDevice);
	s.writeString(3, m_audioType);
	s.writeString(4, m_audioDevice);
	s.writeS32(5, m_sourceIndex);
	s.writeFloat(6, m_latitude);
	s.writeFloat(7, m_longitude);
	s.writeS32(8, (int) m_consoleMinLogLevel);
	s.writeBool(9, m_useLogFile);
	s.writeString(10, m_logFileName);
    s.writeS32(11, (int) m_fileMinLogLevel);
	return s.final();
}

bool Preferences::deserialize(const QByteArray& data)
{
    int tmpInt;

	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
	{
		d.readString(1, &m_sourceType);
		d.readString(2, &m_sourceDevice);
		d.readString(3, &m_audioType);
		d.readString(4, &m_audioDevice);
		d.readS32(5, &m_sourceIndex, 0);
		d.readFloat(6, &m_latitude, 0.0);
		d.readFloat(7, &m_longitude, 0.0);

		d.readS32(8, &tmpInt, (int) QtDebugMsg);

		if ((tmpInt == (int) QtDebugMsg) ||
		    (tmpInt == (int) QtInfoMsg) ||
		    (tmpInt == (int) QtWarningMsg) ||
		    (tmpInt == (int) QtCriticalMsg) ||
		    (tmpInt == (int) QtFatalMsg)) {
            m_consoleMinLogLevel = (QtMsgType) tmpInt;
		} else {
		    m_consoleMinLogLevel = QtDebugMsg;
		}

		d.readBool(9, &m_useLogFile, false);
		d.readString(10, &m_logFileName, "sdrangel.log");

        d.readS32(11, &tmpInt, (int) QtDebugMsg);

        if ((tmpInt == (int) QtDebugMsg) ||
            (tmpInt == (int) QtInfoMsg) ||
            (tmpInt == (int) QtWarningMsg) ||
            (tmpInt == (int) QtCriticalMsg) ||
            (tmpInt == (int) QtFatalMsg)) {
            m_fileMinLogLevel = (QtMsgType) tmpInt;
        } else {
            m_fileMinLogLevel = QtDebugMsg;
        }

		return true;
	} else
	{
		resetToDefaults();
		return false;
	}
}
