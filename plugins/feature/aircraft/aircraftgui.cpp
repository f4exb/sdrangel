///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include <cmath>
#include <tuple>

#include <QTableView>
#include <QAction>
#include <QClipboard>
#include <QFileInfo>
#include <QLocale>
#include <QDebug>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidgetItem>
#include <QSet>
#include <QUrl>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QProgressDialog>
#ifdef QT_TEXTTOSPEECH_FOUND
#include <QTextToSpeech>
#endif

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"
#include "util/osndb.h"

#include "ui_aircraftgui.h"
#include "aircraft.h"
#include "aircraftgui.h"
#include "aircraftnotificationdialog.h"
#include "aircraftsettingsdialog.h"
#include "util/profiler.h"
#include "gui/fastitemdelegate.h"
#include "gui/fastheaderview.h"

// How many columns the user can actually see. Hiding the last one is refused, because
// the menu that unhides them hangs off the header, and a table with nothing visible has
// no header left to right-click - there is then no way back without editing the settings
static int visibleColumnCount(const QTableView *view, int columns)
{
    int visible = 0;
    for (int i = 0; i < columns; i++)
    {
        if (!view->isColumnHidden(i)) {
            visible++;
        }
    }
    return visible;
}

// Gives a table's header the context menu that hides and shows its columns, the same
// one the aircraft and flights tables have. Those two keep their own menus as members,
// because their column state is saved and restored; the tables here only need the menu
static void addColumnMenu(QTableView *view, int columns)
{
    QMenu *menu = new QMenu(view);
    const QAbstractItemModel *model = view->model();

    for (int i = 0; i < columns; i++)
    {
        QAction *action = menu->addAction(model ? model->headerData(i, Qt::Horizontal).toString() : QString());
        action->setCheckable(true);
        action->setChecked(!view->isColumnHidden(i));
        QObject::connect(action, &QAction::toggled, view, [view, columns, i, action](bool checked)
        {
            // Refuse the last one: the menu that would put it back hangs off the header,
            // and a table with nothing visible has no header left to right-click
            if (!checked && (visibleColumnCount(view, columns) <= 1))
            {
                action->setChecked(true);
                return;
            }
            view->setColumnHidden(i, !checked);
        });
    }

    view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view->horizontalHeader(), &QWidget::customContextMenuRequested, view,
        [view, menu](const QPoint& pos)
        {
            menu->popup(view->horizontalHeader()->viewport()->mapToGlobal(pos));
        });
}

// A width for a column that fits both the sample - the widest thing it is likely to show -
// and its own heading, whichever is wider. A column narrower than its title reads as
// truncated even when the data fits, and several numeric columns (Alt, Hdg, Spd) have
// headings wider than their values.
// A sorted column draws a sort arrow in its header, and the header lays that out BESIDE
// the text rather than over it - so a width that only just fits the text hid the text
// itself the moment the column was sorted on. The style knows how big the arrow is;
// asking it is better than picking a number that is wrong on another theme.
static int columnWidthFor(const QTableView *view, int column, const QString& sample)
{
    const QHeaderView *header = view->horizontalHeader();
    const int sortArrow = header->style()->pixelMetric(QStyle::PM_HeaderMarkSize, nullptr, header)
                        + 2 * header->style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, header);
    const QString heading = view->model()->headerData(column, Qt::Horizontal).toString();
    const int text = std::max(view->fontMetrics().horizontalAdvance(sample),
                              header->fontMetrics().horizontalAdvance(heading));
    return text + 12 + sortArrow;
}

// The widest a date and time gets in a given format: two digit day and month, and a time
// late in the day so a 12 hour locale shows two digit hours too
static QString dateTimeSample(const QString& format)
{
    return QDateTime(QDate(2026, 12, 28), QTime(23, 59, 59)).toString(format);
}

// Settings saved before that guard existed can still have everything hidden, so recover
// rather than restoring a table the user cannot use
static void showAllColumnsIfNoneVisible(QTableView *view, int *columnSizes, int columns, QMenu *menu)
{
    if (visibleColumnCount(view, columns) > 0) {
        return;
    }
    qWarning() << "AircraftGUI: every column was hidden in the saved settings - showing them all";
    for (int i = 0; i < columns; i++)
    {
        view->setColumnHidden(i, false);
        if (columnSizes[i] == 0) {
            columnSizes[i] = view->columnWidth(i);
        }
        if (i < menu->actions().size()) {
            menu->actions().at(i)->setChecked(true);
        }
    }
}

// Puts a header's columns in the saved order, each column's saved value being its visual
// index. They are placed leftmost first: moving a column into place only shifts the ones
// to the right of where it lands, so those already placed stay put. Going through them in
// logical order instead lets a later move push an earlier one out of position - 207 of
// the orders of up to six columns come back wrong that way. Sorting by the saved value,
// rather than indexing by it, also copes with a layout saved before every column's
// position was stored, in which two columns can claim the same place.
static void restoreColumnOrder(QHeaderView *header, const int *visualIndexes, int columns)
{
    QList<int> order;
    for (int i = 0; i < columns; i++) {
        order.append(i);
    }
    // Read in full before the first move, because each move emits sectionMoved and the
    // handlers write the new layout back over these very settings
    std::stable_sort(order.begin(), order.end(), [visualIndexes](int a, int b) {
        return visualIndexes[a] < visualIndexes[b];
    });
    for (int visual = 0; visual < columns; visual++) {
        header->moveSection(header->visualIndex(order[visual]), visual);
    }
}

AircraftGUI* AircraftGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    AircraftGUI* gui = new AircraftGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void AircraftGUI::destroy()
{
    delete this;
}

void AircraftGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

// The three splitters travel together in one blob, each holding its own
// QSplitter::saveState(). A version leads it so that adding a fourth later can be read
// by an older build without it making nonsense of the ones it does understand.
QByteArray AircraftGUI::saveSplitters() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);

    // Version 2 adds the Documents and Logons splitters. Bumped rather than appended
    // silently: restoreSplitters() reads a fixed number of blobs, so a version 1 preset
    // has to be recognised as such and its two missing states left at their defaults.
    stream << (quint32) 2;
    stream << ui->splitter->saveState()
           << ui->aircraftSplitter->saveState()
           << ui->weatherSplitter->saveState()
           << ui->documentsSplitter->saveState()
           << ui->logonsSplitter->saveState();
    return data;
}

void AircraftGUI::restoreSplitters(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_15);

    quint32 version = 0;
    stream >> version;
    if (((version != 1) && (version != 2)) || (stream.status() != QDataStream::Ok)) {
        return;             // Written by something else, or truncated: leave the defaults
    }

    QByteArray outer, aircraft, weather;
    stream >> outer >> aircraft >> weather;
    if (stream.status() != QDataStream::Ok) {
        return;
    }
    ui->splitter->restoreState(outer);
    ui->aircraftSplitter->restoreState(aircraft);
    ui->weatherSplitter->restoreState(weather);

    // A version 1 preset stops here, and the two below keep the layout the .ui gives them
    if (version < 2) {
        return;
    }
    QByteArray documents, logons;
    stream >> documents >> logons;
    if (stream.status() != QDataStream::Ok) {
        return;
    }
    ui->documentsSplitter->restoreState(documents);
    ui->logonsSplitter->restoreState(logons);
}

QByteArray AircraftGUI::serialize() const
{
    // The records live in the session database now, written by the tracker that sets
    // them, so nothing goes in the preset. An empty blob is deliberate rather than an
    // omission: a preset written before the move still carries one, and the tracker
    // imports that once, so leaving a stale copy here would only invite confusion.
    AircraftSettings settings = m_settings;
    settings.m_statistics = QByteArray();
    settings.m_splitterStates = saveSplitters();
    return settings.serialize();
}

bool AircraftGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        // m_settings.m_statistics is only still read for the one time import into the
        // session database, which the tracker does - see AircraftTracker::applySettings()
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

void AircraftGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
    m_settingsKeys.append("workspaceIndex");
    applySettings();
}

bool AircraftGUI::handleMessage(const Message& message)
{
    if (Aircraft::MsgConfigureAircraft::match(message))
    {
        const Aircraft::MsgConfigureAircraft& cfg = (Aircraft::MsgConfigureAircraft&) message;

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

    return false;
}

void AircraftGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AircraftGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

AircraftTracker *AircraftGUI::tracker() const
{
    return m_aircraft->getTracker();
}

AircraftGUI::AircraftGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::AircraftGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/aircraft/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_aircraft = reinterpret_cast<Aircraft*>(feature);
    m_aircraft->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    AircraftInformation::init();

    // Before any header configuration below, as setHorizontalHeader() resets section
    // state. QHeaderView draws through QStyle, and the Fusion style asks Windows for the
    // high contrast setting once per drawn primitive, uncached - around 14 system calls
    // for one header repaint. FastHeaderView draws the section itself
    for (QTableView *view : {(QTableView *) ui->aircraft, (QTableView *) ui->oldAircraft,
                             (QTableView *) ui->flights, (QTableView *) ui->oldFlights,
                             (QTableView *) ui->documents, (QTableView *) ui->logons,
                             (QTableView *) ui->atcMessages, (QTableView *) ui->statistics,
                             (QTableView *) ui->weather, (QTableView *) ui->aircraftInfo,
                             (QTableView *) ui->pastFlights, (QTableView *) ui->pastAircraft}) {
        view->setHorizontalHeader(new FastHeaderView(Qt::Horizontal, view));
    }

    // Aircraft and flights tables: model/view, with the data coming as
    // snapshots from the tracker thread
    m_aircraftModel = new AircraftTableModel(this);
    m_aircraftProxy = new ActiveFilterProxy(true, this);
    m_oldAircraftProxy = new ActiveFilterProxy(false, this);
    m_flightModel = new FlightTableModel(this);
    m_flightProxy = new ActiveFilterProxy(true, this);
    m_oldFlightProxy = new ActiveFilterProxy(false, this);
    // The live views are re-sorted and re-filtered once per batch from the tracker, by
    // trackerAircraftUpdated() and trackerFlightsUpdated(), rather than dynamically. The
    // models emit a dataChanged for each row they update, and a sorted dynamic proxy
    // re-sorts on every one of them - a layout change of the whole view per aircraft, for
    // batches of dozens every 300 ms. The archive views stay dynamic: the rows changing
    // are almost all live ones, which they look at and ignore, and invalidating them
    // instead would sort the whole archive on every batch.
    m_aircraftProxy->setDynamicSortFilter(false);
    m_flightProxy->setDynamicSortFilter(false);

    for (auto pair : { qMakePair(ui->aircraft, m_aircraftProxy), qMakePair(ui->oldAircraft, m_oldAircraftProxy) })
    {
        pair.second->setSourceModel(m_aircraftModel);
        pair.second->setSortRole(Qt::DisplayRole);
        pair.first->setModel(pair.second);
        pair.first->setSortingEnabled(true);
        pair.first->horizontalHeader()->setSectionsMovable(true);
        pair.first->setIconSize(QSize(85, 20));
        showRowNumbers(pair.first);
        connect(pair.first->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AircraftGUI::aircraftSelectionChanged);
    }

    for (auto pair : { qMakePair(ui->flights, m_flightProxy), qMakePair(ui->oldFlights, m_oldFlightProxy) })
    {
        pair.second->setSourceModel(m_flightModel);
        pair.first->setModel(pair.second);
        pair.first->setSortingEnabled(true);
        pair.first->horizontalHeader()->setSectionsMovable(true);
        showRowNumbers(pair.first);
        connect(pair.first->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AircraftGUI::flightSelectionChanged);
    }

    // Documents, Logons and ATC log tables. Before the column widths, which are worked
    // out from the headings
    auto documentModel = [this](const QString& what) {
        return new MessageTableModel(
            { "Time", "Callsign", "Reg", "Type", "Title" },
            { QString("Time the %1 was received").arg(what),
              QString("Callsign of the flight the %1 belongs to").arg(what),
              "Aircraft registration or ICAO address",
              QString("Kind of %1").arg(what),
              QString("%1 title").arg(what.left(1).toUpper() + what.mid(1)) },
            this);
    };
    m_documentModel = documentModel("document");
    m_logonModel = documentModel("logon");
    m_atcModel = new MessageTableModel(
        { "Time", "Protocol", "Dir", "From", "To", "Message" },
        { "Time the message was received",
          "Message protocol, e.g. CPDLC",
          "Message direction: up arrow for uplink (ATC to aircraft), down arrow for downlink",
          "Message source, when known",
          "Message recipient, when known",
          "The ATC message text" },
        this);
    m_documentProxy = new MessageFilterProxy(this);
    m_logonProxy = new MessageFilterProxy(this);
    m_atcProxy = new MessageFilterProxy(this);
    for (auto view : { std::make_tuple(ui->documents, m_documentModel, m_documentProxy),
                       std::make_tuple(ui->logons, m_logonModel, m_logonProxy),
                       std::make_tuple(ui->atcMessages, m_atcModel, m_atcProxy) })
    {
        QTableView *table = std::get<0>(view);
        MessageFilterProxy *proxy = std::get<2>(view);
        proxy->setSourceModel(std::get<1>(view));
        table->setModel(proxy);
        table->setSortingEnabled(true);
        table->horizontalHeader()->setSectionsMovable(true);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table, &QTableView::customContextMenuRequested, this,
            [this, table, proxy](QPoint pos) { messageContextMenu(table, proxy, pos); });
    }
    connect(ui->documents->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AircraftGUI::documentSelectionChanged);
    connect(ui->logons->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AircraftGUI::logonSelectionChanged);

    setDefaultColumnWidths();
    connect(ui->weather, &QTableWidget::itemSelectionChanged, this, &AircraftGUI::weatherSelectionChanged);
    ui->weather->setSortingEnabled(true);
    ui->weather->horizontalHeader()->setSectionsMovable(true);
    ui->weather->horizontalHeader()->setStretchLastSection(true);
    ui->weather->verticalHeader()->setVisible(false);
    connect(ui->pastFlights, &QTableWidget::itemSelectionChanged, this, &AircraftGUI::pastFlightSelectionChanged);
    connect(ui->find, &QLineEdit::returnPressed, this, &AircraftGUI::on_find_returnPressed);
    m_findTimer.setSingleShot(true);
    connect(&m_findTimer, &QTimer::timeout, this, [this]() {
        ui->find->setStyleSheet(findFrom(-1, ui->find->text()) ? "" : "QLineEdit { color: red; }");
    });

    // A row found while the keyboard stays in the Find box is a selection in a view
    // that does not have focus, which most styles draw in a washed out grey. Make the
    // unfocused highlight the same as the focused one so the match is actually visible.
    // Painting cells through QStyle costs a SystemParametersInfo call per cell with the
    // Fusion style - see FastItemDelegate. These tables are the ones that showed it
    for (QAbstractItemView *view : {(QAbstractItemView *) ui->aircraft, (QAbstractItemView *) ui->oldAircraft,
                                    (QAbstractItemView *) ui->flights, (QAbstractItemView *) ui->oldFlights,
                                    (QAbstractItemView *) ui->documents, (QAbstractItemView *) ui->logons,
                                    (QAbstractItemView *) ui->atcMessages, (QAbstractItemView *) ui->statistics,
                                    (QAbstractItemView *) ui->weather, (QAbstractItemView *) ui->aircraftInfo,
                                    (QAbstractItemView *) ui->pastFlights, (QAbstractItemView *) ui->pastAircraft}) {
        view->setItemDelegate(new FastItemDelegate(view));
    }

    for (QTableView *view : {ui->aircraft, ui->oldAircraft, ui->flights, ui->oldFlights})
    {
        QPalette palette = view->palette();
        palette.setColor(QPalette::Inactive, QPalette::Highlight,
                         palette.color(QPalette::Active, QPalette::Highlight));
        palette.setColor(QPalette::Inactive, QPalette::HighlightedText,
                         palette.color(QPalette::Active, QPalette::HighlightedText));
        view->setPalette(palette);
    }
    connect(ui->find, &QLineEdit::textChanged, this, &AircraftGUI::on_find_textChanged);
    // Changing which column to look in searches again from the top
    connect(ui->findColumn, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { on_find_textChanged(ui->find->text()); });

    // Selecting an aircraft or a flight filters the Documents and ATC tables
    connect(ui->aircraftTabs, &QTabWidget::currentChanged, this, [this](int) {
        QWidget *tab = ui->aircraftTabs->currentWidget();

        applyMessageFilter();

        // Only a tab that IS one of the selection tables may re-derive the selection
        // from it. Weather is not one, and falling through to the aircraft table there
        // used to silently discard a flight selection - which empties Documents and
        // ATC, since with nothing selected they show nothing.
        if ((tab == ui->flightsTab) || (tab == ui->oldFlightsTab)) {
            flightSelectionChanged();
        } else if ((tab == ui->aircraftTab) || (tab == ui->oldAircraftTab)) {
            aircraftSelectionChanged();
        }
    });

    // Context menus: copy cell contents or find the aircraft on the Map
    for (auto pair : { qMakePair((QTableView *) ui->aircraft, true), qMakePair((QTableView *) ui->oldAircraft, true),
                       qMakePair((QTableView *) ui->flights, false), qMakePair((QTableView *) ui->oldFlights, false) })
    {
        QTableView *view = pair.first;
        bool isAircraft = pair.second;
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QTableView::customContextMenuRequested, this,
            [this, view, isAircraft](QPoint pos) { viewContextMenu(view, isAircraft, pos); });
    }
    ui->statistics->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->statistics, &QTableWidget::customContextMenuRequested, this,
        [this](QPoint pos) { tableContextMenu(ui->statistics, pos); });

    // Aircraft photos from planespotters.net, shown for the selected aircraft
    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &AircraftGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &AircraftGUI::photoClicked);
    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);

    // Charts. Wheel events and double clicks reach the view's viewport, in the
    // coordinates the plot area is in.
    plotChart();
    ui->chart->viewport()->installEventFilter(this);
    plotProfileChart();

    // Column select menu
    m_columnMenu = new QMenu(ui->aircraft);
    for (int i = 0; i < AIRCRAFT_COLUMNS; i++)
    {
        QString text = m_aircraftModel->headerData(i, Qt::Horizontal).toString();
        m_columnMenu->addAction(createCheckableItem(text, i, true, SLOT(aircraftColumnSelectMenuChecked())));
    }
    ui->aircraft->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->aircraft->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(aircraftColumnSelectMenu(QPoint)));
    connect(ui->aircraft->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(aircraft_sectionMoved(int, int, int)));
    connect(ui->aircraft->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(aircraft_sectionResized(int, int, int)));

    m_flightColumnMenu = new QMenu(ui->flights);
    for (int i = 0; i < FLIGHT_COLUMNS; i++)
    {
        QString text = m_flightModel->headerData(i, Qt::Horizontal).toString();
        m_flightColumnMenu->addAction(createCheckableItem(text, i, true, SLOT(flightColumnSelectMenuChecked())));
    }
    ui->flights->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->flights->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(flightColumnSelectMenu(QPoint)));

    // The tables whose column state is not saved, so they need no menu of their own
    addColumnMenu(ui->oldAircraft, AIRCRAFT_COLUMNS);
    addColumnMenu(ui->oldFlights, FLIGHT_COLUMNS);
    addColumnMenu(ui->weather, WEATHER_COL_REPORT + 1);
    connect(ui->flights->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(flights_sectionMoved(int, int, int)));
    connect(ui->flights->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(flights_sectionResized(int, int, int)));

    // The tracker does all the processing on its own thread; these queued
    // connections deliver its batched snapshots
    AircraftTracker *t = tracker();
    connect(t, &AircraftTracker::aircraftUpdated, this, &AircraftGUI::trackerAircraftUpdated);
    connect(t, &AircraftTracker::aircraftRemoved, this, &AircraftGUI::trackerAircraftRemoved);
    connect(t, &AircraftTracker::flightsUpdated, this, &AircraftGUI::trackerFlightsUpdated);
    connect(t, &AircraftTracker::flightsRemoved, this, &AircraftGUI::trackerFlightsRemoved);
    connect(t, &AircraftTracker::documentsAdded, this, &AircraftGUI::trackerDocumentsAdded);
    connect(t, &AircraftTracker::flightsMerged, this, &AircraftGUI::trackerFlightsMerged);
    connect(t, &AircraftTracker::aircraftMerged, this, &AircraftGUI::trackerAircraftMerged);
    connect(t, &AircraftTracker::atcMessages, this, &AircraftGUI::trackerAtcMessages);
    connect(t, &AircraftTracker::weatherReports, this, &AircraftGUI::trackerWeatherReports);
    connect(t, &AircraftTracker::profileUpdated, this, &AircraftGUI::trackerProfileUpdated);
    connect(t, &AircraftTracker::messageRates, this, &AircraftGUI::trackerMessageRates);
    connect(t, &AircraftTracker::statisticsUpdated, this, &AircraftGUI::trackerStatistics);
    connect(t, &AircraftTracker::speechNotification, this, &AircraftGUI::trackerSpeechNotification);
    connect(t, &AircraftTracker::allCleared, this, &AircraftGUI::trackerAllCleared);
    connect(t, &AircraftTracker::databaseFilenameReverted,
            this, &AircraftGUI::trackerDatabaseFilenameReverted);
    connect(t, &AircraftTracker::selectRequested, this, &AircraftGUI::trackerSelectRequested);

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applySettings(true);
    makeUIConnections();
    m_resizer.enableChildMouseTracking();

    // Get the current state - the tracker may have restored a session before
    // this GUI connected
    t->getInputMessageQueue()->push(AircraftTracker::MsgResync::create());
}

