///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QGeoCoordinate>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineSettings>
#include <QWebEngineProfile>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "util/astronomy.h"
#include "util/units.h"
#include "maincore.h"

#include "ui_skymapgui.h"
#include "skymap.h"
#include "skymapgui.h"
#include "skymapsettingsdialog.h"

#include "SWGTargetAzimuthElevation.h"
#include "SWGSkyMapTarget.h"

SkyMapGUI* SkyMapGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    SkyMapGUI* gui = new SkyMapGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void SkyMapGUI::destroy()
{
    delete this;
}

void SkyMapGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray SkyMapGUI::serialize() const
{
    return m_settings.serialize();
}

bool SkyMapGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SkyMapGUI::handleMessage(const Message& message)
{
    if (SkyMap::MsgConfigureSkyMap::match(message))
    {
        qDebug("SkyMapGUI::handleMessage: SkyMap::MsgConfigureSkyMap");
        const SkyMap::MsgConfigureSkyMap& cfg = (SkyMap::MsgConfigureSkyMap&) message;

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
    else if (SkyMap::MsgFind::match(message))
    {
        SkyMap::MsgFind& msgFind = (SkyMap::MsgFind&) message;
        find(msgFind.getTarget());
        return true;
    }
    else if (SkyMap::MsgSetDateTime::match(message))
    {
        SkyMap::MsgSetDateTime& msgSetDateTime = (SkyMap::MsgSetDateTime&) message;
        setDateTime(msgSetDateTime.getDateTime());
        return true;
    }
    else if (MainCore::MsgTargetAzimuthElevation::match(message))
    {
        MainCore::MsgTargetAzimuthElevation& msg = (MainCore::MsgTargetAzimuthElevation&) message;

        // Ignore messages from StarTracker, as we use MsgSkyMapTarget instead, as more accurate and no refraction correction
        if ((msg.getPipeSource() == m_source) && !m_settings.m_source.contains("StarTracker"))
        {
            // Convert from Az/Alt to RA/Dec
            SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = msg.getSWGTargetAzimuthElevation();
            AzAlt aa;
            aa.az = swgTarget->getAzimuth();
            aa.alt = swgTarget->getElevation();

            QGeoCoordinate position = getPosition();
            RADec rd;
            QDateTime dateTime = QDateTime::currentDateTime();

            rd = Astronomy::azAltToRaDec(aa, position.latitude(), position.longitude(), dateTime);

            // Convert from JNOW to J2000
            double jd = Astronomy::julianDate(dateTime);
            rd = Astronomy::precess(rd, jd, Astronomy::jd_j2000());

            m_ra = rd.ra;
            m_dec = rd.dec;
            if (m_settings.m_track) {
                m_webInterface->setView(rd.ra, rd.dec);
            }

            m_webInterface->setAntennaFoV(m_settings.m_hpbw);
         }
        return true;
    }
    else if (MainCore::MsgSkyMapTarget::match(message))
    {
        MainCore::MsgSkyMapTarget& msg = (MainCore::MsgSkyMapTarget&) message;
        if (msg.getPipeSource() == m_source)
        {
            SWGSDRangel::SWGSkyMapTarget *swgTarget = msg.getSWGSkyMapTarget();
            m_ra = swgTarget->getRa();
            m_dec = swgTarget->getDec();
            if (m_settings.m_track) {
                m_webInterface->setView(m_ra, m_dec);
            }
            setPosition(swgTarget->getLatitude(), swgTarget->getLongitude(), swgTarget->getAltitude());
            QDateTime dateTime = QDateTime::currentDateTime();
            if (swgTarget->getDateTime())
            {
                QString dateTimeString = *swgTarget->getDateTime();
                if (!dateTimeString.isEmpty()) {
                    dateTime = QDateTime::fromString(*swgTarget->getDateTime(), Qt::ISODateWithMs);
                }
            }
            setDateTime(dateTime);

            m_webInterface->setAntennaFoV(swgTarget->getHpbw());
        }
        return true;
    }

    return false;
}

void SkyMapGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void SkyMapGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

SkyMapGUI::SkyMapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::SkyMapGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_source(nullptr),
    m_availableChannelOrFeatureHandler(SkyMapSettings::m_pipeURIs, {"target", "skymap.target"}),
    m_ready(false)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/skymap/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_webInterface = new WebInterface();
    connect(m_webInterface, &WebInterface::received, this, &SkyMapGUI::receivedEvent);

    // Web server to serve dynamic files from QResources
    m_webPort = 0;
    m_webServer = new WebServer(m_webPort);

    m_webServer->addSubstitution("/skymap/html/wwt.html", "$WS_PORT$", QString::number(m_webInterface->serverPort()));
    m_webServer->addSubstitution("/skymap/html/esasky.html", "$WS_PORT$", QString::number(m_webInterface->serverPort()));
    m_webServer->addSubstitution("/skymap/html/aladin.html", "$WS_PORT$", QString::number(m_webInterface->serverPort()));

    ui->tabs->tabBar()->tabButton(0, QTabBar::RightSide)->resize(0, 0); // Hide close button
    ui->web->setTabs(ui->tabs);
    ui->web->load(QUrl(QString("http://127.0.0.1:%1/skymap/html/wwt.html").arg(m_webPort)));
    ui->web->show();

    m_skymap = reinterpret_cast<SkyMap*>(feature);
    m_skymap->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    QWebEngineSettings *settings = ui->web->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    connect(ui->web->page(), &QWebEnginePage::fullScreenRequested, this, &SkyMapGUI::fullScreenRequested);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    connect(ui->web->page(), &QWebEnginePage::loadingChanged, this, &SkyMapGUI::loadingChanged);
    connect(ui->web, &QWebEngineView::renderProcessTerminated, this, &SkyMapGUI::renderProcessTerminated);
#endif

    // Default to My Position
    m_settings.m_latitude = MainCore::instance()->getSettings().getLatitude();
    m_settings.m_longitude = MainCore::instance()->getSettings().getLongitude();
    m_settings.m_altitude = MainCore::instance()->getSettings().getAltitude();

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &SkyMapGUI::preferenceChanged);

    displaySettings();
    applyAllSettings();

    makeUIConnections();
    m_resizer.enableChildMouseTracking();

    QObject::connect(&m_availableChannelOrFeatureHandler, &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged, this, &SkyMapGUI::updateSourceList);
    QObject::connect(&m_availableChannelOrFeatureHandler, &AvailableChannelOrFeatureHandler::messageEnqueued, this, &SkyMapGUI::handlePipeMessageQueue);
    m_availableChannelOrFeatureHandler.scanAvailableChannelsAndFeatures();

    connect(&m_wtml, &WTML::dataUpdated, this, &SkyMapGUI::wtmlUpdated);
    m_wtml.getData();
}

