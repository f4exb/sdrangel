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

#ifndef _AARONIARTSA_AARONIARTSAOUTPUTWORKER_H_
#define _AARONIARTSA_AARONIARTSAOUTPUTWORKER_H_

#include <QTimer>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"

#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QObject>
#include "dsp/decimatorsfi.h"

class AaroniaRTSAOutputWorker : public QObject {
	Q_OBJECT
public:
	enum TxFormat {
		FLOAT32,
		INT16,
		ASCII
	};

	AaroniaRTSAOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent = nullptr);
    ~AaroniaRTSAOutputWorker();
	void startWork();
	void stopWork();
    int getStatus() const { return m_status; }
    void setServerAddress(const QString& serverAddress) { m_serverAddress = serverAddress; }
    void setCenterFrequency(quint64 centerFrequency) { m_centerFrequency = centerFrequency; }
    void setSampleRate(int sampleRate);

signals:
	void updateStatus(int status);

private:
    volatile bool m_running;
	QTimer *m_timer;

	SampleVector m_samplesBuf;
	SampleSourceFifo* m_sampleFifo;

	QString m_serverAddress;
	quint64 m_centerFrequency;
    int m_sampleRate;
    int m_status; //!< See GUI for status number detail

    QNetworkAccessManager	*m_networkAccessManager;
    int                     m_packetsPerSecond;
	int						m_samplesPerPacket;
	TxFormat				m_txFormat;
	qint64					m_streamStartTime;
	quint64					m_sumSamples;
	long double				m_lastPacketEnd;
	double					m_sampleResendTime;
    int16_t                 *m_samplesArrayInt16;

	void buildSamples(double startTime, double stopTime);
    void callbackPart(int16_t *buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd);
	void postData(QJsonDocument jdoc, int16_t *samplesArray, int nSamples);

private slots:
	void onGeneratePacket();
    void onError(QNetworkReply::NetworkError code);
	void onFinished(QNetworkReply *);

};

#endif // _AARONIARTSA_AARONIARTSAOUTPUTWORKER_H_
