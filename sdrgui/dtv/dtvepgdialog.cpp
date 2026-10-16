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
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "dtv/dtvepggrid.h"
#include "dtv/dtvepgdialog.h"

DTVEPGDialog::DTVEPGDialog(DTVGuide *guide, DTVRecordingSchedule *schedule, QWidget *parent) :
    QDialog(parent),
    m_schedule(schedule),
    m_epg(guide),
    m_currentService(0)
{
    setWindowTitle("Programme Guide");
    resize(960, 640);

    m_services = new QListWidget();
    m_services->setToolTip("Services by channel number. Those in italics are on other multiplexes, so are played by retuning, and those\n"
        "greyed are on multiplexes whose frequency isn't known. Double-click to play.");
    m_services->setMinimumWidth(220);

    m_day = new QComboBox();
    m_day->setToolTip("Day of the schedule");
    m_otherMultiplexes = new QCheckBox("Other multiplexes");
    m_otherMultiplexes->setToolTip("List services on other multiplexes of the network too");
    m_otherMultiplexes->setChecked(true);
    m_status = new QLabel();
    m_scheduled = new QToolButton();
    m_scheduled->setText("Scheduled");
    m_scheduled->setToolTip("Programmes to record. Select one to cancel its recording");
    m_scheduled->setPopupMode(QToolButton::InstantPopup);
    m_scheduled->setMenu(new QMenu(m_scheduled));
    connect(m_scheduled->menu(), &QMenu::aboutToShow, this, &DTVEPGDialog::scheduledMenu);

    m_events = new QTableWidget(0, 4);
    m_events->setHorizontalHeaderLabels({"Time", "Title", "Genre", "Info"});
    m_events->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_events->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_events->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_events->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_events->verticalHeader()->setVisible(false);
    m_events->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_events->setSelectionMode(QAbstractItemView::SingleSelection);
    m_events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_events->setToolTip("Info: REC to be recorded, HD, S subtitles, AD audio description, and the minimum age from the parental rating.\n"
        "Double-click to play the service. Right-click to record");
    m_events->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_events, &QTableWidget::customContextMenuRequested, this, &DTVEPGDialog::eventMenu);

    m_description = new QTextBrowser();
    m_description->setMaximumHeight(160);

    m_grid = new DTVEPGGrid();
    m_grid->setToolTip("");
    m_grid->setScheduled([this](int networkId, int serviceId, int eventId) { return m_schedule->isScheduled(networkId, serviceId, eventId); });

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addWidget(m_day);
    controls->addWidget(m_otherMultiplexes);
    controls->addStretch(1);
    controls->addWidget(m_status);
    controls->addWidget(m_scheduled);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_services);
    splitter->addWidget(m_events);
    splitter->setStretchFactor(1, 1);

    m_views = new QTabWidget();
    m_views->addTab(splitter, "List");
    m_views->addTab(m_grid, "Grid");
    m_views->setTabToolTip(0, "Services, and the schedule of the one selected");
    m_views->setTabToolTip(1, "Every service's programmes by time. Double-click or Enter to play, right-click to record. "
        "Arrow keys move between programmes");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(m_views, 1);
    layout->addWidget(m_description);

    connect(m_services, &QListWidget::currentRowChanged, this, [this](int) {
        const ListedService *service = currentService();
        m_currentService = service ? service->m_serviceId : m_currentService;
        refreshEvents();
    });
    connect(m_services, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        const ListedService *service = currentService();

        if (service) {
            play(*service);
        }
    });
    connect(m_day, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshEvents();
        showDay();
    });
    connect(m_otherMultiplexes, &QCheckBox::toggled, this, [this](bool) { refreshServices(); });
    connect(m_events, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) { showDescription(); });
    connect(m_events, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const ListedService *service = currentService();

        if (service) {
            play(*service);
        }
    });
    connect(m_views, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1)
        {
            refreshGrid();
            showDay();
        }
        else
        {
            showDescription();
        }
    });
    connect(m_grid, &DTVEPGGrid::eventSelected, this, [this](int row, int event) {
        m_description->setHtml(describe(m_grid->row(row).m_events[event], m_grid->row(row).m_networkId));
    });
    connect(m_grid, &DTVEPGGrid::eventActivated, this, [this](int row, int) {
        ListedService service;

        if (gridService(row, service)) {
            play(service);
        }
    });
    connect(m_grid, &DTVEPGGrid::eventMenuRequested, this, [this](int row, int event, const QPoint& globalPosition) {
        ListedService service;

        // The event's copied: the menu's event loop may rebuild the grid
        if (gridService(row, service) && (event >= 0) && (event < (int) m_grid->row(row).m_events.size())) {
            showEventMenu(service, m_grid->row(row).m_events[event], globalPosition);
        }
    });
    connect(&m_timer, &QTimer::timeout, this, &DTVEPGDialog::refresh);
    m_timer.start(10000); // The guide arrives over minutes

    refresh();
}