SkyMapGUI::~SkyMapGUI()
{
    QObject::disconnect(&m_availableChannelOrFeatureHandler, &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged, this, &SkyMapGUI::updateSourceList);
    if (m_webServer)
    {
        m_webServer->close();
        delete m_webServer;
    }
    delete m_webInterface;
    delete ui;
}

void SkyMapGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void SkyMapGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SkyMapGUI::on_map_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_map = ui->map->currentText();
    applySetting("map");

    m_ready = false;
    if (m_settings.m_map == "WWT") {
        ui->web->load(QUrl(QString("http://127.0.0.1:%1/skymap/html/wwt.html").arg(m_webPort)));
    } else if (m_settings.m_map == "ESASky") {
        ui->web->load(QUrl(QString("http://127.0.0.1:%1/skymap/html/esasky.html").arg(m_webPort)));
    } else if (m_settings.m_map == "Aladin") {
        ui->web->load(QUrl(QString("http://127.0.0.1:%1/skymap/html/aladin.html").arg(m_webPort)));
    } else if (m_settings.m_map == "Moon") {
        ui->web->load(QUrl(QString("http://quickmap.lroc.asu.edu/"))); // Jumping straight to 3D view doesn't seem to work
        setStatusText("");
        m_ready = true;
    }
    updateToolbar();
    updateBackgrounds();
}

