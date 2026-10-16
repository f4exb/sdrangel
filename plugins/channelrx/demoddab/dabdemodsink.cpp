///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2023 Daniele Forsi <iu5hkx@gmail.com>                           //
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

#include <QDateTime>
#include <QDebug>

#include <complex.h>

#include "dsp/datafifo.h"
#include "maincore.h"

#include "modemdab/dabtables.h"

#include "dabdemod.h"
#include "dabdemodsink.h"

using namespace modemdab;

// Input to the receiver is processed in blocks of 10 ms
static const std::size_t NativeBlock = DABDEMOD_CHANNEL_SAMPLE_RATE / 100;
// and its status reported every 0.5 s
static const int NativeStatusBlocks = 50;

void DABDemodSink::systemData(bool sync, float snr, int32_t freqOffset)
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

// Over each status interval. Frames: of DAB+ superframes, those whose Fire code checks; of DAB
// frames, those whose CRC does. RS: of the DAB+ Reed-Solomon codewords, those corrected. AAC:
// of the DAB+ access units, those whose CRC checks. RS and AAC are -1 for DAB; a figure
// stays while no new frames arrive.
void DABDemodSink::audioQuality(const DABReceiverStatus& status)
{
    const AudioCounts counts{true, status.superframes, status.fireCodeErrors, status.rsUncorrectableRows,
        status.audioFrames, status.audioFrameErrors};
    const AudioCounts last = m_audioCounts;
    m_audioCounts = counts;

    // The receiver's counters start again with each service
    if (!last.valid || (counts.superframes < last.superframes) || (counts.audioFrames < last.audioFrames)
        || (counts.fireCodeErrors < last.fireCodeErrors) || (counts.rsUncorrectableRows < last.rsUncorrectableRows)
        || (counts.audioFrameErrors < last.audioFrameErrors)) {
        return;
    }

    const auto percent = [](int64_t bad, int64_t total) {
        return static_cast<int16_t>(std::lround(100.0 * static_cast<double>(std::max<int64_t>(total - bad, 0)) / static_cast<double>(total)));
    };
    const int64_t superframes = counts.superframes - last.superframes;
    const int64_t frames = counts.audioFrames - last.audioFrames;

    if (status.dabPlus)
    {
        // A superframe has a Reed-Solomon codeword per 8 kbit/s of the sub-channel
        const int64_t codewords = superframes * (status.bitrate / 8);
        if (superframes > 0) {
            m_frameQuality = percent(counts.fireCodeErrors - last.fireCodeErrors, superframes);
        }
        if (codewords > 0) {
            m_rsQuality = percent(counts.rsUncorrectableRows - last.rsUncorrectableRows, codewords);
        }
        if (frames > 0) {
            m_aacQuality = percent(counts.audioFrameErrors - last.audioFrameErrors, frames);
        }
    }
    else
    {
        if (frames > 0) {
            m_frameQuality = percent(counts.audioFrameErrors - last.audioFrameErrors, frames);
        }
        m_rsQuality = -1;
        m_aacQuality = -1;
    }

    programQuality(m_frameQuality, m_rsQuality, m_aacQuality);
}

