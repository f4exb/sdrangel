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
#include <vector>

#include <QDateTime>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QStyle>
#include <QEvent>
#include <QTableWidget>
#include <QVBoxLayout>

#include "programmeguide.h"

ProgrammeGuide::ProgrammeGuide(QWidget *parent) :
    QWidget(parent),
    m_sid(0),
    m_scheduleMinute(-1),
    m_layingOut(false),
    m_currentRow(-1),
    m_scrollPending(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setToolTip(tr("The service's description, from the Service and Programme Information (SPI)"));
    layout->addWidget(m_description);

    m_schedule = new QTableWidget(0, 3, this);
    m_schedule->setHorizontalHeaderLabels({tr("Time"), tr("Programme"), tr("Description")});
    m_schedule->setToolTip(tr("The service's schedule, from the Service and Programme Information (SPI). Times are local."));
    m_schedule->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_schedule->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_schedule->setWordWrap(true);
    m_schedule->horizontalHeader()->setStretchLastSection(false);   // Sized by layoutColumns
    m_schedule->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_schedule->verticalHeader()->setVisible(false);
    // The columns follow the space the table has, which a vertical scroll bar takes from
    m_schedule->viewport()->installEventFilter(this);
    layout->addWidget(m_schedule);

    // The programme on now moves on
    connect(&m_timer, &QTimer::timeout, this, &ProgrammeGuide::showSchedule);
    m_timer.start(10000);
    showDescription();
}

QPixmap ProgrammeGuide::logo(const modemdr::SPIData *data, quint32 sid, int width)
{
    QPixmap pixmap;
    if (data)
    {
        const modemdr::MOTFile *file = data->logo(sid, width);
        if (file && file->data) {
            pixmap.loadFromData(file->data->data(), (uint) file->data->size());
        }
    }
    return pixmap;
}

void ProgrammeGuide::setData(std::shared_ptr<const modemdr::SPIData> data)
{
    m_data = data;
    showDescription();
}

void ProgrammeGuide::setService(quint32 sid)
{
    if (sid != m_sid)
    {
        m_sid = sid;
        showDescription();
    }
}

void ProgrammeGuide::showDescription()
{
    const modemdr::SPIService *service = m_data && m_sid ? m_data->service(m_sid) : nullptr;

    if (service)
    {
        const QString description = QString::fromStdString(service->text.description());
        const QString name = QString::fromStdString(service->text.longName.empty() ? service->text.name() : service->text.longName);
        m_description->setText(description.isEmpty() ? name : QString("%1: %2").arg(name, description));
    }
    // Schedules can arrive long before the station information
    else if (m_data && m_sid && m_data->schedules.count(m_sid))
    {
        m_description->setText(tr("Waiting for the station information"));
    }
    else if (m_data && (!m_data->services.empty() || !m_data->schedules.empty()))
    {
        m_description->setText(tr("Not in the programme guide"));
    }
    else
    {
        m_description->setText(tr("No programme guide"));
    }
    showSchedule();
}

void ProgrammeGuide::showSchedule()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    std::vector<modemdr::SPIProgramme> programmes;

    if (m_data && m_sid)
    {
        const auto schedule = m_data->schedules.find(m_sid);
        if (schedule != m_data->schedules.end())
        {
            // From the programme on now, or the last that has ended
            for (const auto& programme : schedule->second)
            {
                if (programme.start + programme.duration > now - 3600) {
                    programmes.push_back(programme);
                }
            }
        }
    }

    // Rebuild only when it changes, or a minute later to move the highlight
    QString summary = QString("%1 %2").arg(m_sid).arg(programmes.size());
    for (const auto& p : programmes) {
        summary += QString(" %1 %2 %3").arg(p.start).arg(p.duration).arg(QString::fromStdString(p.text.name()));
    }
    if ((summary == m_scheduleShown) && (now / 60 == m_scheduleMinute)) {
        return;
    }
    m_scheduleShown = summary;
    m_scheduleMinute = now / 60;

    m_schedule->setRowCount(0);
    int current = -1;
    for (const auto& p : programmes)
    {
        const int row = m_schedule->rowCount();
        m_schedule->setRowCount(row + 1);
        const QDateTime start = QDateTime::fromSecsSinceEpoch(p.start);
        const QDateTime end = QDateTime::fromSecsSinceEpoch(p.start + p.duration);
        const bool today = start.date() == QDate::currentDate();
        QTableWidgetItem *timeItem = new QTableWidgetItem(QString("%1%2-%3")
            .arg(today ? QString() : start.toString("ddd "), start.toString("hh:mm"), end.toString("hh:mm")));
        QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(p.text.name()));
        QTableWidgetItem *descriptionItem = new QTableWidgetItem(QString::fromStdString(p.text.description()));
        descriptionItem->setToolTip(QString::fromStdString(p.text.description()));
        if ((p.start <= now) && (now < p.start + p.duration))
        {
            QFont font = nameItem->font();
            font.setBold(true);
            timeItem->setFont(font);
            nameItem->setFont(font);
            descriptionItem->setFont(font);
            current = row;
        }
        m_schedule->setItem(row, 0, timeItem);
        m_schedule->setItem(row, 1, nameItem);
        m_schedule->setItem(row, 2, descriptionItem);
    }
    m_currentRow = current;
    m_scrollPending = current >= 0;
    layoutColumns();
}

bool ProgrammeGuide::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_schedule->viewport()) && (event->type() == QEvent::Resize)) {
        layoutColumns();
    }
    return QWidget::eventFilter(watched, event);
}

// The time as wide as it needs, the programme up to a third of the rest and the
// description, usually the longest, the remainder; rows tall enough for the
// text wrapped in them
void ProgrammeGuide::layoutColumns()
{
    // Sizing the rows can show or hide the scroll bar, resizing the viewport
    if (m_layingOut) {
        return;
    }
    m_layingOut = true;

    // Widths from the text unwrapped: with word wrap, Qt sizes a column to its
    // contents at the width it already has
    // A cell's text margins, as the item delegate draws them, and the grid line
    const int padding = 2 * (m_schedule->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, m_schedule) + 1) + 2;
    const auto unwrapped = [this, padding](int column) {
        int width = m_schedule->horizontalHeader()->sectionSizeHint(column);
        for (int row = 0; row < m_schedule->rowCount(); row++)
        {
            const QTableWidgetItem *item = m_schedule->item(row, column);
            if (item)
            {
                // The item's own font (bold for the programme on now) over the table's
                const QFont font = item->data(Qt::FontRole).isValid() ? item->font().resolve(m_schedule->font()) : m_schedule->font();
                width = std::max(width, QFontMetrics(font).horizontalAdvance(item->text()) + padding);
            }
        }
        return width;
    };
    const int time = unwrapped(0);
    const int available = std::max(m_schedule->viewport()->width() - time, 0);
    const int programme = std::min(unwrapped(1), std::max(available / 3, 80));

    m_schedule->setColumnWidth(0, time);
    m_schedule->setColumnWidth(1, programme);
    m_schedule->setColumnWidth(2, std::max(available - programme, 80));
    m_schedule->resizeRowsToContents();

    // The programme on now at the top: until the table is shown it has no size to scroll in
    if (m_scrollPending && (m_currentRow >= 0) && (m_currentRow < m_schedule->rowCount()))
    {
        m_schedule->scrollToItem(m_schedule->item(m_currentRow, 0), QAbstractItemView::PositionAtTop);
        m_scrollPending = !m_schedule->isVisible();
    }

    m_layingOut = false;
}
