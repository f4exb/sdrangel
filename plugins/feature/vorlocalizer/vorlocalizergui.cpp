///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QQuickItem>
#include <QGeoLocation>
#include <QGeoCoordinate>
#include <QQmlContext>
#include <QQmlProperty>
#include <QMessageBox>
#include <QAction>

#include "feature/featureuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "ui_vorlocalizergui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "vorlocalizer.h"
#include "vorlocalizerreport.h"
#include "vorlocalizersettings.h"
#include "vorlocalizergui.h"

#include "SWGMapItem.h"

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

// Calculate intersection point along two radials
// https://www.movable-type.co.uk/scripts/latlong.html
static bool calcIntersectionPoint(float lat1, float lon1, float bearing1, float lat2, float lon2, float bearing2, float &intersectLat, float &intersectLon)
{

    double lat1Rad = Units::degreesToRadians(lat1);
    double lon1Rad = Units::degreesToRadians(lon1);
    double lat2Rad = Units::degreesToRadians(lat2);
    double lon2Rad = Units::degreesToRadians(lon2);
    double theta13 = Units::degreesToRadians(bearing1);
    double theta23 = Units::degreesToRadians(bearing2);

    double deltaLat = lat1Rad - lat2Rad;
    double deltaLon = lon1Rad - lon2Rad;
    double sindlat = sin(deltaLat/2.0);
    double sindlon = sin(deltaLon/2.0);
    double cosLat1 = cos(lat1Rad);
    double cosLat2 = cos(lat2Rad);
    double delta12 = 2.0 * asin(sqrt(sindlat*sindlat+cosLat1*cosLat2*sindlon*sindlon));

    if (abs(delta12) < std::numeric_limits<float>::epsilon()) {
        return false;
    }

    double sinLat1 = sin(lat1Rad);
    double sinLat2 = sin(lat2Rad);
    double sinDelta12 = sin(delta12);
    double cosDelta12 = cos(delta12);
    double thetaA = acos((sinLat2-sinLat1*cosDelta12)/(sinDelta12*cosLat1));
    double thetaB = acos((sinLat1-sinLat2*cosDelta12)/(sinDelta12*cosLat2));
    double theta12, theta21;

    if (sin(lon2Rad-lon1Rad) > 0.0)
    {
        theta12 = thetaA;
        theta21 = 2.0*M_PI-thetaB;
    }
    else
    {
        theta12 = 2.0*M_PI-thetaA;
        theta21 = thetaB;
    }

    double alpha1 = theta13 - theta12;
    double alpha2 = theta21 - theta23;
    double sinAlpha1 = sin(alpha1);
    double sinAlpha2 = sin(alpha2);

    if ((sinAlpha1 == 0.0) && (sinAlpha2 == 0.0)) {
        return false;
    }
    if (sinAlpha1*sinAlpha2 < 0.0) {
        return false;
    }

    double cosAlpha1 = cos(alpha1);
    double cosAlpha2 = cos(alpha2);
    double cosAlpha3 = -cosAlpha1*cosAlpha2+sinAlpha1*sinAlpha2*cos(delta12);
    double delta13 = atan2(sin(delta12)*sinAlpha1*sinAlpha2, cosAlpha2+cosAlpha1*cosAlpha3);
    double lat3Rad = asin(sinLat1*cos(delta13)+cosLat1*sin(delta13)*cos(theta13));
    double lon3Rad = lon1Rad + atan2(sin(theta13)*sin(delta13)*cosLat1, cos(delta13)-sinLat1*sin(lat3Rad));

    intersectLat = Units::radiansToDegrees(lat3Rad);
    intersectLon = Units::radiansToDegrees(lon3Rad);

    return true;
}

VORGUI::VORGUI(NavAid *navAid, VORLocalizerGUI *gui) :
    m_navAid(navAid),
    m_gui(gui)
{
    // These are deleted by QTableWidget
    m_nameItem = new QTableWidgetItem();
    m_frequencyItem = new QTableWidgetItem();
    m_radialItem = new QTableWidgetItem();
    m_identItem = new QTableWidgetItem();
    m_morseItem = new QTableWidgetItem();
    m_rxIdentItem = new QTableWidgetItem();
    m_rxMorseItem = new QTableWidgetItem();
    m_varMagItem = new QTableWidgetItem();
    m_refMagItem = new QTableWidgetItem();

    m_muteItem = new QWidget();

    m_muteButton = new QToolButton();
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(false);
    m_muteButton->setToolTip("Mute/unmute audio from this VOR");
    m_muteButton->setIcon(m_gui->m_muteIcon);

    QHBoxLayout* pLayout = new QHBoxLayout(m_muteItem);
    pLayout->addWidget(m_muteButton);
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);
    m_muteItem->setLayout(pLayout);

    connect(m_muteButton, &QPushButton::toggled, this, &VORGUI::on_audioMute_toggled);

    m_coordinates.push_back(QVariant::fromValue(*new QGeoCoordinate(m_navAid->m_latitude, m_navAid->m_longitude, Units::feetToMetres(m_navAid->m_elevation))));
}

