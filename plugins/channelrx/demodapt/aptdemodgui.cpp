///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <limits>
#include <ctype.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QRegExp>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include "aptdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_aptdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/graphicsviewzoom.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "aptdemod.h"
#include "aptdemodsink.h"
#include "aptdemodsettingsdialog.h"

APTDemodGUI* APTDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    APTDemodGUI* gui = new APTDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void APTDemodGUI::destroy()
{
    delete this;
}

void APTDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray APTDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool APTDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool APTDemodGUI::handleMessage(const Message& message)
{
    if (APTDemod::MsgConfigureAPTDemod::match(message))
    {
        qDebug("APTDemodGUI::handleMessage: APTDemod::MsgConfigureAPTDemod");
        const APTDemod::MsgConfigureAPTDemod& cfg = (APTDemod::MsgConfigureAPTDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (APTDemod::MsgImage::match(message))
    {
        const APTDemod::MsgImage& imageMsg = (APTDemod::MsgImage&) message;
        m_image = imageMsg.getImage();
        m_pixmap.convertFromImage(m_image);
        if (m_pixmapItem != nullptr)
        {
            m_pixmapItem->setPixmap(m_pixmap);
            if (ui->zoomAll->isChecked()) {
                ui->image->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
            }
        }
        else
        {
            m_pixmapItem = m_scene->addPixmap(m_pixmap);
            m_pixmapItem->setPos(0, 0);
            ui->image->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
        }

        QStringList imageTypes = imageMsg.getImageTypes();
        if (imageTypes.size() == 0)
        {
            ui->channelALabel->setText("Channel A");
            ui->channelBLabel->setText("Channel B");
        }
        else
        {
            if (imageTypes[0].isEmpty())
                ui->channelALabel->setText("Channel A");
            else
                ui->channelALabel->setText(imageTypes[0]);
            if (imageTypes[1].isEmpty())
                ui->channelBLabel->setText("Channel B");
            else
                ui->channelBLabel->setText(imageTypes[1]);
        }
        QString satelliteName = imageMsg.getSatelliteName();
        if (!satelliteName.isEmpty())
            ui->imageContainer->setWindowTitle("Received image from " + satelliteName);
        else
            ui->imageContainer->setWindowTitle("Received image");
        return true;
    }
    else if (APTDemod::MsgLine::match(message))
    {
        const APTDemod::MsgLine& lineMsg = (APTDemod::MsgLine&) message;

        if (m_image.width() == 0)
        {
            m_image = QImage(lineMsg.getSize(), 1, QImage::Format_Grayscale8);
        }
        else
        {
            m_image = m_image.copy(0, 0, m_image.width(), m_image.height()+1); // Add a line at tne bottom

            if (m_settings.m_flip)
            {
                QImage::Format imageFormat = m_image.format(); // save format
                m_pixmap.convertFromImage(m_image);
                m_pixmap.scroll(0, 1, 0, 0, m_image.width(), m_image.height()-1); // scroll down 1 line
                m_image = m_pixmap.toImage();
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
                m_image.convertTo(imageFormat); // restore format
#else
                m_image = m_image.convertToFormat(imageFormat);
#endif
            }
        }

        int len = std::min(m_image.width(), lineMsg.getSize());
        uchar *p = m_image.scanLine(m_settings.m_flip ? 0 : m_image.height()-1);

        // imageDepth == 8 ? QImage::Format_Grayscale8 : QImage::Format_RGB888

        if (m_image.format() == QImage::Format_Grayscale8)
        {
            std::copy( lineMsg.getLine(), lineMsg.getLine() + len, p);
        }
        else if (m_image.format() == QImage::Format_RGB888)
        {
            for (int i = 0; i < len; i++)
            {
                uchar q = lineMsg.getLine()[i];
                std::fill(&p[3*i], &p[3*i+3], q); // RGB888
            }
        }

        m_pixmap.convertFromImage(m_image);
        if (m_pixmapItem != nullptr)
        {
            m_pixmapItem->setPixmap(m_pixmap);
            if (ui->zoomAll->isChecked()) {
                ui->image->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
            }
        }
        else
        {
            m_pixmapItem = m_scene->addPixmap(m_pixmap);
            m_pixmapItem->setPos(0, 0);
            ui->image->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
        }

        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        return true;
    }

    return false;
}

void APTDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void APTDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void APTDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void APTDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void APTDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void APTDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void APTDemodGUI::on_channels_currentIndexChanged(int index)
{
    m_settings.m_channels = (APTDemodSettings::ChannelSelection)index;
    if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS)
    {
        ui->channelALabel->setVisible(true);
        ui->channelBLabel->setVisible(true);
    }
    else if (m_settings.m_channels == APTDemodSettings::CHANNEL_A)
    {
        ui->channelALabel->setVisible(true);
        ui->channelBLabel->setVisible(false);
    }
    else
    {
        ui->channelALabel->setVisible(false);
        ui->channelBLabel->setVisible(true);
    }
    applySettings();
}

void APTDemodGUI::on_cropNoise_clicked(bool checked)
{
    m_settings.m_cropNoise = checked;
    applySettings();
}

void APTDemodGUI::on_denoise_clicked(bool checked)
{
    m_settings.m_denoise = checked;
    applySettings();
}

void APTDemodGUI::on_linear_clicked(bool checked)
{
    m_settings.m_linearEqualise = checked;
    applySettings();
}

void APTDemodGUI::on_histogram_clicked(bool checked)
{
    m_settings.m_histogramEqualise = checked;
    applySettings();
}

void APTDemodGUI::on_precipitation_clicked(bool checked)
{
    m_settings.m_precipitationOverlay = checked;
    applySettings();
}

void APTDemodGUI::on_flip_clicked(bool checked)
{
    m_settings.m_flip = checked;
    if (m_settings.m_flip)
        ui->image->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    else
        ui->image->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    applySettings();
}

void APTDemodGUI::on_startStop_clicked(bool checked)
{
    m_settings.m_decodeEnabled = checked;
    applySettings();
}

void APTDemodGUI::on_resetDecoder_clicked()
{
    if (m_pixmapItem != nullptr) {
        m_pixmapItem->setPixmap(QPixmap());
    }
    ui->imageContainer->setWindowTitle("Received image");
    // Send message to reset decoder
    m_aptDemod->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
}

void APTDemodGUI::on_showSettings_clicked()
{
    APTDemodSettingsDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
        applySettings();
}

// Save image to disk
void APTDemodGUI::on_saveImage_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save image to", "", "*.png;*.jpg;*.jpeg;*.bmp;*.ppm;*.xbm;*.xpm");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            qDebug() << "APT: Saving image to " << fileNames;
            if (!m_image.save(fileNames[0]))
                QMessageBox::critical(this, "APT Demodulator", QString("Failed to save image to %1").arg(fileNames[0]));
        }
    }
}

