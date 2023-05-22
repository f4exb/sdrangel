///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>
#include <QMenu>

#include "SWGMapItem.h"

#include "ilsdemodgui.h"

#include "device/deviceset.h"
#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/morsedemod.h"
#include "ui_ilsdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/units.h"
#include "util/morse.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "dsp/spectrumvis.h"
#include "gui/crightclickenabler.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "gui/audioselectdialog.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "maincore.h"

#include "ilsdemod.h"
#include "ilsdemodsink.h"

MESSAGE_CLASS_DEFINITION(ILSDemodGUI::MsgGSAngle, Message)

// 3.1.3.2 - ILS can use one or two carrier frequencies
// If one is used, frequency tolerance is +/- 0.005% -> 5.5kHz
// If two are used, frequency tolerance is +/- 0.002% and they should be symmetric about the assigned frequency
// Frequency separation should be within [5kHz,14kHz]
// Called "offset carrier" https://www.pprune.org/tech-log/108843-vhf-offset-carrier.html

// ICAO Anntex 10 Volume 1 - 3.1.6.1
const QStringList ILSDemodGUI::m_locFrequencies = {
    "108.10", "108.15", "108.30", "108.35", "108.50", "108.55", "180.70", "108.75",
    "108.90", "108.95", "109.10", "109.15", "109.30", "109.35", "109.50", "109.55",
    "109.70", "109.75", "109.90", "109.95", "110.10", "110.15", "110.30", "110.35",
    "110.50", "110.55", "110.70", "110.75", "110.90", "110.95", "111.10", "111.15",
    "111.30", "111.35", "111.50", "111.55", "111.70", "111.75", "111.90", "111.95",
};

const QStringList ILSDemodGUI::m_gsFrequencies = {
    "334.70", "334.55", "334.10", "333.95", "329.90", "329.75", "330.50", "330.35",
    "329.30", "329.15", "331.40", "331.25", "332.00", "331.85", "332.60", "332.45",
    "333.20", "333.05", "333.80", "333.65", "334.40", "334.25", "335.00", "334.85",
    "329.60", "329.45", "330.20", "330.05", "330.80", "330.65", "331.70", "331.55",
    "332.30", "332.15", "332.90", "332.75", "333.50", "333.35", "331.10", "330.95",
};

// UK data from NATS: https://nats-uk.ead-it.com/cms-nats/opencms/en/Publications/AIP/
// Note that AIP data is more accurate in tables than on charts
// Some values tweaked from AIP to better align with 3D Map
const QList<ILSDemodGUI::ILS> ILSDemodGUI::m_ils = {
    {"EGKB", "IBGH", "21",  109350000, 205.71, 3.0, 51.338272, 0.038065,  569, 15.25, 1840, 1.13},
    {"EGKK", "IWW",  "26L", 110900000, 257.59, 3.0, 51.150680, -0.171943, 212, 15.5,  3060, 0.06},
    {"EGKK", "IGG",  "08R", 110900000, 77.63,  3.0, 51.145872, -0.206807, 212, 15.5,  3140, -0.06},
    {"EGLL", "ILL",  "27L", 109500000, 269.72, 3.0, 51.464960, -0.434108, 93,  17.1,  3960, 0.0},
    {"EGLL", "IRR",  "27R", 110300000, 269.71, 3.0, 51.477678, -0.433291, 99,  17.7,  4190, 0.0},
    {"EGLL", "IAA",  "09L", 110300000, 89.67,  3.0, 51.477503, -0.484984, 99,  15.5,  4020, 0.0},
    {"EGLL", "IBB",  "09R", 109500000, 89.68,  3.0, 51.464795, -0.482305, 93,  15.25, 3750, 0.0},
    {"EGLC", "ILST", "09",  111150000, 92.87,  5.5, 51.505531, 0.045766,  48,  10.7,  1510, 0.0},
    {"EGLC", "ILSR", "27",  111150000, 272.89, 5.5, 51.504927, 0.064960,  48,  10.7,  1580, 0.0},
    {"EGSS", "ISX",  "22",  110500000, 222.78, 3.0, 51.895165, 0.250051,  352, 14.9,  3430, 0.0},
    {"EGSS", "ISED", "04",  110500000, 42.78,  3.0, 51.877054, 0.222887,  352, 16.2,  3130, 0.0},
};

ILSDemodGUI* ILSDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ILSDemodGUI* gui = new ILSDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ILSDemodGUI::destroy()
{
    delete this;
}

void ILSDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray ILSDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ILSDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QString ILSDemodGUI::formatAngleDirection(float angle) const
{
    QString text;
    if (m_settings.m_mode == ILSDemodSettings::LOC)
    {
        if (angle == 0.0) {
            text = "Centre";
        } else if (angle > 0.0) {
            text = "Left";
        } else {
            text = "Right";
        }
    }
    else
    {
        if (angle == 0.0) {
            text = "On slope";
        } else if (angle > 0.0) {
            text = "Above";
        } else {
            text = "Below";
        }
    }
    return text;
}

