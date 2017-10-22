///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_GUI_CWKEYERGUI_H_
#define SDRBASE_GUI_CWKEYERGUI_H_

#include <QWidget>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include "settings/serializable.h"

namespace Ui {
    class CWKeyerGUI;
}

class MessageQueue;
class CWKeyer;

class SDRANGEL_API CWKeyerGUI : public QWidget, public Serializable {
    Q_OBJECT

public:
    explicit CWKeyerGUI(QWidget* parent = NULL);
    ~CWKeyerGUI();

    void setBuddies(MessageQueue* messageQueue, CWKeyer* cwKeyer);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

private:
    Ui::CWKeyerGUI* ui;

    MessageQueue* m_messageQueue;
    CWKeyer* m_cwKeyer;

    void applySettings();

private slots:
    void on_cwTextClear_clicked(bool checked);
    void on_cwTextEdit_editingFinished();
    void on_cwSpeed_valueChanged(int value);
    void on_playDots_toggled(bool checked);
    void on_playDashes_toggled(bool checked);
    void on_playText_toggled(bool checked);
    void on_playLoopCW_toggled(bool checked);
    void on_playStop_toggled(bool checked);
};


#endif /* SDRBASE_GUI_CWKEYERGUI_H_ */
