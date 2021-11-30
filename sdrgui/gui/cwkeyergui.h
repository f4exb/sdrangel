///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef SDRBASE_GUI_CWKEYERGUI_H_
#define SDRBASE_GUI_CWKEYERGUI_H_

#include <QWidget>
#include "dsp/dsptypes.h"
#include "export.h"
#include "settings/serializable.h"
#include "dsp/cwkeyersettings.h"

namespace Ui {
    class CWKeyerGUI;
}

class QLabel;
class MessageQueue;
class CWKeyer;
class CWKeyerSettings;
class CommandKeyReceiver;

class SDRGUI_API CWKeyerGUI : public QWidget, public Serializable {
    Q_OBJECT

public:
    explicit CWKeyerGUI(QWidget* parent = nullptr);
    ~CWKeyerGUI();

    void setCWKeyer(CWKeyer* cwKeyer);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);

    void setSettings(const CWKeyerSettings& settings) { m_settings = settings; }
    void displaySettings();

private:
    enum KeyScope
    {
        NoKeyScope,
        DotKeyScope,
        DashKeyScope
    };

    Ui::CWKeyerGUI* ui;

    CWKeyer* m_cwKeyer;
    CWKeyerSettings m_settings;
    bool m_doApplySettings;
    CommandKeyReceiver *m_commandKeyReceiver;
    KeyScope m_keyScope;

    void applySettings(bool force = false);
    void blockApplySettings(bool block);
    void setKeyLabel(QLabel *label, Qt::Key key, Qt::KeyboardModifiers keyModifiers);

private slots:
    void on_cwTextClear_clicked(bool checked);
    void on_cwTextEdit_editingFinished();
    void on_cwSpeed_valueChanged(int value);
    void on_playDots_toggled(bool checked);
    void on_playDashes_toggled(bool checked);
    void on_playText_toggled(bool checked);
    void on_playLoopCW_toggled(bool checked);
    void on_playStop_toggled(bool checked);
    void on_keyingStyle_toggled(bool checked);
    void on_keyDotCapture_toggled(bool checked);
    void on_keyDashCapture_toggled(bool checked);
    void on_keyboardKeyer_toggled(bool checked);
    void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
    void keyboardKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
};


#endif /* SDRBASE_GUI_CWKEYERGUI_H_ */