bool ILSDemodGUI::handleMessage(const Message& message)
{
    if (ILSDemod::MsgConfigureILSDemod::match(message))
    {
        qDebug("ILSDemodGUI::handleMessage: ILSDemod::MsgConfigureILSDemod");
        const ILSDemod::MsgConfigureILSDemod& cfg = (ILSDemod::MsgConfigureILSDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
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
    else if (MorseDemod::MsgReportIdent::match(message))
    {
        MorseDemod::MsgReportIdent& report = (MorseDemod::MsgReportIdent&) message;

        QString ident = report.getIdent();
        QString identString = Morse::toString(ident); // Convert Morse to a string

        ui->morseIdent->setText(Morse::toSpacedUnicode(ident) + "   " + identString);

        // Check it matches ident setting
        if (identString == m_settings.m_ident) {
            ui->morseIdent->setStyleSheet("QLabel { color: white }");
        } else {
            ui->morseIdent->setStyleSheet("QLabel { color: red }");
        }

        return true;
    }
    else if (ILSDemod::MsgAngleEstimate::match(message))
    {
        ILSDemod::MsgAngleEstimate& report = (ILSDemod::MsgAngleEstimate&) message;
        ui->md90->setValue(report.getModDepth90());
        ui->md150->setValue(report.getModDepth150());
        float sdmPercent = report.getSDM() * 100.0f;
        ui->sdm->setValue(sdmPercent);
        // 3.1.3.5.3.6.1
        if ((sdmPercent < 30.0f) || (sdmPercent > 60.0f)) {
            ui->sdm->setStyleSheet("QLineEdit { background: rgb(255, 0, 0); }");
        } else {
            ui->sdm->setStyleSheet("");
        }
        ui->ddm->setText(formatDDM(report.getDDM()));
        float angle = report.getAngle();
        float absAngle = std::abs(angle);
        ui->angle->setText(QString("%1").arg(absAngle, 0, 'f', 1));
        ui->angleDirection->setText(formatAngleDirection(angle));
        ui->pCarrier->setText(QString("%1").arg(report.getPowerCarrier(), 0, 'f', 1));
        ui->p90->setText(QString("%1").arg(report.getPower90(), 0, 'f', 1));
        ui->p150->setText(QString("%1").arg(report.getPower150(), 0, 'f', 1));
        if (m_settings.m_mode == ILSDemodSettings::LOC) {
            ui->cdi->setLocalizerDDM(report.getDDM());
        } else {
            ui->cdi->setGlideSlopeDDM(report.getDDM());
        }
        if (m_settings.m_mode == ILSDemodSettings::GS)
        {
            m_gsAngle = angle;
            if (!sendToLOCChannel(angle)) {
                drawPath();
            }
        }
        else
        {
            m_locAngle = angle;
            drawPath();
        }
        return true;
    }
    else if (MsgGSAngle::match(message))
    {
        MsgGSAngle& report = (MsgGSAngle&) message;
        m_gsAngle = report.getAngle();
        drawPath();
        return true;
    }

    return false;
}

void ILSDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void ILSDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void ILSDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ILSDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ILSDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value;
    ui->rfBWText->setText(formatFrequency((int)bw));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void ILSDemodGUI::on_mode_currentIndexChanged(int index)
{
    ui->frequency->clear();
    m_settings.m_mode = (ILSDemodSettings::Mode)index;
    if (m_settings.m_mode == ILSDemodSettings::LOC)
    {
        ui->cdi->setMode(CourseDeviationIndicator::LOC);
        ui->frequency->setMinimumSize(60, 0);
        for (const auto &freq : m_locFrequencies) {
            ui->frequency->addItem(freq);
        }
        scanAvailableChannels();
    }
    else
    {
        ui->cdi->setMode(CourseDeviationIndicator::GS);
        ui->frequency->setMinimumSize(110, 0);
        for (int i = 0; i < m_locFrequencies.size(); i++)
        {
            QString text = m_locFrequencies[i] + "/" + m_gsFrequencies[i];
            ui->frequency->addItem(text);
        }
        closePipes();
    }
    applySettings();
}

void ILSDemodGUI::on_frequency_currentIndexChanged(int index)
{
    m_settings.m_frequencyIndex = index;
    if ((index >= 0) && (index < m_locFrequencies.size()))
    {
        QString text;
        if (m_settings.m_mode == ILSDemodSettings::LOC) {
            text = m_locFrequencies[index];
        } else {
            text = m_gsFrequencies[index];
        }
        double frequency = text.toDouble() * 1e6;
        ChannelWebAPIUtils::setCenterFrequency(m_ilsDemod->getDeviceSetIndex(), frequency);
    }
    applySettings();
}

void ILSDemodGUI::on_average_clicked(bool checked)
{
    m_settings.m_average = checked;
    applySettings();
}

void ILSDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
    applySettings();
}

void ILSDemodGUI::on_squelch_valueChanged(int value)
{
    ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
    applySettings();
}

void ILSDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void ILSDemodGUI::on_thresh_valueChanged(int value)
{
    ui->threshText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_identThreshold = value / 10.0;
    applySettings();
}

void ILSDemodGUI::on_ddmUnits_currentIndexChanged(int index)
{
    m_settings.m_ddmUnits = (ILSDemodSettings::DDMUnits) index;
    applySettings();
}

void ILSDemodGUI::on_ident_editingFinished()
{
   m_settings.m_ident = ui->ident->currentText();
   applySettings();
}

// 3.1.3.7.3 Note 1
float ILSDemodGUI::calcCourseWidth(int m_thresholdToLocalizer) const
{
    float width = 2.0f * Units::radiansToDegrees(atan(105.0f/m_thresholdToLocalizer));
    width = std::min(6.0f, width);
    return width;
}

void ILSDemodGUI::on_ident_currentIndexChanged(int index)
{
    m_settings.m_ident = ui->ident->currentText();
    if ((index >= 0) && (index < m_ils.size()))
    {
        m_disableDrawILS = true;
        ui->trueBearing->setValue(m_ils[index].m_trueBearing);
        ui->latitude->setText(QString::number(m_ils[index].m_latitude, 'f', 8));
        on_latitude_editingFinished();
        ui->longitude->setText(QString::number(m_ils[index].m_longitude, 'f', 8));
        on_longitude_editingFinished();
        ui->elevation->setValue(m_ils[index].m_elevation);
        ui->glidePath->setValue(m_ils[index].m_glidePath);
        ui->height->setValue(m_ils[index].m_refHeight);
        ui->courseWidth->setValue(calcCourseWidth(m_ils[index].m_thresholdToLocalizer));
        ui->slope->setValue(m_ils[index].m_slope);
        ui->runway->setText(QString("%1 %2").arg(m_ils[index].m_airportICAO).arg(m_ils[index].m_runway));
        on_runway_editingFinished();
        int frequency = m_ils[index].m_frequency;
        QString freqText = QString("%1").arg(frequency / 1000000.0f, 0, 'f', 2);
        if (m_settings.m_mode == ILSDemodSettings::GS)
        {
            int index = m_locFrequencies.indexOf(freqText);
            if (index >= 0) {
                freqText = m_gsFrequencies[index];
            }
        }
        ui->frequency->setCurrentText(freqText);
        m_disableDrawILS = false;
    }
    drawILSOnMap();
    applySettings();
}

void ILSDemodGUI::on_runway_editingFinished()
{
    m_settings.m_runway = ui->runway->text();
    drawILSOnMap();
    applySettings();
}

void ILSDemodGUI::on_findOnMap_clicked()
{
    QString pos = QString("%1,%2").arg(m_settings.m_latitude).arg(m_settings.m_longitude);
    FeatureWebAPIUtils::mapFind(pos);
}

void ILSDemodGUI::on_addMarker_clicked()
{
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getAltitude();

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", mapPipes);
    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

        QString mode = m_settings.m_mode == ILSDemodSettings::LOC ? "LOC" : "GS";
        QString name = QString("%1 M%2").arg(mode).arg(m_markerNo);
        swgMapItem->setName(new QString(name));

        swgMapItem->setLatitude(stationLatitude);
        swgMapItem->setLongitude(stationLongitude);
        swgMapItem->setAltitude(stationAltitude);
        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
        swgMapItem->setFixedPosition(true);
        swgMapItem->setPositionDateTime(new QString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));

        swgMapItem->setImage(new QString(QString("qrc:///aprs/aprs/aprs-symbols-24-0-06.png")));
        QString text;
        if (!ui->ddm->text().isEmpty())
        {
            text = QString("ILS %7 Marker %1\n90Hz: %2%\n150Hz: %3%\nDDM: %4\nAngle: %5%6 %8")
                            .arg(m_markerNo)
                            .arg(ui->md90->value())
                            .arg(ui->md150->value())
                            .arg(ui->ddm->text())
                            .arg(ui->angle->text())
                            .arg(QChar(0xb0))
                            .arg(mode)
                            .arg(ui->angleDirection->text());
        }
        else
        {
            text = QString("ILS %1 Marker %2\nNo data").arg(mode).arg(m_markerNo);
        }
        swgMapItem->setText(new QString(text));

        if (!m_mapMarkers.contains(name)) {
            m_mapMarkers.insert(name, true);
        }

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ilsDemod, swgMapItem);
        messageQueue->push(msg);

        m_markerNo++;
    }
}

