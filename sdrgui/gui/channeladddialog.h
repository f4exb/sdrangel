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

#ifndef SDRGUI_GUI_CHANNELADDDIALOG_H_
#define SDRGUI_GUI_CHANNELADDDIALOG_H_

#include <QDialog>
#include <vector>

#include "export.h"

class QStringList;
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
