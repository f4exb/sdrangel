///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/datafifo.h"
#include "util/db.h"
#include "maincore.h"

#include "dabdemod.h"
#include "dabdemodsink.h"

// Callbacks from DAB library

void syncHandler(bool value, void *ctx)
{
    (void)value;
    (void)ctx;
}

void systemDataHandler(bool sync, int16_t snr, int32_t freqOffset, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->systemData(sync, snr, freqOffset);
}

void ensembleNameHandler(const char *name, int32_t id, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->ensembleName(QString::fromUtf8(name), id);
}

void programNameHandler(const char *name, int32_t id, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->programName(QString::fromUtf8(name), id);
}

void fibQualityHandler(int16_t percent, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->fibQuality(percent);
}

void audioHandler(int16_t *buffer, int size, int samplerate, bool stereo, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->audio(buffer, size, samplerate, stereo);
}

void dataHandler(const char *data, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->data(QString::fromUtf8(data));
}

void byteHandler(uint8_t *data, int16_t a, uint8_t b, void *ctx)
{
    (void)data;
    (void)a;
    (void)b;
    (void)ctx;
}

// Note: North America has different table
static const char *dabProgramType[] =
{
    "No programme type",
    "News",
    "Current Affairs",
    "Information",
    "Sport",
    "Education",
    "Drama",
    "Culture",
    "Science",
    "Varied",
    "Pop Music",
    "Rock Music",
    "Easy Listening Music",
    "Light Classical",
    "Serious Classical",
    "Other Music",
    "Weather/meteorology",
    "Finance/Business",
    "Children's programmes",
    "Social Affairs",
    "Religion",
    "Phone In",
    "Travel",
    "Leisure",
    "Jazz Music",
    "Country Music",
    "National Music",
    "Oldies Music",
    "Folk Music",
    "Documentary",
    "Not used",
    "Not used",
};

static const char *dabLanguageCode[] =
{
    "Unknown",
    "Albanian",
    "Breton",
    "Catalan",
    "Croatian",
    "Welsh",
    "Czech",
    "Danish",
    "German",
    "English",
    "Spanish",
    "Esperanto",
    "Estonian",
    "Basque",
    "Faroese",
    "French",
    "Frisian",
    "Irish",
    "Gaelic",
    "Galician",
    "Icelandic",
    "Italian",
    "Sami",
    "Latin",
    "Latvian",
    "Luxembourgian",
    "Lithuanian",
    "Hungarian",
    "Maltese",
    "Dutch",
    "Norwegian",
    "Occitan",
    "Polish",
    "Portuguese",
    "Romanian",
    "Romansh",
    "Serbian",
    "Slovak",
    "Slovene",
    "Finnish",
    "Swedish",
    "Turkish",
    "Flemish",
    "Walloon",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Background sound",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Zulu",
    "Vietnamese",
    "Uzbek",
    "Urdu",
    "Ukranian",
    "Thai",
    "Telugu",
    "Tatar",
    "Tamil",
    "Tadzhik",
    "Swahili",
    "Sranan Tongo",
    "Somali",
    "Sinhalese",
    "Shona",
    "Serbo-Croat",
    "Rusyn",
    "Russian",
    "Quechua",
    "Pushtu",
    "Punjabi",
    "Persian",
    "Papiamento",
    "Oriya",
    "Nepali",
    "Ndebele",
    "Marathi",
    "Moldavian",
    "Malaysian",
    "Malagasay",
    "Macedonian",
    "Laotian",
    "Korean",
    "Khmer",
    "Kazakh",
    "Kannada",
    "Japanese",
    "Indonesian",
    "Hindi",
    "Hebrew",
    "Hausa",
    "Gurani",
    "Gujurati",
    "Greek",
    "Georgian",
    "Fulani",
    "Dari",
    "Chuvash",
    "Chinese",
    "Burmese",
    "Bulgarian",
    "Bengali",
    "Belorussian",
    "Bambora",
    "Azerbaijani",
    "Assamese",
    "Armenian",
    "Arabic",
    "Amharic",
};

void programDataHandler(audiodata *data, void *ctx)
{
     QString audio;
     if (data->ASCTy == 0)
        audio = "DAB";
     else if (data->ASCTy == 63)
        audio = "DAB+";
     else
        audio = "Unknown";

     QString language = "";
     if ((data->language < 0x80) && (data->language >= 0))
         language = dabLanguageCode[data->language & 0x7f];

    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->programData(data->bitRate, audio, language, dabProgramType[data->programType & 0x1f]);
}