void ILSDemodGUI::removeFromMap(const QString& name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ilsDemod, swgMapItem);
        messageQueue->push(msg);
    }
}

void ILSDemodGUI::on_clearMarkers_clicked()
{
    QMutableHashIterator<QString, bool> itr(m_mapMarkers);
    while (itr.hasNext())
    {
        itr.next();
        removeFromMap(itr.key());
        itr.remove();
    }
    m_markerNo = 0;
}

// Lats and longs in decimal degrees. Distance in metres. Bearing in degrees.
// https://www.movable-type.co.uk/scripts/latlong.html
static void calcRadialEndPoint(float startLatitude, float startLongitude, float distance, float bearing, float &endLatitude, float &endLongitude)
{
    double startLatRad = startLatitude*M_PI/180.0;
    double startLongRad = startLongitude*M_PI/180.0;
    double theta = bearing*M_PI/180.0;
    double earthRadius = 6378137.0; // At equator
    double delta = distance/earthRadius;
    double endLatRad = std::asin(sin(startLatRad)*cos(delta) + cos(startLatRad)*sin(delta)*cos(theta));
    double endLongRad = startLongRad + std::atan2(sin(theta)*sin(delta)*cos(startLatRad), cos(delta) - sin(startLatRad)*sin(endLatRad));
    endLatitude = endLatRad*180.0/M_PI;
    endLongitude = endLongRad*180.0/M_PI;
}

