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

#include <boost/endian/conversion.hpp>

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "util/messagequeue.h"
#include "dsp/dspcommands.h"

#include "aaroniartsainputsettings.h"
#include "aaroniartsainputworker.h"

MESSAGE_CLASS_DEFINITION(AaroniaRTSAInputWorker::MsgReportSampleRateAndFrequency, Message)

AaroniaRTSAInputWorker::AaroniaRTSAInputWorker(SampleSinkFifo* sampleFifo) :
	QObject(),
	m_timer(this),
	m_samplesBuf(),
	m_sampleFifo(sampleFifo),
	m_centerFrequency(0),
	m_sampleRate(1),
    m_inputMessageQueue(nullptr),
	m_status(AaroniaRTSAInputSettings::ConnectionIdle),
    mReply(nullptr),
	m_convertBuffer(64e6)
{
	// Initialize network managers
	m_networkAccessManager = new QNetworkAccessManager(this);
    m_networkAccessManagerConfig = new QNetworkAccessManager(this);
    QObject::connect(
        m_networkAccessManagerConfig,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAInputWorker::handleConfigReply
    );

	// Request 16bit raw samples
    // m_serverAddress = "localhost:55123";
	// QUrl url(tr("http://%1/stream?format=float32").arg(m_serverAddress));

	// QNetworkRequest	req(url);
	// mReply = m_networkAccessManager->get(req);

	// // Connect Qt slots to network events
	// connect(mReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
	// connect(mReply, SIGNAL(finished()), this, SLOT(onFinished()));
	// connect(mReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

	mPrevTime = 0;
	mPacketSamples = 0;
}