void programQualityHandler(int16_t frames, int16_t rs, int16_t aac, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->programQuality(frames, rs, aac);
}

void motDataHandler(uint8_t *data, int len, const char *filename, int contentsubType, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->motData(data, len, QString::fromUtf8(filename), contentsubType);
}

void tiiDataHandler(int tii, void *ctx)
{
    DABDemodSink *sink = (DABDemodSink *)ctx;
    sink->tii(tii);
}

void DABDemodSink::systemData(bool sync, int16_t snr, int32_t freqOffset)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABSystemData *msg = DABDemod::MsgDABSystemData::create(sync, snr, freqOffset);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::ensembleName(const QString& name, int id)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABEnsembleName *msg = DABDemod::MsgDABEnsembleName::create(name, id);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::programName(const QString& name, int id)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABProgramName *msg = DABDemod::MsgDABProgramName::create(name, id);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::programData(int bitrate, const QString& audio, const QString& language, const QString& programType)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABProgramData *msg = DABDemod::MsgDABProgramData::create(bitrate, audio, language, programType);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::fibQuality(int16_t percent)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABFIBQuality *msg = DABDemod::MsgDABFIBQuality::create(percent);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::programQuality(int16_t frames, int16_t rs, int16_t aac)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABProgramQuality *msg = DABDemod::MsgDABProgramQuality::create(frames, rs, aac);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::data(const QString& data)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABData *msg = DABDemod::MsgDABData::create(data);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::motData(const uint8_t *data, int len, const QString& filename, int contentSubType)
{
    if (getMessageQueueToChannel())
    {
        QByteArray byteArray((const char *)data, len);
        DABDemod::MsgDABMOTData *msg = DABDemod::MsgDABMOTData::create(byteArray, filename, contentSubType);
        getMessageQueueToChannel()->push(msg);
    }
}

void DABDemodSink::tii(int tii)
{
    if (getMessageQueueToChannel())
    {
        DABDemod::MsgDABTII *msg = DABDemod::MsgDABTII::create(tii);
        getMessageQueueToChannel()->push(msg);
    }
}

static int16_t scale(int16_t sample, float factor)
{
    int32_t prod = (int32_t)(((int32_t)sample) * factor);
    prod = std::min(prod, 32767);
    prod = std::max(prod, -32768);
    return (int16_t)prod;
}

void DABDemodSink::audio(int16_t *buffer, int size, int samplerate, bool stereo)
{
    (void)stereo;
    (void)samplerate;

    if (samplerate != m_dabAudioSampleRate)
    {
        applyDABAudioSampleRate(samplerate);
        if (getMessageQueueToChannel())
        {
            DABDemod::MsgDABSampleRate *msg = DABDemod::MsgDABSampleRate::create(samplerate);
            getMessageQueueToChannel()->push(msg);
        }
    }

    // buffer is always 2 channels
    for (int i = 0; i < size; i+=2)
    {
        Complex ci, ca;
        if (!m_settings.m_audioMute)
        {
            ci.real(buffer[i]);
            ci.imag(buffer[i+1]);
        }
        else
        {
            ci.real(0.0f);
            ci.imag(0.0f);
        }
        if (m_audioInterpolatorDistance < 1.0f) // interpolate
        {
            while (!m_audioInterpolator.interpolate(&m_audioInterpolatorDistanceRemain, ci, &ca))
            {
                processOneAudioSample(ca);
                m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_audioInterpolator.decimate(&m_audioInterpolatorDistanceRemain, ci, &ca))
            {
                processOneAudioSample(ca);
                m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
            }
        }
    }
}

void DABDemodSink::reset()
{
    dabReset(m_dab);
}

void DABDemodSink::resetService()
{
    dabReset_msc(m_dab);
}