AircraftGUI::~AircraftGUI()
{
    delete ui;
}

void AircraftGUI::makeUIConnections()
{
    QObject::connect(
        ui->deleteAircraft,
        &QToolButton::clicked,
        this,
        &AircraftGUI::on_deleteAircraft_clicked
    );
    QObject::connect(
        ui->settings,
        &QToolButton::clicked,
        this,
        &AircraftGUI::on_settings_clicked
    );
    QObject::connect(ui->getAircraftDB, &QToolButton::clicked, this, &AircraftGUI::on_getAircraftDB_clicked);
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &AircraftGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &AircraftGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &AircraftGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &AircraftGUI::downloadAircraftInformationFinished);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &AircraftGUI::on_feed_clicked);
    // The settings are behind a right click, as they were on the demodulator's button
    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, &CRightClickEnabler::rightClick, this, &AircraftGUI::feedSelect);

    // Which protocols to draw, and the reset the ADS-B demodulator offers on its own
    // coverage button
    ui->coverage->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->coverage, &QWidget::customContextMenuRequested, this, &AircraftGUI::coverageContextMenu);
    QObject::connect(
        ui->notifications,
        &QToolButton::clicked,
        this,
        &AircraftGUI::on_notifications_clicked
    );
    QObject::connect(
        ui->atcLabels,
        &ButtonSwitch::clicked,
        this,
        &AircraftGUI::on_atcLabels_clicked
    );
    QObject::connect(ui->findDatalink, &ButtonSwitch::clicked, this, &AircraftGUI::on_findDatalink_clicked);
    CRightClickEnabler *findDatalinkRightClickEnabler = new CRightClickEnabler(ui->findDatalink);
    connect(findDatalinkRightClickEnabler, &CRightClickEnabler::rightClick, this, &AircraftGUI::findDatalinkSelect);
    // The statistics sit to the right of the tables. Give the tables the bulk of the
    // width by default - the statistics are two narrow columns and a date - while still
    // letting the handle be dragged.
    ui->aircraftSplitter->setStretchFactor(0, 4);
    ui->aircraftSplitter->setStretchFactor(1, 1);

    QObject::connect(ui->stats, &ButtonSwitch::clicked, this, &AircraftGUI::on_stats_clicked);
    ui->stats->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->stats, &QWidget::customContextMenuRequested, this, &AircraftGUI::statsContextMenu);
    QObject::connect(ui->displayChart, &ButtonSwitch::clicked, this, &AircraftGUI::on_displayChart_clicked);
    QObject::connect(ui->coverage, &ButtonSwitch::clicked, this, &AircraftGUI::on_coverage_clicked);
    ui->displayChart->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->displayChart, &QWidget::customContextMenuRequested, this, &AircraftGUI::chartContextMenu);

    // Nothing is selected yet, so the two history tabs have nothing to be about. They
    // are visible in the .ui so that they can be seen while it is being edited.
    updateHistoryTables();
}

void AircraftGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AircraftGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        Aircraft::MsgConfigureAircraft* message = Aircraft::MsgConfigureAircraft::create(m_settings, m_settingsKeys, force);
        m_aircraft->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

// Default column widths from representative content
// Row numbers down the left of a table. The numbers themselves come from
// ActiveFilterProxy::headerData(), which counts displayed position rather than source
// row so that the top row is 1 whatever the sort order.
//
// The header is left non-clickable: clicking a vertical section selects the row, which
// the table already does when a cell is clicked, and making it look interactive invites
// the assumption that it sorts.
void AircraftGUI::showRowNumbers(QTableView *table)
{
    QHeaderView *header = table->verticalHeader();
    header->setVisible(true);
    header->setSectionsClickable(false);
    header->setHighlightSections(false);
    // The resize mode is deliberately left alone. Setting ResizeToContents here - which
    // seemed reasonable, since the rows carry airline logos of differing heights - makes
    // the header re-measure EVERY row whenever anything in the table changes. On a table
    // holding a session's worth of restored aircraft, each measurement pulls in that
    // row's logo and sideview images, and start up went from seconds to over a minute of
    // a blocked GUI. Row numbering does not need it: the view sizes its own rows exactly
    // as it did when the header was hidden.
}

// The Statistics tab. Rebuilt wholesale on each update - it is a couple of dozen rows
// arriving a few times a second at most, and a table that small is cheaper to refill
// than to diff.
//
// Every figure is given twice, for this session and for all time, because the pair is
// what makes either readable: 210 km on its own says nothing, 210 km against an all time
// best of 340 km says the band is poor today. The session columns come first, as the
// ones that change while you watch.
void AircraftGUI::trackerStatistics(const AircraftStatistics& statistics)
{
    PROFILER_START()
    m_databaseBytes = statistics.m_databaseBytes;
    QTableWidget *table = ui->statistics;
    const bool first = (table->columnCount() == 0);
    if (first)
    {
        table->setColumnCount(5);
        table->setHorizontalHeaderLabels(
            { "Statistic", "Session", "Session set by", "All time", "All time set by" });
        table->horizontalHeader()->setSectionsMovable(true);
        table->verticalHeader()->setVisible(false);
    }

    QList<QStringList> rows;

    auto record = [](const AircraftStatistics::Record& r, const QString& suffix)
    {
        if (!r.m_valid) {
            return QStringList{ QString(), QString() };
        }
        // Who, when, and on what. The row already says which protocol, but not which of
        // its frequencies - and on HFDL, where the usable band moves through the day,
        // that is most of what the record tells you. Records kept before the frequency
        // was recorded have none, and simply do not show one.
        QString by = QString("%1, %2").arg(r.m_aircraft,
            r.m_when.isValid() ? r.m_when.toString("yyyy-MM-dd hh:mm") : QString("?"));
        if (r.m_frequency) {
            by.append(QString(", %1 MHz").arg(r.m_frequency / 1e6, 0, 'f', 3));
        }
        return QStringList{ QString("%1 %2").arg(r.m_value, 0, 'f', 1).arg(suffix), by };
    };

    // Interleaves a statistic computed for each scope into one row: label, then the
    // session's value and detail, then all time's
    auto both = [&rows](const QString& label, const QStringList& session,
                        const QStringList& allTime)
    {
        rows.append(QStringList{ label } + session + allTime);
    };

    const AircraftStatistics::Scope& ses = statistics.m_session;
    const AircraftStatistics::Scope& all = statistics.m_allTime;

    // Furthest per protocol. They are wildly different by nature - line of sight VHF
    // against HF skywave against a satellite downlink - so one combined figure would
    // say nothing useful.
    for (int i : AircraftReport::displayOrder())
    {
        if (!AircraftReport::reportsPosition(i)) {
            continue;
        }
        // The tracker's names, which the web API report uses too, so the two cannot drift
        both(QString("Maximum range, %1").arg(AircraftTracker::protocolName(i)),
             record(ses.m_maxRange[i], "km"), record(all.m_maxRange[i], "km"));
    }

    both("Fastest ground speed", record(ses.m_fastest, "kn"), record(all.m_fastest, "kn"));
    both("Highest altitude", record(ses.m_highest, "ft"), record(all.m_highest, "ft"));

    auto concurrent = [](const AircraftStatistics::Scope& scope)
    {
        return QStringList{ QString::number(scope.m_maxConcurrent),
            scope.m_maxConcurrentWhen.isValid()
                ? scope.m_maxConcurrentWhen.toString("yyyy-MM-dd hh:mm") : QString() };
    };
    both(QString("Most aircraft at once (%1 min)").arg(AircraftStatistics::ConcurrentWindowMins),
         concurrent(ses), concurrent(all));

    both("Aircraft heard",
         QStringList{ QString::number(ses.m_distinctAircraft), QString() },
         QStringList{ QString::number(all.m_distinctAircraft), QString() });
    both("Total messages",
         QStringList{ QString::number(ses.m_totalMessages), QString() },
         QStringList{ QString::number(all.m_totalMessages), QString() });

    // A property of the database rather than of either period, so it goes in the all time
    // column - which is the one the aircraft, flight and message counts beside it describe
    both("Database size", QStringList{ QString(), QString() },
         QStringList{ statistics.m_databaseBytes > 0
                        ? QString("%1 MB").arg(statistics.m_databaseBytes / (1024.0 * 1024.0), 0, 'f', 1)
                        : QString(),
                      QFileInfo(AircraftTracker::databasePath(m_settings)).fileName() });

    // The share is of that scope's own total, so a protocol that has been quiet today
    // shows its reduced share against its long run one rather than against nothing
    auto messages = [](const AircraftStatistics::Scope& scope, int protocol)
    {
        const quint64 n = scope.m_messagesByProtocol[protocol];
        QString share;
        if (scope.m_totalMessages > 0) {
            share = QString("%1%").arg(100.0 * n / scope.m_totalMessages, 0, 'f', 1);
        }
        return QStringList{ QString::number(n), share };
    };
    for (int i : AircraftReport::displayOrder()) {
        both(QString("Messages, %1").arg(AircraftTracker::protocolName(i)), messages(ses, i), messages(all, i));
    }

    auto elapsed = [](qint64 secs, const QDateTime& since)
    {
        if (secs <= 0) {
            return QStringList{ QString(), QString() };
        }
        return QStringList{ QString("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60),
            since.isValid() ? QString("since %1").arg(since.toString("yyyy-MM-dd hh:mm"))
                            : QString() };
    };
    both("Listening time", elapsed(ses.m_seconds, statistics.m_sessionStart),
         elapsed(all.m_seconds, statistics.m_firstStart));

    auto perMinute = [](const AircraftStatistics::Scope& scope)
    {
        if (scope.m_seconds <= 0) {
            return QStringList{ QString(), QString() };
        }
        return QStringList{
            QString("%1").arg(scope.m_totalMessages * 60.0 / scope.m_seconds, 0, 'f', 1),
            QString() };
    };
    both("Messages per minute", perMinute(ses), perMinute(all));

    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); row++)
    {
        for (int col = 0; col < table->columnCount(); col++)
        {
            QTableWidgetItem *item = table->item(row, col);
            if (!item)
            {
                item = new QTableWidgetItem();
                table->setItem(row, col, item);
            }
            item->setText(rows[row].value(col));
        }
    }
    if (first) {
        table->resizeColumnsToContents();
    }
    PROFILER_STOP("Aircraft table: statistics")
}

