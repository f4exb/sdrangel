///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#include <algorithm>

#include <QTime>
#include <QDebug>

#include "aptdemod.h"
#include "aptdemodimageworker.h"

MESSAGE_CLASS_DEFINITION(APTDemodImageWorker::MsgConfigureAPTDemodImageWorker, Message)
MESSAGE_CLASS_DEFINITION(APTDemodImageWorker::MsgSaveImageToDisk, Message)

APTDemodImageWorker::APTDemodImageWorker() :
    m_messageQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive)
{
    for (int y = 0; y < APT_MAX_HEIGHT; y++)
    {
        m_image.prow[y] = new float[APT_PROW_WIDTH];
        m_tempImage.prow[y] = new float[APT_PROW_WIDTH];
    }

    resetDecoder();
}

APTDemodImageWorker::~APTDemodImageWorker()
{
    m_inputMessageQueue.clear();

    for (int y = 0; y < APT_MAX_HEIGHT; y++)
    {
        delete[] m_image.prow[y];
        delete[] m_tempImage.prow[y];
    }
}

void APTDemodImageWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void APTDemodImageWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void APTDemodImageWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void APTDemodImageWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool APTDemodImageWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureAPTDemodImageWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAPTDemodImageWorker& cfg = (MsgConfigureAPTDemodImageWorker&) cmd;
        qDebug("APTDemodImageWorker::handleMessage: MsgConfigureAPTDemodImageWorker");
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgSaveImageToDisk::match(cmd))
    {
        saveImageToDisk();
        return true;
    }
    else if (APTDemod::MsgPixels::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const APTDemod::MsgPixels& pixelsMsg = (APTDemod::MsgPixels&) cmd;
        const float *pixels = pixelsMsg.getPixels();
        processPixels(pixels);
        delete[] pixels;
        return true;
    }
    else if (APTDemod::MsgResetDecoder::match(cmd))
    {
        resetDecoder();
        return true;
    }
    else
    {
        return false;
    }
}

void APTDemodImageWorker::applySettings(const APTDemodSettings& settings, bool force)
{
    (void) force;
    bool callProcessImage = false;

    if ((settings.m_cropNoise != m_settings.m_cropNoise) ||
        (settings.m_denoise != m_settings.m_denoise) ||
        (settings.m_linearEqualise != m_settings.m_linearEqualise) ||
        (settings.m_histogramEqualise != m_settings.m_histogramEqualise) ||
        (settings.m_precipitationOverlay != m_settings.m_precipitationOverlay) ||
        (settings.m_flip != m_settings.m_flip) ||
        (settings.m_channels != m_settings.m_channels))
    {
        // Call after settings have been applied
        callProcessImage = true;
    }

    m_settings = settings;

    if (callProcessImage) {
        sendImageToGUI();
    }
}

void APTDemodImageWorker::resetDecoder()
{
    m_image.nrow = 0;
    m_tempImage.nrow = 0;
    m_greyImage = QImage(APT_IMG_WIDTH, APT_MAX_HEIGHT, QImage::Format_Grayscale8);
    m_greyImage.fill(0);
    m_colourImage = QImage(APT_IMG_WIDTH, APT_MAX_HEIGHT, QImage::Format_RGB888);
    m_colourImage.fill(0);
    m_satelliteName = "";
}

void APTDemodImageWorker::processPixels(const float *pixels)
{
    std::copy(pixels, pixels + APT_PROW_WIDTH, m_image.prow[m_image.nrow]);

    if (m_image.nrow % 20 == 0) { // send full image only every 20 lines
        sendImageToGUI();
    } else { // else send unprocessed line just to show stg is moving
        sendLineToGUI();
    }

    m_image.nrow++;
}

void APTDemodImageWorker::sendImageToGUI()
{
    // Send image to GUI
    if (m_messageQueueToGUI)
    {
        QStringList imageTypes;
        QImage image = processImage(imageTypes);
        m_messageQueueToGUI->push(APTDemod::MsgImage::create(image, imageTypes, m_satelliteName));
    }
}

void APTDemodImageWorker::sendLineToGUI()
{
    if (m_messageQueueToGUI)
    {
        float *pixels = m_image.prow[m_image.nrow];
        uchar *line;
        APTDemod::MsgLine *msg = APTDemod::MsgLine::create(&line);

        if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS)
        {
            for (int i = 0; i < APT_IMG_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i]);
            }
            msg->setSize(APT_IMG_WIDTH);
        }
        else if (m_settings.m_channels == APTDemodSettings::CHANNEL_A)
        {
            for (int i = 0; i < APT_CH_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i + APT_CHA_OFFSET]);
            }
            msg->setSize(APT_CH_WIDTH);
        }
        else
        {
            for (int i = 0; i < APT_CH_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i + APT_CHB_OFFSET]);
            }
            msg->setSize(APT_CH_WIDTH);
        }

        m_messageQueueToGUI->push(msg);
    }
}