void VORGUI::on_audioMute_toggled(bool checked)
{
    m_gui->m_settings.m_subChannelSettings[m_navAid->m_id].m_audioMute = checked;
    m_gui->applySettings();
}

QVariant VORModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();

    if ((row < 0) || (row >= m_vors.count())) {
        return QVariant();
    }

    if (role == VORModel::positionRole)
    {
        // Coordinates to display the VOR icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_vors[row]->m_latitude);
        coords.setLongitude(m_vors[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_vors[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == VORModel::vorDataRole)
    {
        // Create the text to go in the bubble next to the VOR
        QStringList list;
        list.append(QString("Name: %1").arg(m_vors[row]->m_name));
        list.append(QString("Frequency: %1 MHz").arg(m_vors[row]->m_frequencykHz / 1000.0f, 0, 'f', 1));

        if (m_vors[row]->m_channel != "") {
            list.append(QString("Channel: %1").arg(m_vors[row]->m_channel));
        }

        list.append(QString("Ident: %1 %2").arg(m_vors[row]->m_ident).arg(Morse::toSpacedUnicodeMorse(m_vors[row]->m_ident)));
        list.append(QString("Range: %1 nm").arg(m_vors[row]->m_range));

        if (m_vors[row]->m_alignedTrueNorth) {
            list.append(QString("Magnetic declination: Aligned to true North"));
        } else if (m_vors[row]->m_magneticDeclination != 0.0f) {
            list.append(QString("Magnetic declination: %1%2").arg(std::round(m_vors[row]->m_magneticDeclination)).arg(QChar(0x00b0)));
        }

        QString data = list.join("\n");

        return QVariant::fromValue(data);
    }
    else if (role == VORModel::vorImageRole)
    {
        // Select an image to use for the VOR
        return QVariant::fromValue(QString("/demodvor/map/%1.png").arg(m_vors[row]->m_type));
    }
    else if (role == VORModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the VOR
        if (m_selected[row]) {
            return QVariant::fromValue(QColor("lightgreen"));
        } else {
            return QVariant::fromValue(QColor("lightblue"));
        }
    }
    else if (role == VORModel::vorRadialRole)
    {
       // Draw a radial line from centre of VOR outwards at the demodulated angle
       if (m_radialsVisible && m_selected[row] && (m_vorGUIs[row] != nullptr) && (m_radials[row] != -1.0f))
       {
           QVariantList list;

           list.push_back(m_vorGUIs[row]->m_coordinates[0]); // Centre of VOR

           float endLat, endLong;
           float bearing;

           if (m_gui->m_settings.m_magDecAdjust && !m_vors[row]->m_alignedTrueNorth) {
               bearing = m_radials[row] - m_vors[row]->m_magneticDeclination;
           } else {
               bearing = m_radials[row];
           }

           calcRadialEndPoint(m_vors[row]->m_latitude, m_vors[row]->m_longitude, m_vors[row]->getRangeMetres(), bearing, endLat, endLong);
           list.push_back(QVariant::fromValue(*new QGeoCoordinate(endLat, endLong, Units::feetToMetres(m_vors[row]->m_elevation))));

           return list;
       }
       else
           return QVariantList();
    }
    else if (role == VORModel::selectedRole)
    {
        return QVariant::fromValue(m_selected[row]);
    }

    return QVariant();
}

bool VORModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();

    if ((row < 0) || (row >= m_vors.count())) {
        return false;
    }

    if (role == VORModel::selectedRole)
    {
        bool selected = value.toBool();
        VORGUI *vorGUI;

        if (selected)
        {
            vorGUI = new VORGUI(m_vors[row], m_gui);
            m_vorGUIs[row] = vorGUI;
        }
        else
        {
            vorGUI = m_vorGUIs[row];
        }

        m_gui->selectVOR(vorGUI, selected);
        m_selected[row] = selected;
        emit dataChanged(index, index);

        if (!selected)
        {
            delete vorGUI;
            m_vorGUIs[row] = nullptr;
        }

        return true;
    }

    return true;
}

// Find intersection between first two selected radials
bool VORModel::findIntersection(float &lat, float &lon)
{
    if (m_vors.count() > 2)
    {
        float lat1, lon1, bearing1, valid1 = false;
        float lat2, lon2, bearing2, valid2 = false;

        for (int i = 0; i < m_vors.count(); i++)
        {
            if (m_selected[i] && (m_radials[i] >= 0.0))
            {
                if (!valid1)
                {
                    lat1 = m_vors[i]->m_latitude;
                    lon1 = m_vors[i]->m_longitude;

                    if (m_gui->m_settings.m_magDecAdjust && !m_vors[i]->m_alignedTrueNorth) {
                        bearing1 = m_radials[i] - m_vors[i]->m_magneticDeclination;
                    } else {
                        bearing1 = m_radials[i];
                    }

                    valid1 = true;
                }
                else
                {
                    lat2 = m_vors[i]->m_latitude;
                    lon2 = m_vors[i]->m_longitude;

                    if (m_gui->m_settings.m_magDecAdjust && !m_vors[i]->m_alignedTrueNorth) {
                        bearing2 = m_radials[i] - m_vors[i]->m_magneticDeclination;
                    } else {
                        bearing2 = m_radials[i];
                    }

                    valid2 = true;
                    break;
                }
            }
        }

        if (valid1 && valid2) {
            return calcIntersectionPoint(lat1, lon1, bearing1, lat2, lon2, bearing2, lat, lon);
        }
    }

    return false;
}

QString VORModel::getRadials() const
{
    QStringList text;
    for (int i = 0; i < m_vors.size(); i++)
    {
        if (m_radials[i] >= 0) {
            text.append(QString("%1: %2%3").arg(m_vors[i]->m_name).arg(std::round(m_radials[i])).arg(QChar(0xb0)));
        }
    }
    return text.join("\n");
}

void VORLocalizerGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    QString morse("---- ---- ----");
    int row = ui->vorData->rowCount();
    ui->vorData->setRowCount(row + 1);
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_NAME, new QTableWidgetItem("White Sulphur Springs"));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_FREQUENCY, new QTableWidgetItem("Freq (MHz) "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_IDENT, new QTableWidgetItem("Ident "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_MORSE, new QTableWidgetItem(Morse::toSpacedUnicode(morse)));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RADIAL, new QTableWidgetItem("Radial (o) "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RX_IDENT, new QTableWidgetItem("RX Ident "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RX_MORSE, new QTableWidgetItem(Morse::toSpacedUnicode(morse)));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_VAR_MAG, new QTableWidgetItem("Var (dB) "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_REF_MAG, new QTableWidgetItem("Ref (dB) "));
    ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_MUTE, new QTableWidgetItem("Mute"));
    ui->vorData->resizeColumnsToContents();
    ui->vorData->removeRow(row);
}