void AircraftGUI::setDefaultColumnWidths()
{
    auto set = [](QTableView *view, int column, const QString& sample) {
        view->setColumnWidth(column, columnWidthFor(view, column, sample));
    };

    // What a QDateTime cell shows (the delegate's own short format), and what the message
    // tables show, which add the seconds
    const QString shownDateTime = dateTimeSample(QLocale().dateTimeFormat(QLocale::ShortFormat));
    const QString messageDateTime = dateTimeSample(QLocale().dateFormat(QLocale::ShortFormat) + " hh:mm:ss");
    // A callsign is at most 8 characters, and a station address no longer
    const QString callsign = "RYR33KQW";
    const QString reg = "G-ABCDEF";
    // A protocol and frequency, the longest of which is a satellite L band one
    const QString link = "Iridium 1626.104";
    const QString protocols = "ACARS 131.725; HFDL 10.081";

    set(ui->aircraft, AircraftTableModel::COL_ICAO, "ABCDEF");
    set(ui->aircraft, AircraftTableModel::COL_REG, reg);
    set(ui->aircraft, AircraftTableModel::COL_TYPE, "B77W");
    set(ui->aircraft, AircraftTableModel::COL_FLIGHT, callsign);
    set(ui->aircraft, AircraftTableModel::COL_FLIGHT_NO, "BA1234");
    // A logo where one is known (the icon size is 85 wide), otherwise the name
    ui->aircraft->setColumnWidth(AircraftTableModel::COL_AIRLINE,
        std::max(90, columnWidthFor(ui->aircraft, AircraftTableModel::COL_AIRLINE, "Airline name")));
    ui->aircraft->setColumnWidth(AircraftTableModel::COL_SIDEVIEW, 90);
    // A flag, so the heading is what sets the width
    set(ui->aircraft, AircraftTableModel::COL_COUNTRY, "");
    set(ui->aircraft, AircraftTableModel::COL_LATITUDE, "-90.0000");
    set(ui->aircraft, AircraftTableModel::COL_LONGITUDE, "-180.0000");
    set(ui->aircraft, AircraftTableModel::COL_DISTANCE, "10000");
    set(ui->aircraft, AircraftTableModel::COL_ALTITUDE, "40000");
    set(ui->aircraft, AircraftTableModel::COL_HEADING, "360");
    set(ui->aircraft, AircraftTableModel::COL_SPEED, "500");
    set(ui->aircraft, AircraftTableModel::COL_PROTOCOLS, protocols);
    set(ui->aircraft, AircraftTableModel::COL_MESSAGES, "10000");
    set(ui->aircraft, AircraftTableModel::COL_LAST_SEEN, shownDateTime);
    set(ui->aircraft, AircraftTableModel::COL_SELCAL, "AB-CD");

    // Every column, not just some: the ones left out took Qt's default and came out far
    // wider than a tick or a time needs
    set(ui->flights, FlightTableModel::COL_FLIGHT, callsign);
    set(ui->flights, FlightTableModel::COL_FLIGHT_NO, "BA1234");
    set(ui->flights, FlightTableModel::COL_REG, reg);
    set(ui->flights, FlightTableModel::COL_FROM, "WWWW");
    set(ui->flights, FlightTableModel::COL_TO, "WWWW");
    set(ui->flights, FlightTableModel::COL_DOCS, "99");
    // A tick, so the heading is what sets the width
    set(ui->flights, FlightTableModel::COL_LS, "");
    set(ui->flights, FlightTableModel::COL_CL, "");
    set(ui->flights, FlightTableModel::COL_FP, "");
    // Shown as hh:mm
    set(ui->flights, FlightTableModel::COL_OUT, "00:00");
    set(ui->flights, FlightTableModel::COL_OFF, "00:00");
    set(ui->flights, FlightTableModel::COL_ON, "00:00");
    set(ui->flights, FlightTableModel::COL_IN, "00:00");
    set(ui->flights, FlightTableModel::COL_FIRST_SEEN, shownDateTime);
    set(ui->flights, FlightTableModel::COL_LAST_SEEN, shownDateTime);
    set(ui->flights, FlightTableModel::COL_PROTOCOLS, protocols);
    set(ui->flights, FlightTableModel::COL_ROUTE_SOURCE, "Message <>DB");

    // The archive views show the same columns, so give them the same widths
    for (auto pair : { qMakePair((QTableView *) ui->aircraft, (QTableView *) ui->oldAircraft),
                       qMakePair((QTableView *) ui->flights, (QTableView *) ui->oldFlights) })
    {
        for (int col = 0; col < pair.first->model()->columnCount(); col++) {
            pair.second->setColumnWidth(col, pair.first->columnWidth(col));
        }
    }

    // The message tables. Each stretches its last column - the title, report or message
    // text, which is what the space is wanted for - so that is left out here. Left at
    // Qt's default, the short columns took as much room as the text did.
    for (QTableView *table : {ui->documents, ui->logons})
    {
        set(table, DOCUMENT_COL_TIME, messageDateTime);
        set(table, DOCUMENT_COL_FLIGHT, callsign);
        set(table, DOCUMENT_COL_REG, reg);
    }
    set(ui->documents, DOCUMENT_COL_TYPE, "Departure clearance");
    set(ui->logons, DOCUMENT_COL_TYPE, "Logon");

    set(ui->atcMessages, ATC_COL_TIME, messageDateTime);
    set(ui->atcMessages, ATC_COL_PROTOCOL, link);
    set(ui->atcMessages, ATC_COL_DIR, QString(QChar(0x2191)));
    set(ui->atcMessages, ATC_COL_FROM, callsign);
    set(ui->atcMessages, ATC_COL_TO, callsign);

    set(ui->weather, WEATHER_COL_TIME, shownDateTime);
    set(ui->weather, WEATHER_COL_AIRPORT, "WWWW");
    set(ui->weather, WEATHER_COL_TYPE, "Weather");
    set(ui->weather, WEATHER_COL_VIA, callsign);
}

void AircraftGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);

    ui->atcLabels->setChecked(m_settings.m_atcLabels);
    ui->findDatalink->setChecked(m_settings.m_findDatalinkAircraft);
    ui->stats->setChecked(m_settings.m_displayStatistics);
    ui->statistics->setVisible(m_settings.m_displayStatistics);
    ui->displayChart->setChecked(m_settings.m_displayChart);
    ui->coverage->setChecked(m_settings.m_displayCoverage);
    // The feed itself is restored by applySettings(), which calls updateFeedConnections()
    // whatever changed - so without this the feed came back up on restart while its
    // button sat unlit, saying the opposite of what was happening
    ui->feed->setChecked(m_settings.m_feedEnabled);
    ui->chart->setVisible(m_settings.m_displayChart);

    // Order and size columns
    QHeaderView *header = ui->aircraft->horizontalHeader();

    for (int i = 0; i < AIRCRAFT_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        if (i < m_columnMenu->actions().size()) {
            m_columnMenu->actions().at(i)->setChecked(!hidden);
        }
        if (m_settings.m_columnSizes[i] > 0) {
            ui->aircraft->setColumnWidth(i, m_settings.m_columnSizes[i]);
        }
    }
    restoreColumnOrder(header, m_settings.m_columnIndexes, AIRCRAFT_COLUMNS);
    showAllColumnsIfNoneVisible(ui->aircraft, m_settings.m_columnSizes, AIRCRAFT_COLUMNS, m_columnMenu);

    QHeaderView *flightHeader = ui->flights->horizontalHeader();
    for (int i = 0; i < FLIGHT_COLUMNS; i++)
    {
        bool hidden = m_settings.m_flightColumnSizes[i] == 0;
        flightHeader->setSectionHidden(i, hidden);
        if (i < m_flightColumnMenu->actions().size()) {
            m_flightColumnMenu->actions().at(i)->setChecked(!hidden);
        }
        if (m_settings.m_flightColumnSizes[i] > 0) {
            ui->flights->setColumnWidth(i, m_settings.m_flightColumnSizes[i]);
        }
    }
    restoreColumnOrder(flightHeader, m_settings.m_flightColumnIndexes, FLIGHT_COLUMNS);
    showAllColumnsIfNoneVisible(ui->flights, m_settings.m_flightColumnSizes, FLIGHT_COLUMNS, m_flightColumnMenu);

    // After the column work, so the tables are their final shape before the splitters
    // are asked to divide the space between them
    restoreSplitters(m_settings.m_splitterStates);

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void AircraftGUI::onMenuDialogCalled(const QPoint &p)
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

        m_settingsKeys.append("title");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

// ---- Snapshots from the tracker ----

void AircraftGUI::trackerAircraftUpdated(const QList<AircraftDisplay>& aircraft)
{
    PROFILER_START()
    // As ADS-B does: the view repaints on every dataChanged otherwise, and each repaint
    // of a table cell costs a SystemParametersInfo syscall per primitive drawn through
    // the Fusion style, so a batch of a hundred aircraft repainted the table a hundred
    // times
    // Both views, since both are proxies over the one model and both relayout
    const ViewSelections before = selectedIds(false);
    ui->aircraft->setUpdatesEnabled(false);
    ui->oldAircraft->setUpdatesEnabled(false);
    m_ignoreSelectionChanges = true;
    m_aircraftModel->upsert(aircraft);
    // The one re-sort and re-filter of the live view for the whole batch - see the
    // constructor. An aircraft archived here leaves the view without the neighbouring
    // row being selected in its place, as it would be by a row removal.
    m_aircraftProxy->invalidate();
    m_ignoreSelectionChanges = false;
    ui->aircraft->setUpdatesEnabled(true);
    ui->oldAircraft->setUpdatesEnabled(true);
    keepSelections(false, before);

    // Keep the selection-derived state fresh
    bool survivorArrived = false;
    for (const AircraftDisplay& d : aircraft)
    {
        if (d.m_id == m_selectedAircraftId)
        {
            m_selectedRegistration = d.m_registration;
            m_selectedIcao = d.m_icao;
        }
        survivorArrived = survivorArrived || ((m_reselectAircraftId != 0) && (d.m_id == m_reselectAircraftId));
    }
    // The survivor of a merge that took a selected row with it, as it is after the merge.
    // Not before: the row it had until now can be in the other view of the pair from the
    // one the merge has put it in, and selecting it there would have the selection taken
    // away again when this batch moved it.
    if (survivorArrived) {
        reselectMerged();
    }
    PROFILER_STOP("Aircraft table: aircraft upsert")
}

// Gone for good - discarded by the retention sweep, or absorbed into another aircraft,
// which trackerAircraftMerged() has already been told about. Archiving is not removal:
// an archived aircraft stays in the model and moves to the other view.
void AircraftGUI::trackerAircraftRemoved(const QList<quint64>& ids)
{
    // The row of an aircraft that was merged away goes here, and with it the selection
    // if it was the selected one. That selection has already been moved to the survivor,
    // and is put on its row when the tracker sends it. Any other selection stays with the
    // aircraft it was made on - see keepSelections().
    const ViewSelections before = selectedIds(false);
    m_ignoreSelectionChanges = true;
    m_aircraftModel->remove(ids);
    m_ignoreSelectionChanges = false;
    keepSelections(false, before);
    if (ids.contains(m_selectedAircraftId)) {
        m_selectedAircraftId = 0;
    }
    if (ids.contains(m_reselectAircraftId)) {
        m_reselectAircraftId = 0;       // The survivor went too, so there is nothing to put back
    }

    // Their messages go too, or the tables grow for as long as the feature runs. A row
    // whose flight is still with us is kept: the flight is what it belongs to, and an
    // absorbed aircraft's flights move to the survivor rather than going with it.
    const QSet<quint64> gone(ids.begin(), ids.end());
    removeMessageRows([this, &gone](quint64 aircraftId, quint64 flightId) {
        return gone.contains(aircraftId) && ((flightId == 0) || (m_flightModel->rowOf(flightId) < 0));
    });
    // The retention sweep is what removes aircraft other than by merging, and the tracker
    // prunes its own log of what belongs to nothing at the same time
    pruneOrphanMessageRows();

    // The selected aircraft itself was discarded, which the view was not allowed to turn
    // into a selection of its neighbour, so nothing else will deselect it
    if ((m_selectionOwner == SelectionAircraft) && (m_reselectAircraftId == 0)
        && (m_aircraftModel->rowOf(m_selectedAircraftId) < 0)) {
        dropSelection();
    }
}

void AircraftGUI::trackerFlightsUpdated(const QList<FlightDisplay>& flights)
{
    PROFILER_START()
    // As trackerAircraftUpdated()
    const ViewSelections before = selectedIds(true);
    ui->flights->setUpdatesEnabled(false);
    ui->oldFlights->setUpdatesEnabled(false);
    m_ignoreSelectionChanges = true;
    m_flightModel->upsert(flights);
    m_flightProxy->invalidate();
    m_ignoreSelectionChanges = false;
    ui->flights->setUpdatesEnabled(true);
    ui->oldFlights->setUpdatesEnabled(true);
    keepSelections(true, before);
    // The survivor of a merge is reselected only once its row is as the merge left it, as
    // in trackerAircraftUpdated(). Typically the merged-away flight was the current one
    // and the survivor an older name for it, archived until the merge made it current:
    // selected straight away it was selected in the archive, and lost from there when
    // this batch moved it back to the live view. reselectMerged() refills the history
    // tables itself when it succeeds.
    bool survivorArrived = false;
    for (const FlightDisplay& d : flights)
    {
        if ((m_reselectFlightId != 0) && (d.m_id == m_reselectFlightId))
        {
            survivorArrived = true;
            break;
        }
    }
    const bool reselected = survivorArrived && reselectMerged();
    // A new flight for the aircraft on screen, or a new aircraft on the flight on screen,
    // has to show up without the user clicking away and back
    if (!reselected && ((m_selectedAircraftId != 0) || !m_selectedFlightNumber.isEmpty())) {
        updateHistoryTables();
    }
    PROFILER_STOP("Aircraft table: flights upsert")
}

// Two flights were the same flight, so everything shown against the old one now belongs
// to the survivor.
//
// The document and ATC tables hold every message from every aircraft and show only the
// rows whose flight is the selected one, so a row still tagged with the id that has just
// been merged away matches nothing and is hidden. That is what made a selected flight's
// documents disappear a few seconds after they appeared - the ACARS and ADS-B names for
// it were recognised as one flight, and the rows were left pointing at the half that
// ceased to exist. Selecting the flight again rebuilt them, which is why it looked
// intermittent rather than broken.
void AircraftGUI::trackerFlightsMerged(quint64 from, quint64 to)
{
    for (MessageTableModel *model : {m_documentModel, m_logonModel, m_atcModel}) {
        model->retagFlight(from, to);
    }
    // The selection follows the flight rather than being dropped with the id it was
    // made under. Done here, before flightsRemoved is delivered, so that its own test
    // for the selected flight no longer matches.
    if (m_selectedFlightId == from)
    {
        m_selectedFlightId = to;
        // The row it was made on is about to be removed, which empties the view's
        // selection; the survivor's row takes it over once the tracker sends it as the
        // merge left it - see trackerFlightsUpdated()
        if (m_selectionOwner == SelectionFlight) {
            m_reselectFlightId = to;
        }
    }
    if (m_reselectFlightId == from) {
        m_reselectFlightId = to;        // Merged again before its row arrived
    }
    // The tracker moves its watch to the survivor and sends the profile under the
    // survivor's id, which would otherwise be discarded as being for some other flight
    // and leave the chart frozen at the moment of the merge
    if (m_profileFlightId == from) {
        m_profileFlightId = to;
    }
    applyMessageFilter();
}

// The same for an aircraft: a weak duplicate - known only by one identifier - turned out
// to be an airframe already tracked under another, and has been absorbed into it. Its
// flights keep their ids, so only what is held against the aircraft id has to move.
void AircraftGUI::trackerAircraftMerged(quint64 from, quint64 to)
{
    for (MessageTableModel *model : {m_documentModel, m_logonModel, m_atcModel}) {
        model->retagAircraft(from, to);
    }
    if (m_selectedAircraftId == from)
    {
        m_selectedAircraftId = to;
        if (m_selectionOwner == SelectionAircraft) {
            m_reselectAircraftId = to;
        }
    }
    if (m_reselectAircraftId == from) {
        m_reselectAircraftId = to;
    }
    // Still the same aircraft's history, so a sort chosen in Past Flights is kept
    if (m_pastFlightsAircraftId == from) {
        m_pastFlightsAircraftId = to;
    }
    applyMessageFilter();
}

// Gone for good - discarded with its aircraft by the retention sweep, or merged into
// another flight, which trackerFlightsMerged() has already been told about
void AircraftGUI::trackerFlightsRemoved(const QList<quint64>& ids)
{
    // As in trackerAircraftRemoved(): a selected row removed because its flight was
    // merged is a selection we mean to keep
    const ViewSelections before = selectedIds(true);
    m_ignoreSelectionChanges = true;
    m_flightModel->remove(ids);
    m_ignoreSelectionChanges = false;
    keepSelections(true, before);
    if (ids.contains(m_reselectFlightId)) {
        m_reselectFlightId = 0;
    }

    // A merged flight's rows were moved to the survivor before this, so these are the
    // rows of flights that are genuinely gone
    const QSet<quint64> gone(ids.begin(), ids.end());
    removeMessageRows([&gone](quint64 aircraftId, quint64 flightId) {
        (void) aircraftId;
        return (flightId != 0) && gone.contains(flightId);
    });

    if (ids.contains(m_selectedFlightId))
    {
        m_selectedFlightId = 0;
        // Nothing can match a selection that no longer exists, so the tables would keep
        // showing whatever was last let through until something else refiltered them
        applyMessageFilter();
    }
    // As for an aircraft: the selected flight was discarded, not merged
    if ((m_selectionOwner == SelectionFlight) && (m_reselectFlightId == 0)
        && (m_flightModel->rowOf(m_selectedFlightId) < 0))
    {
        dropSelection();        // Refills the history tables as well
        return;
    }
    // The history tables list flights too, and would go on listing the removed ones
    // until the next flush happened to refill them
    if ((m_selectedAircraftId != 0) || !m_selectedFlightNumber.isEmpty()) {
        updateHistoryTables();
    }
}