void APTDemodGUI::on_zoomIn_clicked()
{
    m_zoom->gentleZoom(1.25);
    ui->zoomAll->setChecked(false);
}

void APTDemodGUI::on_zoomOut_clicked()
{
    m_zoom->gentleZoom(0.75);
    ui->zoomAll->setChecked(false);
}

void APTDemodGUI::on_zoomAll_clicked(bool checked)
{
    if (checked && (m_pixmapItem != nullptr)) {
        ui->image->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    }
}

void APTDemodGUI::on_image_zoomed()
{
    ui->zoomAll->setChecked(false);
}

void APTDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void APTDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_aptDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

APTDemodGUI::APTDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::APTDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true),
    m_tickCount(0),
    m_scene(nullptr),
    m_pixmapItem(nullptr)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_aptDemod = reinterpret_cast<APTDemod*>(rxChannel);
    m_aptDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("APT Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_zoom = new GraphicsViewZoom(ui->image); // Deleted automatically when view is deleted
    connect(m_zoom, SIGNAL(zoomed()), this, SLOT(on_image_zoomed()));

    m_scene = new QGraphicsScene(ui->image);
    ui->image->setScene(m_scene);
    ui->image->show();

    displaySettings();
    applySettings(true);
}

APTDemodGUI::~APTDemodGUI()
{
    delete ui;
}

void APTDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void APTDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        APTDemod::MsgConfigureAPTDemod* message = APTDemod::MsgConfigureAPTDemod::create( m_settings, force);
        m_aptDemod->getInputMessageQueue()->push(message);
    }
}

void APTDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->startStop->setChecked(m_settings.m_decodeEnabled);
    ui->cropNoise->setChecked(m_settings.m_cropNoise);
    ui->denoise->setChecked(m_settings.m_denoise);
    ui->linear->setChecked(m_settings.m_linearEqualise);
    ui->histogram->setChecked(m_settings.m_histogramEqualise);
    ui->precipitation->setChecked(m_settings.m_precipitationOverlay);
    ui->flip->setChecked(m_settings.m_flip);
    if (m_settings.m_flip)
        ui->image->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    else
        ui->image->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    ui->channels->setCurrentIndex((int)m_settings.m_channels);

    displayStreamIndex();

    blockApplySettings(false);
}

void APTDemodGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void APTDemodGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void APTDemodGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void APTDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_aptDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}
