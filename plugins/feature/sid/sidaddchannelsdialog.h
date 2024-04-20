///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_SIDADDCHANNELSDIALOG_H
#define INCLUDE_SIDADDCHANNELSDIALOG_H

#include "channel/channelapi.h"
#include "ui_sidaddchannelsdialog.h"

#include "sidsettings.h"

class SIDAddChannelsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SIDAddChannelsDialog(SIDSettings *settings, QWidget* parent = 0);
    ~SIDAddChannelsDialog();

private:

private slots:
    void accept();

    void channelAdded(int deviceSetIndex, ChannelAPI *channel);

private:
    Ui::SIDAddChannelsDialog* ui;

    enum Column {
        COL_TX_NAME,
        COL_TX_FREQUENCY,
        COL_DEVICE
    };

    SIDSettings *m_settings;

    int m_row;          // Row and column in table, when adding channels
    int m_col;
    int m_count;        // How many channels we've added

    void addNextChannel();
    void nextChannel();
};

#endif // INCLUDE_SIDADDCHANNELSDIALOG_H
