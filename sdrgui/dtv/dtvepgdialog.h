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

#ifndef INCLUDE_DTVEPGDIALOG_H
#define INCLUDE_DTVEPGDIALOG_H

#include <vector>

#include <QDialog>
#include <QTimer>

#include "dtv/dtvguide.h"
#include "export.h"

class QCheckBox;
class QToolButton;
class QComboBox;
class QLabel;
class QListWidget;
class QTabWidget;
class QTableWidget;
class QTextBrowser;
class DTVEPGGrid;

// The programme guide: the services of the network, by channel number, with either the schedule of the one selected a
// day at a time, or a grid of every service's programmes by time. Services in other transport streams (multiplexes)
// are shown in italics, and played by retuning to them, where the NIT gives their frequency, else greyed.
// Double-clicking a service, or one of its programmes, plays it. Right-click a programme to record it
class SDRGUI_API DTVEPGDialog : public QDialog
{
    Q_OBJECT

public:
    //! schedule: null if programmes can't be recorded
    DTVEPGDialog(DTVGuide *guide, DTVRecordingSchedule *schedule, QWidget *parent = nullptr);
    //! Select the service in the list, as when it's playing
    void setCurrentService(int serviceId);

signals:
    void serviceSelected(int networkId, int serviceId);

private:
    struct ListedService
    {
        int m_networkId = 0;
        int m_serviceId = 0;
        bool m_actual = false;
        qint64 m_frequency = 0;         //!< Of its multiplex, 0 if not known
        bool m_multiplexKnown = false;  //!< Its multiplex is in the area, so can be searched for if its frequency isn't known
        bool tunable() const { return m_actual || (m_frequency > 0) || m_multiplexKnown; }
    };

    void refresh();
    void refreshServices();
    void refreshDays();
    void refreshEvents();
    void refreshGrid();
    void showDay();
    //! Copies: what they're from may be rebuilt meanwhile
    void play(ListedService service);
    void showEventMenu(ListedService service, DTVGuide::Event event, const QPoint& globalPosition);
    void showDescription();
    qint64 now() const;
    QString describe(const DTVGuide::Event& event, int networkId) const;
    //! The service an event is also on (as in HD), if it can be played. False if none
    bool linkedService(const DTVGuide::Event& event, DTVGuide::Service& service) const;
    QString scheduleSignature() const;
    const ListedService *currentService() const;
    bool gridService(int row, ListedService& service) const;
    void eventMenu(const QPoint& position);
    void scheduledMenu();

    DTVRecordingSchedule *m_schedule;
    DTVGuide *m_epg;
    QToolButton *m_scheduled;
    QListWidget *m_services;
    QComboBox *m_day;
    QCheckBox *m_otherMultiplexes;
    QLabel *m_status;
    QTableWidget *m_events;
    QTabWidget *m_views;
    DTVEPGGrid *m_grid;
    std::vector<DTVGuide::Service> m_gridServices;   //!< Rows of the grid
    QString m_gridKey;                                  //!< The grid's rows and start
    std::vector<quint64> m_gridServiceRevisions;        //!< Their services' revisions
    QTextBrowser *m_description;
    QTimer m_timer;
    std::vector<ListedService> m_listed;
    std::vector<DTVGuide::Event> m_shown;    //!< Events in the table, in its order
    int m_currentService;                       //!< Service id to select, when listed
    // What the list and grid were built from: when the guide, the services shown, the time window and the schedule
    // are as they were, nothing is rebuilt, as making the keys below goes through every programme
    quint64 m_servicesRevision = ~(quint64) 0;
    bool m_servicesOthers = false;
    quint64 m_gridRevision = ~(quint64) 0;
    bool m_gridOthers = false;
    qint64 m_gridStart = 0;
    QString m_gridSchedule;
    QString m_servicesKey;                      //!< What's listed and shown, so they're only rebuilt if it changes,
    QString m_eventsKey;                        //!< which would lose the scroll position
};

#endif // INCLUDE_DTVEPGDIALOG_H