// Columns in table reordered
void VORLocalizerGUI::vorData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
    m_settingsKeys.append("columnIndexes");
    applySettings();
}

// Column in table resized (when hidden size is 0)
void VORLocalizerGUI::vorData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;
    m_settings.m_columnSizes[logicalIndex] = newSize;
    m_settingsKeys.append("columnSizes");
    applySettings();
}

// Right click in table header - show column select menu
void VORLocalizerGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->vorData->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void VORLocalizerGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());

    if (action)
    {
        int idx = action->data().toInt(nullptr);
        ui->vorData->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *VORLocalizerGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));

    return action;
}

// Called when a VOR is selected on the map
void VORLocalizerGUI::selectVOR(VORGUI *vorGUI, bool selected)
{
    int navId = vorGUI->m_navAid->m_id;

    if (selected)
    {
        VORLocalizer::MsgAddVORChannel *msg = VORLocalizer::MsgAddVORChannel::create(navId);
        m_vorLocalizer->getInputMessageQueue()->push(msg);

        m_selectedVORs.insert(navId, vorGUI);
        ui->vorData->setSortingEnabled(false);
        int row = ui->vorData->rowCount();
        ui->vorData->setRowCount(row + 1);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_NAME, vorGUI->m_nameItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_FREQUENCY, vorGUI->m_frequencyItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_IDENT, vorGUI->m_identItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_MORSE, vorGUI->m_morseItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RADIAL, vorGUI->m_radialItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RX_IDENT, vorGUI->m_rxIdentItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_RX_MORSE, vorGUI->m_rxMorseItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_VAR_MAG, vorGUI->m_varMagItem);
        ui->vorData->setItem(row, VORLocalizerSettings::VOR_COL_REF_MAG, vorGUI->m_refMagItem);
        ui->vorData->setCellWidget(row, VORLocalizerSettings::VOR_COL_MUTE, vorGUI->m_muteItem);
        vorGUI->m_nameItem->setText(vorGUI->m_navAid->m_name);
        vorGUI->m_identItem->setText(vorGUI->m_navAid->m_ident);
        vorGUI->m_morseItem->setText(Morse::toSpacedUnicodeMorse(vorGUI->m_navAid->m_ident));
        vorGUI->m_frequencyItem->setData(Qt::DisplayRole, vorGUI->m_navAid->m_frequencykHz / 1000.0);
        ui->vorData->setSortingEnabled(true);

        // Add to settings to create corresponding demodulator
        m_settings.m_subChannelSettings.insert(navId, VORLocalizerSubChannelSettings{
            navId,
            (int)(vorGUI->m_navAid->m_frequencykHz * 1000),
            false
        });

        applySettings();
    }
    else
    {
        QString radialName = QString("VOR Radial %1").arg(vorGUI->m_navAid->m_name);

        VORLocalizer::MsgRemoveVORChannel *msg = VORLocalizer::MsgRemoveVORChannel::create(navId);
        m_vorLocalizer->getInputMessageQueue()->push(msg);

        m_selectedVORs.remove(navId);
        ui->vorData->removeRow(vorGUI->m_nameItem->row());
        // Remove from settings to remove corresponding demodulator
        m_settings.m_subChannelSettings.remove(navId);

        // Remove radial from Map feature
        m_mapFeatureRadialNames.removeOne(radialName);
        clearFromMapFeature(radialName, 3);

        applySettings();
    }
}