qint64 DTVEPGDialog::now() const
{
    const qint64 t = m_epg->getTime();
    return t > 0 ? t : QDateTime::currentSecsSinceEpoch();
}

void DTVEPGDialog::setCurrentService(int serviceId)
{
    m_currentService = serviceId;

    for (int i = 0; i < (int) m_listed.size(); i++)
    {
        if (m_listed[i].m_actual && (m_listed[i].m_serviceId == serviceId))
        {
            m_services->setCurrentRow(i);
            return;
        }
    }
}

const DTVEPGDialog::ListedService *DTVEPGDialog::currentService() const
{
    const int row = m_services->currentRow();
    return (row >= 0) && (row < (int) m_listed.size()) ? &m_listed[row] : nullptr;
}

// A row of the grid's service, from the services it was built from. False if they've changed since
bool DTVEPGDialog::gridService(int row, ListedService& service) const
{
    if ((row < 0) || (row >= m_grid->rowCount()) || (row >= (int) m_gridServices.size())) {
        return false;
    }

    const DTVEPGGrid::Row& r = m_grid->row(row);
    const DTVGuide::Service& s = m_gridServices[row];

    if ((r.m_networkId != s.m_networkId) || (r.m_serviceId != s.m_serviceId)) {
        return false;
    }

    service = {r.m_networkId, r.m_serviceId, r.m_actual, s.m_frequency, s.m_multiplexKnown};
    return true;
}

void DTVEPGDialog::refresh()
{
    refreshDays();
    refreshServices();
}

// Today and the next 7 days, local time
void DTVEPGDialog::refreshDays()
{
    const QDate today = QDateTime::fromSecsSinceEpoch(now()).toLocalTime().date();

    if ((m_day->count() > 0) && (m_day->itemData(0).toDate() == today)) {
        return;
    }

    const int index = std::max(0, m_day->currentIndex());
    m_day->blockSignals(true);
    m_day->clear();

    for (int d = 0; d < 8; d++)
    {
        const QDate date = today.addDays(d);
        m_day->addItem(d == 0 ? QString("Today") : d == 1 ? QString("Tomorrow") : date.toString("dddd d MMMM"), date);
    }

    m_day->setCurrentIndex(std::min(index, m_day->count() - 1));
    m_day->blockSignals(false);
}

void DTVEPGDialog::refreshServices()
{
    const quint64 revision = m_epg->getRevision();
    const bool others = m_otherMultiplexes->isChecked();

    if ((revision == m_servicesRevision) && (others == m_servicesOthers))
    {
        refreshEvents();
        refreshGrid();
        return;
    }

    m_servicesRevision = revision;
    m_servicesOthers = others;
    std::vector<DTVGuide::Service> services = m_epg->getServices();

    services.erase(std::remove_if(services.begin(), services.end(), [others](const DTVGuide::Service& s) {
        return s.m_name.isEmpty() || !s.m_visible || (!others && !s.m_actual);
    }), services.end());
    std::sort(services.begin(), services.end(), [](const DTVGuide::Service& a, const DTVGuide::Service& b) {
        if ((a.m_channel != b.m_channel) || (a.m_minorChannel != b.m_minorChannel)) {
            return a.channelBefore(b);
        }

        return a.m_name < b.m_name;
    });

    QString key = QString::number(others);

    for (const auto& s : services) {
        key += QString("|%1/%2/%3/%4/%5/%6/%7").arg(s.m_networkId).arg(s.m_serviceId).arg(s.channelText()).arg(s.m_actual).arg(s.m_name)
            .arg(s.m_frequency).arg(s.m_multiplexKnown);
    }

    m_gridServices = services;

    m_status->setText(QString("%1 services, %2 programmes").arg(services.size()).arg(m_epg->getEventCount()));

    if (key == m_servicesKey)
    {
        refreshEvents();
        refreshGrid();
        return;
    }

    m_servicesKey = key;

    // Keep the selection, or select the service playing
    const ListedService *selected = currentService();
    const int selectedNetwork = selected ? selected->m_networkId : -1;
    const int selectedService = selected ? selected->m_serviceId : m_currentService;

    m_services->blockSignals(true);
    m_services->clear();
    m_listed.clear();
    int row = -1;

    for (const auto& s : services)
    {
        QListWidgetItem *item = new QListWidgetItem(s.m_channel >= 0 ? QString("%1  %2").arg(s.channelText()).arg(s.m_name) : s.m_name);

        if (!s.m_actual)
        {
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);

            if (s.m_frequency > 0)
            {
                item->setToolTip(QString("On another multiplex, at %1 MHz: double-click to tune to it").arg(s.m_frequency / 1e6, 0, 'f', 3));
            }
            else if (s.m_multiplexKnown)
            {
                item->setToolTip("On another multiplex, whose frequency isn't known: double-click to search the UHF channels for it");
            }
            else
            {
                item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
                item->setToolTip("On another region's multiplex");
            }
        }

        m_services->addItem(item);
        m_listed.push_back({s.m_networkId, s.m_serviceId, s.m_actual, s.m_frequency, s.m_multiplexKnown});

        if ((s.m_serviceId == selectedService) && ((selectedNetwork < 0) ? s.m_actual : (s.m_networkId == selectedNetwork))) {
            row = (int) m_listed.size() - 1;
        }
    }

    if (row >= 0) {
        m_services->setCurrentRow(row);
    }

    m_services->blockSignals(false);
    refreshEvents();
    refreshGrid();
}

