///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_BLADERFINPUT_H
#define INCLUDE_BLADERFINPUT_H

#include <QString>
#include <QByteArray>

#include <libbladeRF.h>
#include <dsp/devicesamplesource.h>
#include "bladerf/devicebladerf.h"
#include "bladerf/devicebladerfparam.h"
#include "bladerfinputsettings.h"

class DeviceSourceAPI;
class BladerfInputThread;
class FileRecord;

class BladerfInput : public DeviceSampleSource {
public:
	class MsgConfigureBladerf : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const BladeRFInputSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureBladerf* create(const BladeRFInputSettings& settings, bool force)
		{
			return new MsgConfigureBladerf(settings, force);
		}

	private:
		BladeRFInputSettings m_settings;
		bool m_force;

		MsgConfigureBladerf(const BladeRFInputSettings& settings, bool force) :
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

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    BladerfInput(DeviceSourceAPI *deviceAPI);
	virtual ~BladerfInput();
	virtual void destroy();

    virtual void init();
	virtual bool start();
	virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

	virtual bool handleMessage(const Message& message);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

private:
    bool openDevice();
    void closeDevice();
	bool applySettings(const BladeRFInputSettings& settings, bool force);
	bladerf_lna_gain getLnaGain(int lnaGain);
//	struct bladerf *open_bladerf_from_serial(const char *serial);

	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	BladeRFInputSettings m_settings;
	struct bladerf* m_dev;
	BladerfInputThread* m_bladerfThread;
	QString m_deviceDescription;
	DeviceBladeRFParams m_sharedParams;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_BLADERFINPUT_H