void ILSDemodGUI::addPolygonToMap(const QString& name, const QString& label, const QList<QGeoCoordinate>& coordinates, QRgb color)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        if (!m_mapILS.contains(name)) {
            m_mapILS.insert(name, true);
        }

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

            swgMapItem->setName(new QString(name));
            swgMapItem->setLabel(new QString(label));
            swgMapItem->setLatitude(coordinates[0].latitude());
            swgMapItem->setLongitude(coordinates[0].longitude());
            swgMapItem->setAltitude(coordinates[0].altitude());
            QString image = QString("none");
            swgMapItem->setImage(new QString(image));
            swgMapItem->setImageRotation(0);
            swgMapItem->setFixedPosition(true);
            swgMapItem->setAltitudeReference(3); // 1 - CLAMP_TO_GROUND, 3 - CLIP_TO_GROUND
            swgMapItem->setColorValid(true);
            swgMapItem->setColor(color);
            QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

            for (const auto& coord : coordinates)
            {
                SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(coord.latitude());
                c->setLongitude(coord.longitude());
                c->setAltitude(coord.altitude());
                coords->append(c);
            }

            swgMapItem->setCoordinates(coords);
            swgMapItem->setType(2);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ilsDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void ILSDemodGUI::addLineToMap(const QString& name, const QString& label, float startLatitude, float startLongitude, float startAltitude, float endLatitude, float endLongitude, float endAltitude)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        if (!m_mapILS.contains(name)) {
            m_mapILS.insert(name, true);
        }

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

            swgMapItem->setName(new QString(name));
            swgMapItem->setLabel(new QString(label));
            swgMapItem->setLatitude(startLatitude);
            swgMapItem->setLongitude(startLongitude);
            swgMapItem->setAltitude(startAltitude);
            QString image = QString("none");
            swgMapItem->setImage(new QString(image));
            swgMapItem->setImageRotation(0);
            swgMapItem->setFixedPosition(true);
            swgMapItem->setAltitudeReference(3); // CLIP_TO_GROUND
            QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

            SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(startLatitude);
            c->setLongitude(startLongitude);
            c->setAltitude(startAltitude);
            coords->append(c);

            c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(endLatitude);
            c->setLongitude(endLongitude);
            c->setAltitude(endAltitude);
            coords->append(c);

            swgMapItem->setCoordinates(coords);
            swgMapItem->setType(3);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ilsDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void ILSDemodGUI::clearILSFromMap()
{
    QMutableHashIterator<QString, bool> itr(m_mapILS);
    while (itr.hasNext())
    {
        itr.next();
        removeFromMap(itr.key());
        itr.remove();
    }
}

