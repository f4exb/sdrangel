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

#ifndef INCLUDE_FCDPROINPUT_H
#define INCLUDE_FCDPROINPUT_H

#include <dsp/devicesamplesource.h>

#include "fcdprosettings.h"
#include "fcdhid.h"
#include <QString>
#include <inttypes.h>

struct fcd_buffer {
	void *start;
	std::size_t length;
};

class DeviceSourceAPI;
class FCDProThread;
class FileRecord;

class FCDProInput : public DeviceSampleSource {
public:
	class MsgConfigureFCD : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FCDProSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureFCD* create(const FCDProSettings& settings, bool force)
		{
			return new MsgConfigureFCD(settings, force);
		}

	private:
		FCDProSettings m_settings;
		bool m_force;

		MsgConfigureFCD(const FCDProSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

	FCDProInput(DeviceSourceAPI *deviceAPI);
	virtual ~FCDProInput();
	virtual void destroy();

	virtual bool start();
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;

	virtual bool handleMessage(const Message& message);

	void set_center_freq(double freq);
	void set_bias_t(bool on);
	void set_lnaGain(int index);
	void set_rfFilter(int index);
	void set_lnaEnhance(int index);
	void set_band(int index);
	void set_mixerGain(int index);
	void set_mixerFilter(int index);
	void set_biasCurrent(int index);
	void set_mode(int index);
	void set_gain1(int index);
	void set_rcFilter(int index);
	void set_gain2(int index);
	void set_gain3(int index);
	void set_gain4(int index);
	void set_ifFilter(int index);
	void set_gain5(int index);
	void set_gain6(int index);

private:
    bool openDevice();
    void closeDevice();
	void applySettings(const FCDProSettings& settings, bool force);
	void set_lo_ppm();

	DeviceSourceAPI *m_deviceAPI;
	hid_device *m_dev;
	QMutex m_mutex;
	FCDProSettings m_settings;
	FCDProThread* m_FCDThread;
	QString m_deviceDescription;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_FCDPROINPUT_H
