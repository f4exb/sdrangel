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
#include <QMessageBox>
#include <QAction>

#include "vordemodmcgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_vordemodmcgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "vordemodmc.h"
#include "vordemodmcreport.h"
#include "vordemodmcsink.h"

#define VOR_COL_NAME            0
#define VOR_COL_FREQUENCY       1
#define VOR_COL_OFFSET          2
#define VOR_COL_IDENT           3
#define VOR_COL_MORSE           4
#define VOR_COL_RX_IDENT        5
#define VOR_COL_RX_MORSE        6
#define VOR_COL_RADIAL          7
#define VOR_COL_REF_MAG         8
#define VOR_COL_VAR_MAG         9
#define VOR_COL_MUTE            10

static const char *countryCodes[] = {
    "ad",
    "ae",
    "af",
    "ag",
    "ai",
    "al",
    "am",
    "an",
    "ao",
    "aq",
    "ar",
    "as",
    "at",
    "au",
    "aw",
    "ax",
    "az",
    "ba",
    "bb",
    "bd",
    "be",
    "bf",
    "bg",
    "bh",
    "bi",
    "bj",
    "bl",
    "bm",
    "bn",
    "bo",
    "bq",
    "br",
    "bs",
    "bt",
    "bv",
    "bw",
    "by",
    "bz",
    "ca",
    "cc",
    "cd",
    "cf",
    "cg",
    "ch",
    "ci",
    "ck",
    "cl",
    "cm",
    "cn",
    "co",
    "cr",
    "cu",
    "cv",
    "cw",
    "cx",
    "cy",
    "cz",
    "de",
    "dj",
    "dk",
    "dm",
    "do",
    "dz",
    "ec",
    "ee",
    "eg",
    "eh",
    "er",
    "es",
    "et",
    "fi",
    "fj",
    "fk",
    "fm",
    "fo",
    "fr",
    "ga",
    "gb",
    "ge",
    "gf",
    "gg",
    "gh",
    "gi",
    "gl",
    "gm",
    "gn",
    "gp",
    "gq",
    "gr",
    "gs",
    "gt",
    "gu",
    "gw",
    "gy",
    "hk",
    "hm",
    "hn",
    "hr",
    "hu",
    "id",
    "ie",
    "il",
    "im",
    "in",
    "io",
    "iq",
    "ir",
    "is",
    "it",
    "je",
    "jm",
    "jo",
    "jp",
    "ke",
    "kg",
    "kh",
    "ki",
    "km",
    "kn",
    "kp",
    "kr",
    "kw",
    "ky",
    "kz",
    "la",
    "lb",
    "lc",
    "li",
    "lk",
    "lr",
    "ls",
    "lt",
    "lu",
    "lv",
    "ly",
    "ma",
    "mc",
    "md",
    "me",
    "mf",
    "mg",
    "mh",
    "mk",
    "ml",
    "mm",
    "mn",
    "mo",
    "mp",
    "mq",
    "mr",
    "ms",
    "mt",
    "mu",
    "mv",
    "mw",
    "mx",
    "my",
    "mz",
    "na",
    "nc",
    "ne",
    "nf",
    "ng",
    "ni",
    "nl",
    "no",
    "np",
    "nr",
    "nu",
    "nz",
    "om",
    "pa",
    "pe",
    "pf",
    "pg",
    "ph",
    "pk",
    "pl",
    "pm",
    "pn",
    "pr",
    "ps",
    "pt",
    "pw",
    "py",
    "qa",
    "re",
    "ro",
    "rs",
    "ru",
    "rw",
    "sa",
    "sb",
    "sc",
    "sd",
    "se",
    "sg",
    "sh",
    "si",
    "sj",
    "sk",
    "sl",
    "sm",
    "sn",
    "so",
    "sr",
    "ss",
    "st",
    "sv",
    "sx",
    "sy",
    "sz",
    "tc",
    "td",
    "tf",
    "tg",
    "th",
    "tj",
    "tk",
    "tl",
    "tm",
    "tn",
    "to",
    "tr",
    "tt",
    "tv",
    "tw",
    "tz",
    "ua",
    "ug",
    "um",
    "us",
    "uy",
    "uz",
    "va",
    "vc",
    "ve",
    "vg",
    "vi",
    "vn",
    "vu",
    "wf",
    "ws",
    "ye",
    "yt",
    "za",
    "zm",
    "zw",
    nullptr
};

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
    if (abs(delta12) < std::numeric_limits<float>::epsilon())
        return false;

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
    if ((sinAlpha1 == 0.0) && (sinAlpha2 == 0.0))
        return false;
    if (sinAlpha1*sinAlpha2 < 0.0)
        return false;
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