void VORLocalizerGUI::updateVORs()
{
    m_vorModel.removeAllVORs();
    AzEl azEl = m_azEl;

    for (const auto vor : *m_vors)
    {
        if (vor->m_type.contains("VOR")) // Exclude DMEs
        {
            // Calculate distance to VOR from My Position
            azEl.setTarget(vor->m_latitude, vor->m_longitude, Units::feetToMetres(vor->m_elevation));
            azEl.calculate();

            // Only display VOR if in range
            if (azEl.getDistance() <= 200000) {
                m_vorModel.addVOR(vor);
            }
        }
    }
}

VORLocalizerGUI* VORLocalizerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    VORLocalizerGUI* gui = new VORLocalizerGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void VORLocalizerGUI::destroy()
{
    delete this;
}

void VORLocalizerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray VORLocalizerGUI::serialize() const
{
    return m_settings.serialize();
}

bool VORLocalizerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
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

void VORLocalizerGUI::clearFromMapFeature(const QString& name, int type)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_vorLocalizer, "mapitems", mapPipes);
    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));
        swgMapItem->setType(type);
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_vorLocalizer, swgMapItem);
        messageQueue->push(msg);
    }
}

void VORLocalizerGUI::sendPositionToMapFeature(float lat, float lon)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_vorLocalizer, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        QString name = MainCore::instance()->getSettings().getStationName();
        if (name != m_mapFeaturePositionName)
        {
            clearFromMapFeature(m_mapFeaturePositionName, 0);
            m_mapFeaturePositionName = name;
        }

        QString details = QString("%1\nEstimated position based on VORs\n").arg(name);
        details.append(m_vorModel.getRadials());

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

            swgMapItem->setName(new QString(name));
            swgMapItem->setLatitude(lat);
            swgMapItem->setLongitude(lon);
            swgMapItem->setAltitude(0);
            swgMapItem->setImage(new QString("antenna.png"));
            swgMapItem->setImageRotation(0);
            swgMapItem->setText(new QString(details));
            swgMapItem->setModel(new QString("antenna.glb"));
            swgMapItem->setFixedPosition(false);
            swgMapItem->setLabel(new QString(name));
            swgMapItem->setLabelAltitudeOffset(4.5);
            swgMapItem->setAltitudeReference(1);
            swgMapItem->setType(0);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_vorLocalizer, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void VORLocalizerGUI::sendRadialToMapFeature(VORGUI *vorGUI, Real radial)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_vorLocalizer, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        float endLat, endLong;
        float bearing;

        if (m_settings.m_magDecAdjust && !vorGUI->m_navAid->m_alignedTrueNorth) {
            bearing = radial - vorGUI->m_navAid->m_magneticDeclination;
        } else {
            bearing = radial;
        }

        calcRadialEndPoint(vorGUI->m_navAid->m_latitude, vorGUI->m_navAid->m_longitude, vorGUI->m_navAid->getRangeMetres(), bearing, endLat, endLong);

        QString name = QString("VOR Radial %1").arg(vorGUI->m_navAid->m_name);
        QString details = QString("%1%2").arg(std::round(bearing)).arg(QChar(0x00b0));

        if (!m_mapFeatureRadialNames.contains(name)) {
            m_mapFeatureRadialNames.append(name);
        }

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

            swgMapItem->setName(new QString(name));
            swgMapItem->setLatitude(vorGUI->m_navAid->m_latitude);
            swgMapItem->setLongitude(vorGUI->m_navAid->m_longitude);
            swgMapItem->setAltitude(Units::feetToMetres(vorGUI->m_navAid->m_elevation));
            QString image = QString("none");
            swgMapItem->setImage(new QString(image));
            swgMapItem->setImageRotation(0);
            swgMapItem->setText(new QString(details));   // Not used - label is used instead for now
            //swgMapItem->setFixedPosition(true);
            swgMapItem->setLabel(new QString(details));
            swgMapItem->setAltitudeReference(0);
            QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

            SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(vorGUI->m_navAid->m_latitude);
            c->setLongitude(vorGUI->m_navAid->m_longitude);       // Centre of VOR
            c->setAltitude(Units::feetToMetres(vorGUI->m_navAid->m_elevation));
            coords->append(c);

            c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(endLat);
            c->setLongitude(endLong);
            c->setAltitude(Units::feetToMetres(vorGUI->m_navAid->m_elevation));
            coords->append(c);

            swgMapItem->setCoordinates(coords);
            swgMapItem->setType(3);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_vorLocalizer, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