void DABDemodSink::processOneAudioSample(Complex &ci)
{
    float factor = m_settings.m_volume / 5.0f; // Should this be 5 or 10? 5 allows some positive gain

    qint16 l = scale(ci.real(), factor);
    qint16 r = scale(ci.imag(), factor);

    m_audioBuffer[m_audioBufferFill].l = l;
    m_audioBuffer[m_audioBufferFill].r = r;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill)
        {
            qDebug("DABDemodSink::audio: %u/%u audio samples written", res, m_audioBufferFill);
            m_audioFifo.clear();
        }

        m_audioBufferFill = 0;
    }

    m_demodBuffer[m_demodBufferFill++] = l;
    m_demodBuffer[m_demodBufferFill++] = r;

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        if (dataPipes.size() > 0)
        {
            QList<ObjectPipe*>::iterator it = dataPipes.begin();

            for (; it != dataPipes.end(); ++it)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>((*it)->m_element);

                if (fifo) {
                    fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeCI16);
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

DABDemodSink::DABDemodSink(DABDemod *packetDemod) :
        m_dabDemod(packetDemod),
        m_audioSampleRate(48000),
        m_dabAudioSampleRate(10000), // Unused value to begin with
        m_channelSampleRate(DABDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_programSet(false),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_audioFifo(48000)
{
    m_audioBuffer.resize(1<<14);
    m_audioBufferFill = 0;

    m_magsq = 0.0;

    m_demodBuffer.resize(1<<13);
    m_demodBufferFill = 0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    m_api.dabMode = 1; // Latest DAB spec only has mode 1
    m_api.syncsignal_Handler = syncHandler;
    m_api.systemdata_Handler = systemDataHandler;
    m_api.ensemblename_Handler = ensembleNameHandler;
    m_api.programname_Handler = programNameHandler;
    m_api.fib_quality_Handler = fibQualityHandler;
    m_api.audioOut_Handler = audioHandler;
    m_api.dataOut_Handler = dataHandler;
    m_api.bytesOut_Handler = byteHandler;
    m_api.programdata_Handler = programDataHandler;
    m_api.program_quality_Handler = programQualityHandler;
    m_api.motdata_Handler = motDataHandler;
    m_api.tii_data_Handler = tiiDataHandler;
    m_api.timeHandler = nullptr;
    m_dab = dabInit(&m_device,
                &m_api,
                nullptr,
                nullptr,
                this);
    dabStartProcessing(m_dab);
}

DABDemodSink::~DABDemodSink()
{
    dabExit(m_dab);
}

void DABDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance == 1.0f)
        {
            processOneSample(c);
        }
        else if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void DABDemodSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();
    Real magsq = (Real)(magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED));
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // Send sample to DAB library
    std::complex<float> c;
    c.real(ci.real()/SDR_RX_SCALED);
    c.imag(ci.imag()/SDR_RX_SCALED);
    m_device.putSample(c);
}

void DABDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DABDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) DABDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DABDemodSink::applySettings(const DABDemodSettings& settings, bool force)
{
    qDebug() << "DABDemodSink::applySettings:"
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) DABDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_program != m_settings.m_program) || force)
    {
        if (!settings.m_program.isEmpty()) {
            setProgram(settings.m_program);
        } else {
            m_programSet = true;
        }
    }

    m_settings = settings;
}

// Can't call setProgram directly from callback, so we get here via a message
void DABDemodSink::programAvailable(const QString& programName)
{
    if (!m_programSet && (programName == m_settings.m_program)) {
        setProgram(m_settings.m_program);
    }
}

void DABDemodSink::setProgram(const QString& name)
{
    m_programSet = false;
    QByteArray ba = name.toUtf8();
    const char *program = ba.data();
    if (!is_audioService (m_dab, program))
    {
        qWarning() << name << " is not an audio service";
    }
    else
    {
        dataforAudioService(m_dab, program, &m_ad, 0);
        if (!m_ad.defined)
        {
            qWarning() << name << " audio data is not defined";
        }
        else
        {
            dabReset_msc(m_dab);
            set_audioChannel(m_dab, &m_ad);
            m_programSet = true;
        }
    }
}

// Called when audio device sample rate changes
void DABDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("DABDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("DABDemodSink::applyAudioSampleRate: m_audioSampleRate: %d m_dabAudioSampleRate: %d", sampleRate, m_dabAudioSampleRate);

    m_audioInterpolator.create(16, m_dabAudioSampleRate, m_dabAudioSampleRate/2.2f);
    m_audioInterpolatorDistanceRemain = 0;
    m_audioInterpolatorDistance = (Real) m_dabAudioSampleRate / (Real) sampleRate;

    m_audioFifo.setSize(sampleRate);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }

    m_audioSampleRate = sampleRate;
}

// Called when DAB audio sample rate changes
void DABDemodSink::applyDABAudioSampleRate(int sampleRate)
{
    qDebug("DABDemodSink::applyDABAudioSampleRate: m_audioSampleRate: %d new m_dabAudioSampleRate: %d", m_audioSampleRate, sampleRate);

    m_audioInterpolator.create(16, sampleRate, sampleRate/2.2f);
    m_audioInterpolatorDistanceRemain = 0;
    m_audioInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;

    m_dabAudioSampleRate = sampleRate;
}
