///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This is a pure Qt (non ALSA) FCD sound card reader for Windows build          //
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

#include <dsp/devicesamplesource.h>

#include "fcdproplussettings.h"
#include "fcdhid.h"
#include <QString>
#include <inttypes.h>

struct fcd_buffer {
	void *start;
	std::size_t length;
};

class FCDProPlusReader;

class FCDProPlusInput : public DeviceSampleSource {
public:
	class MsgConfigureFCDProPlus : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FCDProPlusSettings& getSettings() const { return m_settings; }

		static MsgConfigureFCDProPlus* create(const FCDProPlusSettings& settings)
		{
			return new MsgConfigureFCDProPlus(settings);
		}

	private:
		FCDProPlusSettings m_settings;

		MsgConfigureFCD(const FCDProPlusSettings& settings) :
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
	void set_mixer_gain(bool on);
	void set_if_gain(int gain);
	void set_rf_filter(int filterIndex);
	void set_if_filter(int filterIndex);
	void set_lo_ppm();

private:
	void applySettings(const FCDProPlusSettings& settings, bool force);

	hid_device *m_dev;
	QMutex m_mutex;
	FCDProPlusSettings m_settings;
	FCDProPlusReader* m_FCDReader;
	QString m_deviceDescription;
};

#endif // INCLUDE_FCD_H