void SkyMapGUI::on_background_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_background = ui->background->currentText();
        applySetting("background");
        m_webInterface->setBackground(backgroundID(m_settings.m_background));
    }
}

void SkyMapGUI::on_projection_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_projection = ui->projection->currentText();
        applySetting("projection");
        if (m_settings.m_map == "WWT") {
            updateBackgrounds();
        }
        m_webInterface->setProjection(m_settings.m_projection);
        if (m_settings.m_map == "WWT") {
            m_webInterface->setBackground(backgroundID(m_settings.m_background));
        }
    }
}

void SkyMapGUI::on_source_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_source = ui->source->currentText();
        applySetting("source");
        ui->track->setEnabled(true);
    }
    else
    {
        ui->track->setChecked(false);
        ui->track->setEnabled(false);
    }
}

void SkyMapGUI::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    qDebug() << "SkyMapGUI::renderProcessTerminated: " << terminationStatus << "exitCode" << exitCode;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

void SkyMapGUI::loadingChanged(const QWebEngineLoadingInfo &loadingInfo)
{
    if (loadingInfo.status() == QWebEngineLoadingInfo::LoadFailedStatus)
    {
        qDebug() << "SkyMapGUI::loadingChanged: Failed to load " << loadingInfo.url().toString()
            << "errorString: " << loadingInfo.errorString() << " "
            << "errorDomain:" << loadingInfo.errorDomain()
            << "errorCode:" << loadingInfo.errorCode()
            ;
    }
}
#endif

void SkyMapGUI::initSkyMap()
{
    QGeoCoordinate position = getPosition();
    updateToolbar();
    m_webInterface->setWWTSettings(m_settings.m_wwtSettings);
    m_webInterface->setPosition(position);
    m_webInterface->showNames(m_settings.m_displayNames);
    m_webInterface->showConstellations(m_settings.m_displayConstellations);
    m_webInterface->showReticle(m_settings.m_displayReticle);
    m_webInterface->showGrid(m_settings.m_displayGrid);
    m_webInterface->showAntennaFoV(m_settings.m_displayAntennaFoV);
    m_webInterface->setProjection(m_settings.m_projection);
    m_webInterface->setBackground(backgroundID(m_settings.m_background));
}

void SkyMapGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->displayNames->setChecked(m_settings.m_displayNames);
    int idx = ui->map->findText(m_settings.m_map);
    if (idx >= 0) {
        ui->map->setCurrentIndex(idx);
    }
    ui->displayConstellations->setChecked(m_settings.m_displayConstellations);
    ui->displayReticle->setChecked(m_settings.m_displayReticle);
    ui->displayGrid->setChecked(m_settings.m_displayGrid);
    ui->displayAntennaFoV->setChecked(m_settings.m_displayAntennaFoV);
    idx = ui->background->findText(m_settings.m_background);
    if (idx >= 0) {
        ui->background->setCurrentIndex(idx);
    }
    ui->track->setChecked(m_settings.m_track);
    idx = ui->source->findText(m_settings.m_source);
    if (idx >= 0)
    {
        ui->source->setCurrentIndex(idx);
        ui->track->setEnabled(true);
    }
    else
    {
        ui->track->setChecked(false);
        ui->track->setEnabled(false);
    }
    initSkyMap();
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void SkyMapGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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

        QList<QString> settingsKeys({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void SkyMapGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void SkyMapGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        SkyMap::MsgConfigureSkyMap* message = SkyMap::MsgConfigureSkyMap::create(m_settings, m_settingsKeys, force);
        m_skymap->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();

        m_availableChannelOrFeatureHandler.deregisterPipes(m_source, {"target", "skymap.target"});

        QObject *oldSource = m_source;
        m_source = m_availableChannelOrFeatureHandler.registerPipes(m_settings.m_source, {"target", "skymap.target"});

        // When we change plugins, default to current date and time and My Position, until we get something different
        if (oldSource && !m_source)
        {
            setDateTime(QDateTime::currentDateTime());
            setPosition(MainCore::instance()->getSettings().getLatitude(),
                MainCore::instance()->getSettings().getLongitude(),
                MainCore::instance()->getSettings().getAltitude());
        }
    }
}

void SkyMapGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void SkyMapGUI::find(const QString& text)
{
    if (!m_ready)
    {
        // Save for when ready
        m_find = text;
    }
    else
    {
        float ra, dec;

        // WWT's find doesn't support coordinates, so we check here
        if (Units::stringToRADec(text, ra, dec)) {
            m_webInterface->setView(ra, dec);
        } else {
            m_webInterface->track(text);
        }
    }
}

void SkyMapGUI::on_find_returnPressed()
{
    find(ui->find->text().trimmed());
}

void SkyMapGUI::on_displaySettings_clicked()
{
    SkyMapSettingsDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings(dialog.m_settingsKeysChanged);

        if (dialog.m_settingsKeysChanged.contains("latitude")
            || dialog.m_settingsKeysChanged.contains("longitude")
            || dialog.m_settingsKeysChanged.contains("altitude")
            || dialog.m_settingsKeysChanged.contains("useMyPosition")) {
           m_webInterface->setPosition(getPosition());
        }
        if (dialog.m_settingsKeysChanged.contains("hpbw")) {
            m_webInterface->setAntennaFoV(m_settings.m_hpbw);
        }
        if (dialog.m_settingsKeysChanged.contains("wwtSettings")) {
            m_webInterface->setWWTSettings(m_settings.m_wwtSettings);
        }
    }
}

void SkyMapGUI::on_displayNames_clicked(bool checked)
{
    m_settings.m_displayNames = checked;
    m_webInterface->showNames(checked);
    applySetting("displayNames");
}

void SkyMapGUI::on_displayConstellations_clicked(bool checked)
{
    m_settings.m_displayConstellations = checked;
    m_webInterface->showConstellations(checked);
    applySetting("displayConstellations");
}

void SkyMapGUI::on_displayReticle_clicked(bool checked)
{
    m_settings.m_displayReticle = checked;
    m_webInterface->showReticle(checked);
    applySetting("displayReticle");
}

void SkyMapGUI::on_displayGrid_clicked(bool checked)
{
    m_settings.m_displayGrid = checked;
    m_webInterface->showGrid(checked);
    applySetting("displayGrid");
}

void SkyMapGUI::on_displayAntennaFoV_clicked(bool checked)
{
    m_settings.m_displayAntennaFoV = checked;
    m_webInterface->showAntennaFoV(checked);
    applySetting("displayAntennaFoV");
}

void SkyMapGUI::on_track_clicked(bool checked)
{
    m_settings.m_track = checked;
    applySetting("track");
    if (m_settings.m_track) {
        m_webInterface->setView(m_ra, m_dec);
    }
}

void SkyMapGUI::fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest)
{
    fullScreenRequest.accept();
    if (fullScreenRequest.toggleOn())
    {
        ui->web->setParent(nullptr);
        ui->web->showFullScreen();
    }
    else
    {
        ui->tab0->layout()->addWidget(ui->web);
    }
}

void SkyMapGUI::setDateTime(QDateTime dateTime)
{
    m_dateTime = dateTime;
    m_webInterface->setDateTime(m_dateTime);
}

QDateTime SkyMapGUI::getDateTime() const
{
    if (!m_source || !m_dateTime.isValid()) {
        return QDateTime::currentDateTime();
    } else {
        return m_dateTime;
    }
}

void SkyMapGUI::setPosition(float latitude, float longitude, float altitude)
{
    m_settings.m_latitude = latitude;
    m_settings.m_longitude = longitude;
    m_settings.m_altitude = altitude;
    m_webInterface->setPosition(getPosition());
}

QGeoCoordinate SkyMapGUI::getPosition() const
{
    float latitude, longitude, altitude;

    if (m_settings.m_useMyPosition)
    {
        latitude = MainCore::instance()->getSettings().getLatitude();
        longitude = MainCore::instance()->getSettings().getLongitude();
        altitude = MainCore::instance()->getSettings().getAltitude();
    }
    else
    {
        latitude = m_settings.m_latitude;
        longitude = m_settings.m_longitude;
        altitude = m_settings.m_altitude;
    }

    QGeoCoordinate position(latitude, longitude, altitude);
    return position;
}

void SkyMapGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        QGeoCoordinate position = getPosition();
        m_webInterface->setPosition(position);
    }
}

void SkyMapGUI::makeUIConnections()
{
    QObject::connect(ui->find, &QLineEdit::returnPressed, this, &SkyMapGUI::on_find_returnPressed);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &SkyMapGUI::on_displaySettings_clicked);
    QObject::connect(ui->displayNames, &QToolButton::clicked, this, &SkyMapGUI::on_displayNames_clicked);
    QObject::connect(ui->displayConstellations, &QToolButton::clicked, this, &SkyMapGUI::on_displayConstellations_clicked);
    QObject::connect(ui->displayReticle, &QToolButton::clicked, this, &SkyMapGUI::on_displayReticle_clicked);
    QObject::connect(ui->displayGrid, &QToolButton::clicked, this, &SkyMapGUI::on_displayGrid_clicked);
    QObject::connect(ui->displayAntennaFoV, &QToolButton::clicked, this, &SkyMapGUI::on_displayAntennaFoV_clicked);
    QObject::connect(ui->background, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkyMapGUI::on_background_currentIndexChanged);
    QObject::connect(ui->map, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkyMapGUI::on_map_currentIndexChanged);
    QObject::connect(ui->projection, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkyMapGUI::on_projection_currentIndexChanged);
    QObject::connect(ui->source, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkyMapGUI::on_source_currentIndexChanged);
    QObject::connect(ui->track, &QToolButton::clicked, this, &SkyMapGUI::on_track_clicked);
    QObject::connect(ui->tabs, &QTabWidget::tabCloseRequested, this, &SkyMapGUI::on_tabs_tabCloseRequested);
}

void SkyMapGUI::receivedEvent(const QJsonObject &obj)
{
    if (obj.contains("event"))
    {
        QString event = obj.value("event").toString();
        if (event == "view")
        {
            QStringList status;

            double ra = 0.0, dec = 0.0;
            double latitude = 0.0, longitude = 0.0;
            double fov = 0.0;
            AzAlt aa = {0.0, 0.0};
            QDateTime dateTime;

            if (obj.contains("dateTime"))
            {
                dateTime = QDateTime::fromString(obj.value("dateTime").toString(), Qt::ISODateWithMs).toLocalTime();
            }
            else
            {
                dateTime = getDateTime();
            }

            if (obj.contains("ra") && obj.contains("dec"))
            {
                ra = obj.value("ra").toDouble();
                dec = obj.value("dec").toDouble();

                // Convert from decimal to DMS
                QString raDMS = Units::decimalHoursToHoursMinutesAndSeconds(ra);
                QString decDMS = Units::decimalDegreesToDegreeMinutesAndSeconds(dec);

                status.append(QString("J2000  RA: %1  Dec: %2")
                    .arg(raDMS).arg(decDMS));
            }

            if (obj.contains("fov"))
            {
                fov = obj.value("fov").toDouble();

                status.append(QString("FoV: %1%2").arg(fov, 0, 'f', 2).arg(QChar(0xb0)));
            }

            if (obj.contains("latitude") && obj.contains("longitude"))
            {
                latitude = obj.value("latitude").toDouble();
                longitude = obj.value("longitude").toDouble();

                status.append(QString("Lat: %1%3 Long: %2%3")
                    .arg(latitude).arg(longitude)
                    .arg(QChar(0xb0)));
            }

            if (obj.contains("dateTime"))
            {
                status.append(QString("Date: %1").arg(dateTime.date().toString()));
                status.append(QString("Time: %1").arg(dateTime.time().toString()));
            }

            if (obj.contains("ra") && obj.contains("dec") && obj.contains("latitude") && obj.contains("longitude") && obj.contains("dateTime"))
            {
                // Convert from RA/Dec to Az/Alt
                RADec rd = {ra, dec};
                aa = Astronomy::raDecToAzAlt(rd, latitude, longitude, dateTime);
                QString az = QString::number(aa.az, 'f', 3);
                QString alt = QString::number(aa.alt, 'f', 3);

                status.insert(1, QString("Az: %1%3  Alt: %2%3")
                    .arg(az).arg(alt)
                    .arg(QChar(0xb0)));

                sendToRotator(status[0], aa.az, aa.alt);
            }
            else if (obj.contains("ra") && obj.contains("dec"))
            {
                RADec rd = {ra, dec};
                QGeoCoordinate position = getPosition();

                AzAlt aa = Astronomy::raDecToAzAlt(rd, position.latitude(), position.longitude(), dateTime);
                sendToRotator(status[0], aa.az, aa.alt);
            }

            setStatusText(status.join(" - "));

            // Send details to SkyMap for use in Web report
            SkyMap::ViewDetails viewDetails;
            viewDetails.m_ra = ra;
            viewDetails.m_dec = dec;
            viewDetails.m_azimuth = aa.az;
            viewDetails.m_elevation = aa.alt;
            viewDetails.m_latitude = latitude;
            viewDetails.m_longitude = longitude;
            viewDetails.m_fov = fov;
            viewDetails.m_dateTime = dateTime;
            SkyMap::MsgReportViewDetails* message = SkyMap::MsgReportViewDetails::create(viewDetails);
            m_skymap->getInputMessageQueue()->push(message);

        }
        else if (event == "ready")
        {
            m_ready = true;
            initSkyMap();

            // Run find that was requested while map was initialising
            if (!m_find.isEmpty())
            {
                find(m_find);
                m_find = "";
            }
        }
    }
    else
    {
        qDebug() << "SkyMapGUI::receivedEvent - Unexpected event: " << obj;
    }
}