VORGUI::VORGUI(NavAid *navAid, VORDemodMCGUI *gui) :
    m_navAid(navAid),
    m_gui(gui)
{
    // These are deleted by QTableWidget
    m_nameItem = new QTableWidgetItem();
    m_frequencyItem = new QTableWidgetItem();
    m_offsetItem = new QTableWidgetItem();
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
    m_gui->m_settings.m_subChannelSettings.value(m_navAid->m_id)->m_audioMute = checked;
    m_gui->applySettings();
}

QVariant VORModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_vors.count()))
        return QVariant();
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
        if (m_vors[row]->m_channel != "")
            list.append(QString("Channel: %1").arg(m_vors[row]->m_channel));
        list.append(QString("Ident: %1 %2").arg(m_vors[row]->m_ident).arg(Morse::toSpacedUnicodeMorse(m_vors[row]->m_ident)));
        list.append(QString("Range: %1 nm").arg(m_vors[row]->m_range));
        if (m_vors[row]->m_alignedTrueNorth)
            list.append(QString("Magnetic declination: Aligned to true North"));
        else if (m_vors[row]->m_magneticDeclination != 0.0f)
            list.append(QString("Magnetic declination: %1%2").arg(std::round(m_vors[row]->m_magneticDeclination)).arg(QChar(0x00b0)));
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
        if (m_selected[row])
            return QVariant::fromValue(QColor("lightgreen"));
        else
            return QVariant::fromValue(QColor("lightblue"));
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
           if (m_gui->m_settings.m_magDecAdjust && !m_vors[row]->m_alignedTrueNorth)
               bearing = m_radials[row] - m_vors[row]->m_magneticDeclination;
           else
               bearing = m_radials[row];
           calcRadialEndPoint(m_vors[row]->m_latitude, m_vors[row]->m_longitude, m_vors[row]->getRangeMetres(), bearing, endLat, endLong);
           list.push_back(QVariant::fromValue(*new QGeoCoordinate(endLat, endLong, Units::feetToMetres(m_vors[row]->m_elevation))));

           return list;
       }
       else
           return QVariantList();
    }
    else if (role == VORModel::selectedRole)
        return QVariant::fromValue(m_selected[row]);
    return QVariant();
}

bool VORModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_vors.count()))
        return false;
    if (role == VORModel::selectedRole)
    {
        bool selected = value.toBool();
        VORGUI *vorGUI;
        if (selected == true)
        {
            vorGUI = new VORGUI(m_vors[row], m_gui);
            m_vorGUIs[row] = vorGUI;
        }
        else
            vorGUI = m_vorGUIs[row];
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
                    if (m_gui->m_settings.m_magDecAdjust && !m_vors[i]->m_alignedTrueNorth)
                        bearing1 = m_radials[i] - m_vors[i]->m_magneticDeclination;
                    else
                        bearing1 = m_radials[i];
                    valid1 = true;
                }
                else
                {
                    lat2 = m_vors[i]->m_latitude;
                    lon2 = m_vors[i]->m_longitude;
                    if (m_gui->m_settings.m_magDecAdjust && !m_vors[i]->m_alignedTrueNorth)
                        bearing2 = m_radials[i] - m_vors[i]->m_magneticDeclination;
                    else
                        bearing2 = m_radials[i];
                    valid2 = true;
                    break;
                }
            }
        }

        if (valid1 && valid2)
        {
            return calcIntersectionPoint(lat1, lon1, bearing1, lat2, lon2, bearing2, lat, lon);
        }
    }

    return false;
}

void VORDemodMCGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    QString morse("---- ---- ----");
    int row = ui->vorData->rowCount();
    ui->vorData->setRowCount(row + 1);
    ui->vorData->setItem(row, VOR_COL_NAME, new QTableWidgetItem("White Sulphur Springs"));
    ui->vorData->setItem(row, VOR_COL_FREQUENCY, new QTableWidgetItem("Freq (MHz) "));
    ui->vorData->setItem(row, VOR_COL_OFFSET, new QTableWidgetItem("Offset (kHz) "));
    ui->vorData->setItem(row, VOR_COL_IDENT, new QTableWidgetItem("Ident "));
    ui->vorData->setItem(row, VOR_COL_MORSE, new QTableWidgetItem(Morse::toSpacedUnicode(morse)));
    ui->vorData->setItem(row, VOR_COL_RADIAL, new QTableWidgetItem("Radial (o) "));
    ui->vorData->setItem(row, VOR_COL_RX_IDENT, new QTableWidgetItem("RX Ident "));
    ui->vorData->setItem(row, VOR_COL_RX_MORSE, new QTableWidgetItem(Morse::toSpacedUnicode(morse)));
    ui->vorData->setItem(row, VOR_COL_VAR_MAG, new QTableWidgetItem("Var (dB) "));
    ui->vorData->setItem(row, VOR_COL_REF_MAG, new QTableWidgetItem("Ref (dB) "));
    ui->vorData->setItem(row, VOR_COL_MUTE, new QTableWidgetItem("Mute"));
    ui->vorData->resizeColumnsToContents();
    ui->vorData->removeRow(row);
}

// Columns in table reordered
void VORDemodMCGUI::vorData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void VORDemodMCGUI::vorData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void VORDemodMCGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->vorData->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void VORDemodMCGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->vorData->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *VORDemodMCGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

// Called when a VOR is selected on the map
void VORDemodMCGUI::selectVOR(VORGUI *vorGUI, bool selected)
{
    if (selected)
    {
        m_selectedVORs.insert(vorGUI->m_navAid->m_id, vorGUI);
        ui->vorData->setSortingEnabled(false);
        int row = ui->vorData->rowCount();
        ui->vorData->setRowCount(row + 1);
        ui->vorData->setItem(row, VOR_COL_NAME, vorGUI->m_nameItem);
        ui->vorData->setItem(row, VOR_COL_FREQUENCY, vorGUI->m_frequencyItem);
        ui->vorData->setItem(row, VOR_COL_OFFSET, vorGUI->m_offsetItem);
        ui->vorData->setItem(row, VOR_COL_IDENT, vorGUI->m_identItem);
        ui->vorData->setItem(row, VOR_COL_MORSE, vorGUI->m_morseItem);
        ui->vorData->setItem(row, VOR_COL_RADIAL, vorGUI->m_radialItem);
        ui->vorData->setItem(row, VOR_COL_RX_IDENT, vorGUI->m_rxIdentItem);
        ui->vorData->setItem(row, VOR_COL_RX_MORSE, vorGUI->m_rxMorseItem);
        ui->vorData->setItem(row, VOR_COL_VAR_MAG, vorGUI->m_varMagItem);
        ui->vorData->setItem(row, VOR_COL_REF_MAG, vorGUI->m_refMagItem);
        ui->vorData->setCellWidget(row, VOR_COL_MUTE, vorGUI->m_muteItem);
        vorGUI->m_nameItem->setText(vorGUI->m_navAid->m_name);
        vorGUI->m_identItem->setText(vorGUI->m_navAid->m_ident);
        vorGUI->m_morseItem->setText(Morse::toSpacedUnicodeMorse(vorGUI->m_navAid->m_ident));
        vorGUI->m_frequencyItem->setData(Qt::DisplayRole, vorGUI->m_navAid->m_frequencykHz / 1000.0);
        ui->vorData->setSortingEnabled(true);

        // Add to settings to create corresponding demodulator
        VORDemodSubChannelSettings *subChannelSettings = new VORDemodSubChannelSettings();
        subChannelSettings->m_id = vorGUI->m_navAid->m_id;
        subChannelSettings->m_frequency = vorGUI->m_navAid->m_frequencykHz * 1000;
        subChannelSettings->m_audioMute = false;
        m_settings.m_subChannelSettings.insert(vorGUI->m_navAid->m_id, subChannelSettings);
        applySettings();
    }
    else
    {
        m_selectedVORs.remove(vorGUI->m_navAid->m_id);
        ui->vorData->removeRow(vorGUI->m_nameItem->row());
        // Remove from settings to remove corresponding demodulator
        VORDemodSubChannelSettings *subChannelSettings = m_settings.m_subChannelSettings.value(vorGUI->m_navAid->m_id);
        m_settings.m_subChannelSettings.remove(vorGUI->m_navAid->m_id);
        delete subChannelSettings;
        applySettings();
    }
}

