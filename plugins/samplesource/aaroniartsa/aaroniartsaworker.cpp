///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                                       //
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

#include "util/messagequeue.h"
#include "dsp/dspcommands.h"

#include "aaroniartsaworker.h"

MESSAGE_CLASS_DEFINITION(AaroniaRTSAWorker::MsgReportSampleRateAndFrequency, Message)

AaroniaRTSAWorker::AaroniaRTSAWorker(SampleSinkFifo* sampleFifo) :
	QObject(),
	m_timer(this),
	m_samplesBuf(),
	m_sampleFifo(sampleFifo),
	m_centerFrequency(1450000),
	m_sampleRate(10000000),
    m_inputMessageQueue(nullptr),
	m_status(0),
	m_convertBuffer(64e6)
{
	// Initialize network manager
	mNetworkAccessManager = new QNetworkAccessManager(this);

	// Request 16bit raw samples
	QUrl	url("http://localhost:55123/stream?format=float32");

	QNetworkRequest	req(url);
	mReply = mNetworkAccessManager->get(req);

	// Connect Qt slots to network events
	connect(mReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
	connect(mReply, SIGNAL(finished()), this, SLOT(onFinished()));
	connect(mReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

	mPrevTime = 0;
	mPacketSamples = 0;
}

void AaroniaRTSAWorker::onSocketError(QAbstractSocket::SocketError error)
{
	(void) error;
    m_status = 3;
	emit updateStatus(3);
}

void AaroniaRTSAWorker::sendCenterFrequency()
{
	//if (!m_webSocket.isValid())
	//	return;

	/*QString freq = QString::number(m_centerFrequency / 1000.0, 'f', 3);
    int bw = (m_sampleRate/2) - 20;
	QString msg = QString("SET mod=iq low_cut=-%1 high_cut=%2 freq=%3").arg(bw).arg(bw).arg(freq);
	m_webSocket.sendTextMessage(msg);*/
	//mNetworkAccessManager->put()

}

void AaroniaRTSAWorker::onCenterFrequencyChanged(quint64 centerFrequency)
{
	if (m_centerFrequency == centerFrequency)
		return;

	m_centerFrequency = centerFrequency;
	sendCenterFrequency();
}

void AaroniaRTSAWorker::onServerAddressChanged(QString serverAddress)
{
	/*if (m_serverAddress == serverAddress) {
		return;
    }

	m_serverAddress = serverAddress;

    m_status = 1;
	emit updateStatus(1);

	QString url("ws://");
	url.append(m_serverAddress);
	url.append("/rtsa/");
	url.append(QString::number(QDateTime::currentMSecsSinceEpoch()));
	url.append("/SND");
	m_webSocket.open(QUrl(url));*/
}

void AaroniaRTSAWorker::tick()
{
}

/**************************CPY ********************************* */

void AaroniaRTSAWorker::onError(QNetworkReply::NetworkError code)
{
	QTextStream	qerr(stderr);
	qerr << "Network Error: " + mReply->errorString();
}

void AaroniaRTSAWorker::onFinished()
{
	QTextStream	qerr(stderr);
	qerr << "Finished: " + mReply->errorString();

	mBuffer.append(mReply->readAll());

	mReply->deleteLater();
	mReply = nullptr;
}

// bytes received from the socket
void AaroniaRTSAWorker::onReadyRead()
{
	QTextStream	qout(stdout);

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


//				qout << "IQ " << sp[0] << ", " << sp[1] << "\n";
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
					//double	startTime = jdoc["startTime"].toDouble(), endTime = jdoc["endTime"].toDouble();
					int	samples = jdoc["samples"].toInt();

					// Dump packet loss
					//if (startTime != mPrevTime)
					//	qout << QDateTime::fromMSecsSinceEpoch(startTime * 1000).toString() << " D " << endTime - startTime << " O " << startTime - mPrevTime << " S " << samples << " L " << QDateTime::currentMSecsSinceEpoch() / 1000.0 - startTime << "\n";

					// Switch to data phase
					//mPrevTime = endTime;
					mPacketSamples = samples;
                    // qDebug() << jdoc.toJson();
                    quint64 endFreq = jdoc["endFrequency"].toDouble();
                    quint64 startFreq = jdoc["startFrequency"].toDouble();
                    int bw = endFreq - startFreq;
                    quint64 midFreq = (endFreq + startFreq) / 2;

                    if ((bw != m_sampleRate) || (midFreq != m_centerFrequency))
                    {
                        if (m_inputMessageQueue)
                        {
                            MsgReportSampleRateAndFrequency *msg = MsgReportSampleRateAndFrequency::create(bw, midFreq);
                            m_inputMessageQueue->push(msg);
                        }

                        m_sampleRate = bw;
                        m_centerFrequency = midFreq;
                    }
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