static const QStringList wwtPlanets = {
    QStringLiteral("Sun"),
    QStringLiteral("Mercury"),
    QStringLiteral("Venus"),
    QStringLiteral("Earth"),
    QStringLiteral("Moon"),
    QStringLiteral("Mars"),
    QStringLiteral("Jupiter"),
    QStringLiteral("Saturn"),
    QStringLiteral("Uranus"),
    QStringLiteral("Neptune"),
    QStringLiteral("Pluto"),
    QStringLiteral("Io"),
    QStringLiteral("Europa"),
    QStringLiteral("Ganymede"),
    QStringLiteral("Callisto")
};

static const QStringList wwtPlanetIDs = {
    QStringLiteral("Sun"),
    QStringLiteral("Mercury"),
    QStringLiteral("Venus"),
    QStringLiteral("Bing Maps Aerial"), //"Earth",
    QStringLiteral("Moon"),
    QStringLiteral("Visible Imagery"), // Mars
    QStringLiteral("Jupiter"),
    QStringLiteral("Saturn"),
    QStringLiteral("Uranus"),
    QStringLiteral("Neptune"),
    QStringLiteral("Pluto (New Horizons)"),
    QStringLiteral("Io"),
    QStringLiteral("Europa"),
    QStringLiteral("Ganymede"),
    QStringLiteral("Callisto")
};

// From https://github.com/cds-astro/aladin-lite/blob/master/src/js/ImageLayer.js
static const QStringList aladinBackgrounds = {
    QStringLiteral("DSS colored"),
    QStringLiteral("DSS2 Red (F+R)"),
    QStringLiteral("2MASS colored"),
    QStringLiteral("Density map for Gaia EDR3 (I/350/gaiaedr3)"),
    QStringLiteral("PanSTARRS DR1 g"),
    QStringLiteral("PanSTARRS DR1 color"),
    QStringLiteral("DECaPS DR1 color"),
    QStringLiteral("Fermi color"),
    QStringLiteral("Halpha"),
    QStringLiteral("GALEXGR6_7 NUV"),
    QStringLiteral("IRIS colored"),
    QStringLiteral("Mellinger colored"),
    QStringLiteral("SDSS9 colored"),
    QStringLiteral("SDSS9 band-g"),
    QStringLiteral("IRAC color I1,I2,I4 - (GLIMPSE, SAGE, SAGE-SMC, SINGS)"),
    QStringLiteral("VTSS-Ha"),
    QStringLiteral("XMM PN colored"),
    QStringLiteral("AllWISE color"),
    QStringLiteral("GLIMPSE360")
};