// Every listed service's programmes, if the grid is shown, rebuilt if they've changed
void DTVEPGDialog::refreshGrid()
{
    if (m_views->currentIndex() != 1) {
        return;
    }

    const qint64 t = now();
    const qint64 start = t - 3600 - (t % 1800);
    const quint64 revision = m_epg->getRevision();
    const QString schedule = scheduleSignature();

    if (!m_gridKey.isEmpty() && (revision == m_gridRevision) && (m_servicesOthers == m_gridOthers) && (start == m_gridStart)
        && (schedule == m_gridSchedule))
    {
        m_grid->setNow(t);
        return;
    }

    const bool scheduleChanged = schedule != m_gridSchedule;
    m_gridRevision = revision;
    m_gridOthers = m_servicesOthers;
    m_gridStart = start;
    m_gridSchedule = schedule;

    // The rows, and each one's service's revision, which changes with its programmes: a big network's programmes are
    // only fetched if one has. Those to be recorded are marked as drawn, so only the schedule changing matters
    std::vector<std::pair<int, int>> ids;
    ids.reserve(m_gridServices.size());
    QString key = QString::number(start);

    for (const auto& s : m_gridServices)
    {
        ids.emplace_back(s.m_networkId, s.m_serviceId);
        key += QString("|%1/%2/%3/%4").arg(s.m_networkId).arg(s.m_serviceId).arg(s.m_name)
            .arg(s.m_actual || (s.m_frequency > 0) || s.m_multiplexKnown);
    }

    const std::vector<quint64> revisions = m_epg->getServiceRevisions(ids);

    if (!scheduleChanged && (key == m_gridKey) && (revisions == m_gridServiceRevisions))
    {
        m_grid->setNow(t);
        return;
    }

    std::vector<std::vector<DTVGuide::Event>> events = m_epg->getEvents(ids);
    std::vector<DTVEPGGrid::Row> rows;
    rows.reserve(m_gridServices.size());

    for (size_t i = 0; i < m_gridServices.size(); i++)
    {
        const DTVGuide::Service& s = m_gridServices[i];
        DTVEPGGrid::Row row;
        row.m_networkId = s.m_networkId;
        row.m_serviceId = s.m_serviceId;
        row.m_name = s.m_name;
        row.m_channel = s.channelText();
        row.m_actual = s.m_actual;
        row.m_tunable = s.m_actual || (s.m_frequency > 0) || s.m_multiplexKnown;
        row.m_events = std::move(events[i]);
        rows.push_back(row);
    }

    const bool first = m_gridKey.isEmpty();
    m_gridKey = key;
    m_gridServiceRevisions = revisions;
    m_grid->setRows(rows, start, t);

    if (first)
    {
        m_grid->scrollToTime(t - 1800);
        const ListedService *service = currentService();

        if (service) {
            m_grid->scrollToService(service->m_networkId, service->m_serviceId);
        }
    }
}

