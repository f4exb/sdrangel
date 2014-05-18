#ifndef INCLUDE_PREFERENCES_H
#define INCLUDE_PREFERENCES_H

#include <QString>

class Preferences {
public:
	Preferences();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setSourceType(const QString& value) { m_sourceType = value; }
	const QString& getSourceType() const { return m_sourceType; }
	void setSourceDevice(const QString& value) { m_sourceDevice= value; }
	const QString& getSourceDevice() const { return m_sourceDevice; }

	void setAudioType(const QString& value) { m_audioType = value; }
	const QString& getAudioType() const { return m_audioType; }
	void setAudioDevice(const QString& value) { m_audioDevice= value; }
	const QString& getAudioDevice() const { return m_audioDevice; }

protected:
	QString m_sourceType;
	QString m_sourceDevice;

	QString m_audioType;
	QString m_audioDevice;
};

#endif // INCLUDE_PREFERENCES_H