void VORDemodMCGUI::updateVORs()
{
    m_vorModel.removeAllVORs();
    QHash<int, NavAid *>::iterator i = m_vors->begin();
    AzEl azEl = m_azEl;

    while (i != m_vors->end())
    {
        NavAid *vor = i.value();

        // Calculate distance to VOR from My Position
        azEl.setTarget(vor->m_latitude, vor->m_longitude, Units::feetToMetres(vor->m_elevation));
        azEl.calculate();

        // Only display VOR if in range
        if (azEl.getDistance() <= 200000)
        {
            m_vorModel.addVOR(vor);
        }
        ++i;
    }
}

VORDemodMCGUI* VORDemodMCGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    VORDemodMCGUI* gui = new VORDemodMCGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void VORDemodMCGUI::destroy()
{
    delete this;
}

void VORDemodMCGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray VORDemodMCGUI::serialize() const
{
    return m_settings.serialize();
}

bool VORDemodMCGUI::deserialize(const QByteArray& data)
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

bool VORDemodMCGUI::handleMessage(const Message& message)
{
    if (VORDemodMC::MsgConfigureVORDemod::match(message))
    {
        qDebug("VORDemodMCGUI::handleMessage: VORDemodMC::MsgConfigureVORDemod");
        const VORDemodMC::MsgConfigureVORDemod& cfg = (VORDemodMC::MsgConfigureVORDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (VORDemodMCReport::MsgReportRadial::match(message))
    {
        VORDemodMCReport::MsgReportRadial& report = (VORDemodMCReport::MsgReportRadial&) message;
        int subChannelId = report.getSubChannelId();

        VORGUI *vorGUI = m_selectedVORs.value(subChannelId);

        // Display radial and signal magnitudes in table

        Real varMagDB = std::round(20.0*std::log10(report.getVarMag()));
        Real refMagDB = std::round(20.0*std::log10(report.getRefMag()));

        bool validRadial = (refMagDB > m_settings.m_refThresholdDB) && (varMagDB > m_settings.m_varThresholdDB);

        vorGUI->m_radialItem->setData(Qt::DisplayRole, std::round(report.getRadial()));
        if (validRadial)
            vorGUI->m_radialItem->setForeground(QBrush(Qt::white));
        else
            vorGUI->m_radialItem->setForeground(QBrush(Qt::red));

        vorGUI->m_refMagItem->setData(Qt::DisplayRole, refMagDB);
        if (refMagDB > m_settings.m_refThresholdDB)
            vorGUI->m_refMagItem->setForeground(QBrush(Qt::white));
        else
            vorGUI->m_refMagItem->setForeground(QBrush(Qt::red));

        vorGUI->m_varMagItem->setData(Qt::DisplayRole, varMagDB);
        if (varMagDB > m_settings.m_varThresholdDB)
            vorGUI->m_varMagItem->setForeground(QBrush(Qt::white));
        else
            vorGUI->m_varMagItem->setForeground(QBrush(Qt::red));

        // Update radial on map
        m_vorModel.setRadial(subChannelId, validRadial, report.getRadial());

        return true;
    }
    else if (VORDemodMCReport::MsgReportFreqOffset::match(message))
    {
        VORDemodMCReport::MsgReportFreqOffset& report = (VORDemodMCReport::MsgReportFreqOffset&) message;
        int subChannelId = report.getSubChannelId();

        VORGUI *vorGUI = m_selectedVORs.value(subChannelId);

        vorGUI->m_offsetItem->setData(Qt::DisplayRole, report.getFreqOffset() / 1000.0);
        if (report.getOutOfBand())
        {
            vorGUI->m_offsetItem->setForeground(QBrush(Qt::red));
            // Clear other fields as data is now invalid
            vorGUI->m_radialItem->setText("");
            vorGUI->m_refMagItem->setText("");
            vorGUI->m_varMagItem->setText("");
            m_vorModel.setRadial(subChannelId, false, -1.0f);
        }
        else
            vorGUI->m_offsetItem->setForeground(QBrush(Qt::white));
    }
    else if (VORDemodMCReport::MsgReportIdent::match(message))
    {
        VORDemodMCReport::MsgReportIdent& report = (VORDemodMCReport::MsgReportIdent&) message;
        int subChannelId = report.getSubChannelId();

        VORGUI *vorGUI = m_selectedVORs.value(subChannelId);

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
        return true;
    }

    return false;
}

void VORDemodMCGUI::handleInputMessages()
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

void VORDemodMCGUI::channelMarkerChangedByCursor()
{
}

void VORDemodMCGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void VORDemodMCGUI::on_thresh_valueChanged(int value)
{
    ui->threshText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_identThreshold = value / 10.0;
    applySettings();
}

void VORDemodMCGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
    applySettings();
}

void VORDemodMCGUI::on_squelch_valueChanged(int value)
{
    ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
    applySettings();
}

void VORDemodMCGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

qint64 VORDemodMCGUI::fileAgeInDays(QString filename)
{
    QFile file(filename);
    if (file.exists())
    {
        QDateTime modified = file.fileTime(QFileDevice::FileModificationTime);
        if (modified.isValid())
            return modified.daysTo(QDateTime::currentDateTime());
        else
            return -1;
    }
    return -1;
}

bool VORDemodMCGUI::confirmDownload(QString filename)
{
    qint64 age = fileAgeInDays(filename);
    if ((age == -1) || (age > 100))
        return true;
    else
    {
        QMessageBox::StandardButton reply;
        if (age == 0)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded today. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else if (age == 1)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded yesterday. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else
            reply = QMessageBox::question(this, "Confirm download", QString("This file was last downloaded %1 days ago. Are you sure you wish to redownload this file?").arg(age), QMessageBox::Yes|QMessageBox::No);
        return reply == QMessageBox::Yes;
    }
}

QString VORDemodMCGUI::getDataDir()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString VORDemodMCGUI::getOpenAIPVORDBFilename(int i)
{
    if (countryCodes[i] != nullptr)
        return getDataDir() + "/" + countryCodes[i] + "_nav.aip";
    else
        return "";
}

QString VORDemodMCGUI::getOpenAIPVORDBURL(int i)
{
    if (countryCodes[i] != nullptr)
        return QString(OPENAIP_NAVAIDS_URL).arg(countryCodes[i]);
    else
        return "";
}

QString VORDemodMCGUI::getVORDBFilename()
{
    return getDataDir() + "/vorDatabase.csv";
}

void VORDemodMCGUI::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void VORDemodMCGUI::downloadFinished(const QString& filename, bool success)
{
    bool closeDialog = true;
    if (success)
    {
        if (filename == getVORDBFilename())
        {
            m_vors = NavAid::readNavAidsDB(filename);
            if (m_vors != nullptr)
                updateVORs();
        }
        else if (filename == getOpenAIPVORDBFilename(m_countryIndex))
        {
            m_countryIndex++;
            if (countryCodes[m_countryIndex] != nullptr)
            {
                QString vorDBFile = getOpenAIPVORDBFilename(m_countryIndex);
                QString urlString = getOpenAIPVORDBURL(m_countryIndex);
                QUrl dbURL(urlString);
                m_progressDialog->setLabelText(QString("Downloading %1.").arg(urlString));
                m_progressDialog->setValue(m_countryIndex);
                m_dlm.download(dbURL, vorDBFile);
                closeDialog = false;
            }
            else
            {
                readNavAids();
                if (m_vors != nullptr)
                    updateVORs();
            }
        }
        else
        {
            qDebug() << "VORDemodMCGUI::downloadFinished: Unexpected filename: " << filename;
        }
    }
    else
    {
        qDebug() << "VORDemodMCGUI::downloadFinished: Failed: " << filename;
        QMessageBox::warning(this, "Download failed", QString("Failed to download %1").arg(filename));
    }
    if (closeDialog && m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void VORDemodMCGUI::on_getOurAirportsVORDB_clicked(bool checked)
{
    (void) checked;

    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        QString vorDBFile = getVORDBFilename();
        if (confirmDownload(vorDBFile))
        {
            // Download OurAirports navaid database to disk
            QUrl dbURL(QString(OURAIRPORTS_NAVAIDS_URL));
            m_progressDialog = new QProgressDialog(this);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setMinimumDuration(500);
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(OURAIRPORTS_NAVAIDS_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, vorDBFile);
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
        }
    }
}

void VORDemodMCGUI::on_getOpenAIPVORDB_clicked(bool checked)
{
    (void) checked;

    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_countryIndex = 0;
        QString vorDBFile = getOpenAIPVORDBFilename(m_countryIndex);
        if (confirmDownload(vorDBFile))
        {
            // Download OpenAIP XML to disk
            QString urlString = getOpenAIPVORDBURL(m_countryIndex);
            QUrl dbURL(urlString);
            m_progressDialog = new QProgressDialog(this);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setMinimumDuration(500);
            m_progressDialog->setMaximum(sizeof(countryCodes)/sizeof(countryCodes[0]));
            m_progressDialog->setValue(0);
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(urlString));
            m_dlm.download(dbURL, vorDBFile);
        }
    }
}