// Inserts a new row into the Weather table, which stays sorted throughout, where it
// belongs.
//
// Turning sorting off for a batch and back on after it sorts the WHOLE table, with a
// layout change that updates every persistent index - for a few new rows. Leaving sorting
// on and appending rows is no better: setting the item in the sort column then MOVES the
// row to where it belongs, and tables sort newest first by default, so that was every
// new row, moved from the bottom to the top.
//
// So the row is inserted where it belongs in the first place, found with the same
// search Qt uses, std::lower_bound with its comparators. When the sort item is set,
// last, Qt searches the rows above the first empty sort cell - this one - finds the
// same place, and moves nothing.
static void insertSortedRow(QTableWidget *table, std::initializer_list<std::pair<int, QTableWidgetItem *>> items)
{
    const QHeaderView *header = table->horizontalHeader();
    const int sortColumn = table->isSortingEnabled() ? header->sortIndicatorSection() : -1;
    const bool ascending = header->sortIndicatorOrder() == Qt::AscendingOrder;
    QTableWidgetItem *sortItem = nullptr;
    for (const auto& columnItem : items)
    {
        if (columnItem.first == sortColumn) {
            sortItem = columnItem.second;
        }
    }
    int row = table->rowCount();
    if (sortItem)
    {
        int low = 0;
        int high = row;
        while (low < high)
        {
            const int mid = low + (high - low) / 2;
            const QTableWidgetItem *other = table->item(mid, sortColumn);
            const bool before = other && (ascending ? (*other < *sortItem) : (*sortItem < *other));
            if (before) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        row = low;
    }
    table->insertRow(row);
    for (const auto& columnItem : items)
    {
        if (columnItem.second != sortItem) {
            table->setItem(row, columnItem.first, columnItem.second);
        }
    }
    if (sortItem) {
        table->setItem(row, sortColumn, sortItem);
    }
}

// Takes out of the Documents, Logons and ATC tables the rows the test picks, given each
// row's aircraft and flight ids - and, when olderThan is valid, only those received before
// it. Unattributed SELCAL calls carry neither id, so a test for the ids of something that
// has gone never picks them; only pruneOrphanMessageRows() does, by age.
void AircraftGUI::removeMessageRows(const std::function<bool(quint64 aircraftId, quint64 flightId)>& matches,
                                    const QDateTime& olderThan)
{
    const qint64 olderThanMs = olderThan.isValid() ? olderThan.toMSecsSinceEpoch() : 0;
    for (MessageTableModel *model : {m_documentModel, m_logonModel, m_atcModel})
    {
        model->removeIf([&matches](const MessageRow& row) {
            return matches(row.m_aircraftId, row.m_flightId);
        }, olderThanMs);
    }
}

// The tracker's retention sweep discards an ATC message that belongs to no aircraft it
// still holds once the message is older than the shorter retention period (see
// AircraftSettings::messageCutoff()): an unattributed SELCAL
// call, or a message from an aircraft that went before its messages were taken with it.
// The tables here are only ever pruned by the aircraft and flights the tracker removes, so
// without the same rule those rows stayed until the feature was restarted. Documents and
// logons go by it too, keeping a row while its flight is still with us as
// trackerAircraftRemoved() does, and weather reports, which are about airports, go by
// age alone as the tracker's do.
void AircraftGUI::pruneOrphanMessageRows()
{
    const QDateTime cutoff = m_settings.messageCutoff();
    if (!cutoff.isValid()) {
        return;
    }
    m_orphansPrunedMs = QDateTime::currentMSecsSinceEpoch();
    removeMessageRows([this](quint64 aircraftId, quint64 flightId) {
        return ((aircraftId == 0) || (m_aircraftModel->rowOf(aircraftId) < 0))
            && ((flightId == 0) || (m_flightModel->rowOf(flightId) < 0));
    }, cutoff);

    bool updatesSuspended = false;
    for (int row = ui->weather->rowCount() - 1; row >= 0; row--)
    {
        const QTableWidgetItem *item = ui->weather->item(row, WEATHER_COL_TIME);
        const QDateTime received = item ? item->data(Qt::DisplayRole).toDateTime() : QDateTime();
        if (received.isValid() && (received < cutoff))
        {
            if (!updatesSuspended)
            {
                ui->weather->setUpdatesEnabled(false);
                updatesSuspended = true;
            }
            ui->weather->removeRow(row);
        }
    }
    if (updatesSuspended) {
        ui->weather->setUpdatesEnabled(true);
    }
}

// A batch can be the whole history the tracker hands over when the GUI opens, which on a
// long session is over a hundred thousand rows. Rows are appended to the model as they
// are, with no per-row work beyond copying the strings, which are shared with the event
void AircraftGUI::trackerDocumentsAdded(const QList<DocumentEvent>& documents)
{
    PROFILER_START()
    QList<MessageRow> documentRows;
    QList<MessageRow> logonRows;
    for (const DocumentEvent& e : documents)
    {
        MessageRow row;
        row.m_aircraftId = e.m_aircraftId;
        row.m_flightId = e.m_flightId;
        // Free for the history, which the database hands over in UTC; a local time costs
        // a time zone lookup
        row.m_receivedMs = e.m_received.isValid() ? e.m_received.toMSecsSinceEpoch() : 0;
        row.m_mapName = e.m_mapName;
        row.m_text = e.m_text;
        row.m_cells = QStringList{ e.m_flight, e.m_reg, e.m_kind, e.m_title };
        // Logons go to their own tab. There are far more of them than of everything else
        // together - 6610 against 700 in a week here - so left in with the flight plans
        // and clearances they buried them.
        if (e.m_kind == "Logon") {
            logonRows.append(row);
        } else {
            documentRows.append(row);
        }
    }
    m_documentModel->append(documentRows);
    m_logonModel->append(logonRows);
    PROFILER_STOP("Aircraft table: documents")
}

// Weather is about airports, so unlike the Documents and ATC tables it is not filtered
// by the selected aircraft - the aircraft that carried a METAR is incidental to it.
void AircraftGUI::trackerWeatherReports(const QList<WeatherEvent>& reports)
{
    PROFILER_START()
    for (const WeatherEvent& e : reports)
    {
        QTableWidgetItem *timeItem = new QTableWidgetItem();
        timeItem->setData(Qt::DisplayRole, e.m_received);
        timeItem->setData(Qt::UserRole, e.m_text);      // Read back by the detail pane
        QTableWidgetItem *reportItem = new QTableWidgetItem(e.m_summary);
        reportItem->setToolTip(e.m_text);
        insertSortedRow(ui->weather, {
            {WEATHER_COL_TIME, timeItem},
            {WEATHER_COL_AIRPORT, new QTableWidgetItem(e.m_airport)},
            {WEATHER_COL_TYPE, new QTableWidgetItem(e.m_kind)},
            {WEATHER_COL_VIA, new QTableWidgetItem(e.m_from)},
            {WEATHER_COL_REPORT, reportItem}
        });
    }
    ui->weather->scrollToBottom();
    PROFILER_STOP("Aircraft table: weather")
}

void AircraftGUI::weatherSelectionChanged()
{
    const QList<QTableWidgetItem *> selected = ui->weather->selectedItems();
    for (const auto *selItem : selected)
    {
        QTableWidgetItem *timeItem = ui->weather->item(selItem->row(), WEATHER_COL_TIME);
        if (timeItem)
        {
            // The report carries HTML, which setPlainText() would show as tags
            ui->weatherText->setHtml(timeItem->data(Qt::UserRole).toString());
            return;
        }
    }
    ui->weatherText->clear();
}

void AircraftGUI::trackerAtcMessages(const QList<AtcEvent>& messages)
{
    PROFILER_START()
    QList<MessageRow> rows;
    rows.reserve(messages.size());
    for (const AtcEvent& e : messages)
    {
        MessageRow row;
        row.m_aircraftId = e.m_aircraftId;
        row.m_flightId = e.m_flightId;
        row.m_receivedMs = e.m_received.isValid() ? e.m_received.toMSecsSinceEpoch() : 0;
        row.m_mapName = e.m_mapName;
        row.m_tooltip = e.m_tooltip;
        // The link it arrived on, not e.m_protocol: every ATC message is CPDLC by
        // definition - addAtcMessage() returns early for anything else - so that column
        // held the same word in every row
        row.m_cells = QStringList{ e.m_link, QString(QChar(e.m_uplink ? 0x2191 : 0x2193)), e.m_from, e.m_to, e.m_message };
        rows.append(row);
    }
    m_atcModel->append(rows);
    ui->atcMessages->scrollToBottom();
    PROFILER_STOP("Aircraft table: ATC messages")
}

// The tracker could not move to the database that was asked for and has stayed with the
// one it had. These settings are the ones that get serialised, so they have to agree -
// otherwise the preset would name a file nothing is writing to.
void AircraftGUI::trackerDatabaseFilenameReverted(const QString& filename)
{
    if (m_settings.m_databaseFilename == filename) {
        return;
    }
    m_settings.m_databaseFilename = filename;
    m_settingsKeys.append("databaseFilename");
    applySettings();
}

// Asked from outside - the web API, and so a link clicked in an aircraft's Map popup -
// to show a particular aircraft or flight. The row may be in the live table or the
// archive, so both are searched and the tab holding the match is brought forward.
void AircraftGUI::trackerSelectRequested(int kind, const QString& value)
{
    if (value.isEmpty()) {
        return;
    }

    if (kind == AircraftTracker::MsgSelect::Flight)
    {
        // The most recently seen flight of that number, which is what someone clicking a
        // flight number on the Map means by it
        quint64 best = 0;
        QDateTime bestSeen;
        for (int i = 0; i < m_flightModel->rowCount(); i++)
        {
            const FlightDisplay *f = m_flightModel->flightAt(i);
            if (!f || f->m_flight.compare(value, Qt::CaseInsensitive)) {
                continue;
            }
            if (!bestSeen.isValid() || (f->m_lastSeen > bestSeen))
            {
                bestSeen = f->m_lastSeen;
                best = f->m_id;
            }
        }
        // By id, so it is the flight found above and not merely the first of that number
        if (best) {
            selectFlightById(best);
        }
        return;
    }

    for (int i = 0; i < m_aircraftModel->rowCount(); i++)
    {
        const AircraftDisplay *d = m_aircraftModel->aircraftAt(i);
        if (!d) {
            continue;
        }
        const bool match = (kind == AircraftTracker::MsgSelect::Icao)
            ? (d->m_icao && (QString("%1").arg(d->m_icao, 6, 16, QChar('0')).compare(
                   value, Qt::CaseInsensitive) == 0))
            : (!d->m_registration.isEmpty()
               && (d->m_registration.compare(value, Qt::CaseInsensitive) == 0));
        if (match)
        {
            selectAircraft(d->m_id);
            return;
        }
    }
}

void AircraftGUI::trackerAllCleared()
{
    m_aircraftModel->clear();
    m_flightModel->clear();
    m_documentModel->clear();
    m_logonModel->clear();
    ui->documentText->clear();
    ui->logonText->clear();
    m_atcModel->clear();
    ui->weather->setRowCount(0);
    ui->weatherText->clear();
    clearSelectionState();
}

// Back to nothing selected. The models were reset rather than having rows removed, and a
// reset emits no selection change, so none of the handlers that would otherwise do this
// get to run - and everything the selection drove went on showing the deleted aircraft.
void AircraftGUI::clearSelectionState()
{
    m_selectionOwner = SelectionNone;
    m_selectedAircraftId = 0;
    m_selectedFlightId = 0;
    m_selectedFlightNumber.clear();
    m_selectedRegistration.clear();
    m_selectedIcao = 0;
    m_reselectAircraftId = 0;
    m_reselectFlightId = 0;
    m_findRow = -1;

    // The local half of watchFlight(0) only. The tracker is watching nothing already:
    // Delete All clears its watch, and a resync is for a GUI that has not chosen one.
    // Left as it was, plotProfileChart() redrew the deleted flight from what it held.
    m_profileFlightId = 0;
    m_profileTitle.clear();
    m_profileTimes.clear();
    m_profileAltFt.clear();
    m_profileSpeedKts.clear();
    plotProfileChart();

    updatePhoto(QString(), 0);      // Hides the photograph and empties the details table
    updateHistoryTables();          // Empties both history tabs, and hides them
}

void AircraftGUI::trackerSpeechNotification(const QString& speech)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (!m_speech) {
        m_speech = new QTextToSpeech(this);
    }
    m_speech->say(speech);
#else
    qWarning() << "AircraftGUI::trackerSpeechNotification: TextToSpeech not supported. Unable to say " << speech;
#endif
}

// ---- Selection, filtering, photo and profile ----

// The Documents and ATC tables show the selected aircraft (Aircraft tab active) or flight
// (Flights tab active) - see MessageFilterProxy::filterAcceptsRow()
void AircraftGUI::applyMessageFilter()
{
    PROFILER_START()
    int shown = 0;
    for (MessageFilterProxy *proxy : {m_documentProxy, m_logonProxy, m_atcProxy})
    {
        proxy->setSelection(m_selectedAircraftId, m_selectedFlightId);
        shown += proxy->rowCount();
    }

    // A row filtered out takes its selection with it, but the pane below is only cleared
    // by the selection change that causes, so make sure it does not go on showing a
    // message whose row is no longer on screen
    if (!ui->documents->selectionModel()->hasSelection()) {
        ui->documentText->clear();
    }
    if (!ui->logons->selectionModel()->hasSelection()) {
        ui->logonText->clear();
    }

    // Only when what it filters on has moved, so this is a handful of lines around a
    // selection change rather than one on every message. The Documents and ATC tables
    // hide EVERY row when nothing is selected, so a selection lost without the user
    // touching anything empties them, and nothing used to say when that happened.
    static quint64 lastFlight = 0;
    static quint64 lastAircraft = 0;
    static int lastShown = -1;
    // Also when the number of visible rows moves without the selection moving, which is
    // the symptom itself: documents that were on screen going away while the same flight
    // is still selected. Logging only the selection could never have caught that.
    if ((m_selectedFlightId != lastFlight) || (m_selectedAircraftId != lastAircraft)
        || (shown != lastShown))
    {
        lastShown = shown;
        qDebug() << "AircraftGUI::applyMessageFilter: flight" << lastFlight << "->"
                 << m_selectedFlightId << " aircraft" << lastAircraft << "->"
                 << m_selectedAircraftId << " showing" << shown << "rows of"
                 << (m_documentModel->rowCount() + m_logonModel->rowCount() + m_atcModel->rowCount());
        lastFlight = m_selectedFlightId;
        lastAircraft = m_selectedAircraftId;
    }
    PROFILER_STOP("Aircraft table: message filter")
}

// The archive tabs show what we are no longer hearing from
bool AircraftGUI::showingArchive() const
{
    QWidget *tab = ui->aircraftTabs->currentWidget();
    return (tab == ui->oldAircraftTab) || (tab == ui->oldFlightsTab);
}

// What the aircraft database knows about this airframe, which does not change as it flies
void AircraftGUI::updateAircraftInfo(const QString& registration, quint32 icao)
{
    ui->aircraftInfo->setRowCount(0);

    QSharedPointer<const QHash<QString, AircraftInformation *>> byReg = OsnDB::getAircraftInformationByReg();
    QSharedPointer<const QHash<int, AircraftInformation *>> byIcao = OsnDB::getAircraftInformation();
    const AircraftInformation *info = nullptr;

    if (byReg && byReg->contains(registration)) {
        info = byReg->value(registration);
    } else if (icao && byIcao && byIcao->contains((int) icao)) {
        info = byIcao->value((int) icao);
    }
    if (!info) {
        return;
    }

    const QList<QPair<QString, QString>> details = {
        { "ICAO", QString("%1").arg(info->m_icao, 6, 16, QChar('0')).toUpper() },
        { "Registration", info->m_registration },
        { "Manufacturer", info->m_manufacturerName },
        { "Model", info->m_model },
        { "Type", info->m_type },
        { "Owner", info->m_owner },
        { "Operator", info->m_operator },
        { "Operator ICAO", info->m_operatorICAO },
        { "Registered", info->m_registered },
    };

    for (const auto& detail : details)
    {
        if (detail.second.isEmpty()) {
            continue;
        }
        int row = ui->aircraftInfo->rowCount();
        ui->aircraftInfo->setRowCount(row + 1);
        ui->aircraftInfo->setItem(row, 0, new QTableWidgetItem(detail.first));
        ui->aircraftInfo->setItem(row, 1, new QTableWidgetItem(detail.second));
    }

    // The country is shown as its flag, as it is in the aircraft table
    const QString flag = info->getFlag();
    if (!flag.isEmpty())
    {
        int row = ui->aircraftInfo->rowCount();
        ui->aircraftInfo->setRowCount(row + 1);
        ui->aircraftInfo->setItem(row, 0, new QTableWidgetItem("Country"));

        QTableWidgetItem *country = new QTableWidgetItem();
        QIcon *icon = AircraftInformation::getFlagIcon(flag);
        if (icon)
        {
            country->setIcon(*icon);
            country->setToolTip(flag);
        }
        else
        {
            country->setText(flag);
        }
        ui->aircraftInfo->setItem(row, 1, country);
    }
    ui->aircraftInfo->resizeColumnToContents(0);
}

