///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_CHANNEL_CHANNELGUI_H_
#define SDRGUI_CHANNEL_CHANNELGUI_H_

#include "gui/rollupwidget.h"
#include "export.h"

class QCloseEvent;
class MessageQueue;

class SDRGUI_API ChannelGUI : public RollupWidget
{
    Q_OBJECT
public:
	ChannelGUI(QWidget *parent = nullptr) :
        RollupWidget(parent)
    { }
	virtual ~ChannelGUI() { }
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

protected:
    void closeEvent(QCloseEvent *event);

signals:
    void closing();
};

#endif // SDRGUI_CHANNEL_CHANNELGUI_H_