void VORDemodMCGUI::readNavAids()
{
    m_vors = new QHash<int, NavAid *>();
    for (int countryIndex = 0; countryCodes[countryIndex] != nullptr; countryIndex++)
    {
        QString vorDBFile = getOpenAIPVORDBFilename(countryIndex);
        NavAid::readNavAidsXML(m_vors, vorDBFile);
    }
}

void VORDemodMCGUI::on_magDecAdjust_clicked(bool checked)
{
    m_settings.m_magDecAdjust = checked;
    m_vorModel.allVORUpdated();
    applySettings();
}

void VORDemodMCGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void VORDemodMCGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_vorDemod->getNumberOfDeviceStreams());
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

VORDemodMCGUI::VORDemodMCGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::VORDemodMCGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_squelchOpen(false),
    m_tickCount(0),
    m_progressDialog(nullptr),
    m_vorModel(this),
    m_vors(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodvor/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->map->rootContext()->setContextProperty("vorModel", &m_vorModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/demodvor/map/map.qml")));

    m_muteIcon.addPixmap(QPixmap("://sound_off.png"), QIcon::Normal, QIcon::On);
    m_muteIcon.addPixmap(QPixmap("://sound_on.png"), QIcon::Normal, QIcon::Off);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &VORDemodMCGUI::downloadFinished);

    m_vorDemod = reinterpret_cast<VORDemodMC*>(rxChannel);
    m_vorDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(2*48000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("VOR Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

    // Centre map at My Position
    QQuickItem *item = ui->map->rootObject();
    QObject *object = item->findChild<QObject*>("map");
    if(object != NULL)
    {
        QGeoCoordinate coords = object->property("center").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        object->setProperty("center", QVariant::fromValue(coords));
    }
    // Move antenna icon to My Position to start with
    QObject *stationObject = item->findChild<QObject*>("station");
    if(stationObject != NULL)
    {
        QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        stationObject->setProperty("coordinate", QVariant::fromValue(coords));
        stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
    }

    // Read in VOR information if it exists
    bool useOurAirports = false;
    if (useOurAirports)
    {
        m_vors = NavAid::readNavAidsDB(getVORDBFilename());
        ui->getOpenAIPVORDB->setVisible(false);
    }
    else
    {
        readNavAids();
        ui->getOurAirportsVORDB->setVisible(false);
    }
    if (m_vors != nullptr)
        updateVORs();

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

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

VORDemodMCGUI::~VORDemodMCGUI()
{
    delete ui;
}

void VORDemodMCGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void VORDemodMCGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        VORDemodMC::MsgConfigureVORDemod* message = VORDemodMC::MsgConfigureVORDemod::create( m_settings, force);
        m_vorDemod->getInputMessageQueue()->push(message);
    }
}

void VORDemodMCGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setBandwidth(m_basebandSampleRate > 0 ? m_basebandSampleRate : 2*48000);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->thresh->setValue(m_settings.m_identThreshold * 10.0);
    ui->threshText->setText(QString("%1").arg(m_settings.m_identThreshold, 0, 'f', 1));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioMute->setChecked(m_settings.m_audioMute);

    updateIndexLabel();

    // Order and size columns
    QHeaderView *header = ui->vorData->horizontalHeader();
    for (int i = 0; i < VORDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->vorData->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void VORDemodMCGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void VORDemodMCGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void VORDemodMCGUI::audioSelect()
{
    qDebug("VORDemodMCGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void VORDemodMCGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_vorDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    int audioSampleRate = m_vorDemod->getAudioSampleRate();
    bool squelchOpen = m_vorDemod->getSquelchOpen();

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

    // Try to determine position, based on intersection of two radials
    if (m_tickCount % 50)
    {
        float lat, lon;

        if (m_vorModel.findIntersection(lat, lon))
        {
            // Move antenna icon to estimated position
            QQuickItem *item = ui->map->rootObject();
            QObject *stationObject = item->findChild<QObject*>("station");
            if(stationObject != NULL)
            {
                QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                coords.setLatitude(lat);
                coords.setLongitude(lon);
                stationObject->setProperty("coordinate", QVariant::fromValue(coords));
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
    }

    m_tickCount++;
}

void VORDemodMCGUI::makeUIConnections()
{
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &VORDemodMCGUI::on_audioMute_toggled);
    QObject::connect(ui->thresh, &QDial::valueChanged, this, &VORDemodMCGUI::on_thresh_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &VORDemodMCGUI::on_volume_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &VORDemodMCGUI::on_squelch_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &VORDemodMCGUI::on_audioMute_toggled);
    QObject::connect(ui->getOurAirportsVORDB, &QPushButton::clicked, this, &VORDemodMCGUI::on_getOurAirportsVORDB_clicked);
    QObject::connect(ui->getOpenAIPVORDB, &QPushButton::clicked, this, &VORDemodMCGUI::on_getOpenAIPVORDB_clicked);
    QObject::connect(ui->magDecAdjust, &QPushButton::clicked, this, &VORDemodMCGUI::on_magDecAdjust_clicked);
}

void VORDemodMCGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency);
}