void DABDemodSink::resetAudioQuality()
{
    m_audioCounts = AudioCounts();
    m_frameQuality = 0;
    m_rsQuality = 0;
    m_aacQuality = 0;
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

static int16_t scale(Real sample, float factor)
{
    int32_t prod = (int32_t)(((int32_t)sample) * factor);
    prod = std::min(prod, 32767);
    prod = std::max(prod, -32768);
    return (int16_t)prod;
}

bool DABDemodSink::isAudioActive() const
{
    return QDateTime::currentMSecsSinceEpoch() - m_lastAudioMs.load() < 500;
}

void DABDemodSink::audio(int16_t *buffer, int size, int samplerate, bool stereo)
{
    (void)stereo;
    (void)samplerate;

    m_lastAudioMs = QDateTime::currentMSecsSinceEpoch();

    const bool rateChanged = samplerate != m_dabAudioSampleRate;
    if (rateChanged) {
        applyDABAudioSampleRate(samplerate);
    }

    // Reported again for each program, as the program's other details are
    if ((rateChanged || !m_dabAudioSampleRateReported) && getMessageQueueToChannel())
    {
        DABDemod::MsgDABSampleRate *msg = DABDemod::MsgDABSampleRate::create(samplerate);
        getMessageQueueToChannel()->push(msg);
        m_dabAudioSampleRateReported = true;
    }

    // buffer is always 2 channels
    for (int i = 0; i < size; i+=2)
    {
        Complex ci, ca;
        ci.real(buffer[i]);
        ci.imag(buffer[i+1]);

        if (m_audioInterpolatorDistance == 1.0f)
        {
            processOneAudioSample(ci);
        }
        else if (m_audioInterpolatorDistance < 1.0f) // interpolate
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
    m_native->reset();
    m_nativeBuffer.clear();
    m_nativeVersion = 0;
    m_nativeEnsembleId = -1;
    m_nativeEnsembleLabel.clear();
    m_nativePrograms.clear();
    m_nativeProgramData.clear();
    m_nativeTII = -2;
    m_nativeServiceInfo.clear();
    m_dabAudioSampleRateReported = false;
    resetAudioQuality();
    m_programSet = m_settings.m_program.isEmpty();
    m_lastRSCorrected = 0;
}

void DABDemodSink::resetService()
{
    m_native->selectService(0);
    m_nativeProgramData.clear();
    m_dabAudioSampleRateReported = false;
    resetAudioQuality();
    m_programSet = false;
    selectNativeProgram();
}

void DABDemodSink::processNative()
{
    m_native->process(m_nativeBuffer.data(), m_nativeBuffer.size());
    m_nativeBuffer.clear();

    if (++m_nativeStatusBlocks >= NativeStatusBlocks)
    {
        m_nativeStatusBlocks = 0;
        nativeStatus();
    }
    else if (m_native->labelUpdated())
    {
        nativeLabel();
    }
}

// Report the state of the receiver
void DABDemodSink::nativeStatus()
{
    const DABReceiverStatus status = m_native->status();
    const DABEnsemble& ensemble = m_native->ensemble();

    systemData(status.locked && status.ficLocked, status.snrDB, (int32_t) std::lround(status.frequencyOffsetHz));
    fibQuality((int16_t) std::lround(status.fibQuality));

    if (ensemble.version() != m_nativeVersion)
    {
        m_nativeVersion = ensemble.version();
        const DABEnsembleInfo& info = ensemble.info();

        if ((info.eid >= 0) && (info.eid != m_nativeEnsembleId))
        {
            if (m_nativeEnsembleId >= 0)
            {
                // Another ensemble: its services replace those of the previous one
                m_nativePrograms.clear();
                m_nativeProgramData.clear();
                m_nativeTII = -2;
                m_programSet = m_settings.m_program.isEmpty();
            }
            m_nativeEnsembleId = info.eid;
            m_nativeEnsembleLabel.clear();
        }
        if ((info.eid >= 0) && !info.label.empty() && (info.label != m_nativeEnsembleLabel))
        {
            m_nativeEnsembleLabel = info.label;
            ensembleName(QString::fromStdString(info.label), info.eid);
        }

        // Services with audio, once they have a label
        for (const DABService& service : ensemble.services())
        {
            const DABServiceComponent *primary = service.primaryComponent();
            if (!primary || !primary->isAudio() || service.label.empty()) {
                continue;
            }
            auto it = m_nativePrograms.find(service.sid);
            if ((it == m_nativePrograms.end()) || (it->second != service.label))
            {
                m_nativePrograms[service.sid] = service.label;
                programName(QString::fromStdString(service.label), (int) service.sid);
            }
        }
        selectNativeProgram();
    }

    // The selected service
    if (status.serviceFound)
    {
        DABService service;
        if (ensemble.service(status.sid, service))
        {
            const DABServiceComponent *primary = service.primaryComponent();
            const QString audio = status.dabPlus ? "DAB+" : "DAB";
            const QString language = primary && (primary->language >= 0)
                ? QString::fromStdString(DABTables::languageName(primary->language)) : QString();
            const QString programType = service.pty >= 0
                ? QString::fromStdString(DABTables::programmeTypeName(service.pty, ensemble.northAmericanPTy())) : QString();
            const std::string summary = std::to_string(status.bitrate) + audio.toStdString() + language.toStdString() + programType.toStdString();
            if (summary != m_nativeProgramData)
            {
                m_nativeProgramData = summary;
                programData(status.bitrate, audio, language, programType);
            }
        }
        audioQuality(status);
    }

    if (m_native->labelUpdated()) {
        nativeLabel();
    }
    if (m_native->journalineUpdated() && getMessageQueueToChannel())
    {
        auto objects = std::make_shared<const DABDemod::JournalineObjects>(m_native->journaline());
        getMessageQueueToChannel()->push(DABDemod::MsgDABJournaline::create(objects));
    }
    if (m_native->websiteUpdated() && getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(DABDemod::MsgDABWebsite::create(m_native->website(), status.website));
    }
    if (m_native->spiUpdated())
    {
        m_nativeSPI = m_native->spi();
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(DABDemod::MsgDABSPI::create(m_nativeSPI, m_nativeEnsembleId));
        }
    }
    nativeServiceInfo(status);

    const auto transmitters = m_native->tii();
    if (!transmitters.empty())
    {
        const int value = (transmitters[0].mainId << 8) | transmitters[0].subId;
        if (value != m_nativeTII)
        {
            m_nativeTII = value;
            tii(value);
        }
    }
    else if (m_native->tiiAnalysed() && (m_nativeTII != DABDemod::TIINone))
    {
        // Synchronized for a few seconds with no TII: the ensemble sends none
        m_nativeTII = DABDemod::TIINone;
        tii(DABDemod::TIINone);
    }
}

// The selected service's announcement, programme on now and links, when they change
void DABDemodSink::nativeServiceInfo(const DABReceiverStatus& status)
{
    DABDemod::ServiceInfo info;
    info.sid = status.serviceFound ? status.sid : 0;
    if (status.announcementType >= 0)
    {
        info.announcement = QString::fromStdString(DABTables::announcementTypeName(status.announcementType));
        info.announcementService = QString::fromStdString(status.announcementService);
        info.announcementSwitched = status.announcementSwitched;
    }
    if (m_nativeSPI && info.sid)
    {
        const modemdr::SPIProgramme *programme = m_nativeSPI->programmeAt(info.sid, QDateTime::currentSecsSinceEpoch());
        if (programme)
        {
            info.programmeNow = QString::fromStdString(programme->text.name());
            info.programmeNowDescription = QString::fromStdString(programme->text.description());
        }
    }
    if (info.sid) {
        info.links = m_native->serviceLinks();
    }

    // Changes only
    QString summary = QString("%1|%2|%3|%4|%5|%6").arg(info.sid).arg(info.announcement).arg(info.announcementService)
        .arg(info.announcementSwitched).arg(info.programmeNow).arg(info.programmeNowDescription);
    for (const DABServiceLink& link : info.links)
    {
        summary += QString("|%1 %2 %3 %4 %5 %6").arg(link.kind).arg(link.id).arg(link.hard).arg(link.active)
            .arg(QString::fromStdString(link.label)).arg(link.eids.size());
        for (int64_t f : link.frequencies) {
            summary += QString(" %1").arg(f);
        }
    }
    if (summary == m_nativeServiceInfo) {
        return;
    }
    m_nativeServiceInfo = summary;
    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(DABDemod::MsgDABServiceInfo::create(info));
    }
}