// The grid goes to the start of the day chosen, or now for today
void DTVEPGDialog::showDay()
{
    if (m_views->currentIndex() != 1) {
        return;
    }

    const QDate date = m_day->currentData().toDate();
    const qint64 t = now();
    const qint64 dayStart = QDateTime(date, QTime(0, 0)).toSecsSinceEpoch();
    m_grid->scrollToTime(m_day->currentIndex() == 0 ? t - 1800 : dayStart);
}

void DTVEPGDialog::play(ListedService service)
{
    if (service.tunable()) {
        emit serviceSelected(service.m_networkId, service.m_serviceId);
    }
}

void DTVEPGDialog::refreshEvents()
{
    const ListedService *service = currentService();

    if (!service)
    {
        m_events->setRowCount(0);
        m_shown.clear();
        m_eventsKey.clear();
        m_description->clear();
        return;
    }

    const QDate date = m_day->currentData().toDate();
    const qint64 dayStart = QDateTime(date, QTime(0, 0)).toSecsSinceEpoch();
    const qint64 dayEnd = QDateTime(date.addDays(1), QTime(0, 0)).toSecsSinceEpoch();
    const qint64 t = now();
    std::vector<DTVGuide::Event> events = m_epg->getEvents(service->m_networkId, service->m_serviceId);
    events.erase(std::remove_if(events.begin(), events.end(), [dayStart, dayEnd](const DTVGuide::Event& e) {
        return (e.m_start >= dayEnd) || (e.end() <= dayStart);
    }), events.end());

    // Rebuilt only if the programmes, or the one on now, have changed
    QString key = QString("%1/%2/%3").arg(service->m_networkId).arg(service->m_serviceId).arg(date.toString(Qt::ISODate));

    for (const auto& e : events) {
        key += QString("|%1/%2/%3/%4/%5").arg(e.m_eventId).arg(e.m_start).arg((e.m_start <= t) && (t < e.end())).arg(e.m_title)
            .arg(m_schedule->isScheduled(service->m_networkId, service->m_serviceId, e.m_eventId));
    }

    if (key == m_eventsKey) {
        return;
    }

    m_eventsKey = key;
    const int previousRow = m_events->currentRow();
    const int previousId = (previousRow >= 0) && (previousRow < (int) m_shown.size()) ? m_shown[previousRow].m_eventId : -1;
    m_events->setRowCount(0);
    m_shown.clear();
    int nowRow = -1;
    int selectRow = -1;

    for (const auto& e : events)
    {

        const int row = (int) m_shown.size();
        m_shown.push_back(e);
        m_events->insertRow(row);
        const QDateTime start = QDateTime::fromSecsSinceEpoch(e.m_start).toLocalTime();
        const QDateTime end = QDateTime::fromSecsSinceEpoch(e.end()).toLocalTime();
        QStringList info;
        const bool scheduled = m_schedule->isScheduled(service->m_networkId, service->m_serviceId, e.m_eventId);

        if (scheduled) {
            info.append("REC");
        }

        if (e.m_hd) {
            info.append("HD");
        }

        if (e.m_subtitles) {
            info.append("S");
        }

        if (e.m_audioDescription) {
            info.append("AD");
        }

        if (e.m_minimumAge > 0) {
            info.append(QString("%1+").arg(e.m_minimumAge));
        }

        QTableWidgetItem *items[4] = {
            new QTableWidgetItem(QString("%1 - %2").arg(start.toString("hh:mm")).arg(end.toString("hh:mm"))),
            new QTableWidgetItem(e.m_title),
            new QTableWidgetItem(m_epg->genreName(e.m_genre, service->m_networkId)),
            new QTableWidgetItem(info.join(' '))
        };
        const bool onNow = (e.m_start <= t) && (t < e.end());

        for (int c = 0; c < 4; c++)
        {
            if (onNow)
            {
                QFont font = items[c]->font();
                font.setBold(true);
                items[c]->setFont(font);
            }

            items[c]->setToolTip(e.m_text);

            if (scheduled) {
                items[c]->setForeground(Qt::red);
            }
            m_events->setItem(row, c, items[c]);
        }

        if (onNow) {
            nowRow = row;
        }

        if (e.m_eventId == previousId) {
            selectRow = row;
        }
    }

    // The programme selected before, else the one on now
    selectRow = selectRow >= 0 ? selectRow : nowRow;

    if (selectRow >= 0)
    {
        m_events->setCurrentCell(selectRow, 0);
        m_events->scrollToItem(m_events->item(selectRow, 0), QAbstractItemView::PositionAtTop);
    }

    showDescription();
}

