///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <cmath>
#include <QMessageBox>
#include <QLineEdit>
#include <QQmlContext>
#include <QQuickItem>
#include <QGeoLocation>
#include <QGeoCoordinate>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "util/units.h"
#include "util/maidenhead.h"

#include "ui_mapgui.h"
#include "map.h"
#include "mapgui.h"
#include "SWGMapItem.h"

QVariant MapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();

    if ((row < 0) || (row >= m_items.count()))
        return QVariant();
    if (role == MapModel::positionRole)
    {
        // Coordinates to display the item at
        QGeoCoordinate coords;
        coords.setLatitude(m_items[row]->m_latitude);
        coords.setLongitude(m_items[row]->m_longitude);
        return QVariant::fromValue(coords);
    }
    else if (role == MapModel::mapTextRole)
    {
        // Create the text to go in the bubble next to the image
        if (m_selected[row])
            return QVariant::fromValue(m_items[row]->m_text);
        else
            return QVariant::fromValue(m_items[row]->m_name);
    }
    else if (role == MapModel::mapTextVisibleRole)
    {
        return QVariant::fromValue(m_selected[row] || m_displayNames);
    }
    else if (role == MapModel::mapImageRole)
    {
        // Set an image to use
        return QVariant::fromValue(m_items[row]->m_image);
    }
    else if (role == MapModel::mapImageRotationRole)
    {
        // Angle to rotate image by
        return QVariant::fromValue(m_items[row]->m_imageRotation);
    }
    else if (role == MapModel::mapImageFixedSizeRole)
    {
        // Whether image changes size with zoom level
        return QVariant::fromValue(m_items[row]->m_imageFixedSize);
    }
    else if (role == MapModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the item
        if (m_selected[row])
            return QVariant::fromValue(QColor("lightgreen"));
        else
            return QVariant::fromValue(QColor("lightblue"));
    }
    else if (role == MapModel::selectedRole)
        return QVariant::fromValue(m_selected[row]);
    return QVariant();
}

bool MapModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count()))
        return false;
    if (role == MapModel::selectedRole)
    {
        m_selected[row] = value.toBool();
        emit dataChanged(index, index);
        return true;
    }
    return true;
}

MapGUI* MapGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    MapGUI* gui = new MapGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void MapGUI::destroy()
{
    delete this;
}

void MapGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray MapGUI::serialize() const
{
    return m_settings.serialize();
}

bool MapGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
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

bool MapGUI::handleMessage(const Message& message)
{
    if (Map::MsgConfigureMap::match(message))
    {
        qDebug("MapGUI::handleMessage: Map::MsgConfigureMap");
        const Map::MsgConfigureMap& cfg = (Map::MsgConfigureMap&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (PipeEndPoint::MsgReportPipes::match(message))
    {
        PipeEndPoint::MsgReportPipes& report = (PipeEndPoint::MsgReportPipes&) message;
        m_availablePipes = report.getAvailablePipes();
        updatePipeList();

        return true;
    }
    else if (Map::MsgFind::match(message))
    {
        Map::MsgFind& msgFind = (Map::MsgFind&) message;
        find(msgFind.getTarget());
        return true;
    }
    else if (MainCore::MsgMapItem::match(message))
    {
        MainCore::MsgMapItem& msgMapItem = (MainCore::MsgMapItem&) message;
        SWGSDRangel::SWGMapItem *swgMapItem = msgMapItem.getSWGMapItem();
        m_mapModel.update(msgMapItem.getPipeSource(), swgMapItem);
        return true;
    }

    return false;
}

void MapGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MapGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

MapGUI::MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::MapGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_mapModel(this)
{
    ui->setupUi(this);

    ui->map->rootContext()->setContextProperty("mapModel", &m_mapModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map.qml")));

    setAttribute(Qt::WA_DeleteOnClose, true);
    setChannelWidget(false);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    m_map = reinterpret_cast<Map*>(feature);
    m_map->setMessageQueueToGUI(&m_inputMessageQueue);

    m_featureUISet->addRollupWidget(this);

    // Get station position
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getAltitude();

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

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    applySettings(true);
}

MapGUI::~MapGUI()
{
    delete ui;
}

void MapGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MapGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->displayNames->setChecked(m_settings.m_displayNames);
    m_mapModel.setDisplayNames(m_settings.m_displayNames);
    blockApplySettings(false);
}

void MapGUI::updatePipeList()
{
    ui->pipes->blockSignals(true);
    ui->pipes->clear();

    QList<PipeEndPoint::AvailablePipeSource>::const_iterator it = m_availablePipes.begin();

    for (int i = 0; it != m_availablePipes.end(); ++it, i++)
    {
        ui->pipes->addItem(it->getName());
    }

    ui->pipes->blockSignals(false);
}

void MapGUI::leaveEvent(QEvent*)
{
}

void MapGUI::enterEvent(QEvent*)
{
}

void MapGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setColor(m_settings.m_rgbColor);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = dialog.getColor().rgb();
        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void MapGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        Map::MsgConfigureMap* message = Map::MsgConfigureMap::create(m_settings, force);
        m_map->getInputMessageQueue()->push(message);
    }
}

void MapGUI::on_displayNames_clicked(bool checked)
{
    m_settings.m_displayNames = checked;
    m_mapModel.setDisplayNames(checked);
}

void MapGUI::on_find_returnPressed()
{
    find(ui->find->text().trimmed());
}

void MapGUI::find(const QString& target)
{
    if (!target.isEmpty())
    {
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");
        if (map != NULL)
        {
            float latitude, longitude;
            if (Units::stringToLatitudeAndLongitude(target, latitude, longitude))
            {
                map->setProperty("center", QVariant::fromValue(QGeoCoordinate(latitude, longitude)));
            }
            else if (Maidenhead::fromMaidenhead(target, latitude, longitude))
            {
                map->setProperty("center", QVariant::fromValue(QGeoCoordinate(latitude, longitude)));
            }
            else
            {
                MapItem *mapItem = m_mapModel.findMapItem(target);
                if (mapItem != nullptr)
                    map->setProperty("center", QVariant::fromValue(mapItem->getCoordinates()));
            }
        }
    }
}

void MapGUI::on_deleteAll_clicked()
{
    m_mapModel.removeAll();
}