bool VORLocalizerGUI::handleMessage(const Message& message)
{
    if (VORLocalizer::MsgConfigureVORLocalizer::match(message))
    {
        qDebug("VORLocalizerGUI::handleMessage: VORLocalizer::MsgConfigureVORLocalizer");
        const VORLocalizer::MsgConfigureVORLocalizer& cfg = (VORLocalizer::MsgConfigureVORLocalizer&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        return true;
    }
    else if (VORLocalizerReport::MsgReportRadial::match(message))
    {
        VORLocalizerReport::MsgReportRadial& report = (VORLocalizerReport::MsgReportRadial&) message;
        int subChannelId = report.getSubChannelId();

        VORGUI *vorGUI = m_selectedVORs.value(subChannelId);
        if (vorGUI)
        {
            // Display radial and signal magnitudes in table

            Real varMagDB = std::round(20.0*std::log10(report.getVarMag()));
            Real refMagDB = std::round(20.0*std::log10(report.getRefMag()));

            bool validRadial = report.getValidRadial();
            vorGUI->m_radialItem->setData(Qt::DisplayRole, std::round(report.getRadial()));

            if (validRadial) {
                vorGUI->m_radialItem->setForeground(QBrush(Qt::white));
            } else {
                vorGUI->m_radialItem->setForeground(QBrush(Qt::red));
            }

            vorGUI->m_refMagItem->setData(Qt::DisplayRole, refMagDB);

            if (report.getValidRefMag()) {
                vorGUI->m_refMagItem->setForeground(QBrush(Qt::white));
            } else {
                vorGUI->m_refMagItem->setForeground(QBrush(Qt::red));
            }

            vorGUI->m_varMagItem->setData(Qt::DisplayRole, varMagDB);

            if (report.getValidVarMag()) {
                vorGUI->m_varMagItem->setForeground(QBrush(Qt::white));
            } else {
                vorGUI->m_varMagItem->setForeground(QBrush(Qt::red));
            }

            // Update radial on map
            m_vorModel.setRadial(subChannelId, validRadial, report.getRadial());

            // Send to map feature as well
            sendRadialToMapFeature(vorGUI, report.getRadial());

            // Try to determine position, based on intersection of two radials
            float lat, lon;

            if (m_vorModel.findIntersection(lat, lon))
            {
                // Move antenna icon to estimated position
                QQuickItem *item = ui->map->rootObject();
                QObject *stationObject = item->findChild<QObject*>("station");

                if (stationObject != NULL)
                {
                    QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                    coords.setLatitude(lat);
                    coords.setLongitude(lon);
                    stationObject->setProperty("coordinate", QVariant::fromValue(coords));
                    stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
                }

                // Send estimated position to Map Feature as well
                sendPositionToMapFeature(lat, lon);
            }
        }
        else
        {
            qDebug() << "VORLocalizerGUI::handleMessage: Got MsgReportRadial for non-existant subChannelId " << subChannelId;
        }

        return true;
    }
    else if (VORLocalizerReport::MsgReportIdent::match(message))
    {
        VORLocalizerReport::MsgReportIdent& report = (VORLocalizerReport::MsgReportIdent&) message;
        int subChannelId = report.getSubChannelId();
        VORGUI *vorGUI = m_selectedVORs.value(subChannelId);

        if (vorGUI)
        {

            QString ident = report.getIdent();
            // Convert Morse to a string
            QString identString = Morse::toString(ident);
            // Idents should only be two or three characters, so filter anything else
            // other than TEST which indicates a VOR is under maintainance (may also be TST)

            if (((identString.size() >= 2) && (identString.size() <= 3)) || (identString == "TEST"))
            {
                vorGUI->m_rxIdentItem->setText(identString);
                vorGUI->m_rxMorseItem->setText(Morse::toSpacedUnicode(ident));

                if (vorGUI->m_navAid->m_ident == identString)
                {
                    // Set colour to green if matching expected ident
                    vorGUI->m_rxIdentItem->setForeground(QBrush(Qt::green));
                    vorGUI->m_rxMorseItem->setForeground(QBrush(Qt::green));
                }
                else
                {
                    // Set colour to green if not matching expected ident
                    vorGUI->m_rxIdentItem->setForeground(QBrush(Qt::red));
                    vorGUI->m_rxMorseItem->setForeground(QBrush(Qt::red));
                }
            }
            else
            {
                // Set yellow to indicate we've filtered something (unless red)
                if (vorGUI->m_rxIdentItem->foreground().color() != Qt::red)
                {
                    vorGUI->m_rxIdentItem->setForeground(QBrush(Qt::yellow));
                    vorGUI->m_rxMorseItem->setForeground(QBrush(Qt::yellow));
                }
            }
        }
        else
        {
            qDebug() << "VORLocalizerGUI::handleMessage: Got MsgReportIdent for non-existant subChannelId " << subChannelId;
        }

        return true;
    }
    else if (VORLocalizerReport::MsgReportChannels::match(message))
    {
        VORLocalizerReport::MsgReportChannels& report = (VORLocalizerReport::MsgReportChannels&) message;
        const std::vector<VORLocalizerReport::MsgReportChannels::Channel>& channels = report.getChannels();
        std::vector<VORLocalizerReport::MsgReportChannels::Channel>::const_iterator it = channels.begin();
        ui->channels->clear();

        for (; it != channels.end(); ++it) {
            ui->channels->addItem(tr("R%1:%2").arg(it->m_deviceSetIndex).arg(it->m_channelIndex));
        }

        return true;
    }
    else if (VORLocalizerReport::MsgReportServiceddVORs::match(message))
    {
        VORLocalizerReport::MsgReportServiceddVORs& report = (VORLocalizerReport::MsgReportServiceddVORs&) message;
        std::vector<int>& servicedVORNavIds = report.getNavIds();

        for (auto vorGUI : m_selectedVORs) {
            vorGUI->m_frequencyItem->setForeground(QBrush(Qt::white));
        }

        for (auto navId : servicedVORNavIds)
        {
            if (m_selectedVORs.contains(navId))
            {
                VORGUI *vorGUI = m_selectedVORs[navId];
                vorGUI->m_frequencyItem->setForeground(QBrush(Qt::green));
            }
        }

        ui->rrTurnTimeProgress->setMaximum(m_settings.m_rrTime);
        ui->rrTurnTimeProgress->setValue(0);
        ui->rrTurnTimeProgress->setToolTip(tr("Round robin turn %1s").arg(0));
        m_rrSecondsCount = 0;
    }

    return false;
}

void VORLocalizerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void VORLocalizerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        VORLocalizer::MsgStartStop *message = VORLocalizer::MsgStartStop::create(checked);
        m_vorLocalizer->getInputMessageQueue()->push(message);

        if (checked)
        {
            // Refresh channels in case device b/w has changed
            channelsRefresh();
        }
    }
}