void ILSDemodGUI::drawILSOnMap()
{
    m_ilsValid = false;
    if (m_disableDrawILS) {
        return;
    }

    // Check and get all needed settings to display the ILS
    float thresholdLatitude, thresholdLongitude;
    if (!Units::stringToLatitudeAndLongitude(m_settings.m_latitude + " " + m_settings.m_longitude, thresholdLatitude, thresholdLongitude))
    {
        qDebug() << "ILSDemodGUI::drawILSOnMap: Lat/lon invalid: " << (m_settings.m_latitude + " " + m_settings.m_longitude);
        return;
    }
    // Can't set this to 0 and use CLIP_TO_GROUND or CLAMP_TO_GROUND, as ground at GARP is below runway at somewhere like EGKB
    // Perhaps Map needs API so we can query altitude in current terrain
    m_altitude = Units::feetToMetres(m_settings.m_elevation);
    float slope = m_settings.m_slope;

    // Estimate touchdown point, which is roughly where glide-path should intersect runway
    float thresholdToTouchDown = m_settings.m_refHeight / tan(Units::degreesToRadians(m_settings.m_glidePath));
    calcRadialEndPoint(thresholdLatitude, thresholdLongitude, thresholdToTouchDown, m_settings.m_trueBearing, m_tdLatitude, m_tdLongitude);

    // Estimate localizer reference point (can be localizer location, but may be behind - similar to GARP)
    float thresholdToLoc = 105.0f / tan(Units::degreesToRadians(m_settings.m_courseWidth / 2.0f));
    calcRadialEndPoint(thresholdLatitude, thresholdLongitude, thresholdToLoc, m_settings.m_trueBearing, m_locLatitude, m_locLongitude);
    m_locAltitude = m_altitude + thresholdToLoc * (slope / 100.0f);

    // Update GPS angle
    updateGPSAngle();

    // Check to see if there are any Maps open
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", pipes);
    if (pipes.size() == 0) {
        return;
    }
    m_hasDrawnILS = true;

    // Check to see if we have a LOC Demod open, if so, G/S Demod doesn't need to draw
    if (m_settings.m_mode == ILSDemodSettings::GS)
    {
        MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "ilsdemod", pipes);
        if (pipes.size() > 0) {
            return;
        }
    }

    clearILSFromMap();

    // Check to see if map is using Ellipsoid for terrain
    int featureSetIndex = -1;
    int featureIndex = -1;
    QString terrain;
    Feature *feature = FeatureWebAPIUtils::getFeature(featureSetIndex, featureIndex, "sdrangel.feature.map");
    if (feature && ChannelWebAPIUtils::getFeatureSetting(featureSetIndex, featureIndex, "terrain", terrain))
    {
        if (terrain == "Ellipsoid")
        {
            m_altitude = 0.0f;
            slope = 0.0f;
        }
    }

    m_locDistance = Units::nauticalMilesToMetres(18); // 3.1.3.3.1 (Can be 25NM)
    m_gsDistance = Units::nauticalMilesToMetres(10);
    m_ilsValid = true;

     // Runway true bearing points towards runway (direction aircraft land) - calculate bearing towards aircraft
    float bearing = fmod(m_settings.m_trueBearing - 180.0f, 360.0f);

   // Calculate course line (actually true bearing (rather than magnetic course) so we can plot on map)
    float lineEndLatitude, lineEndLongitude;
    calcRadialEndPoint(m_locLatitude, m_locLongitude, m_locDistance, bearing, lineEndLatitude, lineEndLongitude);

    QList<QGeoCoordinate> coords;
    float midLatitude, midLongitude;
    float endLatitude, endLongitude;
    // Calculate sector lines
    m_locToTouchdown = thresholdToLoc - thresholdToTouchDown;
    float tdToEnd =  (m_locDistance - m_locToTouchdown);
    float locEndAltitude = m_altitude + sin(Units::degreesToRadians(m_settings.m_glidePath)) * tdToEnd;
    // Coverage should be +/- 10 degrees
    // We plot course sector (i.e. +/- 0.155 DDM)
    float bearingR = fmod(bearing - m_settings.m_courseWidth / 2.0, 360.0f);
    calcRadialEndPoint(m_locLatitude, m_locLongitude, m_locToTouchdown, bearingR, midLatitude, midLongitude);
    calcRadialEndPoint(midLatitude, midLongitude, tdToEnd, bearingR, endLatitude, endLongitude);

    coords.clear();
    coords.append(QGeoCoordinate(m_locLatitude, m_locLongitude, m_locAltitude));
    coords.append(QGeoCoordinate(midLatitude, midLongitude, m_altitude));
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    addPolygonToMap("ILS Localizer Sector 150Hz Runway", "", coords, QColor(0, 200, 0, 125).rgba());

    coords.clear();
    coords.append(QGeoCoordinate(midLatitude, midLongitude, m_altitude));
    coords.append(QGeoCoordinate(endLatitude, endLongitude, locEndAltitude));
    coords.append(QGeoCoordinate(lineEndLatitude, lineEndLongitude, locEndAltitude));
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    addPolygonToMap("ILS Localizer Sector 150Hz", "", coords, QColor(0, 200, 0, 125).rgba());

    float bearingL = fmod(bearing + m_settings.m_courseWidth / 2.0, 360.0f);
    calcRadialEndPoint(m_locLatitude, m_locLongitude, m_locToTouchdown, bearingL, midLatitude, midLongitude);
    calcRadialEndPoint(midLatitude, midLongitude, tdToEnd, bearingL, endLatitude, endLongitude);

    coords.clear();
    coords.append(QGeoCoordinate(m_locLatitude, m_locLongitude, m_locAltitude));
    coords.append(QGeoCoordinate(midLatitude, midLongitude, m_altitude));
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    addPolygonToMap("ILS Localizer Sector 90Hz Runway", "", coords, QColor(0, 150, 0, 125).rgba());

    coords.clear();
    coords.append(QGeoCoordinate(midLatitude, midLongitude, m_altitude));
    coords.append(QGeoCoordinate(endLatitude, endLongitude, locEndAltitude));
    coords.append(QGeoCoordinate(lineEndLatitude, lineEndLongitude, locEndAltitude));
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    addPolygonToMap("ILS Localizer Sector 90Hz", "", coords, QColor(0, 150, 0, 125).rgba());

    // Calculate start of glide path
    // 1.75*theta and 0.45*theta are the required limits of glidepath coverage (Figure 10)
    // We plot full scale deflection (i.e. +/- 0.175 DDM)
    float gpEndLatitude, gpEndLongitude;
    calcRadialEndPoint(m_tdLatitude, m_tdLongitude, m_gsDistance, bearing, gpEndLatitude, gpEndLongitude);
    float endAltitudeA = m_altitude + sin(Units::degreesToRadians(m_settings.m_glidePath + 0.36f)) * m_gsDistance;
    float endAltitudeB = m_altitude + sin(Units::degreesToRadians(m_settings.m_glidePath - 0.36f)) * m_gsDistance;
    float gpEndAltitude = m_altitude + sin(Units::degreesToRadians(m_settings.m_glidePath)) * m_gsDistance;

    coords.clear();
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    coords.append(QGeoCoordinate(gpEndLatitude, gpEndLongitude, endAltitudeA));
    coords.append(QGeoCoordinate(gpEndLatitude, gpEndLongitude, gpEndAltitude));
    addPolygonToMap("ILS Glide Path 90Hz", "", coords, QColor(150, 150, 0, 175).rgba());

    coords.clear();
    coords.append(QGeoCoordinate(m_tdLatitude, m_tdLongitude, m_altitude));
    coords.append(QGeoCoordinate(gpEndLatitude, gpEndLongitude, endAltitudeB));
    coords.append(QGeoCoordinate(gpEndLatitude, gpEndLongitude, gpEndAltitude));
    addPolygonToMap("ILS Glide Path 150Hz", "", coords, QColor(200, 200, 0, 175).rgba());

    drawPath();
}