QImage APTDemodImageWorker::processImage(QStringList& imageTypes)
{
    copyImage(&m_tempImage, &m_image);

    // Calibrate channels according to wavelength
    if (m_tempImage.nrow >= APT_CALIBRATION_ROWS)
    {
        m_tempImage.chA = apt_calibrate(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        m_tempImage.chB = apt_calibrate(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
        QStringList channelTypes({
            "",  // Unknown
            "Visible (0.58-0.68 um)",
            "Near-IR (0.725-1.0 um)",
            "Near-IR (1.58-1.64 um)",
            "Mid-infrared (3.55-3.93 um)",
            "Thermal-infrared (10.3-11.3 um)",
            "Thermal-infrared (11.5-12.5 um)"
            });

        imageTypes.append(channelTypes[m_tempImage.chA]);
        imageTypes.append(channelTypes[m_tempImage.chB]);
    }

    // Crop noise due to low elevation at top and bottom of image
    if (m_settings.m_cropNoise)
        m_tempImage.zenith -= apt_cropNoise(&m_tempImage);

    // Denoise filter
    if (m_settings.m_denoise)
    {
        apt_denoise(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_denoise(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    // Flip image if satellite pass is North to South
    if (m_settings.m_flip)
    {
        apt_flipImage(&m_tempImage, APT_CH_WIDTH, APT_CHA_OFFSET);
        apt_flipImage(&m_tempImage, APT_CH_WIDTH, APT_CHB_OFFSET);
    }

    // Linear equalise to improve contrast
    if (m_settings.m_linearEqualise)
    {
        apt_linearEnhance(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_linearEnhance(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    // Histogram equalise to improve contrast
    if (m_settings.m_histogramEqualise)
    {
        apt_histogramEqualise(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_histogramEqualise(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    if (m_settings.m_precipitationOverlay)
    {
        // Overlay precipitation
        for (int r = 0; r < m_tempImage.nrow; r++)
        {
            uchar *l = m_colourImage.scanLine(r);
            for (int i = 0; i < APT_IMG_WIDTH; i++)
            {
                float p = m_tempImage.prow[r][i];

                if ((i >= APT_CHB_OFFSET) && (i < APT_CHB_OFFSET + APT_CH_WIDTH) && (p >= 198))
                {
                    apt_rgb_t rgb = apt_applyPalette(apt_PrecipPalette, p - 198);
                    // Negative float values get converted to positive uchars here
                    l[i*3] = (uchar)rgb.r;
                    l[i*3+1] = (uchar)rgb.g;
                    l[i*3+2] = (uchar)rgb.b;
                    int a = i - APT_CHB_OFFSET + APT_CHA_OFFSET;
                    l[a*3] = (uchar)rgb.r;
                    l[a*3+1] = (uchar)rgb.g;
                    l[a*3+2] = (uchar)rgb.b;
                }
                else
                {
                    uchar q = roundAndClip(p);
                    l[i*3] = q;
                    l[i*3+1] = q;
                    l[i*3+2] = q;
                }
            }
        }
        return extractImage(m_colourImage);
    }
    else
    {
        for (int r = 0; r < m_tempImage.nrow; r++)
        {
            uchar *l = m_greyImage.scanLine(r);

            for (int i = 0; i < APT_IMG_WIDTH; i++)
            {
                float p = m_tempImage.prow[r][i];
                l[i] = roundAndClip(p);
            }
        }
        return extractImage(m_greyImage);
    }
}

QImage APTDemodImageWorker::extractImage(QImage image)
{
    if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS) {
        return image.copy(0, 0, APT_IMG_WIDTH, m_tempImage.nrow);
    } else if (m_settings.m_channels == APTDemodSettings::CHANNEL_A) {
        return image.copy(APT_CHA_OFFSET, 0, APT_CH_WIDTH, m_tempImage.nrow);
    } else {
        return image.copy(APT_CHB_OFFSET, 0, APT_CH_WIDTH, m_tempImage.nrow);
    }
}

void APTDemodImageWorker::saveImageToDisk()
{
    QStringList imageTypes;
    QImage image = processImage(imageTypes);

    if (image.height() >= m_settings.m_autoSaveMinScanLines)
    {
        QString filename;
        QDateTime datetime = QDateTime::currentDateTime();
        filename = QString("apt_%1_%2.png").arg(m_satelliteName).arg(datetime.toString("yyyyMMdd_hhmm"));

        if (!m_settings.m_autoSavePath.isEmpty())
        {
            if (m_settings.m_autoSavePath.endsWith('/')) {
                filename = m_settings.m_autoSavePath + filename;
            } else {
                filename = m_settings.m_autoSavePath + '/' + filename;
            }
        }

        if (!image.save(filename)) {
            qCritical() << "Failed to save APT image to: " << filename;
        }
    }
}

void APTDemodImageWorker::copyImage(apt_image_t *dst, apt_image_t *src)
{
    dst->nrow = src->nrow;
    dst->zenith = src->zenith;
    dst->chA = src->chA;
    dst->chB = src->chB;

    for (int i = 0; i < src->nrow; i++) {
        std::copy(src->prow[i], src->prow[i] + APT_PROW_WIDTH, dst->prow[i]);
    }
}

uchar APTDemodImageWorker::roundAndClip(float p)
{
    int q = (int) round(p);
    q = q > 255 ? 255 : q < 0 ? 0 : q;
    return q;
}