// SPI files of the ensemble kept from before
void DABDemodSink::setSPICache(const std::vector<modemdr::MOTFile>& files)
{
    m_native->setSPICache(files);
}

// The dynamic label and its DL Plus tags
void DABDemodSink::nativeLabel()
{
    const DABDynamicLabel label = m_native->dynamicLabel();
    data(QString::fromStdString(label.text));

    QList<QPair<QString, QString>> tags;
    for (const DABDLPlusObject& object : label.objects) {
        tags.append({QString::fromStdString(DABDLPlusObject::contentTypeName(object.contentType)), QString::fromStdString(object.text)});
    }
    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(DABDemod::MsgDABDLPlus::create(tags, label.itemRunning));
    }
}

// Input level and clipping, and the receiver's statistics, twice a second
void DABDemodSink::signalQuality()
{
    DABDemod::SignalQuality quality;
    const double seconds = (double) m_inputSamples / m_channelSampleRate;
    const double meanPower = m_inputPower / (m_inputSamples * (double) SDR_RX_SCALEF * SDR_RX_SCALEF);
    quality.inputLevel = meanPower > 0.0 ? (float) (10.0 * std::log10(meanPower)) : -100.0f;
    quality.clipping = (float) (100.0 * m_inputClipped / m_inputSamples);
    m_inputPower = 0.0;
    m_inputSamples = 0;
    m_inputClipped = 0;

    std::vector<float> carrierCN;
    const DABReceiverStatus status = m_native->status();
    quality.measured = true;
    quality.cn = status.cnDB;
    quality.nullDepth = status.nullDepthDB;
    quality.mscBER = status.mscBER;
    quality.ficBER = status.ficBER;
    quality.rsCorrectedRate = (float) (std::max<int64_t>(0, status.rsCorrectedBytes - m_lastRSCorrected) / seconds);
    m_lastRSCorrected = status.rsCorrectedBytes;
    quality.framesLost = (int) status.framesLost;
    quality.reacquisitions = std::max(0, status.locks - 1);
    if (status.locked) {
        carrierCN = m_native->carrierCN();
    }
    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(DABDemod::MsgDABCarrierCN::create(carrierCN, status.cnDB));
    }
    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(DABDemod::MsgDABSignalQuality::create(quality));
    }
}

