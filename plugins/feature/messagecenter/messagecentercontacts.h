///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_FEATURE_MESSAGECENTERCONTACTS_H_
#define INCLUDE_FEATURE_MESSAGECENTERCONTACTS_H_

#include <QDialog>
#include <QIcon>
#include <QJsonObject>
#include <QStringList>

#include <vector>

#include "messagecenterengine.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QToolButton;

// The icons of the contacts: the built-in ones (Microsoft Fluent Emoji, flat, MIT License) and image files
struct MessageCenterContactIcons
{
    //!< The icon of a contact: :name built-in, else an image file; a null icon for none
    static QIcon icon(const QString& icon);
};

// Picks an icon: the built-in ones by category, an image file or none
class MessageCenterIconPicker : public QDialog
{
    Q_OBJECT
public:
    MessageCenterIconPicker(const QString& current, QWidget *parent = nullptr);
    QString icon() const { return m_icon; }

private:
    QString m_icon;
};

// Edits a contact: its name, its rules (conditions on the fields of the messages), its icon, colour and note
class MessageCenterContactDialog : public QDialog
{
    Q_OBJECT
public:
    //!< otherNames: the names of the other contacts (a name is unique). fields: the fields offered (those of the
    //!< messages of the log). log: the fields of messages of the log (MessageCenterContact::recordFields), to count
    //!< those each rule finds
    MessageCenterContactDialog(const MessageCenterContact& contact, const QStringList& otherNames, const QStringList& fields,
        const std::vector<QJsonObject>& log, const QStringList& transmitters, QWidget *parent = nullptr);
    MessageCenterContact contact() const;
    //!< The operators of a condition in a combo box (each with its operator as data), the current one selected
    static void fillOperators(QComboBox *combo, MessageCenterContact::Condition::Operator current);

private:
    MessageCenterContact m_contact;
    QStringList m_otherNames;
    QStringList m_fields;
    const std::vector<QJsonObject>& m_log;
    QLineEdit *m_name;
    QTableWidget *m_conditions;         //!< A row a condition: its rule, field, is or matches, value, what it finds
    QTableWidget *m_sendTo;             //!< A row a send to, in order: a condition (none: always) and the page it gives
    QStringList m_transmitters;
    QToolButton *m_icon;
    QToolButton *m_color;
    QLineEdit *m_note;
    QLabel *m_error;

    enum ConditionColumn { ColRule, ColField, ColOperator, ColValue, ColCheck };
    enum SendToColumn { SendWhenField, SendWhenOperator, SendWhenValue, SendTransmitter, SendProtocol, SendAddress, SendFunction, SendPolarity,
        SendCheck };

    void addSendToRow(const MessageCenterContact::SendTo& sendTo);
    MessageCenterContact::SendTo rowSendTo(int row) const;
    //!< The address of each send to checked (the check of the pages), shown in its row
    void showSendToChecks();

    void showIcon();
    void showColor();
    void addConditionRow(int rule, const MessageCenterContact::Condition& condition);
    //!< The rule (1...) and the condition of a row
    int rowRule(int row) const;
    MessageCenterContact::Condition rowCondition(int row) const;
    //!< Each row: the regular expression checked, the messages of the log its rule finds
    void showChecks();
    bool check();
    void accept() override;
    //!< A field of a row that gets the focus selects its row (for Remove)
    bool eventFilter(QObject *object, QEvent *event) override;
    //!< The row selected in a table, -1 for none
    static int selectedRow(const QTableWidget *table);

    bool m_selectingRow = false;
};

#endif // INCLUDE_FEATURE_MESSAGECENTERCONTACTS_H_
