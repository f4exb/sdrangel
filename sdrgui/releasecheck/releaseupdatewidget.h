///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                                //
// written by Robin Getz <rgetz503@gmail.com>                                        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATEWIDGET_H
#define INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATEWIDGET_H

#include <QLabel>

class QTimer;
class ReleaseUpdateChecker;

class ReleaseUpdateWidget : public QLabel
{
    Q_OBJECT

public:
    explicit ReleaseUpdateWidget(QWidget *parent = nullptr);
    void setVisible(bool visible) override;

private:
    void updateNotificationText();
    void updateBlink();

    void onReleaseUpdateAvailabilityChanged(bool available);
    void onLinkActivated(const QString& link);

    ReleaseUpdateChecker *m_checker;
    QTimer *m_blinkTimer;
    bool m_blink;
    bool m_wantVisible = false;
};

#endif // INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATEWIDGET_H