void VORLocalizerGUI::on_getOpenAIPVORDB_clicked()
{
    // Don't try to download while already in progress
    if (!m_progressDialog)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setMaximum(OpenAIP::m_countryCodes.size());
        m_progressDialog->setCancelButton(nullptr);

        m_openAIP.downloadNavAids();
    }
}

void VORLocalizerGUI::readNavAids()
{
    m_vors = OpenAIP::getNavAids();
    updateVORs();
}

void VORLocalizerGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void VORLocalizerGUI::downloadError(const QString& error)
{
    QMessageBox::critical(this, "VOR Localizer", error);

    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void VORLocalizerGUI::downloadNavAidsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading NAVAIDs.");
    }

    readNavAids();

    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void VORLocalizerGUI::on_magDecAdjust_toggled(bool checked)
{
    m_settings.m_magDecAdjust = checked;
    m_vorModel.allVORUpdated();
    m_settingsKeys.append("magDecAdjust");
    applySettings();
}

void VORLocalizerGUI::on_rrTime_valueChanged(int value)
{
    m_settings.m_rrTime = value;
    ui->rrTimeText->setText(tr("%1s").arg(m_settings.m_rrTime));
    m_settingsKeys.append("rrTime");
    applySettings();
}

void VORLocalizerGUI::on_centerShift_valueChanged(int value)
{
    m_settings.m_centerShift = value * 1000;
    ui->centerShiftText->setText(tr("%1k").arg(value));
    m_settingsKeys.append("centerShift");
    applySettings();
}

void VORLocalizerGUI::channelsRefresh()
{
    if (m_doApplySettings)
    {
        VORLocalizer::MsgRefreshChannels* message = VORLocalizer::MsgRefreshChannels::create();
        m_vorLocalizer->getInputMessageQueue()->push(message);
    }
}

void VORLocalizerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();
    rollupContents->saveState(m_rollupState);
}

void VORLocalizerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void VORLocalizerGUI::applyMapSettings()
{
    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

    QQuickItem *item = ui->map->rootObject();

    QObject *object = item->findChild<QObject*>("map");
    QGeoCoordinate coords;
    double zoom;

    if (object != nullptr)
    {
        // Save existing position of map
        coords = object->property("center").value<QGeoCoordinate>();
        zoom = object->property("zoomLevel").value<double>();
    }
    else
    {
        // Center on my location when map is first opened
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        zoom = 10.0;
    }

    // Create the map using the specified provider
    QQmlProperty::write(item, "mapProvider", m_settings.m_mapProvider);
    QVariantMap parameters;
    QString mapType;

    if (m_settings.m_mapProvider == "osm") {
        mapType = "Street Map";
    } else if (m_settings.m_mapProvider == "mapboxgl") {
        mapType = "mapbox://styles/mapbox/streets-v10";
    }

    QVariant retVal;

    if (!QMetaObject::invokeMethod(
            item,
            "createMap",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, retVal),
            Q_ARG(QVariant, QVariant::fromValue(parameters)),
            Q_ARG(QVariant, mapType),
            Q_ARG(QVariant, QVariant::fromValue(this))
        )
    )
    {
        qCritical() << "VORLocalizerGUI::applyMapSettings - Failed to invoke createMap";
    }

    QObject *newMap = retVal.value<QObject *>();

    // Restore position of map
    if (newMap != nullptr)
    {
        if (coords.isValid())
        {
            newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
            newMap->setProperty("center", QVariant::fromValue(coords));
        }
    }
    else
    {
        qDebug() << "VORLocalizerGUI::applyMapSettings - createMap returned a nullptr";
    }

    // Move antenna icon to My Position
    QObject *stationObject = newMap->findChild<QObject*>("station");

    if(stationObject != NULL)
    {
        QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        stationObject->setProperty("coordinate", QVariant::fromValue(coords));
        stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
    }
    else
    {
        qDebug() << "VORLocalizerGUI::applyMapSettings - Couldn't find station";
    }
}

VORLocalizerGUI::VORLocalizerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::VORLocalizerGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_squelchOpen(false),
    m_tickCount(0),
    m_progressDialog(nullptr),
    m_vorModel(this),
    m_lastFeatureState(0),
    m_rrSecondsCount(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/vorlocalizer/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->map->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->map->rootContext()->setContextProperty("vorModel", &m_vorModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/demodvor/map/map.qml")));

    m_muteIcon.addPixmap(QPixmap("://sound_off.png"), QIcon::Normal, QIcon::On);
    m_muteIcon.addPixmap(QPixmap("://sound_on.png"), QIcon::Normal, QIcon::Off);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_openAIP, &OpenAIP::downloadingURL, this, &VORLocalizerGUI::downloadingURL);
    connect(&m_openAIP, &OpenAIP::downloadError, this, &VORLocalizerGUI::downloadError);
    connect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &VORLocalizerGUI::downloadNavAidsFinished);

    m_vorLocalizer = reinterpret_cast<VORLocalizer*>(feature);
    m_vorLocalizer->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_settings.setRollupState(&m_rollupState);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    applyMapSettings();

    // Read in VOR information if it exists
    readNavAids();

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->vorData->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->vorData->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->vorData);

    for (int i = 0; i < ui->vorData->horizontalHeader()->count(); i++)
    {
        QString text = ui->vorData->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }

    ui->vorData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->vorData->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->vorData->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(vorData_sectionMoved(int, int, int)));
    connect(ui->vorData->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(vorData_sectionResized(int, int, int)));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    ui->rrTurnTimeProgress->setMaximum(m_settings.m_rrTime);
    ui->rrTurnTimeProgress->setValue(0);
    ui->rrTurnTimeProgress->setToolTip(tr("Round robin turn time %1s").arg(0));

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &VORLocalizerGUI::preferenceChanged);

    displaySettings();
    applySettings(true);

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &VORLocalizerGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    ui->map->installEventFilter(this);

    makeUIConnections();

    // Update channel list when added/removed
    connect(MainCore::instance(), &MainCore::channelAdded, this, &VORLocalizerGUI::channelsRefresh);
    connect(MainCore::instance(), &MainCore::channelRemoved, this, &VORLocalizerGUI::channelsRefresh);
    // Also replan when device changed (as bandwidth may change or may becomed fixed center freq)
    connect(MainCore::instance(), &MainCore::deviceChanged, this, &VORLocalizerGUI::channelsRefresh);
    // List already opened channels
    channelsRefresh();
    DialPopup::addPopupsToChildDials(this);
}

VORLocalizerGUI::~VORLocalizerGUI()
{
    clearFromMapFeature(m_mapFeaturePositionName, 0);
    for (auto const &radialName : m_mapFeatureRadialNames) {
        clearFromMapFeature(radialName, 3);
    }
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &VORLocalizerGUI::redrawMap);
    m_redrawMapTimer.stop();
    delete ui;
}

void VORLocalizerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void VORLocalizerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void VORLocalizerGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        VORLocalizer::MsgConfigureVORLocalizer* message = VORLocalizer::MsgConfigureVORLocalizer::create( m_settings, m_settingsKeys, force);
        m_vorLocalizer->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void VORLocalizerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);

    blockApplySettings(true);

    // Order and size columns
    QHeaderView *header = ui->vorData->horizontalHeader();

    for (int i = 0; i < VORLocalizerSettings::VORDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);

        if (m_settings.m_columnSizes[i] > 0) {
            ui->vorData->setColumnWidth(i, m_settings.m_columnSizes[i]);
        }

        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    ui->rrTimeText->setText(tr("%1s").arg(m_settings.m_rrTime));
    ui->rrTime->setValue(m_settings.m_rrTime);
    ui->centerShiftText->setText(tr("%1k").arg(m_settings.m_centerShift/1000));
    ui->centerShift->setValue(m_settings.m_centerShift/1000);
    ui->forceRRAveraging->setChecked(m_settings.m_forceRRAveraging);

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void VORLocalizerGUI::updateStatus()
{
    int state = m_vorLocalizer->getState();

    if (m_lastFeatureState != state)
    {
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_vorLocalizer->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void VORLocalizerGUI::tick()
{
    if (++m_tickCount == 20)
    {
        m_rrSecondsCount++;
        ui->rrTurnTimeProgress->setMaximum(m_settings.m_rrTime);
        ui->rrTurnTimeProgress->setValue(m_rrSecondsCount <= m_settings.m_rrTime ? m_rrSecondsCount : m_settings.m_rrTime);
        ui->rrTurnTimeProgress->setToolTip(tr("Round robin turn time %1s").arg(m_rrSecondsCount));
        m_tickCount = 0;
    }
}

void VORLocalizerGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
        Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
        Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

        if (   (stationLatitude != m_azEl.getLocationSpherical().m_latitude)
            || (stationLongitude != m_azEl.getLocationSpherical().m_longitude)
            || (stationAltitude != m_azEl.getLocationSpherical().m_altitude))
        {
            m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

            // Update distances and what is visible
            updateVORs();

            // Update icon position on Map
            QQuickItem *item = ui->map->rootObject();
            QObject *map = item->findChild<QObject*>("map");

            if (map != nullptr)
            {
                QObject *stationObject = map->findChild<QObject*>("station");

                if(stationObject != NULL)
                {
                    QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                    coords.setLatitude(stationLatitude);
                    coords.setLongitude(stationLongitude);
                    coords.setAltitude(stationAltitude);
                    stationObject->setProperty("coordinate", QVariant::fromValue(coords));
                }
            }
        }
    }

    if (pref == Preferences::StationName)
    {
        // Update icon label on Map
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");

        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");

            if(stationObject != NULL) {
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
    }
}

void VORLocalizerGUI::redrawMap()
{
    // An awful workaround for https://bugreports.qt.io/browse/QTBUG-100333
    // Also used in ADS-B demod
    QQuickItem *item = ui->map->rootObject();

    if (item)
    {
        QObject *object = item->findChild<QObject*>("map");

        if (object)
        {
            double zoom = object->property("zoomLevel").value<double>();
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1));
            object->setProperty("zoomLevel", QVariant::fromValue(zoom));
        }
    }
}

void VORLocalizerGUI::showEvent(QShowEvent *event)
{
    if (!event->spontaneous())
    {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
        // MapQuickItems can be in wrong position when window is first displayed
        m_redrawMapTimer.start(500);
    }
}

bool VORLocalizerGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->map)
    {
        if (event->type() == QEvent::Resize)
        {
            // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
            // MapQuickItems can be in wrong position after vertical resize
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            QSize oldSize = resizeEvent->oldSize();
            QSize size = resizeEvent->size();

            if (oldSize.height() != size.height()) {
                redrawMap();
            }
        }
    }
    return FeatureGUI::eventFilter(obj, event);
}

void VORLocalizerGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &VORLocalizerGUI::on_startStop_toggled);
    QObject::connect(ui->getOpenAIPVORDB, &QPushButton::clicked, this, &VORLocalizerGUI::on_getOpenAIPVORDB_clicked);
    QObject::connect(ui->magDecAdjust, &ButtonSwitch::toggled, this, &VORLocalizerGUI::on_magDecAdjust_toggled);
    QObject::connect(ui->rrTime, &QDial::valueChanged, this, &VORLocalizerGUI::on_rrTime_valueChanged);
    QObject::connect(ui->centerShift, &QDial::valueChanged, this, &VORLocalizerGUI::on_centerShift_valueChanged);
}