void ILSDemodGUI::drawPath()
{
    if (!m_hasDrawnILS) {
        drawILSOnMap();
    }
    if (!m_ilsValid) {
        return;
    }
    float locAngle = std::isnan(m_locAngle) ? 0.0f : m_locAngle;
    float gsAngle = std::isnan(m_gsAngle) ? m_settings.m_glidePath : (m_settings.m_glidePath + m_gsAngle);

    // Plot line at current estimated loc & g/s angles
    float bearing = fmod(m_settings.m_trueBearing - 180.0f + locAngle, 360.0f);
    float tdLatitude, tdLongitude;
    float endLatitude, endLongitude;
    float distance = m_locDistance - m_locToTouchdown;

    calcRadialEndPoint(m_locLatitude, m_locLongitude, m_locToTouchdown, bearing, tdLatitude, tdLongitude);
    calcRadialEndPoint(tdLatitude, tdLongitude, distance, bearing, endLatitude, endLongitude);
    float endAltitude = m_altitude + sin(Units::degreesToRadians(gsAngle)) * distance;

    QStringList runwayStrings = m_settings.m_runway.split(" ");
    QString label;
    if (runwayStrings.size() == 2) {
        label = QString("%1 %2").arg(runwayStrings[1]).arg(m_settings.m_ident); // Assume first string is airport ICAO
    } else if (!runwayStrings[0].isEmpty()) {
        label = QString("%1 %2").arg(runwayStrings[0]).arg(m_settings.m_ident);
    } else {
        label = QString("%2%3T %1").arg(m_settings.m_ident).arg((int)std::round(m_settings.m_trueBearing)).arg(QChar(0xb0));
    }
    addLineToMap("ILS Radial Runway", label, m_locLatitude, m_locLongitude, m_locAltitude, tdLatitude, tdLongitude, m_altitude);
    addLineToMap("ILS Radial", "", tdLatitude, tdLongitude, m_altitude, endLatitude, endLongitude, endAltitude);
}

void ILSDemodGUI::updateGPSAngle()
{
    // Get GPS position
    float gpsLatitude = MainCore::instance()->getSettings().getLatitude();
    float gpsLongitude = MainCore::instance()->getSettings().getLongitude();
    float gpsAltitude = MainCore::instance()->getSettings().getAltitude();
    QGeoCoordinate gpsPos(gpsLatitude, gpsLongitude, gpsAltitude);

    if (m_settings.m_mode == ILSDemodSettings::LOC)
    {
        // Calculate angle from localizer to GPS
        QGeoCoordinate locPos(m_locLatitude, m_locLongitude, m_altitude);
        qreal angle = gpsPos.azimuthTo(locPos);
        angle = angle - m_settings.m_trueBearing;
        ui->gpsAngle->setText(QString::number(std::abs(angle), 'f', 1));
        ui->gpsAngleDirection->setText(formatAngleDirection(angle));
    }
    else
    {
        // Calculate angle from glide path runway intersection to GPS
        QGeoCoordinate tdPos(m_tdLatitude, m_tdLongitude, m_altitude);
        qreal d = tdPos.distanceTo(gpsPos);
        float h = gpsAltitude - m_altitude;
        float angle = Units::radiansToDegrees(atan(h/d)) - m_settings.m_glidePath;
        ui->gpsAngle->setText(QString::number(std::abs(angle), 'f', 1));
        ui->gpsAngleDirection->setText(formatAngleDirection(angle));
    }
}

