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

#ifndef INCLUDE_V4LINPUT_H
#define INCLUDE_V4LINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <QString>
#include "v4lsource.h"

class V4LThread;

class V4LInput : public SampleSource {
public:
	struct Settings {
		qint32 m_gain;
		qint32 m_decimation;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureV4L : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureV4L* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureV4L(generalSettings, settings);
		}

	private:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureV4L(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	class MsgReportV4L : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::vector<int>& getGains() const { return m_gains; }

		static MsgReportV4L* create(const std::vector<int>& gains)
		{
			return new MsgReportV4L(gains);
		}

	protected:
		std::vector<int> m_gains;

		MsgReportV4L(const std::vector<int>& gains) :
			Message(),
			m_gains(gains)
		{ }
	};

	V4LInput(MessageQueue* msgQueueToGUI);
	~V4LInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;
	bool handleMessage(Message* message);

	void OpenSource(const char *filename);
	void CloseSource();
	void set_sample_rate(double samp_rate);
	void set_center_freq(double freq);
	void set_bandwidth(double bandwidth);
	void set_tuner_gain(double gain);
	int work(int noutput_items,
			void* input_items,
			void* output_items);
private:
	int fd;
	quint32 pixelformat;
	struct v4l_buffer *buffers;
	unsigned int n_buffers;
	void *recebuf_ptr;
	unsigned int recebuf_len;
	unsigned int recebuf_mmap_index;

	QMutex m_mutex;
	Settings m_settings;
	int m_dev;
	V4LThread* m_V4LThread;
	QString m_deviceDescription;
	std::vector<int> m_gains;

	bool applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force);
};

#endif // INCLUDE_V4L_H
