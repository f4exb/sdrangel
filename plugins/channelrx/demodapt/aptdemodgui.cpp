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
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>

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
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/graphicsviewzoom.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "aptdemod.h"
#include "aptdemodsink.h"
#include "aptdemodsettingsdialog.h"
#include "aptdemodselectdialog.h"

#include "SWGMapItem.h"

TempScale::TempScale(QGraphicsItem *parent) :
    QGraphicsRectItem(parent)
{
    // Temp scale appears to be -100 to +60C
    // We just draw -100 to +50C, so it's nicely divides up according to the palette
    setRect(30, 30, 25, 240);
    m_gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    m_gradient.setStart(0.0, 0.0);
    m_gradient.setFinalStop(0.0, 1.0);

    for (int i = 0; i < 240; i++)
    {
        int idx = (240 - i) * 3;
        QColor color((unsigned char)apt_TempPalette[idx], (unsigned char)apt_TempPalette[idx+1], (unsigned char)apt_TempPalette[idx+2]);
        m_gradient.setColorAt(i/240.0, color);
    }
}

void TempScale::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    int left = rect().left() + rect().width() + 10;
    painter->setPen(QPen(Qt::black));
    painter->setBrush(m_gradient);
    painter->drawRect(rect());
    painter->drawText(left, rect().top(), "50C");
    painter->drawText(left, rect().top() + rect().height() * 1 / 6, "25C");
    painter->drawText(left, rect().top() + rect().height() * 2 / 6, "0C");
    painter->drawText(left, rect().top() + rect().height() * 3 / 6, "-25C");
    painter->drawText(left, rect().top() + rect().height() * 4 / 6, "-50C");
    painter->drawText(left, rect().top() + rect().height() * 5 / 6, "-75C");
    painter->drawText(left, rect().top() + rect().height(), "-100C");
}

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
    if(m_settings.deserialize(data))
    {
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
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (APTDemod::MsgImage::match(message))
    {
        const APTDemod::MsgImage& imageMsg = (APTDemod::MsgImage&) message;
        m_image = imageMsg.getImage();
        // Display can be corrupted if we try to drawn an image with 0 height
        if (m_image.height() > 0)
        {
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
            bool temp = m_settings.m_channels == APTDemodSettings::TEMPERATURE;
            m_tempScale->setVisible(temp);
            m_tempScaleBG->setVisible(temp);
            if (!temp) {
                m_tempText->setVisible(false);
            }
        }

        QStringList imageTypes = imageMsg.getImageTypes();
        if (imageTypes.size() == 0)
        {
            ui->channelALabel->setText("Channel A");
            ui->channelBLabel->setText("Channel B");
        }
        else
        {
            if (imageTypes[0].isEmpty()) {
                ui->channelALabel->setText("Channel A");
            } else {
                ui->channelALabel->setText(imageTypes[0]);
            }
            if (imageTypes[1].isEmpty()) {
                ui->channelBLabel->setText("Channel B");
            } else {
                ui->channelBLabel->setText(imageTypes[1]);
            }
        }
        QString satelliteName = imageMsg.getSatelliteName();
        if (!satelliteName.isEmpty()) {
            ui->imageContainer->setWindowTitle("Received image from " + satelliteName);
        } else {
            ui->imageContainer->setWindowTitle("Received image");
        }

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
    else if (APTDemod::MsgMapImageName::match(message))
    {
        const APTDemod::MsgMapImageName& mapNameMsg = (APTDemod::MsgMapImageName&) message;
        QString name = mapNameMsg.getName();
        if (!m_mapImages.contains(name)) {
            m_mapImages.append(name);
        }
    }
    else if (APTDemod::MsgResetDecoder::match(message))
    {
        resetDecoder();
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
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
    updateAbsoluteCenterFrequency();
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

void APTDemodGUI::displayLabels()
{
    if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS)
    {
        ui->channelALabel->setVisible(true);
        ui->channelBLabel->setVisible(true);
        ui->precipitation->setVisible(true);
    }
    else if (m_settings.m_channels == APTDemodSettings::CHANNEL_A)
    {
        ui->channelALabel->setVisible(true);
        ui->channelBLabel->setVisible(false);
        ui->precipitation->setVisible(true);
    }
    else if (m_settings.m_channels == APTDemodSettings::CHANNEL_B)
    {
        ui->channelALabel->setVisible(false);
        ui->channelBLabel->setVisible(true);
        ui->precipitation->setVisible(true);
    }
    else if (m_settings.m_channels == APTDemodSettings::TEMPERATURE)
    {
        ui->channelALabel->setVisible(false);
        ui->channelBLabel->setVisible(false);
        ui->precipitation->setVisible(false);
    }
    else
    {
        ui->channelALabel->setVisible(false);
        ui->channelBLabel->setVisible(false);
        ui->precipitation->setVisible(false);
    }
}

void APTDemodGUI::on_channels_currentIndexChanged(int index)
{
    if (index <= (int)APTDemodSettings::CHANNEL_B)
    {
        m_settings.m_channels = (APTDemodSettings::ChannelSelection)index;
    }
    else if (index == (int)APTDemodSettings::VISIBLE)
    {
        m_settings.m_channels = APTDemodSettings::VISIBLE;
        m_settings.m_precipitationOverlay = false;
    }
    else if (index == (int)APTDemodSettings::TEMPERATURE)
    {
        m_settings.m_channels = APTDemodSettings::TEMPERATURE;
        m_settings.m_precipitationOverlay = false;
    }
    else
    {
        m_settings.m_channels = APTDemodSettings::PALETTE;
        m_settings.m_palette = index - (int)APTDemodSettings::PALETTE;
        m_settings.m_precipitationOverlay = false;
    }
    displayLabels();
    applySettings();
}

void APTDemodGUI::on_transparencyThreshold_valueChanged(int value)
{
    m_settings.m_transparencyThreshold = value;
    ui->transparencyThresholdText->setText(QString::number(m_settings.m_transparencyThreshold));
    // Don't applySettings while tracking, as processing an image takes a long time
    if (!ui->transparencyThreshold->isSliderDown()) {
        applySettings();
    }
}

void APTDemodGUI::on_transparencyThreshold_sliderReleased()
{
    applySettings();
}

void APTDemodGUI::on_opacityThreshold_valueChanged(int value)
{
    m_settings.m_opacityThreshold = value;
    ui->opacityThresholdText->setText(QString::number(m_settings.m_opacityThreshold));
    // Don't applySettings while tracking, as processing an image takes a long time
    if (!ui->opacityThreshold->isSliderDown()) {
        applySettings();
    }
}

void APTDemodGUI::on_opacityThreshold_sliderReleased()
{
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
    if (m_settings.m_flip) {
        ui->image->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    } else {
        ui->image->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    }
    applySettings();
}

void APTDemodGUI::on_startStop_clicked(bool checked)
{
    m_settings.m_decodeEnabled = checked;
    applySettings();
}

void APTDemodGUI::resetDecoder()
{
    if (m_pixmapItem != nullptr)
    {
        m_image = QImage();
        m_pixmapItem->setPixmap(QPixmap());
    }
    ui->imageContainer->setWindowTitle("Received image");
    ui->channelALabel->setText("Channel A");
    ui->channelBLabel->setText("Channel B");
}

void APTDemodGUI::on_resetDecoder_clicked()
{
    resetDecoder();
    // Send message to reset decoder to other parts of demod
    m_aptDemod->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
}

void APTDemodGUI::on_showSettings_clicked()
{
    APTDemodSettingsDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        displayPalettes();
        applySettings();
    }
}

// Save image to disk
void APTDemodGUI::on_saveImage_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save image to", "", "*.png *.jpg *.jpeg *.bmp *.ppm *.xbm *.xpm");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            qDebug() << "APT: Saving image to " << fileNames;
            if (!m_image.save(fileNames[0])) {
                QMessageBox::critical(this, "APT Demodulator", QString("Failed to save image to %1").arg(fileNames[0]));
            }
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

void APTDemodGUI::onImageZoomed()
{
    ui->zoomAll->setChecked(false);
}

void APTDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
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
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_aptDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

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
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0),
    m_scene(nullptr),
    m_pixmapItem(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodapt/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
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
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_zoom = new GraphicsViewZoom(ui->image); // Deleted automatically when view is deleted
    connect(m_zoom, SIGNAL(zoomed()), this, SLOT(onImageZoomed()));

    // Create slightly transparent white background so labels can be seen
    m_tempScale = new TempScale();
    m_tempScale->setZValue(2.0);
    m_tempScale->setVisible(false);
    QRectF rect = m_tempScale->rect();
    m_tempScaleBG = new QGraphicsRectItem(rect.left()-10, rect.top()-15, rect.width()+60, rect.height()+45);
    m_tempScaleBG->setPen(QColor(200, 200, 200, 200));
    m_tempScaleBG->setBrush(QColor(200, 200, 200, 200));
    m_tempScaleBG->setZValue(1.0);
    m_tempScaleBG->setVisible(false);
    m_tempText = new QGraphicsSimpleTextItem("");
    m_tempText->setZValue(3.0);
    m_tempText->setVisible(false);
    m_scene = new QGraphicsScene(ui->image);
    m_scene->addItem(m_tempScale);
    m_scene->addItem(m_tempScaleBG);
    m_scene->addItem(m_tempText);
    ui->image->setScene(m_scene);
    ui->image->show();
    ui->image->setDragMode(QGraphicsView::ScrollHandDrag);

    m_scene->installEventFilter(this);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

APTDemodGUI::~APTDemodGUI()
{
    delete ui;
}

bool APTDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == m_scene) && (m_settings.m_channels == APTDemodSettings::TEMPERATURE))
    {
        if (event->type() == QEvent::GraphicsSceneMouseMove)
        {
            QGraphicsSceneMouseEvent *mouseEvent = static_cast<QGraphicsSceneMouseEvent *>(event);
            // Find temperature under cursor
            int x = round(mouseEvent->scenePos().x());
            int y = round(mouseEvent->scenePos().y());
            if ((x >= 0) && (y >= 0) && (x < m_image.width()) && (y < m_image.height()))
            {
                // Map from colored temperature pixel back to greyscale level
                // This is perhaps a bit slow - might be better to give GUI access to greyscale image as well
                QRgb p = m_image.pixel(x, y);
                int r = qRed(p);
                int g = qGreen(p);
                int b = qBlue(p);
                int i;
                for (i = 0; i < 256; i++)
                {
                    if (   (r == (unsigned char)apt_TempPalette[i*3])
                        && (g == (unsigned char)apt_TempPalette[i*3+1])
                        && (b == (unsigned char)apt_TempPalette[i*3+2]))
                    {
                        // Map from palette index to degrees C
                        int temp = (i / 255.0) * 160.0 - 100.0;
                        m_tempText->setText(QString("%1C").arg(temp));
                        int width = m_tempText->boundingRect().width();
                        int height = m_tempText->boundingRect().height();
                        QRectF rect = m_tempScaleBG->rect();
                        m_tempText->setPos(rect.left()+rect.width()/2-width/2, rect.top()+rect.height()-height-5);
                        m_tempText->setVisible(true);
                        break;
                    }
                }
                if (i == 256) {
                    m_tempText->setVisible(false);
                }
            }
            else
            {
                m_tempText->setVisible(false);
            }
        }
    }
    return ChannelGUI::eventFilter(obj, event);
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
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->transparencyThreshold->setValue(m_settings.m_transparencyThreshold);
    ui->transparencyThresholdText->setText(QString::number(m_settings.m_transparencyThreshold));
    ui->opacityThreshold->setValue(m_settings.m_opacityThreshold);
    ui->opacityThresholdText->setText(QString::number(m_settings.m_opacityThreshold));

    ui->startStop->setChecked(m_settings.m_decodeEnabled);
    ui->cropNoise->setChecked(m_settings.m_cropNoise);
    ui->denoise->setChecked(m_settings.m_denoise);
    ui->linear->setChecked(m_settings.m_linearEqualise);
    ui->histogram->setChecked(m_settings.m_histogramEqualise);
    ui->precipitation->setChecked(m_settings.m_precipitationOverlay);
    ui->flip->setChecked(m_settings.m_flip);

    if (m_settings.m_flip) {
        ui->image->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    } else {
        ui->image->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    }

    displayPalettes();
    displayLabels();
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void APTDemodGUI::displayPalettes()
{
    ui->channels->blockSignals(true);
    ui->channels->clear();
    ui->channels->addItem("Both");
    ui->channels->addItem("A");
    ui->channels->addItem("B");
    ui->channels->addItem("Temperature");
    ui->channels->addItem("Visible");
    for (auto palette : m_settings.m_palettes)
    {
        QFileInfo fi(palette);
        ui->channels->addItem(fi.baseName());
    }
    if (m_settings.m_channels == APTDemodSettings::PALETTE)
    {
        ui->channels->setCurrentIndex(((int)m_settings.m_channels) + m_settings.m_palette);
    }
    else
    {
        ui->channels->setCurrentIndex((int)m_settings.m_channels);
    }
    ui->channels->blockSignals(false);
}

void APTDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void APTDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
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

void APTDemodGUI::on_deleteImageFromMap_clicked()
{
    // If more than one image, pop up a dialog to select which to delete
    if (m_mapImages.size() > 1)
    {
        APTDemodSelectDialog dialog(m_mapImages, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            for (auto name : dialog.getSelected())
            {
                deleteImageFromMap(name);
                m_mapImages.removeAll(name);
            }
        }
    }
    else
    {
        for (auto name : m_mapImages) {
            deleteImageFromMap(name);
        }
        m_mapImages.clear();
    }
}

void APTDemodGUI::deleteImageFromMap(const QString &name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_aptDemod, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString());  // Set image to "" to delete it
        swgMapItem->setType(1);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_aptDemod, swgMapItem);
        messageQueue->push(msg);
    }
}

void APTDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &APTDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &APTDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &APTDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &APTDemodGUI::on_channels_currentIndexChanged);
    QObject::connect(ui->transparencyThreshold, &QDial::valueChanged, this, &APTDemodGUI::on_transparencyThreshold_valueChanged);
    QObject::connect(ui->transparencyThreshold, &QDial::sliderReleased, this, &APTDemodGUI::on_transparencyThreshold_sliderReleased);
    QObject::connect(ui->opacityThreshold, &QDial::valueChanged, this, &APTDemodGUI::on_opacityThreshold_valueChanged);
    QObject::connect(ui->opacityThreshold, &QDial::sliderReleased, this, &APTDemodGUI::on_opacityThreshold_sliderReleased);
    QObject::connect(ui->deleteImageFromMap, &QToolButton::clicked, this, &APTDemodGUI::on_deleteImageFromMap_clicked);
    QObject::connect(ui->cropNoise, &ButtonSwitch::clicked, this, &APTDemodGUI::on_cropNoise_clicked);
    QObject::connect(ui->denoise, &ButtonSwitch::clicked, this, &APTDemodGUI::on_denoise_clicked);
    QObject::connect(ui->linear, &ButtonSwitch::clicked, this, &APTDemodGUI::on_linear_clicked);
    QObject::connect(ui->histogram, &ButtonSwitch::clicked, this, &APTDemodGUI::on_histogram_clicked);
    QObject::connect(ui->precipitation, &ButtonSwitch::clicked, this, &APTDemodGUI::on_precipitation_clicked);
    QObject::connect(ui->flip, &ButtonSwitch::clicked, this, &APTDemodGUI::on_flip_clicked);
    QObject::connect(ui->startStop, &ButtonSwitch::clicked, this, &APTDemodGUI::on_startStop_clicked);
    QObject::connect(ui->showSettings, &QToolButton::clicked, this, &APTDemodGUI::on_showSettings_clicked);
    QObject::connect(ui->resetDecoder, &QToolButton::clicked, this, &APTDemodGUI::on_resetDecoder_clicked);
    QObject::connect(ui->saveImage, &QToolButton::clicked, this, &APTDemodGUI::on_saveImage_clicked);
    QObject::connect(ui->zoomIn, &QToolButton::clicked, this, &APTDemodGUI::on_zoomIn_clicked);
    QObject::connect(ui->zoomOut, &QToolButton::clicked, this, &APTDemodGUI::on_zoomOut_clicked);
    QObject::connect(ui->zoomAll, &ButtonSwitch::clicked, this, &APTDemodGUI::on_zoomAll_clicked);
}

void APTDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
