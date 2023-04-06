///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AARONIARTSA_AARONIARTSAWORKER_H_
#define _AARONIARTSA_AARONIARTSAWORKER_H_

#include <QTimer>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"


#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QObject>
#include "dsp/decimatorsfi.h"



class MessageQueue;

class AaroniaRTSAInputWorker : public QObject {
	Q_OBJECT

public:
	class MsgReportSampleRateAndFrequency : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
        quint64 getCenterFrequency() const { return m_centerFrequency; }

		static MsgReportSampleRateAndFrequency* create(int sampleRate, quint64 centerFrequency) {
			return new MsgReportSampleRateAndFrequency(sampleRate, centerFrequency);
		}

	private:
		int m_sampleRate;
        quint64 m_centerFrequency;

		MsgReportSampleRateAndFrequency(int sampleRate, qint64 centerFrequency) :
			Message(),
			m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
		{ }
	};

	AaroniaRTSAInputWorker(SampleSinkFifo* sampleFifo);
    ~AaroniaRTSAInputWorker();
    int getStatus() const { return m_status; }
    void setInputMessageQueue(MessageQueue *messageQueue) { m_inputMessageQueue = messageQueue; }

private:
	QTimer m_timer;

	SampleVector m_samplesBuf;
	SampleSinkFifo* m_sampleFifo;

	QString m_serverAddress;
	quint64 m_centerFrequency;
    int m_sampleRate;
    MessageQueue *m_inputMessageQueue;

    int m_status; //!< See GUI for status number detail

	void sendCenterFrequencyAndSampleRate();
    void getConfig();
    void parseConfig(QByteArray bytes);

	// QT htttp clients
	QNetworkAccessManager *m_networkAccessManager;
	QNetworkAccessManager *m_networkAccessManagerConfig;
	// Reply from the HTTP server
	QNetworkReply *mReply;
	// Input buffer
	QByteArray mBuffer;
	// Number of IQ sample pairs in the current packet
	int mPacketSamples;
	// Previous sample end time to check for packet loss
	double mPrevTime;
    // Current iQ demodulator name
    QString m_iqDemodName;
	//Decimators<qint32, float, SDR_RX_SAMP_SZ, 32, true> m_decimatorsIQ;
	DecimatorsFI<true> m_decimatorsFloatIQ;
	SampleVector m_convertBuffer;
	//void workIQ(unsigned int n_items);


signals:
	void updateStatus(int status);

public slots:
	void onCenterFrequencyChanged(quint64 centerFrequency);
    void onSampleRateChanged(int sampleRate);
	void onServerAddressChanged(QString serverAddress);

private slots:
	void onSocketError(QAbstractSocket::SocketError error);
	void onError(QNetworkReply::NetworkError code);
	void onFinished(void);
	void onReadyRead(void);
    void handleConfigReply(QNetworkReply* reply);
    void tick();
};

#endif // _AARONIARTSA_AARONIARTSAWORKER_H_
