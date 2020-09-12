///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRGUI_GUI_CHANNELDOCK_H_
#define SDRGUI_GUI_CHANNELDOCK_H_

#include <QDockWidget>

#include "channeladddialog.h"

class QHBoxLayout;
class QLabel;
class QPushButton;
class QStringList;

class ChannelsDock : public QDockWidget
{
    Q_OBJECT
public:
    ChannelsDock(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~ChannelsDock();

    void resetAvailableChannels() { m_channelAddDialog.resetChannelNames(); }
    void addAvailableChannels(const QStringList& channelNames) { m_channelAddDialog.addChannelNames(channelNames); }

private:
    QPushButton *m_addChannelButton;
    QWidget *m_titleBar;
    QHBoxLayout *m_titleBarLayout;
    QLabel *m_titleLabel;
    QPushButton *m_normalButton;
    QPushButton *m_closeButton;
    ChannelAddDialog m_channelAddDialog;

private slots:
    void toggleFloating();
    void addChannelDialog();
    void addChannelEmitted(int channelIndex);

signals:
    void addChannel(int);
};

#endif // SDRGUI_GUI_CHANNELDOCK_H_