QString DTVEPGDialog::describe(const DTVGuide::Event& e, int networkId) const
{
    const QDateTime start = QDateTime::fromSecsSinceEpoch(e.m_start).toLocalTime();
    const QDateTime end = QDateTime::fromSecsSinceEpoch(e.end()).toLocalTime();
    QString html = QString("<b>%1</b><br>%2 %3 - %4 (%5 min)")
        .arg(e.m_title.toHtmlEscaped())
        .arg(start.toString("dddd d MMMM"))
        .arg(start.toString("hh:mm"))
        .arg(end.toString("hh:mm"))
        .arg(e.m_duration / 60);
    const QString genre = m_epg->genreName(e.m_genre, networkId);

    if (!genre.isEmpty()) {
        html += "&nbsp;&nbsp;" + genre.toHtmlEscaped();
    }

    if (!e.m_guidance.isEmpty()) {
        html += "<br><i>" + e.m_guidance.toHtmlEscaped() + "</i>";
    }

    DTVGuide::Service linked;

    if (linkedService(e, linked)) {
        html += "<br>Also on " + linked.m_name.toHtmlEscaped();
    }

    if (!e.m_text.isEmpty()) {
        html += "<p>" + e.m_text.toHtmlEscaped().replace('\n', "<br>") + "</p>";
    }

    return html;
}

bool DTVEPGDialog::linkedService(const DTVGuide::Event& event, DTVGuide::Service& service) const
{
    return (event.m_linkServiceId >= 0) && m_epg->getService(event.m_linkNetworkId, event.m_linkServiceId, service)
        && !service.m_name.isEmpty() && (service.m_actual || service.m_multiplexKnown);
}

// The programmes to be recorded, which the grid marks
QString DTVEPGDialog::scheduleSignature() const
{
    QString signature;

    for (const auto& r : m_schedule->getScheduledRecordings()) {
        signature += QString("%1/%2/%3|").arg(r.m_networkId).arg(r.m_serviceId).arg(r.m_eventId);
    }

    return signature;
}

void DTVEPGDialog::showDescription()
{
    if (m_views->currentIndex() != 0) {
        return; // The grid shows its selection's
    }

    const int row = m_events->currentRow();
    const ListedService *service = currentService();

    if ((row < 0) || (row >= (int) m_shown.size()) || !service) {
        m_description->clear();
    } else {
        m_description->setHtml(describe(m_shown[row], service->m_networkId));
    }
}

void DTVEPGDialog::eventMenu(const QPoint& position)
{
    const int row = m_events->rowAt(position.y());
    const ListedService *service = currentService();

    if ((row < 0) || (row >= (int) m_shown.size()) || !service) {
        return;
    }

    showEventMenu(*service, m_shown[row], m_events->viewport()->mapToGlobal(position)); // Copied
}

