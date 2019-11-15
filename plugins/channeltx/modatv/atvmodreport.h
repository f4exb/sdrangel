///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODATV_ATVMODREPORT_H_
#define PLUGINS_CHANNELTX_MODATV_ATVMODREPORT_H_

#include <vector>
#include <QObject>
#include "util/message.h"

class ATVModReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportVideoFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrameCount() const { return m_frameCount; }

        static MsgReportVideoFileSourceStreamTiming* create(int frameCount)
        {
            return new MsgReportVideoFileSourceStreamTiming(frameCount);
        }

    protected:
        int m_frameCount;

        MsgReportVideoFileSourceStreamTiming(int frameCount) :
            Message(),
            m_frameCount(frameCount)
        { }
    };

    class MsgReportVideoFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrameRate() const { return m_frameRate; }
        quint32 getVideoLength() const { return m_videoLength; }

        static MsgReportVideoFileSourceStreamData* create(int frameRate,
                quint32 recordLength)
        {
            return new MsgReportVideoFileSourceStreamData(frameRate, recordLength);
        }

    protected:
        int m_frameRate;
        int m_videoLength; //!< Video length in frames

        MsgReportVideoFileSourceStreamData(int frameRate,
                int videoLength) :
            Message(),
            m_frameRate(frameRate),
            m_videoLength(videoLength)
        { }
    };

    class MsgReportCameraData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getdeviceNumber() const { return m_deviceNumber; }
        float getFPS() const { return m_fps; }
        float getFPSManual() const { return m_fpsManual; }
        bool getFPSManualEnable() const { return m_fpsManualEnable; }
        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }
        int getStatus() const { return m_status; }

        static MsgReportCameraData* create(
        		int deviceNumber,
				float fps,
				float fpsManual,
				bool fpsManualEnable,
				int width,
				int height,
				int status)
        {
            return new MsgReportCameraData(
            		deviceNumber,
					fps,
					fpsManual,
					fpsManualEnable,
					width,
					height,
					status);
        }

    protected:
        int m_deviceNumber;
        float m_fps;
        float m_fpsManual;
        bool m_fpsManualEnable;
        int m_width;
        int m_height;
        int m_status;

        MsgReportCameraData(
        		int deviceNumber,
        		float fps,
				float fpsManual,
				bool fpsManualEnable,
				int width,
				int height,
				int status) :
            Message(),
			m_deviceNumber(deviceNumber),
			m_fps(fps),
			m_fpsManual(fpsManual),
			m_fpsManualEnable(fpsManualEnable),
			m_width(width),
			m_height(height),
			m_status(status)
        { }
    };

    class MsgReportEffectiveSampleRate : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        uint32_t gatNbPointsPerLine() const { return m_nbPointsPerLine; }

        static MsgReportEffectiveSampleRate* create(int sampleRate, uint32_t nbPointsPerLine)
        {
            return new MsgReportEffectiveSampleRate(sampleRate, nbPointsPerLine);
        }

    protected:
        int m_sampleRate;
        uint32_t m_nbPointsPerLine;

        MsgReportEffectiveSampleRate(
                int sampleRate,
                uint32_t nbPointsPerLine) :
            Message(),
            m_sampleRate(sampleRate),
            m_nbPointsPerLine(nbPointsPerLine)
        { }
    };

public:
    ATVModReport();
    ~ATVModReport();
};

#endif // PLUGINS_CHANNELTX_MODATV_ATVMODREPORT_H_