// The two history tabs, both built from the flight model rather than from the tracker.
//
// Every flight the feature knows about is already in that model, live and archived alike,
// and a FlightDisplay carries both the aircraft it was flown by and the flight number -
// so "which flights has this aircraft operated" and "which aircraft have operated this
// flight" are two readings of the same list, and neither needs the tracker to be asked.
// Highlight the next row matching what was typed, in whichever table is on show.
//
// A filter would hide everything else, and what is wanted here is to pick one aircraft
// out of a busy table while still seeing the rest of the traffic around it - so this
// selects and scrolls to the match instead. Enter steps on to the next one and wraps.
bool AircraftGUI::findFrom(int startRow, const QString& text)
{
    if (text.isEmpty()) {
        return false;
    }

    const bool flights = (ui->aircraftTabs->currentWidget() == ui->flightsTab)
                      || (ui->aircraftTabs->currentWidget() == ui->oldFlightsTab);
    QTableView *view = flights ? (showingArchive() ? ui->oldFlights : ui->flights)
                               : (showingArchive() ? ui->oldAircraft : ui->aircraft);
    ActiveFilterProxy *proxy = flights ? (showingArchive() ? m_oldFlightProxy : m_flightProxy)
                                       : (showingArchive() ? m_oldAircraftProxy : m_aircraftProxy);
    const int rows = proxy->rowCount();
    if (rows == 0) {
        return false;
    }
    const int column = ui->findColumn->currentIndex();
    // Fetched once for the whole search. Each fetch takes a lock and looks at the
    // database file's modification time, twice, and doing that for every row of a
    // couple of thousand aircraft made a search thousands of file system calls.
    const QSharedPointer<const QHash<QString, AircraftInformation *>> byReg
        = OsnDB::getAircraftInformationByReg();

    for (int i = 0; i < rows; i++)
    {
        // Walk in the order the rows are displayed, so stepping through the matches
        // follows the sort the user is looking at rather than the model's own order
        const int row = (startRow + 1 + i) % rows;
        const int sourceRow = proxy->mapToSource(proxy->index(row, 0)).row();

        // Type and airline are not on either snapshot - they come from the database,
        // which is what the table itself displays in those columns, so searching them
        // matches what is on screen
        auto typeFields = [&byReg](const QString& registration) {
            QStringList f;
            if (byReg && byReg->contains(registration))
            {
                const AircraftInformation *info = byReg->value(registration);
                f << info->m_type << info->m_model << info->m_manufacturerName;
            }
            return f;
        };

        QStringList fields;
        if (flights)
        {
            const FlightDisplay *d = m_flightModel->flightAt(sourceRow);
            if (!d) {
                continue;
            }
            // A flight has no ICAO address of its own - the airframe flying it does,
            // and that is what the Aircraft table is for, so ICAO simply finds nothing
            switch (column)
            {
            case FindIcao:   break;
            case FindSelcal: break;     // Belongs to the airframe, like the ICAO address
            case FindReg:    fields << d->m_reg; break;
            case FindType:   fields << typeFields(d->m_reg); break;
            case FindFlight: fields << d->m_flight << d->m_aliases; break;
            default:
                fields << d->m_flight << d->m_aliases << d->m_reg
                       << d->m_departure << d->m_arrival << d->m_route
                       << typeFields(d->m_reg);
                break;
            }
        }
        else
        {
            const AircraftDisplay *d = m_aircraftModel->aircraftAt(sourceRow);
            if (!d) {
                continue;
            }
            const QString icao = QString("%1").arg(d->m_icao, 6, 16, QChar('0')).toUpper();
            switch (column)
            {
            case FindIcao:   fields << icao; break;
            case FindReg:    fields << d->m_registration; break;
            case FindType:   fields << typeFields(d->m_registration); break;
            case FindFlight: fields << d->m_flight; break;
            case FindSelcal: fields << d->m_selcal; break;
            default:
                fields << icao << d->m_registration << d->m_flight << d->m_mapName
                       << d->m_selcal << typeFields(d->m_registration);
                if (byReg && byReg->contains(d->m_registration))
                {
                    const AircraftInformation *info = byReg->value(d->m_registration);
                    fields << info->m_operator << info->m_operatorICAO << info->m_owner;
                }
                break;
            }
        }

        for (const QString& field : fields)
        {
            if (!field.isEmpty() && field.contains(text, Qt::CaseInsensitive))
            {
                m_findRow = row;
                view->selectRow(row);
                view->scrollTo(proxy->index(row, 0), QAbstractItemView::PositionAtCenter);
                return true;
            }
        }
    }
    return false;
}

// Typing searches from the top, so the match follows what has been typed so far
void AircraftGUI::on_find_textChanged(const QString& text)
{
    m_findRow = -1;
    if (text.isEmpty())
    {
        m_findTimer.stop();
        ui->find->setStyleSheet("");
        return;
    }
    // Wait for a pause in typing. Each match selects a row, and selecting reloads the
    // message tables, the flight profile and the photograph, so searching on every
    // keystroke would do all of that for each half-typed registration
    m_findTimer.start(300);
}

// Enter steps on to the next match
void AircraftGUI::on_find_returnPressed()
{
    const QString text = ui->find->text();
    ui->find->setStyleSheet(findFrom(m_findRow, text) ? "" : "QLineEdit { color: red; }");
}

void AircraftGUI::updateHistoryTables()
{
    m_updatingHistoryTables = true;
    // "Callsign", not "Flight": this is what the aircraft transmits - BAW31, TOM3TW -
    // which is not the flight number a passenger is sold, and the two often differ
    static const QStringList flightHeadings =
        { "Callsign", "From", "To", "First seen", "Last seen", "Msgs" };
    static const QStringList aircraftHeadings =
        { "Reg", "From", "To", "First seen", "Last seen", "Msgs" };
    // The tables in the .ui carry these on each column; these two are built here, so
    // they had none at all
    static const QStringList flightTips = {
        "Callsign the flight was flown under",
        "Departure airport, from a message where one said so, otherwise from the callsign"
            " to route database",
        "Arrival airport, from a message where one said so, otherwise from the callsign"
            " to route database",
        "When the flight was first heard",
        "When the flight was last heard",
        "Messages received for this flight" };
    static const QStringList aircraftTips = {
        "Registration, or the ICAO 24 bit address where no registration is known",
        "Departure airport of the last flight heard",
        "Arrival airport of the last flight heard",
        "When the aircraft was first heard",
        "When the aircraft was last heard",
        "Messages received from this aircraft" };

    // QTableWidgetItem sorts on the display role, so the dates have to sort correctly as
    // TEXT. This format does - year first, zero padded throughout - which is why it is
    // used here rather than a friendlier local format.
    const QString dateFormat = "yyyy-MM-dd hh:mm";
    // The widest each column is likely to hold, for its initial width
    const QStringList flightSamples = { "RYR33KQW", "WWWW", "WWWW",
        dateTimeSample(dateFormat), dateTimeSample(dateFormat), "10000" };
    const QStringList aircraftSamples = { "G-ABCDEF", "WWWW", "WWWW",
        dateTimeSample(dateFormat), dateTimeSample(dateFormat), "10000" };

    auto prepare = [](QTableWidget *table, const QStringList& headings,
                      const QStringList& tips, const QStringList& samples)
    {
        if (table->columnCount() == 0)
        {
            table->setColumnCount(headings.size());
            table->setHorizontalHeaderLabels(headings);
            for (int col = 0; (col < samples.size()) && (col < table->columnCount()); col++) {
                table->setColumnWidth(col, columnWidthFor(table, col, samples[col]));
            }
            for (int col = 0; (col < tips.size()) && (col < table->columnCount()); col++)
            {
                if (QTableWidgetItem *header = table->horizontalHeaderItem(col)) {
                    header->setToolTip(tips[col]);
                }
            }
            table->horizontalHeader()->setSectionsMovable(true);
            table->verticalHeader()->setVisible(false);
        }
        // Sorting has to be off while the rows are replaced, or Qt re-sorts after each
        // cell is set and the rows shuffle out from under the loop
        table->setSortingEnabled(false);
        table->setRowCount(0);
    };

    auto fill = [](QTableWidget *table, int row, const QStringList& values)
    {
        for (int col = 0; col < values.size(); col++) {
            table->setItem(row, col, new QTableWidgetItem(values[col]));
        }
    };

    // The tracker flushes every 300 ms and every flush that touches a flight rebuilds
    // these tables, so a selection the user has just made would be wiped within a frame
    // or two of making it. Remember it and put it back.
    const quint64 keepSelected = m_selectedFlightId;

    // Past Flights: everything this aircraft has operated
    prepare(ui->pastFlights, flightHeadings, flightTips, flightSamples);
    if (m_selectedAircraftId != 0)
    {
        int row = 0;
        for (int i = 0; i < m_flightModel->rowCount(); i++)
        {
            const FlightDisplay *f = m_flightModel->flightAt(i);
            if (!f || (f->m_aircraftId != m_selectedAircraftId)) {
                continue;
            }
            ui->pastFlights->setRowCount(row + 1);
            fill(ui->pastFlights, row,
                 { f->m_flight, f->m_departure, f->m_arrival,
                   f->m_firstSeen.toString(dateFormat), f->m_lastSeen.toString(dateFormat),
                   QString::number(f->m_messages) });
            ui->pastFlights->item(row, 0)->setData(Qt::UserRole, f->m_id);
            row++;
        }
    }
    // Re-enabling sorting re-sorts by whatever the header says, which keeps a sort the
    // user picked. Most recent first only when the table is filled for a different
    // aircraft - forcing it on every refill undid their choice within a second.
    ui->pastFlights->setSortingEnabled(true);
    if (m_selectedAircraftId != m_pastFlightsAircraftId)
    {
        ui->pastFlights->sortByColumn(4, Qt::DescendingOrder);
        m_pastFlightsAircraftId = m_selectedAircraftId;
    }

    // After the sort, because sorting moves the rows around
    if (keepSelected)
    {
        for (int row = 0; row < ui->pastFlights->rowCount(); row++)
        {
            const QTableWidgetItem *first = ui->pastFlights->item(row, 0);
            if (first && (first->data(Qt::UserRole).toULongLong() == keepSelected))
            {
                ui->pastFlights->selectRow(row);
                break;
            }
        }
    }

    // Past Aircraft: everything that has operated this flight number
    prepare(ui->pastAircraft, aircraftHeadings, aircraftTips, aircraftSamples);
    if (!m_selectedFlightNumber.isEmpty())
    {
        int row = 0;
        for (int i = 0; i < m_flightModel->rowCount(); i++)
        {
            const FlightDisplay *f = m_flightModel->flightAt(i);
            if (!f || f->m_flight.compare(m_selectedFlightNumber, Qt::CaseInsensitive)) {
                continue;
            }
            ui->pastAircraft->setRowCount(row + 1);
            fill(ui->pastAircraft, row,
                 { f->m_reg, f->m_departure, f->m_arrival,
                   f->m_firstSeen.toString(dateFormat), f->m_lastSeen.toString(dateFormat),
                   QString::number(f->m_messages) });
            row++;
        }
    }
    ui->pastAircraft->setSortingEnabled(true);
    if (m_selectedFlightNumber.compare(m_pastAircraftFlightNumber, Qt::CaseInsensitive))
    {
        ui->pastAircraft->sortByColumn(4, Qt::DescendingOrder);
        m_pastAircraftFlightNumber = m_selectedFlightNumber;
    }

    // These two tabs live in messageTabs, NOT aircraftTabs - asking aircraftTabs for
    // them returns -1, and setTabVisible(-1) does nothing at all, which is why they
    // used to stay visible whatever was selected.
    ui->messageTabs->setTabVisible(ui->messageTabs->indexOf(ui->pastFlightsTab),
                                   m_selectedAircraftId != 0);
    ui->messageTabs->setTabVisible(ui->messageTabs->indexOf(ui->pastAircraftTab),
                                   !m_selectedFlightNumber.isEmpty());
    m_updatingHistoryTables = false;
}

// Selecting one of the aircraft's past flights charts its profile and draws its track
// on the map. Nothing is selected once the table is rebuilt, which returns both to the
// flight the aircraft is on now.
void AircraftGUI::pastFlightSelectionChanged()
{
    if (m_updatingHistoryTables) {
        return;
    }

    quint64 flightId = 0;
    QString title;
    const QList<QTableWidgetItem *> selected = ui->pastFlights->selectedItems();
    if (!selected.isEmpty())
    {
        const QTableWidgetItem *first = ui->pastFlights->item(selected.first()->row(), 0);
        if (first)
        {
            flightId = first->data(Qt::UserRole).toULongLong();
            title = first->text();
        }
    }
    // With no past flight selected the messages widen back out to the whole aircraft,
    // while the track and the chart go back to the flight it is on now
    m_selectedFlightId = flightId;
    applyMessageFilter();

    if (flightId == 0)
    {
        for (int i = 0; i < m_aircraftModel->rowCount(); i++)
        {
            const AircraftDisplay *d = m_aircraftModel->aircraftAt(i);
            if (d && (d->m_id == m_selectedAircraftId))
            {
                flightId = d->m_currentFlightId;
                title = d->m_flight.isEmpty() ? d->m_mapName : d->m_flight;
                break;
            }
        }
    }
    watchFlight(flightId, title);
}

void AircraftGUI::aircraftSelectionChanged()
{
    if (m_ignoreSelectionChanges) {
        return;
    }
    QTableView *aircraftView = showingArchive() ? ui->oldAircraft : ui->aircraft;
    const bool haveSelection = !aircraftView->selectionModel()->selectedRows().isEmpty();

    // Nothing selected here and the documents are not ours to change: a selection model
    // re-emitting with nothing in it says nothing about what the other table holds, and
    // acting on it is what stole the selection from a flight the user had just clicked.
    // Sorting by a column whose value changes - Docs, as documents arrive - moves rows
    // under the selection and makes that happen every few seconds.
    if (!haveSelection && (m_selectionOwner != SelectionAircraft)) {
        return;
    }
    // Whatever happens below re-derives the selection from the table, so a merge's
    // survivor no longer needs putting back
    m_reselectAircraftId = 0;
    m_reselectFlightId = 0;
    if (haveSelection)
    {
        m_selectionOwner = SelectionAircraft;
        // Only one table may hold a selection, or the loser goes on re-asserting itself.
        // Blocked, or clearing it would re-enter through flightSelectionChanged(). Its
        // current index goes as well: left behind, Qt selects the next row in its place
        // when that row leaves the view, which handed the selection to a flight nobody
        // had clicked.
        QTableView *flightView = showingArchive() ? ui->oldFlights : ui->flights;
        const QSignalBlocker blocker(flightView->selectionModel());
        flightView->selectionModel()->clear();
    }
    else
    {
        m_selectionOwner = SelectionNone;
    }

    m_selectedAircraftId = 0;
    m_selectedRegistration.clear();
    m_selectedIcao = 0;
    // An airframe is selected, so "what else has flown this number" no longer has a
    // number to be about
    m_selectedFlightId = 0;
    m_selectedFlightNumber.clear();
    quint64 currentFlightId = 0;
    QString title;

    QTableView *view = showingArchive() ? ui->oldAircraft : ui->aircraft;
    ActiveFilterProxy *proxy = showingArchive() ? m_oldAircraftProxy : m_aircraftProxy;
    QModelIndexList selected = view->selectionModel()->selectedRows();
    if (!selected.isEmpty())
    {
        int sourceRow = proxy->mapToSource(selected.first()).row();
        const AircraftDisplay *d = m_aircraftModel->aircraftAt(sourceRow);
        if (d)
        {
            m_selectedAircraftId = d->m_id;
            m_selectedRegistration = d->m_registration;
            m_selectedIcao = d->m_icao;
            currentFlightId = d->m_currentFlightId;
            title = d->m_flight.isEmpty() ? d->m_mapName : d->m_flight;
        }
    }
    applyMessageFilter();
    updateHistoryTables();
    updatePhoto(m_selectedRegistration, m_selectedIcao);
    watchFlight(currentFlightId, title);
}