// Play the service, retuning if it's on another multiplex (searching for it if its frequency isn't known), or the
// service the programme is also on, as in HD. Record a programme, or cancel its recording: only those of services that
// can be received, that haven't ended. The service and event are copies, as what they're from (the list, the table
// and the grid) can be rebuilt while the menu's open
void DTVEPGDialog::showEventMenu(ListedService service, DTVGuide::Event event, const QPoint& globalPosition)
{
    const bool scheduled = m_schedule->isScheduled(service.m_networkId, service.m_serviceId, event.m_eventId);
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    QAction *playAction = menu.addAction(service.m_actual ? QString("Play")
        : service.m_frequency > 0 ? QString("Tune to %1 MHz and play").arg(service.m_frequency / 1e6, 0, 'f', 3)
        : service.m_multiplexKnown ? QString("Search for its multiplex and play")
        : QString("Play (on another region's multiplex)"));
    playAction->setEnabled(service.tunable());
    DTVGuide::Service linked;
    QAction *linkedAction = nullptr;

    if (linkedService(event, linked))
    {
        linkedAction = menu.addAction(QString("Watch on %1").arg(linked.m_name));
        linkedAction->setToolTip(linked.m_actual ? QString() : linked.m_frequency > 0
            ? QString("Tunes to %1 MHz").arg(linked.m_frequency / 1e6, 0, 'f', 3) : QString("Searches the UHF channels for its multiplex"));
    }

    QAction *recordAction = menu.addAction(scheduled ? "Cancel recording" : "Record");
    // A service on another multiplex: recordable if scheduled recordings may retune to it
    const bool reachable = service.m_actual || (m_schedule->recordRetune() && service.tunable());

    if (!scheduled && !reachable)
    {
        recordAction->setEnabled(false);
        recordAction->setText("Record (on another multiplex: allow retuning from the Record button's menu)");
    }
    else if (!scheduled && (event.end() <= now()))
    {
        recordAction->setEnabled(false);
    }

    // Its series, by its series CRID
    QAction *seriesAction = nullptr;
    const bool series = !event.m_seriesCrid.isEmpty() && m_schedule->isSeriesScheduled(service.m_networkId, service.m_serviceId, event.m_eventId);

    if (!event.m_seriesCrid.isEmpty())
    {
        seriesAction = menu.addAction(series ? "Cancel series recording" : "Record series");
        seriesAction->setToolTip("Every programme of the series on this service, each once (not repeats)");
    }

    // Not refreshed while the menu's open (its event loop runs the timer)
    const bool refreshing = m_timer.isActive();
    m_timer.stop();
    QAction *chosen = menu.exec(globalPosition);

    if (refreshing) {
        m_timer.start();
    }

    if (seriesAction && (chosen == seriesAction))
    {
        if (series) {
            m_schedule->cancelSeries(service.m_networkId, service.m_serviceId, event.m_eventId);
        } else {
            m_schedule->scheduleSeries(service.m_networkId, service.m_serviceId, event.m_eventId);
        }

        m_eventsKey.clear();
        refreshEvents();
        refreshGrid();
        return;
    }

    if (chosen == playAction)
    {
        play(service);
        return;
    }

    if (linkedAction && (chosen == linkedAction))
    {
        emit serviceSelected(linked.m_networkId, linked.m_serviceId);
        return;
    }

    if (chosen != recordAction) {
        return;
    }

    if (scheduled) {
        m_schedule->cancelScheduledRecording(service.m_networkId, service.m_serviceId, event.m_eventId);
    } else {
        m_schedule->scheduleRecording(service.m_networkId, service.m_serviceId, event.m_eventId);
    }

    m_eventsKey.clear();
    refreshEvents();
    refreshGrid();
}

// The programmes to record, in time order: selecting one cancels it
void DTVEPGDialog::scheduledMenu()
{
    QMenu *menu = m_scheduled->menu();
    menu->clear();
    QList<DTVScheduledRecording> schedule = m_schedule->getScheduledRecordings();
    std::sort(schedule.begin(), schedule.end(), [](const DTVScheduledRecording& a, const DTVScheduledRecording& b) {
        return a.m_start < b.m_start;
    });
    const QList<DTVSeriesRecording> series = m_schedule->getSeriesRecordings();

    if (schedule.isEmpty() && series.isEmpty())
    {
        menu->addAction("None")->setEnabled(false);
        return;
    }

    // The series recorded, then the programmes
    for (const auto& s : series)
    {
        QAction *action = menu->addAction(QString("Cancel series: %1  %2").arg(s.m_serviceName).arg(s.m_title));
        const int networkId = s.m_networkId, serviceId = s.m_serviceId;
        const QString crid = s.m_seriesCrid;
        connect(action, &QAction::triggered, this, [this, networkId, serviceId, crid]() {
            m_schedule->cancelSeries(networkId, serviceId, crid);
            m_eventsKey.clear();
            refreshEvents();
            refreshGrid();
        });
    }

    if (!series.isEmpty() && !schedule.isEmpty()) {
        menu->addSeparator();
    }

    for (const auto& r : schedule)
    {
        const QDateTime start = QDateTime::fromSecsSinceEpoch(r.m_start).toLocalTime();
        QAction *action = menu->addAction(QString("Cancel: %1 - %2  %3 - %4").arg(start.toString("ddd d MMM hh:mm"))
            .arg(QDateTime::fromSecsSinceEpoch(r.end()).toLocalTime().toString("hh:mm")).arg(r.m_serviceName).arg(r.m_title));
        const int networkId = r.m_networkId, serviceId = r.m_serviceId, eventId = r.m_eventId;
        connect(action, &QAction::triggered, this, [this, networkId, serviceId, eventId]() {
            m_schedule->cancelScheduledRecording(networkId, serviceId, eventId);
            m_eventsKey.clear();
            refreshEvents();
            refreshGrid();
        });
    }
}
