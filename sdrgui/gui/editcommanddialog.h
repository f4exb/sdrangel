///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_GUI_EDITCOMMANDDIALOG_H_
#define SDRGUI_GUI_EDITCOMMANDDIALOG_H_

#include <QDialog>
#include <vector>

namespace Ui {
    class EditCommandDialog;
}

class Command;
class CommandKeyReceiver;

class EditCommandDialog : public QDialog {
    Q_OBJECT

public:
    explicit EditCommandDialog(const QStringList& groups, const QString& group, QWidget* parent = 0);
    ~EditCommandDialog();

    QString getGroup() const;
    void setGroup(const QString& group);
    QString getDescription() const;
    void setDescription(const QString& description);
    QString getCommand() const;
    void setCommand(const QString& command);
    QString getArguments() const;
    void setArguments(const QString& arguments);
    Qt::Key getKey() const;
    Qt::KeyboardModifiers getKeyModifiers() const;
    void setKey(Qt::Key key, Qt::KeyboardModifiers modifiers);
    bool getAssociateKey() const;
    void setAssociateKey(bool associate);
    bool getRelease() const;
    void setRelease(bool release);

    void toCommand(Command& command) const;
    void fromCommand(const Command& command);

private:
    Ui::EditCommandDialog* ui;
    Qt::Key m_key;
    Qt::KeyboardModifiers m_keyModifiers;
    CommandKeyReceiver *m_commandKeyReceiver;

    void setKeyLabel();
    void setKeyAssociate();

private slots:
    void on_showFileDialog_clicked(bool checked);
    void on_keyCapture_toggled(bool checked);
    void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
};



#endif /* SDRGUI_GUI_EDITCOMMANDDIALOG_H_ */