static const QStringList aladinBackgroundIDs = {
    QStringLiteral("P/DSS2/color"),
    QStringLiteral("P/DSS2/red"),
    QStringLiteral("P/2MASS/color"),
    QStringLiteral("P/DM/I/350/gaiaedr3"),
    QStringLiteral("P/PanSTARRS/DR1/g"),
    QStringLiteral("P/PanSTARRS/DR1/color-z-zg-g"),
    QStringLiteral("P/DECaPS/DR1/color"),
    QStringLiteral("P/Fermi/color"),
    QStringLiteral("P/Finkbeiner"),
    QStringLiteral("P/GALEXGR6_7/NUV"),
    QStringLiteral("P/IRIS/color"),
    QStringLiteral("P/Mellinger/color"),
    QStringLiteral("P/SDSS9/color"),
    QStringLiteral("P/SDSS9/g"),
    QStringLiteral("P/SPITZER/color"),
    QStringLiteral("P/VTSS/Ha"),
    QStringLiteral("xcatdb/P/XMM/PN/color"),
    QStringLiteral("P/allWISE/color"),
    QStringLiteral("P/GLIMPSE360")
};

QString SkyMapGUI::backgroundID(const QString& name)
{
    QString id = name;

    if (m_settings.m_map == "Aladin")
    {
        int idx = aladinBackgrounds.indexOf(name);
        if (idx >= 0) {
            id = aladinBackgroundIDs[idx];
        }
    }
    else if (m_settings.m_map == "WWT")
    {
         if (m_settings.m_projection == "Solar system")
         {
             m_webInterface->track(m_settings.m_background);
             id = "Solar system";
         }
         else
         {
             int idx = wwtPlanets.indexOf(name);
             if (idx >= 0) {
                 id = wwtPlanetIDs[idx];
             }
         }
    }
    return id;
}

void SkyMapGUI::updateBackgrounds()
{
    QStringList backgrounds;

    if (m_settings.m_map == "WWT") {
        if (m_settings.m_projection == "Sky") {
            backgrounds = m_wwtBackgrounds;
        } else if (m_settings.m_projection == "Solar system") {
            backgrounds = wwtPlanets;
        } else {
            backgrounds = wwtPlanets;
        }
    } else if (m_settings.m_map == "ESASky") {
        backgrounds = QStringList();
    } else if (m_settings.m_map == "Aladin") {
        backgrounds = aladinBackgrounds;
    }

    ui->background->blockSignals(true);
    ui->background->clear();
    for (int i = 0; i < backgrounds.size(); i++) {
        ui->background->addItem(backgrounds[i]);
    }
    ui->background->blockSignals(false);
    int idx = ui->background->findText(m_settings.m_background);
    if (idx >= 0) {
        ui->background->setCurrentIndex(idx);
    } else {
        ui->background->setCurrentIndex(0);
    }

    on_background_currentIndexChanged(ui->projection->currentIndex());
}

void SkyMapGUI::wtmlUpdated(const QList<WTML::ImageSet>& dataSets)
{
    m_wwtBackgrounds.clear();
    for (int i = 0; i < dataSets.size(); i++)
    {
        if (dataSets[i].m_dataSetType == "Sky") {
            m_wwtBackgrounds.append(dataSets[i].m_name);
        }
    }
    updateBackgrounds();
}

void SkyMapGUI::updateProjection()
{
    QStringList projections;

    if (m_settings.m_map == "WWT") {
        projections = QStringList{"Sky", "Solar system", "Planet"};
    } else if (m_settings.m_map == "ESASky") {
        projections = QStringList();
    } else if (m_settings.m_map == "Aladin") {
        projections = QStringList{"SIN", "TAN", "STG", "ZEA", "FEYE", "AIR", "ARC", "NCP", "MER", "CAR", "CEA", "CYP", "AIT", "MOL", "PAR", "SFL", "COD", "HPX"};
    }

    ui->projection->blockSignals(true);
    ui->projection->clear();
    for (int i = 0; i < projections.size(); i++){
        ui->projection->addItem(projections[i]);
    }
    ui->projection->blockSignals(false);
    int idx = ui->projection->findText(m_settings.m_projection);
    if (idx >= 0) {
        ui->projection->setCurrentIndex(idx);
    } else {
        ui->projection->setCurrentIndex(0);
    }

    on_projection_currentIndexChanged(ui->projection->currentIndex());
}