void AircraftGUI::flightSelectionChanged()
{
    if (m_ignoreSelectionChanges) {
        return;
    }
    QTableView *flightView = showingArchive() ? ui->oldFlights : ui->flights;
    const bool haveSelection = !flightView->selectionModel()->selectedRows().isEmpty();

    // The same rule as aircraftSelectionChanged(), for the same reason
    if (!haveSelection && (m_selectionOwner != SelectionFlight)) {
        return;
    }
    m_reselectAircraftId = 0;
    m_reselectFlightId = 0;
    if (haveSelection)
    {
        m_selectionOwner = SelectionFlight;
        QTableView *aircraftView = showingArchive() ? ui->oldAircraft : ui->aircraft;
        const QSignalBlocker blocker(aircraftView->selectionModel());
        aircraftView->selectionModel()->clear();
    }
    else
    {
        m_selectionOwner = SelectionNone;
    }

    m_selectedFlightId = 0;
    m_selectedFlightNumber.clear();
    m_selectedAircraftId = 0;
    quint64 watchId = 0;
    QString title;
    QString registration;

    QTableView *view = showingArchive() ? ui->oldFlights : ui->flights;
    ActiveFilterProxy *proxy = showingArchive() ? m_oldFlightProxy : m_flightProxy;
    QModelIndexList selected = view->selectionModel()->selectedRows();
    if (!selected.isEmpty())
    {
        int sourceRow = proxy->mapToSource(selected.first()).row();
        const FlightDisplay *d = m_flightModel->flightAt(sourceRow);
        if (d)
        {
            m_selectedFlightId = d->m_id;
            m_selectedFlightNumber = d->m_flight;
            watchId = d->m_id;
            title = d->m_flight.isEmpty() ? d->m_reg : d->m_flight;
            registration = d->m_reg;
        }
    }
    applyMessageFilter();
    updateHistoryTables();
    updatePhoto(registration, 0);
    watchFlight(watchId, title);
}

// Puts the selection back on the survivor of a merge, once the tracker has sent its row as
// the merge left it - the tracker always does, in the flush after the merge, and only then
// is the row in the view it is going to stay in. Called with that batch, by
// trackerAircraftUpdated() and trackerFlightsUpdated().
// The merged-away row was removed, and the view's selection with it. Done quietly, with
// the selection handlers ignoring it: trackerAircraftMerged() and trackerFlightsMerged()
// moved everything the selection drives across already, and going through the handlers
// would redo it from nothing - restarting the profile and redrawing the track on the Map
// for what is still the same flight. Returns whether it reselected.
bool AircraftGUI::reselectMerged()
{
    const bool flights = (m_selectionOwner == SelectionFlight);
    const quint64 id = flights ? m_reselectFlightId : m_reselectAircraftId;
    const quint64 selectedId = flights ? m_selectedFlightId : m_selectedAircraftId;
    if ((id == 0) || (m_selectionOwner == SelectionNone) || (selectedId != id))
    {
        // Nothing to do, or something else has been selected since
        m_reselectAircraftId = 0;
        m_reselectFlightId = 0;
        return false;
    }

    const int sourceRow = flights ? m_flightModel->rowOf(id) : m_aircraftModel->rowOf(id);
    if (sourceRow < 0) {
        return false;       // Not delivered yet - tried again when it is
    }
    m_reselectAircraftId = 0;
    m_reselectFlightId = 0;

    struct Place {
        QWidget *m_tab;
        QTableView *m_view;
        ActiveFilterProxy *m_proxy;
    };
    const QList<Place> places = flights
        ? QList<Place>{ { ui->flightsTab, ui->flights, m_flightProxy },
                        { ui->oldFlightsTab, ui->oldFlights, m_oldFlightProxy } }
        : QList<Place>{ { ui->aircraftTab, ui->aircraft, m_aircraftProxy },
                        { ui->oldAircraftTab, ui->oldAircraft, m_oldAircraftProxy } };
    const QModelIndex sourceIndex = flights ? m_flightModel->index(sourceRow, 0)
                                            : m_aircraftModel->index(sourceRow, 0);
    QWidget *shown = ui->aircraftTabs->currentWidget();
    const bool showingThisKind = (shown == places[0].m_tab) || (shown == places[1].m_tab);

    m_ignoreSelectionChanges = true;
    for (const Place& place : places)
    {
        const QModelIndex index = place.m_proxy->mapFromSource(sourceIndex);
        if (index.isValid())
        {
            // The survivor can be live where the merged-away one was archived, or the
            // other way round. If the other of the pair is on show, bring this one
            // forward, as selectRow() does - but not from some other tab altogether,
            // which would be taking the user somewhere they did not go.
            if (showingThisKind && (shown != place.m_tab)) {
                ui->aircraftTabs->setCurrentWidget(place.m_tab);
            }
            place.m_view->selectRow(index.row());
        }
        else
        {
            // With the current index, which would otherwise make Qt select a neighbour
            // when its row left the view
            place.m_view->selectionModel()->clear();
        }
    }
    m_ignoreSelectionChanges = false;

    // What the survivor brings that the merged-away one may not have had
    if (flights)
    {
        const FlightDisplay *d = m_flightModel->flightAt(sourceRow);
        m_selectedFlightNumber = d->m_flight;
        const QString title = d->m_flight.isEmpty() ? d->m_reg : d->m_flight;
        if ((m_profileFlightId == id) && (title != m_profileTitle))
        {
            m_profileTitle = title;
            plotProfileChart();
        }
        updatePhoto(d->m_reg, 0);
    }
    else
    {
        // The watched flight is left alone: flights keep their ids when their aircraft is
        // absorbed, so the tracker goes on sending the same one
        const AircraftDisplay *d = m_aircraftModel->aircraftAt(sourceRow);
        m_selectedRegistration = d->m_registration;
        m_selectedIcao = d->m_icao;
        updatePhoto(m_selectedRegistration, m_selectedIcao);
    }
    applyMessageFilter();
    updateHistoryTables();
    return true;
}

// The id of the aircraft or flight selected in one of the four views, or 0 for none
quint64 AircraftGUI::selectedIdIn(bool flights, bool archive) const
{
    QTableView *view = flights ? (archive ? ui->oldFlights : ui->flights)
                               : (archive ? ui->oldAircraft : ui->aircraft);
    const QModelIndexList selected = view->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return 0;
    }
    if (flights)
    {
        const ActiveFilterProxy *proxy = archive ? m_oldFlightProxy : m_flightProxy;
        const FlightDisplay *d = m_flightModel->flightAt(proxy->mapToSource(selected.first()).row());
        return d ? d->m_id : 0;
    }
    const ActiveFilterProxy *proxy = archive ? m_oldAircraftProxy : m_aircraftProxy;
    const AircraftDisplay *d = m_aircraftModel->aircraftAt(proxy->mapToSource(selected.first()).row());
    return d ? d->m_id : 0;
}

AircraftGUI::ViewSelections AircraftGUI::selectedIds(bool flights) const
{
    ViewSelections ids;
    ids.m_live = selectedIdIn(flights, false);
    ids.m_archive = selectedIdIn(flights, true);
    return ids;
}

// Puts each of a pair of views' selections back on the aircraft or flight it was made on,
// after the tracker's changes have been applied to their model with the selection handlers
// ignoring what that did. Taken by id beforehand, as selectedIds().
//
// A selection belongs to an aircraft or a flight, not to a row. Rows move under it as the
// view re-sorts, which the views' persistent indexes take care of, but they also leave a
// view - archived, back from the archive, merged, discarded - and then Qt, keeping one row
// selected in a single selection view, selects the row next to the one that went. Taken as
// the user's choice, that put an unrelated aircraft's documents, photograph and profile on
// screen, and it happened just as readily in a view whose selection had been cleared but
// whose current index had not, which was how the other table came to take the selection.
// Applied in the middle of the model's removal, it also read the model while its rows and
// the proxy's mapping disagreed.
//
// So a view's selection is put back on its own aircraft or flight if that is still in the
// view, and otherwise cleared with the current index, and a row Qt selected in a view that
// had nothing is cleared again. The aircraft or flight the documents are being shown for
// moves to the other view of the pair along with its row, bringing that tab forward if the
// pair is on show, as reselectMerged() and selectRow() do.
void AircraftGUI::keepSelections(bool flights, const ViewSelections& before)
{
    struct Place {
        QWidget *m_tab;
        QTableView *m_view;
        ActiveFilterProxy *m_proxy;
        quint64 m_was;
        QModelIndex m_keep;
    };
    Place places[2] = {
        { flights ? ui->flightsTab : ui->aircraftTab, flights ? ui->flights : ui->aircraft,
          flights ? m_flightProxy : m_aircraftProxy, before.m_live, QModelIndex() },
        { flights ? ui->oldFlightsTab : ui->oldAircraftTab, flights ? ui->oldFlights : ui->oldAircraft,
          flights ? m_oldFlightProxy : m_oldAircraftProxy, before.m_archive, QModelIndex() }
    };
    auto indexIn = [this, flights](const Place& place, quint64 id) {
        const int sourceRow = (id == 0) ? -1 : (flights ? m_flightModel->rowOf(id) : m_aircraftModel->rowOf(id));
        if (sourceRow < 0) {
            return QModelIndex();
        }
        return place.m_proxy->mapFromSource(flights ? m_flightModel->index(sourceRow, 0)
                                                    : m_aircraftModel->index(sourceRow, 0));
    };

    for (Place& place : places) {
        place.m_keep = indexIn(place, place.m_was);
    }
    const bool owned = (m_selectionOwner == (flights ? SelectionFlight : SelectionAircraft));
    const quint64 ownedId = flights ? m_selectedFlightId : m_selectedAircraftId;
    int followedTo = -1;
    for (int i = 0; owned && (ownedId != 0) && (i < 2); i++)
    {
        if ((places[i].m_was == ownedId) && !places[i].m_keep.isValid())
        {
            const QModelIndex index = indexIn(places[1 - i], ownedId);
            if (index.isValid())
            {
                places[1 - i].m_keep = index;
                followedTo = 1 - i;
            }
        }
    }

    m_ignoreSelectionChanges = true;
    QWidget *shown = ui->aircraftTabs->currentWidget();
    if ((followedTo >= 0) && ((shown == places[0].m_tab) || (shown == places[1].m_tab))) {
        ui->aircraftTabs->setCurrentWidget(places[followedTo].m_tab);
    }
    for (const Place& place : places)
    {
        QItemSelectionModel *selection = place.m_view->selectionModel();
        if (place.m_keep.isValid())
        {
            // Usually it is still selected where it was, and there is nothing to do
            if ((selection->currentIndex().row() != place.m_keep.row())
                || !selection->isRowSelected(place.m_keep.row(), QModelIndex())) {
                selection->setCurrentIndex(place.m_keep, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        }
        else if ((place.m_was != 0) || selection->hasSelection())
        {
            selection->clear();
        }
    }
    m_ignoreSelectionChanges = false;
}

// The aircraft or flight the documents were being shown for has gone for good, so there is
// nothing left for the selection to follow. Its row went without Qt being allowed to select
// a neighbour in its place, so this is the deselection the handlers used to do then.
void AircraftGUI::dropSelection()
{
    clearSelectionState();
    applyMessageFilter();
    // The tracker half of watchFlight(0), which clearSelectionState() leaves out
    tracker()->getInputMessageQueue()->push(AircraftTracker::MsgWatchFlight::create(0));
}

// Tell the tracker which flight's profile to send us
void AircraftGUI::watchFlight(quint64 flightId, const QString& title)
{
    m_profileFlightId = flightId;
    m_profileTitle = title;
    m_profileTimes.clear();
    m_profileAltFt.clear();
    m_profileSpeedKts.clear();
    plotProfileChart();
    tracker()->getInputMessageQueue()->push(AircraftTracker::MsgWatchFlight::create(flightId));
}

void AircraftGUI::trackerProfileUpdated(quint64 flightId, const QList<qint64>& times, const QList<float>& altitudeFt, const QList<float>& speedKts)
{
    if (flightId != m_profileFlightId) {
        return;
    }
    m_profileTimes = times;
    m_profileAltFt = altitudeFt;
    m_profileSpeedKts = speedKts;
    plotProfileChart();
}

// Selecting a row shows its document in the pane underneath. With nothing selected the
// pane is cleared: the previous message is not the selected one, and leaving it there
// reads as though it is
void AircraftGUI::messageSelectionChanged(QTableView *view, MessageFilterProxy *proxy, AcarsMessageView *pane)
{
    const QModelIndexList selected = view->selectionModel()->selectedRows();
    const MessageRow *row = selected.isEmpty() ? nullptr : proxy->rowAt(selected.first());
    if (row) {
        pane->setPlainText(row->m_text);
    } else {
        pane->clear();
    }
}

// The Logons tab is the Documents tab over a different set of rows, so it behaves the
// same way
void AircraftGUI::logonSelectionChanged()
{
    messageSelectionChanged(ui->logons, m_logonProxy, ui->logonText);
}

void AircraftGUI::documentSelectionChanged()
{
    messageSelectionChanged(ui->documents, m_documentProxy, ui->documentText);
}

// Fetch and display the planespotters.net photo for an aircraft
void AircraftGUI::updatePhoto(const QString& registration, quint32 icao)
{
    QString key = !registration.isEmpty() ? registration
        : (icao ? QString("%1").arg(icao, 6, 16, QChar('0')).toUpper() : QString());
    if (key == m_photoKey) {
        return;
    }
    m_photoKey = key;

    // Hide the previous aircraft's photo
    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->photo->setPixmap(QPixmap());
    ui->flightDetails->setVisible(false);
    m_photoLink.clear();

    if (key.isEmpty())
    {
        // updateAircraftInfo() is what empties the details table, and it is not reached
        // from here, so the last airframe's details stayed beside nothing selected
        ui->aircraftInfo->setRowCount(0);
        return;
    }

    updatePhotoText(registration, icao);
    updateAircraftInfo(registration, icao);
    if (!registration.isEmpty()) {
        m_planeSpotters.getAircraftPhotoByRegistration(registration);
    } else {
        m_planeSpotters.getAircraftPhoto(key);
    }
}

void AircraftGUI::updatePhotoText(const QString& registration, quint32 icao)
{
    ui->photoHeader->setText(registration.isEmpty()
        ? QString("%1").arg(icao, 6, 16, QChar('0')).toUpper() : registration);

    ui->photoFlag->setPixmap(QPixmap());
    ui->flightDetails->setPixmap(QPixmap());

    QSharedPointer<const QHash<QString, AircraftInformation *>> aircraftInfoDB = OsnDB::getAircraftInformationByReg();
    if (aircraftInfoDB && aircraftInfoDB->contains(registration))
    {
        const AircraftInformation *aircraftInfo = aircraftInfoDB->value(registration);

        QString flag = aircraftInfo->getFlag();
        if (!flag.isEmpty())
        {
            QIcon *icon = AircraftInformation::getFlagIcon(flag);
            if (icon)
            {
                QList<QSize> sizes = icon->availableSizes();
                if (sizes.size() > 0) {
                    ui->photoFlag->setPixmap(icon->pixmap(sizes[0]));
                }
            }
        }

        // The airline's logo under the photograph. The manufacturer, model, owner,
        // operator and registration date that used to sit beside the photograph are
        // gone - the aircraftInfo table next to it lists all five, so they were on
        // screen twice.
        QIcon *airlineIcon = AircraftInformation::getAirlineIcon(aircraftInfo->m_operatorICAO);
        if (airlineIcon)
        {
            QList<QSize> sizes = airlineIcon->availableSizes();
            if (sizes.size() > 0) {
                ui->flightDetails->setPixmap(airlineIcon->pixmap(sizes[0]));
            }
        }
    }
}

void AircraftGUI::aircraftPhoto(const PlaneSpottersPhoto *photo)
{
    // The download takes a while, and the selection can move on while it runs. Checking
    // only that SOMETHING is selected would put aircraft A's photograph next to aircraft
    // B's details, which reads as a wrong answer rather than a slow one.
    if (!photo->m_pixmap.isNull() && !m_photoKey.isEmpty() && (photo->m_id == m_photoKey))
    {
        ui->photo->setPixmap(photo->m_pixmap);
        ui->photo->setToolTip(QString("Photographer: %1").arg(photo->m_photographer)); // Required by terms of use
        ui->photoHeader->setVisible(true);
        ui->photoFlag->setVisible(true);
        ui->photo->setVisible(true);
        ui->flightDetails->setVisible(true);
        m_photoLink = photo->m_link;
    }
}

void AircraftGUI::photoClicked()
{
    // Photo needs to link back to PlaneSpotters, as per terms of use
    if (!m_photoLink.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_photoLink));
    }
}

// ---- Context menus ----

