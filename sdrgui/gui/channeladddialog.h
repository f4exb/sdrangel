///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_GUI_CHANNELADDDIALOG_H_
#define SDRGUI_GUI_CHANNELADDDIALOG_H_

#include <QDialog>
#include <QStringList>
#include <vector>

#include "export.h"

class QAbstractButton;

namespace Ui {
    class ChannelAddDialog;
}

class SDRGUI_API ChannelAddDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChannelAddDialog(QWidget* parent = nullptr);
    ~ChannelAddDialog();

    void resetChannelNames();
    void addChannelNames(const QStringList& channelNames);

private:
    Ui::ChannelAddDialog* ui;
    std::vector<int> m_channelIndexes;

private slots:
    void apply(QAbstractButton*);

signals:
    void addChannel(int channelPluginIndex);
};

#endif /* SDRGUI_GUI_CHANNELADDDIALOG_H_ */
