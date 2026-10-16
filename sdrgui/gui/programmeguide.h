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

#ifndef SDRGUI_GUI_PROGRAMMEGUIDE_H_
#define SDRGUI_GUI_PROGRAMMEGUIDE_H_

#include <memory>

#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "modemdr/spi.h"
#include "export.h"

class QLabel;
class QTableWidget;

// A service's description and schedule from the Service and Programme Information (SPI) of DAB
// or DRM: the programme on now in bold. Times are local.
class SDRGUI_API ProgrammeGuide : public QWidget
{
    Q_OBJECT

public:
    explicit ProgrammeGuide(QWidget *parent = nullptr);

    // The guide decoded so far, and the service (SId) whose schedule is shown
    void setData(std::shared_ptr<const modemdr::SPIData> data);
    void setService(quint32 sid);
    // A service's logo: the one nearest the given width, else a null pixmap
    static QPixmap logo(const modemdr::SPIData *data, quint32 sid, int width);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showDescription();
    void showSchedule();
    void layoutColumns();

    QLabel *m_description;
    QTableWidget *m_schedule;
    QTimer m_timer;
    std::shared_ptr<const modemdr::SPIData> m_data;
    quint32 m_sid;
    QString m_scheduleShown;    // A summary of the schedule shown
    qint64 m_scheduleMinute;    // The minute it was shown in
    bool m_layingOut;
    int m_currentRow;           // The programme on now, -1 for none
    bool m_scrollPending;       // To the programme on now, once the table is shown
};

#endif // SDRGUI_GUI_PROGRAMMEGUIDE_H_