// Jump to a flight in the Flights table, from the aircraft that is flying it
// Select the first row the test accepts, looking in the live table and then in the
// archive, and bring forward whichever tab holds it. The archive matters: an aircraft
// that has just gone quiet is still linked from its Map popup and from other rows.
bool AircraftGUI::selectRow(bool flights, const std::function<bool(int sourceRow)>& matches)
{
    struct Place {
        QWidget *m_tab;
        QTableView *m_view;
        ActiveFilterProxy *m_proxy;
    };
    const QList<Place> places = flights
        ? QList<Place>{ { ui->flightsTab, ui->flights, m_flightProxy },
                        { ui->oldFlightsTab, ui->oldFlights, m_oldFlightProxy } }
        : QList<Place>{ { ui->aircraftTab, ui->aircraft, m_aircraftProxy },
                        { ui->oldAircraftTab, ui->oldAircraft, m_oldAircraftProxy } };

    for (const Place& place : places)
    {
        for (int row = 0; row < place.m_proxy->rowCount(); row++)
        {
            const int sourceRow = place.m_proxy->mapToSource(place.m_proxy->index(row, 0)).row();
            if (matches(sourceRow))
            {
                ui->aircraftTabs->setCurrentWidget(place.m_tab);
                place.m_view->selectRow(row);
                place.m_view->scrollTo(place.m_proxy->index(row, 0), QAbstractItemView::PositionAtCenter);
                return true;
            }
        }
    }
    return false;
}

void AircraftGUI::selectFlight(const QString& flight)
{
    selectRow(true, [this, &flight](int sourceRow) {
        const FlightDisplay *d = m_flightModel->flightAt(sourceRow);
        return d && !d->m_flight.compare(flight, Qt::CaseInsensitive);
    });
}

void AircraftGUI::selectFlightById(quint64 flightId)
{
    selectRow(true, [this, flightId](int sourceRow) {
        const FlightDisplay *d = m_flightModel->flightAt(sourceRow);
        return d && (d->m_id == flightId);
    });
}

// And the other way: the airframe that flew a flight
void AircraftGUI::selectAircraft(quint64 aircraftId)
{
    selectRow(false, [this, aircraftId](int sourceRow) {
        const AircraftDisplay *d = m_aircraftModel->aircraftAt(sourceRow);
        return d && (d->m_id == aircraftId);
    });
}

// Whether the Map still holds this aircraft, and so whether finding it there can work.
// Asked of the tracker's own expiry rather than recomputed here, so this cannot drift
// away from what the Map was actually told: an aircraft comes off the Map when the
// position it was placed at stops being believable - after "Keep ADS-B position for" or
// "Keep ACARS position for" depending on where the position came from - and when it is
// archived, which takes it off whatever its position said.
bool AircraftGUI::aircraftOnMap(quint64 aircraftId) const
{
    const AircraftDisplay *d = m_aircraftModel->aircraftById(aircraftId);
    if (!d) {
        return false;           // Discarded entirely, so certainly not on the Map
    }
    return d->m_active
        && d->m_mapAvailableUntil.isValid()
        && (d->m_mapAvailableUntil > QDateTime::currentDateTime());
}

void AircraftGUI::showContextMenu(QWidget *parent, const QString& cellText, const QString& mapName,
                                  QPoint globalPos, const QString& showFlight, quint64 showAircraftId,
                                  quint64 mapAircraftId)
{
    QMenu *menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);     // Off by default, and the disabled Find explains itself

    if (!cellText.isEmpty())
    {
        QAction *copyAction = new QAction(QString("Copy %1").arg(cellText), menu);
        connect(copyAction, &QAction::triggered, this, [cellText]() {
            QGuiApplication::clipboard()->setText(cellText);
        });
        menu->addAction(copyAction);
    }
    if (!mapName.isEmpty())
    {
        QAction *findAction = new QAction(QString("Find %1 on map").arg(mapName), menu);
        connect(findAction, &QAction::triggered, this, [mapName]() {
            FeatureWebAPIUtils::mapFind(mapName);
        });
        // Shown but disabled once the aircraft has come off the Map, rather than hidden:
        // the menu stays the same shape wherever it is opened, and the reason is worth
        // saying - otherwise Find silently does nothing and looks broken
        if ((mapAircraftId != 0) && !aircraftOnMap(mapAircraftId))
        {
            findAction->setEnabled(false);
            findAction->setToolTip(QString("%1 is no longer on the map - its position was"
                                           " last updated too long ago").arg(mapName));
        }
        menu->addAction(findAction);
    }
    if (!showFlight.isEmpty())
    {
        QAction *action = new QAction(QString("Display flight %1").arg(showFlight), menu);
        connect(action, &QAction::triggered, this, [this, showFlight]() {
            selectFlight(showFlight);
        });
        menu->addAction(action);
    }
    if (showAircraftId != 0)
    {
        QAction *action = new QAction("Display aircraft", menu);
        connect(action, &QAction::triggered, this, [this, showAircraftId]() {
            selectAircraft(showAircraftId);
        });
        menu->addAction(action);
    }
    if (menu->actions().isEmpty())
    {
        delete menu;
        return;
    }
    menu->popup(globalPos);
}

void AircraftGUI::viewContextMenu(QTableView *view, bool aircraftView, QPoint pos)
{
    QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel *>(view->model());
    QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    QString cellText = index.data(Qt::DisplayRole).toString();
    QString mapName;
    QString showFlight;
    quint64 showAircraftId = 0;
    quint64 mapAircraftId = 0;
    if (aircraftView)
    {
        const AircraftDisplay *d = m_aircraftModel->aircraftAt(proxy->mapToSource(index).row());
        if (d)
        {
            mapName = d->m_mapName;
            showFlight = d->m_flight;
            mapAircraftId = d->m_id;
        }
    }
    else
    {
        const FlightDisplay *d = m_flightModel->flightAt(proxy->mapToSource(index).row());
        if (d)
        {
            mapName = d->m_reg;
            showAircraftId = d->m_aircraftId;
            mapAircraftId = d->m_aircraftId;
        }
    }
    showContextMenu(view, cellText, mapName, view->viewport()->mapToGlobal(pos),
                    showFlight, showAircraftId, mapAircraftId);
}

void AircraftGUI::messageContextMenu(QTableView *view, MessageFilterProxy *proxy, QPoint pos)
{
    const QModelIndex index = view->indexAt(pos);
    const MessageRow *row = proxy->rowAt(index);
    if (!row) {
        return;
    }
    showContextMenu(view, index.data().toString(), row->m_mapName, view->viewport()->mapToGlobal(pos),
                    QString(), 0, row->m_aircraftId);
}

void AircraftGUI::tableContextMenu(QTableWidget *table, QPoint pos)
{
    QTableWidgetItem *item = table->itemAt(pos);
    if (!item) {
        return;
    }
    // The statistics table shares this menu but carries no map name, and its first
    // column is a label rather than something to find
    QTableWidgetItem *first = table->item(item->row(), 0);
    const QString mapName = first ? first->data(Qt::UserRole + 3).toString() : QString();
    const quint64 mapAircraftId = first ? first->data(Qt::UserRole + 1).toULongLong() : 0;
    showContextMenu(table, item->text(), mapName, table->viewport()->mapToGlobal(pos),
                    QString(), 0, mapAircraftId);
}

// ---- Charts ----

void AircraftGUI::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();
    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);
    m_chart->legend()->setAlignment(Qt::AlignRight);

    m_chartXAxis = new QDateTimeAxis();
    m_chartXAxis->setFormat("hh:mm:ss");
    // ADS-B runs at hundreds of messages a second where the other protocols manage a few,
    // so on a shared axis every other series lay flat along the bottom. It, and the
    // total it dominates, get an axis of their own on the right.
    m_chartYAxis = new QValueAxis();
    m_chartYAxis->setTitleText("Msgs/s");
    m_chartADSBYAxis = new QValueAxis();
    m_chartADSBYAxis->setTitleText("ADS-B and total msgs/s");

    m_chart->addAxis(m_chartXAxis, Qt::AlignBottom);
    m_chart->addAxis(m_chartYAxis, Qt::AlignLeft);
    m_chart->addAxis(m_chartADSBYAxis, Qt::AlignRight);

    for (int i = 0; i < CHART_SERIES; i++)
    {
        m_chartSeries[i] = new QLineSeries();
        // One per protocol, named as everywhere else, and then the total. A fixed list
        // of names here went out of step the moment a protocol was added.
        m_chartSeries[i]->setName((i < AircraftReport::ProtocolCount)
            ? AircraftTracker::protocolName(i) : QString("Total"));
        m_chart->addSeries(m_chartSeries[i]);
        m_chartSeries[i]->attachAxis(m_chartXAxis);
        m_chartSeries[i]->attachAxis(onADSBAxis(i) ? m_chartADSBYAxis : m_chartYAxis);
    }

    const QDateTime now = QDateTime::currentDateTime();
    m_chartXAxis->setMin(now.addSecs(-60));
    m_chartXAxis->setMax(now);
    m_chartYAxis->setRange(0.0, 10.0);
    m_chartADSBYAxis->setRange(0.0, 10.0);
    m_chartYZoomed = false;

    ui->chart->setChart(m_chart);

    // Click a legend entry to show/hide its series
    const auto markers = m_chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        connect(marker, &QLegendMarker::clicked, this, &AircraftGUI::legendMarkerClicked);
    }

    delete oldChart;
}

void AircraftGUI::legendMarkerClicked()
{
    QLegendMarker *marker = qobject_cast<QLegendMarker *>(sender());
    if (marker)
    {
        marker->series()->setVisible(!marker->series()->isVisible());
        marker->setVisible(true);
        fitChartYAxes();
    }
}

bool AircraftGUI::onADSBAxis(int series)
{
    return (series == AircraftReport::ADSB) || (series == AircraftReport::ProtocolCount);
}

// Each axis fits what is plotted against it now, falling as well as rising. Only ever
// growing left one burst flattening the chart for as long as the feature ran. Every
// retained point counts, as the x axis normally starts at the oldest, and a series hidden
// from the legend does not. Once the user zooms the y axes they are left where they are.
void AircraftGUI::fitChartYAxes()
{
    if (m_chartYZoomed) {
        return;
    }

    qreal maxRate = 0.0;
    qreal maxADSBRate = 0.0;
    for (int i = 0; i < CHART_SERIES; i++)
    {
        if (m_chartSeries[i]->isVisible())
        {
            qreal& max = onADSBAxis(i) ? maxADSBRate : maxRate;
            for (int j = 0; j < m_chartSeries[i]->count(); j++) {
                max = std::max(max, m_chartSeries[i]->at(j).y());
            }
        }
    }
    m_chartYAxis->setRange(0.0, std::ceil(maxRate) + 1.0);
    m_chartADSBYAxis->setRange(0.0, std::ceil(maxADSBRate) + 1.0);
}

// Back to following the data: every retained point in view, the y axes fitted to them
void AircraftGUI::resetChartZoom()
{
    m_chartYZoomed = false;
    fitChartYAxes();
    QLineSeries *series = m_chartSeries[0];
    if (series->count() > 1)
    {
        m_chartXAxis->setMin(QDateTime::fromMSecsSinceEpoch((qint64) series->at(0).x()));
        m_chartXAxis->setMax(QDateTime::fromMSecsSinceEpoch((qint64) series->at(series->count() - 1).x()));
    }
}

// Scale the range [start, end] by a factor of two per wheel click, keeping the point at
// centre (0 to 1 along the range) where it is
static void scaleRange(qreal& start, qreal& end, qreal minRange, int delta, qreal centre)
{
    const qreal range = end - start;
    const qreal factor = std::pow(2.0, delta / 120.0);
    const qreal newRange = std::max(minRange, range / factor);
    start = start + centre * (range - newRange);
    end = start + newRange;
}

// The same controls as the ADS-B demodulator's chart: the wheel zooms the time axis about
// the cursor, and with shift held the rate axes
void AircraftGUI::zoomChart(QWheelEvent *event)
{
    // Some platforms turn a shifted vertical wheel into a horizontal one
    const int delta = (event->angleDelta().y() != 0) ? event->angleDelta().y() : event->angleDelta().x();
    const QPointF point = event->position();
    const QRectF plotArea = m_chart->plotArea();

    if (delta == 0) {
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier)
    {
        qreal y = (point.y() - plotArea.y()) / plotArea.height();
        y = 1.0 - std::min(1.0, std::max(0.0, y));

        for (QValueAxis *axis : {m_chartYAxis, m_chartADSBYAxis})
        {
            bool visible = false;
            for (int i = 0; i < CHART_SERIES; i++) {
                visible |= (onADSBAxis(i) == (axis == m_chartADSBYAxis)) && m_chartSeries[i]->isVisible();
            }
            if (visible)
            {
                qreal min = axis->min();
                qreal max = axis->max();
                scaleRange(min, max, 0.1, delta, y);
                axis->setRange(std::max(0.0, min), std::min(1e6, max));
            }
        }
        m_chartYZoomed = true;
    }
    else
    {
        QLineSeries *series = m_chartSeries[0];
        if (series->count() > 1)
        {
            qreal x = (point.x() - plotArea.x()) / plotArea.width();
            x = std::min(1.0, std::max(0.0, x));

            qreal startMS = m_chartXAxis->min().toMSecsSinceEpoch();
            qreal endMS = m_chartXAxis->max().toMSecsSinceEpoch();
            // A sample comes every ten seconds, so a minute shows a handful
            scaleRange(startMS, endMS, 60000.0, delta, x);
            // Don't let the range exceed the data
            startMS = std::max(series->at(0).x(), startMS);
            endMS = std::min(series->at(series->count() - 1).x(), endMS);
            m_chartXAxis->setMin(QDateTime::fromMSecsSinceEpoch((qint64) startMS));
            m_chartXAxis->setMax(QDateTime::fromMSecsSinceEpoch((qint64) endMS));
        }
    }
}

bool AircraftGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (m_chart && (obj == ui->chart->viewport()))
    {
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            zoomChart(wheelEvent);
            wheelEvent->accept();
            return true;
        }
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            // Only in the plot area: a double click on the legend is two clicks there
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->button() == Qt::LeftButton) && m_chart->plotArea().contains(mouseEvent->pos()))
            {
                resetChartZoom();
                return true;
            }
        }
    }
    return FeatureGUI::eventFilter(obj, event);
}

// A pane shown again in the vertical splitter returns at the size it had when hidden,
// which is nothing if it was dragged shut. Give it a share of the splitter then, taken
// from the largest pane, or the button would seem to do nothing.
void AircraftGUI::showInSplitter(QWidget *widget)
{
    widget->setVisible(true);
    // The splitter lays the pane out once the show reaches it
    QTimer::singleShot(0, this, [this, widget]() {
        QSplitter *splitter = ui->splitter;
        const int index = splitter->indexOf(widget);
        if ((index < 0) || !widget->isVisible()) {
            return;
        }
        QList<int> sizes = splitter->sizes();
        const int minimum = std::max(widget->minimumSizeHint().height(), 50);
        if (sizes[index] >= minimum) {
            return;
        }
        int total = 0;
        int largest = -1;
        for (int i = 0; i < sizes.size(); i++)
        {
            total += sizes[i];
            if ((i != index) && ((largest < 0) || (sizes[i] > sizes[largest]))) {
                largest = i;
            }
        }
        if (largest < 0) {
            return;
        }
        const int wanted = std::max(minimum, total / 4);
        const int take = std::min(wanted - sizes[index], sizes[largest] / 2);
        sizes[index] += take;
        sizes[largest] -= take;
        splitter->setSizes(sizes);
    });
}

void AircraftGUI::trackerMessageRates(const QList<float>& rates)
{
    const QDateTime now = QDateTime::currentDateTime();

    // The tracker prunes its ATC log of what belongs to nothing even in a sweep that
    // discards no aircraft, and then sends nothing to say so. These samples come every ten
    // seconds; the sweep runs once a minute, so pruning as often as that is enough.
    if (now.toMSecsSinceEpoch() - m_orphansPrunedMs >= 60 * 1000) {
        pruneOrphanMessageRows();
    }

    // The x axis follows the data at whichever ends it was showing: both when unzoomed,
    // so every retained point stays in view, and only the newest when zoomed into the
    // recent past, as the ADS-B demodulator's chart does
    QLineSeries *first = m_chartSeries[0];
    const bool xAtMax = (first->count() == 0)
        || ((qint64) first->at(first->count() - 1).x() == m_chartXAxis->max().toMSecsSinceEpoch());
    const bool xAtMin = (first->count() == 0)
        || ((qint64) first->at(0).x() == m_chartXAxis->min().toMSecsSinceEpoch());

    for (int i = 0; (i < CHART_SERIES) && (i < rates.size()); i++)
    {
        m_chartSeries[i]->append(now.toMSecsSinceEpoch(), rates[i]);
        // Bound long-running growth
        if (m_chartSeries[i]->count() > 1000) {
            m_chartSeries[i]->removePoints(0, m_chartSeries[i]->count() - 1000);
        }
    }
    fitChartYAxes();
    if (xAtMax) {
        m_chartXAxis->setMax(now);
    }
    if (xAtMin && (first->count() > 0)) {
        m_chartXAxis->setMin(QDateTime::fromMSecsSinceEpoch((qint64) first->at(0).x()));
    }
}