// Select the service named in the settings, once it is in the ensemble
void DABDemodSink::selectNativeProgram()
{
    if (m_programSet || m_settings.m_program.isEmpty()) {
        return;
    }
    const QString wanted = m_settings.m_program.trimmed();

    for (const auto& program : m_nativePrograms)
    {
        if (QString::fromStdString(program.second).trimmed() == wanted)
        {
            m_native->selectService(program.first);
            m_nativeProgramData.clear();
            m_programSet = true;
            break;
        }
    }
}

void DABDemodSink::processOneAudioSample(Complex &ci)
{
    float factor = m_settings.m_volume / 5.0f; // Should this be 5 or 10? 5 allows some positive gain

    qint16 l = scale(ci.real(), factor);
    qint16 r = scale(ci.imag(), factor);

    // Muting DAB's direct output must not silence consumers of the demod pipe.
    m_audioBuffer[m_audioBufferFill].l = m_settings.m_audioMute ? 0 : l;
    m_audioBuffer[m_audioBufferFill].r = m_settings.m_audioMute ? 0 : r;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

        if (res != m_audioBufferFill)
        {
            qDebug("DABDemodSink::audio: %lu/%lu audio samples written", res, m_audioBufferFill);
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

DABDemodSink::DABDemodSink() :
        m_audioSampleRate(48000),
        m_dabAudioSampleRate(10000), // Unused value to begin with
        m_dabAudioSampleRateReported(false),
        m_channelSampleRate(DABDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_programSet(false),
        m_nativeVersion(0),
        m_nativeEnsembleId(-1),
        m_nativeTII(-2),
        m_nativeStatusBlocks(0),
        m_inputPower(0.0),
        m_inputSamples(0),
        m_inputClipped(0),
        m_lastRSCorrected(0),
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

    m_native.reset(new DABReceiver());
    // Decode in a thread of its own, so the baseband thread only synchronizes and demodulates
    m_native->setThreaded(true);
    m_native->setAudioCallback([this](const int16_t *samples, int frames, int sampleRate) {
        audio(const_cast<int16_t *>(samples), 2 * frames, sampleRate, true);
    });
    m_native->setSlideCallback([this](const modemdr::MOTObject& slide) {
        motData(slide.body.data(), (int) slide.body.size(), QString::fromStdString(slide.name), slide.contentSubtype);
    });
    m_nativeBuffer.reserve(NativeBlock);

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

DABDemodSink::~DABDemodSink()
{
}

void DABDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    // Samples at or near full scale (an 8-bit ADC's extremes)
    const Real clipLevel = 0.98f * SDR_RX_SCALEF;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        m_inputPower += (double) c.real() * c.real() + (double) c.imag() * c.imag();
        if ((std::abs(c.real()) >= clipLevel) || (std::abs(c.imag()) >= clipLevel)) {
            m_inputClipped++;
        }
        if (++m_inputSamples >= m_channelSampleRate / 2) {
            signalQuality();
        }
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

    std::complex<float> c;
    c.real(ci.real()/SDR_RX_SCALED);
    c.imag(ci.imag()/SDR_RX_SCALED);

    m_nativeBuffer.push_back(c);
    if (m_nativeBuffer.size() >= NativeBlock) {
        processNative();
    }
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

void DABDemodSink::applySettings(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force)
{
    qDebug() << "DABDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) DABDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    const bool programChanged = (settingsKeys.contains("program") && (settings.m_program != m_settings.m_program)) || force;
    const bool coherentChanged = (settingsKeys.contains("coherent") && (settings.m_coherent != m_settings.m_coherent)) || force;
    const bool announcementsChanged = (settingsKeys.contains("announcementTypes") && (settings.m_announcementTypes != m_settings.m_announcementTypes)) || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (coherentChanged)
    {
        // Applied by the receiver's decoding thread; the audio restarts
        m_native->setDetection(m_settings.m_coherent ? DABReceiver::Detection::Coherent : DABReceiver::Detection::ThreeSymbol);
    }

    if (announcementsChanged) {
        m_native->setAnnouncementTypes(static_cast<uint16_t>(m_settings.m_announcementTypes));
    }

    if (programChanged)
    {
        if (!m_settings.m_program.isEmpty()) {
            setProgram();
        } else {
            m_programSet = true;
        }
    }
}

// Select the program in the settings, once it is in the ensemble
void DABDemodSink::setProgram()
{
    m_programSet = false;
    m_native->selectService(0);
    m_nativeProgramData.clear();
    m_dabAudioSampleRateReported = false;
    resetAudioQuality();
    selectNativeProgram();
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
