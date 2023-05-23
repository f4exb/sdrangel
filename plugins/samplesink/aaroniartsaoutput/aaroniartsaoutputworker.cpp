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

#include <QJsonObject>
#include "aaroniartsaoutputsettings.h"
#include "aaroniartsaoutputworker.h"

AaroniaRTSAOutputWorker::AaroniaRTSAOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent) :
	QObject(parent),
    m_running(false),
	m_sampleFifo(sampleFifo),
    m_sampleRate(100000),
    m_packetsPerSecond(10),
    m_samplesArrayInt16(nullptr)
{
    m_samplesPerPacket = m_sampleRate / m_packetsPerSecond;
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_centerFrequency = 145000000;
}

AaroniaRTSAOutputWorker::~AaroniaRTSAOutputWorker()
{
	if (m_running) {
		stopWork();
	}
}

void AaroniaRTSAOutputWorker::startWork()
{
    qDebug("aroniaRTSAOutputWorker::startWork");
    m_samplesPerPacket = m_sampleRate / m_packetsPerSecond;
    m_sampleResendTime = 1000 / (m_sampleRate / m_samplesPerPacket) ;
	m_streamStartTime = ( QDateTime::currentDateTime().currentMSecsSinceEpoch() );
	m_lastPacketEnd = m_streamStartTime / 1000.0;
	m_sumSamples = 0;

    connect(m_timer, SIGNAL(timeout()), this, SLOT(onGeneratePacket()));
    qDebug("AaroniaRTSAOutputWorker::startWork: m_sampleResendTime: %f", m_sampleResendTime);
	m_timer->start(m_sampleResendTime); //send period
    m_running = true;
}

void AaroniaRTSAOutputWorker::stopWork()
{
    qDebug("aroniaRTSAOutputWorker::stopWork");
    m_running = false;
    m_status = AaroniaRTSAOutputSettings::ConnectionIdle;
    emit updateStatus(m_status);
    disconnect(m_timer, SIGNAL(timeout()), this, SLOT(onGeneratePacket()));
    m_timer->stop();
}

void AaroniaRTSAOutputWorker::setSampleRate(int sampleRate)
{
    qDebug("aroniaRTSAOutputWorker::setSampleRate: %d", sampleRate);

    if (sampleRate == m_sampleRate) {
        return;
    }

    m_samplesPerPacket = sampleRate / m_packetsPerSecond;
    m_sampleResendTime = 1000 / (sampleRate / m_samplesPerPacket) ;
	m_timer->start(m_sampleResendTime); //send period
    m_sampleRate = sampleRate;
}

void AaroniaRTSAOutputWorker::onGeneratePacket()
{
	double sampleTimeUS = 1000000.0 / m_sampleRate;
	qint64 deltaT = QDateTime::currentDateTime().currentMSecsSinceEpoch();
    deltaT = deltaT - (m_lastPacketEnd*1000);
	double ndiff =  1000*((double)deltaT + 1 ) / sampleTimeUS ;
	double reawaketime = m_sampleResendTime -  (sampleTimeUS*(ndiff - m_samplesPerPacket ))/1000 ;
    // qDebug("AaroniaRTSAOutputWorker::onGeneratePacket: %f", reawaketime);
	m_timer->setInterval( reawaketime );

	double newStart = m_lastPacketEnd;
	m_lastPacketEnd = newStart + ((m_samplesPerPacket + 1) *  sampleTimeUS)/1000000 ;

	double timeOffset = 0.4; //put it into future

    buildSamples( newStart + timeOffset , m_lastPacketEnd + timeOffset);
}

void AaroniaRTSAOutputWorker::buildSamples(double startTime, double stopTime)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    SampleVector& data = m_sampleFifo->getData();
    m_sampleFifo->read(m_samplesPerPacket, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

	if (m_samplesArrayInt16 == nullptr) {
		m_samplesArrayInt16 = new int16_t[2*m_samplesPerPacket];
	}

    if (iPart1Begin != iPart1End) {
        callbackPart(m_samplesArrayInt16, data, iPart1Begin, iPart1End);
    }

    if (iPart2Begin != iPart2End) {
        callbackPart(&m_samplesArrayInt16[(iPart1End - iPart1Begin)*2], data, iPart2Begin, iPart2End);
    }

    double startFrequency = m_centerFrequency -  m_sampleRate/2;
    double endFrequency = m_centerFrequency + m_sampleRate/2;

	QJsonDocument jdoc(QJsonObject({
		{"startTime", startTime },
		{"endTime" , stopTime },
		{"startFrequency", startFrequency },
		{"endFrequency" , endFrequency },
		{"minPower", -2},
		{"maxPower" , 2},
		{"sampleSize", 2},
		{"sampleDepth" , 1},
		{"payload", "iq"},
		{"format", "int16"},
		{"scale", 512.0},
		{"unit" , "volt"},
		{"samples" , 2*m_samplesPerPacket},
	}));

    postData(jdoc, m_samplesArrayInt16, 2*m_samplesPerPacket);
}

void AaroniaRTSAOutputWorker::callbackPart(int16_t *buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    for (unsigned int j = 0, i = iBegin; i < iEnd; j++, i++)
    {
        buf[2*j]   = data[i].m_real;
        buf[2*j+1] = data[i].m_imag;
    }
}

void AaroniaRTSAOutputWorker::postData(QJsonDocument jdoc, int16_t *samplesArray, int nSamples)
{
    QUrl url(tr("http://%1/sample").arg(m_serverAddress));
    // qDebug() << "AaroniaRTSAOutputWorker::postData:" << url;
	QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));
    QByteArray byteArray = jdoc.toJson(QJsonDocument::Compact);
    byteArray.append(0x1e);
    byteArray.append(QByteArray::fromRawData(reinterpret_cast<char *>(samplesArray), nSamples*sizeof(int16_t)));
	QNetworkReply *networkReply = m_networkAccessManager->post(request, byteArray);

	connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
	connect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onFinished(QNetworkReply*)));
}

void AaroniaRTSAOutputWorker::onError(QNetworkReply::NetworkError)
{
	QNetworkReply* nReply = qobject_cast<QNetworkReply*>( sender() );
	qDebug() << "AaroniaRTSAOutputWorker::onError: Network Error: " + nReply->errorString();
	m_timer->stop();
    m_status = AaroniaRTSAOutputSettings::ConnectionError;
	emit updateStatus(m_status);
}

void AaroniaRTSAOutputWorker::onFinished(QNetworkReply *reply)
{
    if ((m_status != AaroniaRTSAOutputSettings::ConnectionOK) && (reply->error() == QNetworkReply::NoError))
    {
        m_status = AaroniaRTSAOutputSettings::ConnectionOK;
        emit updateStatus(m_status);
    }

	reply->deleteLater();
}