// Altitude and speed against time for the watched flight
void AircraftGUI::plotProfileChart()
{
    QChart *oldChart = m_profileChart;
    m_profileChart = new QChart();
    m_profileChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_profileChart->setMargins(QMargins(1, 1, 1, 1));
    m_profileChart->setTheme(QChart::ChartThemeDark);
    m_profileChart->legend()->setAlignment(Qt::AlignRight);
    m_profileChart->setTitle(m_profileFlightId ? m_profileTitle : "No flight selected");

    QDateTimeAxis *xAxis = new QDateTimeAxis();
    xAxis->setFormat("hh:mm");
    QValueAxis *altAxis = new QValueAxis();
    altAxis->setTitleText("Alt (ft)");
    QValueAxis *speedAxis = new QValueAxis();
    speedAxis->setTitleText("Speed (kn)");
    m_profileChart->addAxis(xAxis, Qt::AlignBottom);
    m_profileChart->addAxis(altAxis, Qt::AlignLeft);
    m_profileChart->addAxis(speedAxis, Qt::AlignRight);

    QLineSeries *altSeries = new QLineSeries();
    altSeries->setName("Altitude");
    QLineSeries *speedSeries = new QLineSeries();
    speedSeries->setName("Speed");

    float maxAlt = 0.0f;
    float maxSpeed = 0.0f;
    for (int i = 0; i < m_profileTimes.size(); i++)
    {
        if (!std::isnan(m_profileAltFt[i]))
        {
            altSeries->append(m_profileTimes[i], m_profileAltFt[i]);
            maxAlt = std::max(maxAlt, m_profileAltFt[i]);
        }
        if (!std::isnan(m_profileSpeedKts[i]))
        {
            speedSeries->append(m_profileTimes[i], m_profileSpeedKts[i]);
            maxSpeed = std::max(maxSpeed, m_profileSpeedKts[i]);
        }
    }

    m_profileChart->addSeries(altSeries);
    altSeries->attachAxis(xAxis);
    altSeries->attachAxis(altAxis);
    m_profileChart->addSeries(speedSeries);
    speedSeries->attachAxis(xAxis);
    speedSeries->attachAxis(speedAxis);

    if (!m_profileTimes.isEmpty())
    {
        xAxis->setMin(QDateTime::fromMSecsSinceEpoch(m_profileTimes.first()));
        xAxis->setMax(QDateTime::fromMSecsSinceEpoch(std::max(m_profileTimes.first() + 60000, m_profileTimes.last())));
    }
    else
    {
        const QDateTime now = QDateTime::currentDateTime();
        xAxis->setMin(now.addSecs(-60));
        xAxis->setMax(now);
    }
    altAxis->setRange(0.0, std::max(1000.0, std::ceil(maxAlt / 1000.0) * 1000.0));
    speedAxis->setRange(0.0, std::max(100.0, std::ceil(maxSpeed / 100.0) * 100.0));

    ui->profileChart->setChart(m_profileChart);
    delete oldChart;
}

// ---- Toolbar ----

// Not just the tables: every aircraft the database holds, current and past, goes with its
// flights, tracks, profiles, ACARS documents, ATC messages and weather reports. A database
// that has grown large took days or weeks of listening to build up, which is too much to
// lose to a stray click, so past a size like that it asks first.
void AircraftGUI::on_deleteAircraft_clicked()
{
    const qint64 askAboveBytes = 100LL * 1024 * 1024;
    if (m_databaseBytes > askAboveBytes)
    {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            "Delete all aircraft",
            QString("Delete every aircraft from the database, current and past, with all of their "
                    "flights, tracks, messages and weather reports?\n\n"
                    "The database is %1 MB. This cannot be undone. Statistics and coverage are kept.")
                .arg(m_databaseBytes / (1024.0 * 1024.0), 0, 'f', 0),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    tracker()->getInputMessageQueue()->push(AircraftTracker::MsgDeleteAll::create());
}

void AircraftGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    m_settingsKeys.append("feedEnabled");
    applySettings();
}

// Feed settings dialog: station ident plus per-aggregator enable, host and port.
//
// Here rather than in the ACARS demodulator, where it used to be: a receiver runs one
// demodulator per band - VHF, VDL-2, HFDL - and this had to be filled in identically in
// each of them, opening three sets of connections to the same aggregator under the same
// station identity. The station is a property of the receiver, so it is configured once.
void AircraftGUI::feedSelect(const QPoint& p)
{
    QDialog dialog(this);
    dialog.setWindowTitle("ACARS Feed Settings");

    QLineEdit *stationId = new QLineEdit(m_settings.m_feedStationId);
    stationId->setToolTip("Station identifier sent with each message (e.g. XX-JBLOGGS-VHF1). Register it with the aggregator to claim your feed");
    QCheckBox *airframes = new QCheckBox("Feed airframes.io");
    airframes->setChecked(m_settings.m_feedAirframes);
    QLineEdit *airframesHost = new QLineEdit(m_settings.m_feedAirframesHost);
    QSpinBox *airframesPort = new QSpinBox();
    airframesPort->setRange(1, 65535);
    airframesPort->setValue(m_settings.m_feedAirframesPort);
    QComboBox *airframesProtocol = new QComboBox();
    airframesProtocol->addItems({"TCP", "UDP"});
    airframesProtocol->setCurrentIndex(m_settings.m_feedAirframesTcp ? 0 : 1);
    airframesProtocol->setToolTip("Use the protocol, host and port the airframes.io Add Ground Station wizard assigns to your station");
    QCheckBox *avdelphi = new QCheckBox("Feed avdelphi.com");
    avdelphi->setChecked(m_settings.m_feedAvdelphi);
    QLineEdit *avdelphiHost = new QLineEdit(m_settings.m_feedAvdelphiHost);
    QSpinBox *avdelphiPort = new QSpinBox();
    avdelphiPort->setRange(1, 65535);
    avdelphiPort->setValue(m_settings.m_feedAvdelphiPort);
    QComboBox *avdelphiProtocol = new QComboBox();
    avdelphiProtocol->addItems({"TCP", "UDP"});
    avdelphiProtocol->setCurrentIndex(m_settings.m_feedAvdelphiTcp ? 0 : 1);

    QFormLayout *form = new QFormLayout(&dialog);
    form->addRow("Station ID", stationId);
    form->addRow(airframes);
    form->addRow("Host", airframesHost);
    form->addRow("Port", airframesPort);
    form->addRow("Protocol", airframesProtocol);
    form->addRow(avdelphi);
    form->addRow("Host", avdelphiHost);
    form->addRow("Port", avdelphiPort);
    form->addRow("Protocol", avdelphiProtocol);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.move(p);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_feedStationId = stationId->text().trimmed();
        m_settings.m_feedAirframes = airframes->isChecked();
        m_settings.m_feedAirframesHost = airframesHost->text().trimmed();
        m_settings.m_feedAirframesPort = airframesPort->value();
        m_settings.m_feedAirframesTcp = airframesProtocol->currentIndex() == 0;
        m_settings.m_feedAvdelphi = avdelphi->isChecked();
        m_settings.m_feedAvdelphiHost = avdelphiHost->text().trimmed();
        m_settings.m_feedAvdelphiPort = avdelphiPort->value();
        m_settings.m_feedAvdelphiTcp = avdelphiProtocol->currentIndex() == 0;
        m_settingsKeys.append({"feedStationId", "feedAirframes", "feedAirframesHost",
                               "feedAirframesPort", "feedAirframesTcp", "feedAvdelphi",
                               "feedAvdelphiHost", "feedAvdelphiPort", "feedAvdelphiTcp"});
        applySettings();
    }
}

// The same download the ADS-B and ACARS demodulators offer. Here as well, because this is
// where most of what the database provides is shown - registrations, types, operators and
// the 3D models and livery icons matched from them - and without it none of the other
// protocols' aircraft can be told apart from one another by anything but address
void AircraftGUI::on_getAircraftDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_osnDB.downloadAircraftInformation();
    }
}

void AircraftGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void AircraftGUI::downloadError(const QString& error)
{
    QMessageBox::critical(this, "Aircraft", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void AircraftGUI::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void AircraftGUI::downloadAircraftInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Aircraft Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    // Read here, under the dialog, rather than by the tracker on its own thread with
    // nothing to say why the aircraft are not updating
    OsnDB::getAircraftInformation();
    OsnDB::getAircraftInformationByReg();
    QMetaObject::invokeMethod(tracker(), "reloadAircraftDatabase", Qt::QueuedConnection);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void AircraftGUI::on_settings_clicked()
{
    AircraftSettingsDialog dialog(&m_settings, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        const QStringList& changed = dialog.changedSettings();

        if (!changed.isEmpty())
        {
            m_settingsKeys.append(changed);
            applySettings();
        }
    }
}

void AircraftGUI::on_notifications_clicked()
{
    AircraftNotificationDialog dialog(&m_settings, this);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settingsKeys.append("notificationSettings");
        applySettings();
    }
}

// The statistics table and the message rate chart live in the same vertical splitter as
// the tables, rather than in tabs, so they can be watched at the same time as the traffic
// they describe - the same arrangement the ADS-B demodulator uses.
// Right clicking either toggle resets what it shows, which is how the same two buttons
// behave in the ADS-B demodulator
void AircraftGUI::statsContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    QAction *session = menu.addAction("Reset session statistics");
    QAction *allTime = menu.addAction("Reset all time statistics");
    QAction *chosen = menu.exec(ui->stats->mapToGlobal(pos));

    if (chosen == session)
    {
        tracker()->getInputMessageQueue()->push(
            AircraftTracker::MsgResetStatistics::create(false));
    }
    else if (chosen == allTime)
    {
        // Clearing the preset's copy as well, so a preset written before the records
        // moved into the database cannot be imported over the reset that just happened
        m_settings.m_statistics = QByteArray();
        m_settingsKeys.append("statistics");
        tracker()->getInputMessageQueue()->push(
            AircraftTracker::MsgResetStatistics::create(true));
        applySettings();
    }
}

void AircraftGUI::on_stats_clicked(bool checked)
{
    m_settings.m_displayStatistics = checked;
    if (checked) {
        showInSplitter(ui->statistics);
    } else {
        ui->statistics->setVisible(false);
    }
    m_settingsKeys.append("displayStatistics");
    applySettings();
}

void AircraftGUI::chartContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    QAction *reset = menu.addAction("Reset chart data");
    if (menu.exec(ui->displayChart->mapToGlobal(pos)) == reset)
    {
        for (int i = 0; i < CHART_SERIES; i++)
        {
            if (m_chartSeries[i]) {
                m_chartSeries[i]->clear();
            }
        }
        m_chartYZoomed = false;
    }
}

void AircraftGUI::on_displayChart_clicked(bool checked)
{
    m_settings.m_displayChart = checked;
    if (checked) {
        showInSplitter(ui->chart);
    } else {
        ui->chart->setVisible(false);
    }
    m_settingsKeys.append("displayChart");
    applySettings();
}

void AircraftGUI::coverageContextMenu(const QPoint& pos)
{
    QMenu menu(this);

    // Each in the colour it is drawn in, which doubles as the key to the Map. SELCAL is
    // left out: it carries no position, so it never has any coverage to show.
    QList<QPair<QAction *, int>> protocols;
    for (int protocol : AircraftReport::displayOrder())
    {
        if (!AircraftReport::reportsPosition(protocol)) {
            continue;
        }
        QPixmap swatch(12, 12);
        swatch.fill(QColor::fromRgb(AircraftTracker::coverageColor(protocol)));   // Opaque, so it can be seen
        QAction *action = menu.addAction(QIcon(swatch), AircraftTracker::protocolName(protocol));
        action->setCheckable(true);
        action->setChecked(m_settings.coverageShown(protocol));
        protocols.append(qMakePair(action, protocol));
    }
    menu.addSeparator();
    QAction *reset = menu.addAction("Reset coverage data");

    QAction *chosen = menu.exec(ui->coverage->mapToGlobal(pos));
    if (!chosen) {
        return;
    }
    if (chosen == reset)
    {
        // Queued because the tracker lives on its own thread
        QMetaObject::invokeMethod(m_aircraft->getTracker(), "clearCoverage", Qt::QueuedConnection);
        return;
    }
    for (const auto& protocol : protocols)
    {
        if (chosen == protocol.first)
        {
            const quint32 bit = 1u << protocol.second;
            if (chosen->isChecked()) {
                m_settings.m_coverageHiddenProtocols &= ~bit;
            } else {
                m_settings.m_coverageHiddenProtocols |= bit;
            }
            m_settingsKeys.append("coverageHiddenProtocols");
            applySettings();
        }
    }
}

void AircraftGUI::on_coverage_clicked(bool checked)
{
    m_settings.m_displayCoverage = checked;
    m_settingsKeys.append("displayCoverage");
    applySettings();
}

void AircraftGUI::on_atcLabels_clicked(bool checked)
{
    m_settings.m_atcLabels = checked;
    m_settingsKeys.append("atcLabels");
    applySettings();    // The tracker refreshes the Map labels
}

void AircraftGUI::on_findDatalink_clicked(bool checked)
{
    m_settings.m_findDatalinkAircraft = checked;
    m_settingsKeys.append("findDatalinkAircraft");
    applySettings();
}

// Which messages are sent to the Map without finding their aircraft - the routine
// replies, which would otherwise turn the view for every WILCO
void AircraftGUI::findDatalinkSelect(const QPoint& p)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Find Datalink Aircraft");

    QFormLayout *form = new QFormLayout(&dialog);
    form->addRow(new QLabel("Send these messages to the Map without finding the aircraft:"));
    QList<QCheckBox *> checkBoxes;
    for (const QString& element : AircraftSettings::m_datalinkNoFindChoices)
    {
        QCheckBox *checkBox = new QCheckBox(element);
        checkBox->setChecked(m_settings.m_datalinkNoFindElements.contains(element));
        checkBox->setToolTip(QString("A message that is only %1, or only that and other checked messages,"
                                     " is shown on the DCDU of an aircraft already selected or followed,"
                                     " but does not find its aircraft").arg(element));
        form->addRow(checkBox);
        checkBoxes.append(checkBox);
    }
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.move(p);
    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList elements;
        for (const QCheckBox *checkBox : checkBoxes)
        {
            if (checkBox->isChecked()) {
                elements.append(checkBox->text());
            }
        }
        m_settings.m_datalinkNoFindElements = elements;
        m_settingsKeys.append("datalinkNoFindElements");
        applySettings();
    }
}

// ---- Column management ----

// Every column's position, not just the one dragged: those between where it was and where
// it went have each moved one place, and storing only the dragged column left them
// claiming their old places - so the order restored was not the order saved
void AircraftGUI::aircraft_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) logicalIndex;
    (void) oldVisualIndex;
    (void) newVisualIndex;

    const QHeaderView *header = ui->aircraft->horizontalHeader();
    for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
        m_settings.m_columnIndexes[i] = header->visualIndex(i);
    }
    m_settingsKeys.append("columnIndexes");
    applySettings();
}

void AircraftGUI::aircraft_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
    m_settingsKeys.append("columnSizes");
    applySettings();
}

// As aircraft_sectionMoved()
void AircraftGUI::flights_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) logicalIndex;
    (void) oldVisualIndex;
    (void) newVisualIndex;

    const QHeaderView *header = ui->flights->horizontalHeader();
    for (int i = 0; i < FLIGHT_COLUMNS; i++) {
        m_settings.m_flightColumnIndexes[i] = header->visualIndex(i);
    }
    m_settingsKeys.append("flightColumnIndexes");
    applySettings();
}

void AircraftGUI::flights_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_flightColumnSizes[logicalIndex] = newSize;
    m_settingsKeys.append("flightColumnSizes");
    applySettings();
}

void AircraftGUI::flightColumnSelectMenu(QPoint pos)
{
    m_flightColumnMenu->popup(ui->flights->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void AircraftGUI::flightColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        if (!action->isChecked() && (visibleColumnCount(ui->flights, FLIGHT_COLUMNS) <= 1))
        {
            // The menu that would put it back lives on the header, and a table with no
            // visible columns has no header to right-click: refuse the last one
            action->setChecked(true);
            return;
        }
        ui->flights->setColumnHidden(idx, !action->isChecked());
    }
}

void AircraftGUI::aircraftColumnSelectMenu(QPoint pos)
{
    m_columnMenu->popup(ui->aircraft->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void AircraftGUI::aircraftColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        if (!action->isChecked() && (visibleColumnCount(ui->aircraft, AIRCRAFT_COLUMNS) <= 1))
        {
            // The menu that would put it back lives on the header, and a table with no
            // visible columns has no header to right-click: refuse the last one
            action->setChecked(true);
            return;
        }
        ui->aircraft->setColumnHidden(idx, !action->isChecked());
    }
}

QAction *AircraftGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}