AaroniaRTSAInputWorker::~AaroniaRTSAInputWorker()
{
    if (mReply)
    {
        // disconnect previous sugnals
        disconnect(mReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
        disconnect(mReply, SIGNAL(finished()), this, SLOT(onFinished()));
        disconnect(mReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

        mReply->abort();
        mReply->deleteLater();
    }

    m_networkAccessManager->deleteLater();

    QObject::disconnect(
        m_networkAccessManagerConfig,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAInputWorker::handleConfigReply
    );
    m_networkAccessManagerConfig->deleteLater();
}

void AaroniaRTSAInputWorker::onSocketError(QAbstractSocket::SocketError error)
{
	(void) error;
    m_status = AaroniaRTSAInputSettings::ConnectionError;
	emit updateStatus(m_status);
}

void AaroniaRTSAInputWorker::sendCenterFrequencyAndSampleRate()
{
    if (m_iqDemodName.size() == 0) {
        return;
    }

    qDebug("AaroniaRTSAInputWorker::sendCenterFrequencyAndSampleRate: %llu samplerate: %d", m_centerFrequency, m_sampleRate);

    QJsonObject object {
        {"receiverName", m_iqDemodName},
        {"simpleconfig", QJsonObject({
            {"main", QJsonObject({
                {"centerfreq", QJsonValue((qint64) m_centerFrequency)},
                {"samplerate", QJsonValue(m_sampleRate)},
                {"spanfreq", QJsonValue(m_sampleRate)},
            })}
        })}
    };

    QJsonDocument document;
    document.setObject(object);
    QUrl url(tr("http://%1/remoteconfig").arg(m_serverAddress));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_networkAccessManagerConfig->put(request, document.toJson());
}

void AaroniaRTSAInputWorker::getConfig()
{
    QUrl url(tr("http://%1/remoteconfig").arg(m_serverAddress));
    QNetworkRequest request(url);
    m_networkAccessManagerConfig->get(request);
}

void AaroniaRTSAInputWorker::onCenterFrequencyChanged(quint64 centerFrequency)
{
	if (m_centerFrequency == centerFrequency) {
		return;
    }

	m_centerFrequency = centerFrequency;
	sendCenterFrequencyAndSampleRate();
}

void AaroniaRTSAInputWorker::onSampleRateChanged(int sampleRate)
{
	if (m_sampleRate == sampleRate) {
		return;
    }

	m_sampleRate = sampleRate;
	sendCenterFrequencyAndSampleRate();
}

void AaroniaRTSAInputWorker::onServerAddressChanged(QString serverAddress)
{
    m_status = AaroniaRTSAInputSettings::ConnectionDisconnected;
    updateStatus(m_status);

    if (mReply)
    {
        // disconnect previous sugnals
        disconnect(mReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
        disconnect(mReply, SIGNAL(finished()), this, SLOT(onFinished()));
        disconnect(mReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

        mReply->abort();
        mReply->deleteLater();
    }

    QUrl url(tr("http://%1/stream?format=float32").arg(serverAddress));
	QNetworkRequest	req(url);
	mReply = m_networkAccessManager->get(req);

	// Connect Qt slots to network events
	connect(mReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
	connect(mReply, SIGNAL(finished()), this, SLOT(onFinished()));
	connect(mReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

	mPrevTime = 0;
	mPacketSamples = 0;
    m_serverAddress = serverAddress;

    getConfig();
}

void AaroniaRTSAInputWorker::tick()
{
}

/**************************CPY ********************************* */

void AaroniaRTSAInputWorker::onError(QNetworkReply::NetworkError code)
{
    (void) code;
	qWarning() << "AaroniaRTSAInputWorker::onError: network Error: " << mReply->errorString();
    m_status = 3;
	emit updateStatus(3);
}

void AaroniaRTSAInputWorker::onFinished()
{
	qDebug() << "AaroniaRTSAInputWorker::onFinished(: finished: " << mReply->errorString();
	mBuffer.append(mReply->readAll());
	mReply->deleteLater();
	mReply = nullptr;
}

// bytes received from the socket
void AaroniaRTSAInputWorker::onReadyRead()
{
    if (m_status != AaroniaRTSAInputSettings::ConnectionOK)
    {
        m_status = AaroniaRTSAInputSettings::ConnectionOK;
        emit updateStatus(m_status);
    }

	// read as many bytes as possible into input buffer
	qint64 n = mReply->bytesAvailable();
	qint64 bs = mBuffer.size();
	mBuffer.resize(bs + n);
	qint64 done = mReply->read(mBuffer.data() + bs, n);
	mBuffer.resize(bs + done);

	// intialize parsing
	int	offset = 0;
	int	avail = mBuffer.size();

	// consume all input data if possible
	while (offset < avail)
	{
		// any samples so far (not looking for meta data)
		if (mPacketSamples)
		{
			// enough samples
			if (offset + mPacketSamples * 2 * sizeof(float) <= (unsigned long) avail)
			{
				// do something with the IQ data
				const float	*sp = (const float *)(mBuffer.constData() + offset);

				 SampleVector::iterator it = m_convertBuffer.begin();

				m_decimatorsFloatIQ.decimate1(&it, sp, 2*mPacketSamples);

				/*m_samplesBuf.clear();
				for (int i = 0; i < mPacketSamples*2; i+=2)
				{
					m_samplesBuf.push_back(Sample(
						sp[i] << (SDR_RX_SAMP_SZ - 8),
						sp[i+1] << (SDR_RX_SAMP_SZ - 8)
					));
				}*/

				//m_sampleFifo->write(m_samplesBuf.begin(), m_samplesBuf.end());
				m_sampleFifo->write(m_convertBuffer.begin(), it);

                //qDebug() << "IQ " << sp[0] << ", " << sp[1];
				//m_sampleFifo->write()

				// consume all samples from the input buffer
				offset += mPacketSamples * 2 * sizeof(float);
				mPacketSamples = 0;
			}
			else
            {
				break;
            }
		}
		else
		{
			// is there a complete JSON metadata object in the buffer
			int	split = mBuffer.indexOf('\x1e', offset);

            if (split != -1)
			{
				// Extract it
				QByteArray data	= mBuffer.mid(offset, split - offset);
				offset = split + 1;

				// Parse the JSON data

				QJsonParseError	error;
				QJsonDocument jdoc = QJsonDocument::fromJson(data, &error);

				if (error.error == QJsonParseError::NoError)
				{
					// Extract fields of interest
					// double	startTime = jdoc["startTime"].toDouble(), endTime = jdoc["endTime"].toDouble();
					int	samples = jdoc["samples"].toInt();

					// Dump packet loss
					// if (startTime != mPrevTime)
                    // {
					// 	qDebug() << "AaroniaRTSAInputWorker::onReadyRead: packet loss: "
                    //         << QDateTime::fromMSecsSinceEpoch(startTime * 1000).toString()
                    //         << " D " << endTime - startTime
                    //         << " O " << startTime - mPrevTime
                    //         << " S " << samples
                    //         << " L " << QDateTime::currentMSecsSinceEpoch() / 1000.0 - startTime;

                    //     if (m_status != AaroniaRTSAInputSettings::ConnectionUnstable)
                    //     {
                    //         m_status = AaroniaRTSAInputSettings::ConnectionUnstable;
                    //         emit updateStatus(m_status);
                    //     }
                    // }

					// Switch to data phase
					// mPrevTime = endTime;
					mPacketSamples = samples;
                    // qDebug() << jdoc.toJson();
                    quint64 endFreq = jdoc["endFrequency"].toDouble();
                    quint64 startFreq = jdoc["startFrequency"].toDouble();
                    int bw = jdoc["sampleFrequency"].toInt();
                    quint64 midFreq = (endFreq + startFreq) / 2;

                    if ((bw != m_sampleRate) || (midFreq != m_centerFrequency))
                    {
                        if (m_inputMessageQueue)
                        {
                            MsgReportSampleRateAndFrequency *msg = MsgReportSampleRateAndFrequency::create(bw, midFreq);
                            m_inputMessageQueue->push(msg);
                        }

                    }

                    m_sampleRate = bw;
                    m_centerFrequency = midFreq;
				}
				else
				{
					QTextStream	qerr(stderr);
					qerr << "Json Parse Error: " + error.errorString();
				}
			}
			else
            {
				break;
            }
		}
	}

	// Remove consumed data from the buffer
	mBuffer.remove(0, offset);
}

void AaroniaRTSAInputWorker::handleConfigReply(QNetworkReply* reply)
{
    if (reply->operation() == QNetworkAccessManager::GetOperation) // return from GET to /remoteconfig
    {
        parseConfig(reply->readAll());
    }
    else if (reply->operation() == QNetworkAccessManager::PutOperation) // return from PUT to /remoteconfig
    {
        int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if ((httpStatusCode / 100) == 2) {
            qDebug("AaroniaRTSAInputWorker::handleConfigReply: remoteconfig OK (%d)", httpStatusCode);
        } else {
            qWarning("AaroniaRTSAInputWorker::handleConfigReply: remoteconfig ended with error (%d)", httpStatusCode);
        }
    }

    reply->deleteLater();
}

void AaroniaRTSAInputWorker::parseConfig(QByteArray bytes)
{
    QJsonDocument document = QJsonDocument::fromJson(bytes);
    m_iqDemodName = "";

    if (document.isObject())
    {
        QJsonObject documentObject = document.object();

        if (documentObject.contains(QStringLiteral("config")))
        {
            QJsonObject config = documentObject.value(QStringLiteral("config")).toObject();

            if (config.contains(QStringLiteral("items")))
            {
                QJsonArray configItems = config.value(QStringLiteral("items")).toArray();

                for (const auto& configIem : configItems)
                {
                    QJsonObject configIemObject = configIem.toObject();

                    if (configIemObject.contains(QStringLiteral("name")))
                    {
                        QString nameItem = configIemObject.value(QStringLiteral("name")).toString();

                        if (nameItem.startsWith("Block_IQDemodulator"))
                        {
                            m_iqDemodName = nameItem;
                            break;
                        }
                    }
                }
            }
            else
            {
                qDebug() << "AaroniaRTSAInputWorker::parseConfig: config has no items: " << config;
            }

        }
        else
        {
            qDebug() << "AaroniaRTSAInputWorker::parseConfig: document has no config object: " << documentObject;
        }

    }
    else
    {
        qDebug() << "AaroniaRTSAInputWorker::parseConfig: document is not an object: " << document;
    }

    if (m_iqDemodName == "") {
        qWarning("AaroniaRTSAInputWorker.parseConfig: could not find IQ demdulator");
    } else {
        qDebug("AaroniaRTSAInputWorker::parseConfig: IQ demdulator name: %s", qPrintable(m_iqDemodName));
    }
}