void ILSDemodGUI::on_trueBearing_valueChanged(double value)
{
    m_settings.m_trueBearing = (float) value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_elevation_valueChanged(int value)
{
    m_settings.m_elevation = value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_latitude_editingFinished()
{
    m_settings.m_latitude = ui->latitude->text();
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_longitude_editingFinished()
{
    m_settings.m_longitude = ui->longitude->text();
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_glidePath_valueChanged(double value)
{
    m_settings.m_glidePath = value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_height_valueChanged(double value)
{
    m_settings.m_refHeight = (float)value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_courseWidth_valueChanged(double value)
{
    m_settings.m_courseWidth = (float)value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_slope_valueChanged(double value)
{
    m_settings.m_slope = (float)value;
    applySettings();
    drawILSOnMap();
}

void ILSDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void ILSDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void ILSDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void ILSDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void ILSDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void ILSDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void ILSDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_ilsDemod->getNumberOfDeviceStreams());
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

ILSDemodGUI::ILSDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ILSDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_squelchOpen(false),
    m_tickCount(0),
    m_markerNo(0),
    m_disableDrawILS(false),
    m_hasDrawnILS(false),
    m_locAngle(NAN),
    m_gsAngle(NAN)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodils/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_ilsDemod = reinterpret_cast<ILSDemod*>(rxChannel);
    m_ilsDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_spectrumVis = m_ilsDemod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_scopeVis = m_ilsDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    // Scope settings to display the IQ waveforms
    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceDataI, traceDataQ;
    traceDataI.m_projectionType = Projector::ProjectionReal;
    traceDataI.m_amp = 1.0;      // for -1 to +1
    traceDataI.m_ofs = 0.0;      // vertical offset
    traceDataQ.m_projectionType = Projector::ProjectionImag;
    traceDataQ.m_amp = 1.0;
    traceDataQ.m_ofs = 0.0;
    ui->scopeGUI->changeTrace(0, traceDataI);
    ui->scopeGUI->addTrace(traceDataQ);
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayXYV);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(ILSDemodSettings::ILSDEMOD_SPECTRUM_SAMPLE_RATE);
    m_scopeVis->configure(500, 1, 0, 0, true);   // not working!
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(ILSDemodSettings::ILSDEMOD_SPECTRUM_SAMPLE_RATE);
    ui->glSpectrum->setMeasurementParams(SpectrumSettings::MeasurementPeaks, 0, 1000, 90, 150, 1, 5, true, 1);
    ui->glSpectrum->setMeasurementsVisible(true);

    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("ILS Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    on_mode_currentIndexChanged(ui->mode->currentIndex()); // Populate frequencies

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    for (const auto& ils : m_ils) {
        ui->ident->addItem(ils.m_ident);
    }

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ILSDemodGUI::preferenceChanged);

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    drawILSOnMap();

    bool devMode = false;
    ui->pCarrierLabel->setVisible(devMode);
    ui->pCarrier->setVisible(devMode);
    ui->pCarrierUnits->setVisible(devMode);
    ui->p90Label->setVisible(devMode);
    ui->p90->setVisible(devMode);
    ui->p90Units->setVisible(devMode);
    ui->p150Label->setVisible(devMode);
    ui->p150->setVisible(devMode);
    ui->p150Units->setVisible(devMode);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_fftSize = 256;
    spectrumSettings.m_fftWindow = FFTWindow::Flattop;  // To match what's used in sink
    spectrumSettings.m_averagingMode = SpectrumSettings::AvgModeMoving;
    spectrumSettings.m_averagingValue = 1;
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_ssb = false;
    spectrumSettings.m_displayHistogram = false;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_spectrumStyle = SpectrumSettings::Gradient;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    scanAvailableChannels();
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &ILSDemodGUI::handleChannelAdded
    );
}

ILSDemodGUI::~ILSDemodGUI()
{
    QObject::disconnect(&MainCore::instance()->getMasterTimer(), &QTimer::timeout, this, &ILSDemodGUI::tick);
    QObject::disconnect(MainCore::instance(), &MainCore::channelAdded, this, &ILSDemodGUI::handleChannelAdded);
    on_clearMarkers_clicked();
    clearILSFromMap();
    delete ui;
}

void ILSDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ILSDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        ILSDemod::MsgConfigureILSDemod* message = ILSDemod::MsgConfigureILSDemod::create( m_settings, force);
        m_ilsDemod->getInputMessageQueue()->push(message);
    }
}

QString ILSDemodGUI::formatDDM(float ddm) const
{
    switch (m_settings.m_ddmUnits)
    {
    case ILSDemodSettings::PERCENT:
        return QString::number(ddm * 100.0f, 'f', 1);
    case ILSDemodSettings::MICROAMPS:
        if (m_settings.m_mode == ILSDemodSettings::LOC) {
            return QString::number(ddm * 967.75f, 'f', 1);  // 0.155 -> 150uA
        } else {
            return QString::number(ddm * 857.125f, 'f', 1);
        }
    default:
        return QString::number(ddm, 'f', 3);
    }
}

QString ILSDemodGUI::formatFrequency(int frequency) const
{
    QString suffix = "";
    if (width() > 450) {
        suffix = " Hz";
    }
    return QString("%1%2").arg(frequency).arg(suffix);
}

void ILSDemodGUI::displaySettings()
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

    ui->rfBWText->setText(formatFrequency((int)m_settings.m_rfBandwidth));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->average->setChecked(m_settings.m_average);

    ui->thresh->setValue(m_settings.m_identThreshold * 10.0);
    ui->threshText->setText(QString("%1").arg(m_settings.m_identThreshold, 0, 'f', 1));

    m_disableDrawILS = true;
    ui->mode->setCurrentIndex((int) m_settings.m_mode);
    ui->frequency->setCurrentIndex(m_settings.m_frequencyIndex);
    ui->ident->setCurrentText(m_settings.m_ident);
    ui->runway->setText(m_settings.m_runway);
    on_runway_editingFinished();
    ui->trueBearing->setValue(m_settings.m_trueBearing);
    ui->height->setValue(m_settings.m_refHeight);
    ui->courseWidth->setValue(m_settings.m_courseWidth);
    ui->slope->setValue(m_settings.m_slope);
    ui->latitude->setText(m_settings.m_latitude);
    ui->longitude->setText(m_settings.m_longitude);
    ui->elevation->setValue(m_settings.m_elevation);
    ui->glidePath->setValue(m_settings.m_glidePath);
    m_disableDrawILS = false;

    updateIndexLabel();

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
    drawILSOnMap();
}

void ILSDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ILSDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ILSDemodGUI::audioSelect(const QPoint& p)
{
    qDebug("ILSDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void ILSDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_ilsDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    int audioSampleRate = m_ilsDemod->getAudioSampleRate();
    bool squelchOpen = m_ilsDemod->getSquelchOpen();

    if (squelchOpen != m_squelchOpen)
    {
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_squelchOpen = squelchOpen;
    }

    if (!m_hasDrawnILS && (m_tickCount % 25 == 0))
    {
        // Check to see if there are any Maps open - Is there a signal we can use instead?
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "mapitems", mapPipes);
        if (mapPipes.size() > 0) {
            drawILSOnMap();
        }
    }

    m_tickCount++;
}

void ILSDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void ILSDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select CSV file to log data to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySettings();
        }
    }
}

void ILSDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ILSDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ILSDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->frequency, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_frequency_currentIndexChanged);
    QObject::connect(ui->average, &QPushButton::clicked, this, &ILSDemodGUI::on_average_clicked);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &ILSDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &ILSDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &ILSDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->thresh, &QDial::valueChanged, this, &ILSDemodGUI::on_thresh_valueChanged);
    QObject::connect(ui->ddmUnits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_ddmUnits_currentIndexChanged);
    QObject::connect(ui->ident, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_ident_currentIndexChanged);
    QObject::connect(ui->ident->lineEdit(), &QLineEdit::editingFinished, this, &ILSDemodGUI::on_ident_editingFinished);
    QObject::connect(ui->runway, &QLineEdit::editingFinished, this, &ILSDemodGUI::on_runway_editingFinished);
    QObject::connect(ui->trueBearing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ILSDemodGUI::on_trueBearing_valueChanged);
    QObject::connect(ui->elevation, QOverload<int>::of(&QSpinBox::valueChanged), this, &ILSDemodGUI::on_elevation_valueChanged);
    QObject::connect(ui->latitude, &QLineEdit::editingFinished, this, &ILSDemodGUI::on_latitude_editingFinished);
    QObject::connect(ui->longitude, &QLineEdit::editingFinished, this, &ILSDemodGUI::on_longitude_editingFinished);
    QObject::connect(ui->glidePath, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ILSDemodGUI::on_glidePath_valueChanged);
    QObject::connect(ui->height, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ILSDemodGUI::on_height_valueChanged);
    QObject::connect(ui->courseWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ILSDemodGUI::on_courseWidth_valueChanged);
    QObject::connect(ui->slope, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ILSDemodGUI::on_slope_valueChanged);
    QObject::connect(ui->findOnMap, &QPushButton::clicked, this, &ILSDemodGUI::on_findOnMap_clicked);
    QObject::connect(ui->addMarker, &QPushButton::clicked, this, &ILSDemodGUI::on_addMarker_clicked);
    QObject::connect(ui->clearMarkers, &QPushButton::clicked, this, &ILSDemodGUI::on_clearMarkers_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &ILSDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &ILSDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &ILSDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &ILSDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &ILSDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ILSDemodGUI::on_channel2_currentIndexChanged);
}

void ILSDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void ILSDemodGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude)) {
        updateGPSAngle();
    }
}

bool ILSDemodGUI::sendToLOCChannel(float angle)
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ilsDemod, "ilsdemod", pipes);
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MsgGSAngle *msg = MsgGSAngle::create(angle);
        messageQueue->push(msg);
    }
    return pipes.size() > 0;
}

void ILSDemodGUI::closePipes()
{
    for (const auto channel : m_availableChannels)
    {
        ObjectPipe *pipe = MainCore::instance()->getMessagePipes().unregisterProducerToConsumer(channel, m_ilsDemod, "ilsdemod");
        if (pipe)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

            if (messageQueue) {
                disconnect(messageQueue, &MessageQueue::messageEnqueued, this, nullptr);  // Have to use nullptr, as slot is a lambda.
            }
        }
    }
}

void ILSDemodGUI::scanAvailableChannels()
{
    MainCore *mainCore = MainCore::instance();
    MessagePipes& messagePipes = mainCore->getMessagePipes();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    m_availableChannels.clear();
    int deviceSetIndex = 0;

    for (const auto& deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = deviceSet->getChannelAt(chi);

                if ((channel->getURI() == "sdrangel.channel.ilsdemod") && !m_availableChannels.contains(channel) && (m_ilsDemod != channel))
                {
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, m_ilsDemod, "ilsdemod");
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    QObject::connect(
                        messageQueue,
                        &MessageQueue::messageEnqueued,
                        this,
                        [=](){ this->handleChannelMessageQueue(messageQueue); },
                        Qt::QueuedConnection
                    );
                    QObject::connect(
                        pipe,
                        &ObjectPipe::toBeDeleted,
                        this,
                        &ILSDemodGUI::handleMessagePipeToBeDeleted
                    );
                    m_availableChannels.insert(channel);
                }
            }
        }
        deviceSetIndex++;
    }
}

void ILSDemodGUI::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("ILSDemodGUI::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();
    DeviceSet *deviceSet = deviceSets[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && (channel->getURI() == "sdrangel.channel.ilsdemod"))
    {
        if (!m_availableChannels.contains(channel) && (m_ilsDemod != channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, m_ilsDemod, "ilsdemod");
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            QObject::connect(
                pipe,
                &ObjectPipe::toBeDeleted,
                this,
                &ILSDemodGUI::handleMessagePipeToBeDeleted
            );
            m_availableChannels.insert(channel);
        }
    }
}

void ILSDemodGUI::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        qDebug("ILSDemodGUI::handleMessagePipeToBeDeleted: removing channel at (%p)", object);
        m_availableChannels.remove((ChannelAPI*) object);
    }
}

void ILSDemodGUI::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

