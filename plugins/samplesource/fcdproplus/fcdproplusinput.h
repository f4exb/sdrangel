///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_FCDINPUT_H
#define INCLUDE_FCDINPUT_H

#include "dsp/samplesource.h"
#include "fcdhid.h"
#include <QString>
#include <inttypes.h>

struct fcd_buffer {
	void *start;
	std::size_t length;
};

class FCDProPlusThread;

class FCDProPlusInput : public SampleSource {
public:
	struct Settings {
		Settings();
		quint64 centerFrequency;
		qint32 range;
		qint32 gain;
		qint32 bias;
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureFCD : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureFCD* create(const Settings& settings)
		{
			return new MsgConfigureFCD(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureFCD(const Settings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	FCDProPlusInput();
	virtual ~FCDProPlusInput();

	virtual bool init(const Message& cmd);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

	void set_center_freq(double freq);
	void set_bias_t(bool on);
	void set_lna_gain(bool on);

private:
	void applySettings(const Settings& settings, bool force);

	hid_device *m_dev;
	QMutex m_mutex;
	Settings m_settings;
	FCDProPlusThread* m_FCDThread;
	QString m_deviceDescription;
};

#endif // INCLUDE_FCD_H
