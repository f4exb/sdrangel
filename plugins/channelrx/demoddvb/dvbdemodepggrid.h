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

#ifndef INCLUDE_DVBDEMODEPGGRID_H
#define INCLUDE_DVBDEMODEPGGRID_H

#include <functional>
#include <vector>

#include <QAbstractScrollArea>

#include "dvbdemodepg.h"

// The programme guide as a grid of channels by time: a row per service, with its programmes as boxes along the time
// axis. The channel names on the left and the times along the top stay in place while scrolling. Clicking selects a
// programme, double-clicking or Enter activates it (to play its service), and right-clicking asks for a menu
class DVBDemodEPGGrid : public QAbstractScrollArea
{
    Q_OBJECT

public:
    struct Row
    {
        int m_networkId = 0;
        int m_serviceId = 0;
        QString m_name;
        int m_channel = -1;
        bool m_actual = false;      //!< In the transport stream being received
        bool m_tunable = false;     //!< Can be played: received, or its multiplex is in the area
        std::vector<DVBDemodEPG::Event> m_events;   //!< In time order
    };

    explicit DVBDemodEPGGrid(QWidget *parent = nullptr);
    //! The grid starts at start (UTC), with a line at now. The selection is kept if its programme is still there
    void setRows(const std::vector<Row>& rows, qint64 start, qint64 now);
    //! Move the line at now (UTC), as time passes, drawing again if it's moved by a pixel or more
    void setNow(qint64 now);
    //! To mark programmes to be recorded
    void setScheduled(std::function<bool(int networkId, int serviceId, int eventId)> isScheduled) { m_isScheduled = isScheduled; }
    //! Scroll so the time (UTC) is at the left
    void scrollToTime(qint64 utc);
    //! Scroll to a service's row
    void scrollToService(int networkId, int serviceId);
    int rowCount() const { return (int) m_rows.size(); }
    const Row& row(int index) const { return m_rows[index]; }
    //! The programme selected, false if none
    bool selection(int& row, int& event) const;

signals:
    void eventSelected(int row, int event);
    void eventActivated(int row, int event);
    void eventMenuRequested(int row, int event, const QPoint& globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    void updateScrollBars();
    bool hit(const QPoint& position, int& row, int& event) const;
    void select(int row, int event);
    double x(qint64 utc) const;     //!< In the viewport
    qint64 timeAt(double x) const;
    int nameWidth() const;
    int headerHeight() const;
    int rowHeight() const;

    std::vector<Row> m_rows;
    std::vector<qint64> m_longest;  //!< Each row's longest programme's duration, seconds
    qint64 m_start;
    qint64 m_end;
    qint64 m_now;
    int m_selectedRow;
    int m_selectedEvent;
    std::function<bool(int, int, int)> m_isScheduled;
    static constexpr double m_pixelsPerHour = 240.0;
};

#endif // INCLUDE_DVBDEMODEPGGRID_H