void SkyMapGUI::updateToolbar()
{
    bool constellationsVisible = false;
    bool reticleVisible = true;
    bool namesVisible = false;
    bool projectionVisible = true;
    bool backgroundVisible = true;
    bool basicVisible = true;

    if (m_settings.m_map == "WWT")
    {
        constellationsVisible = true;
        namesVisible = true;
    }
    else if (m_settings.m_map == "ESASky")
    {
        projectionVisible = false;
        backgroundVisible = false;
        reticleVisible = false;
    }
    else if (m_settings.m_map == "Moon")
    {
        projectionVisible = false;
        backgroundVisible = false;
        reticleVisible = false;
        basicVisible = false;
    }

    ui->background->setVisible(backgroundVisible);
    ui->projection->setVisible(projectionVisible);

    ui->displayNames->setVisible(namesVisible);
    ui->displayConstellations->setVisible(constellationsVisible);
    ui->displayReticle->setVisible(reticleVisible);

    ui->find->setVisible(basicVisible);
    ui->findLabel->setVisible(basicVisible);
    ui->displayGrid->setVisible(basicVisible);
    ui->displayReticle->setVisible(basicVisible);
    ui->displayAntennaFoV->setVisible(basicVisible);
    ui->track->setVisible(basicVisible);
    ui->source->setVisible(basicVisible);

    updateProjection();
}

void SkyMapGUI::on_tabs_tabCloseRequested(int index)
{
    QWidget *widget = ui->tabs->widget(index);
    ui->tabs->removeTab(index);
    delete widget;
}

void SkyMapGUI::sendToRotator(const QString& name, double az, double alt)
{
    QList<ObjectPipe*> rotatorPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_skymap, "target", rotatorPipes);

    for (const auto& pipe : rotatorPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
        swgTarget->setName(new QString(name));
        swgTarget->setAzimuth(az);
        swgTarget->setElevation(alt);
        messageQueue->push(MainCore::MsgTargetAzimuthElevation::create(m_skymap, swgTarget));
    }
}

void SkyMapGUI::updateSourceList(const QStringList& renameFrom, const QStringList& renameTo)
{
    m_availableChannelOrFeatures = m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList();

    // Update source setting if it has been renamed
    if (renameFrom.contains(m_settings.m_source))
    {
        m_settings.m_source = renameTo[renameFrom.indexOf(m_settings.m_source)];
        applySetting("source"); // Only call after m_availableChannelOrFeatures has been updated
    }

    int prevIdx = ui->source->currentIndex();
    ui->source->blockSignals(true);
    ui->source->clear();

    for (const auto& item : m_availableChannelOrFeatures) {
        ui->source->addItem(item.getLongId());
    }

    // Select current setting, if it exists
    // If not, and no prior setting, make sure nothing selected, as channel/feature may be created later on
    // If not found and something was previously selected, clear the setting, as probably deleted
    int idx = ui->source->findText(m_settings.m_source);
    if (idx >= 0)
    {
        ui->source->setCurrentIndex(idx);
        ui->track->setEnabled(true);
    }
    else if (prevIdx == -1)
    {
        ui->source->setCurrentIndex(-1);
        ui->track->setChecked(false);
        ui->track->setEnabled(false);
    }
    else
    {
        m_settings.m_source = "";
        applySetting("source");
    }

    ui->source->blockSignals(false);

    // If no current setting, select first available
    if (m_settings.m_source.isEmpty() && (ui->source->count() > 0))
    {
        ui->source->setCurrentIndex(0);
        on_source_currentIndexChanged(0);
    }
}

void SkyMapGUI::handlePipeMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
