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

#ifndef SDRGUI_GUI_PAGINGMESSAGETABLE_H_
#define SDRGUI_GUI_PAGINGMESSAGETABLE_H_

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPersistentModelIndex>
#include <QMenu>
#include <QString>
#include <QTableWidget>

#include "export.h"
#include "util/messagerecord.h"

class FeatureGUI;
class QMouseEvent;
class QStyledItemDelegate;
class QResizeEvent;
class QToolButton;
class PagingVoicePlayer;

// The messages of the paging channels and of the message center (POCSAG, FLEX, GSC) as a table: the same
// columns and the same chips for the received and the transmitted messages.
//
// A record is a row, found by its id: the record that completes a message (the end of a voice message, the
// state of a transmitted message, a transmitted copy) updates its row. Each user shows the columns it needs;
// the header menu shows and hides them, and columns() and setColumns() keep the order, the sizes and what is
// hidden. Duplicates are grey, without chips. A voice message with a file has a play icon in its message cell:
// a click plays the file on the audio output of the table (PagingVoicePlayer), or stops it; the right click menu
// has the audio output.
class SDRGUI_API PagingMessageTable : public QTableWidget
{
    Q_OBJECT
public:
    enum Column {
        ColTime,
        ColDirection,
        ColChannel,
        ColProtocol,
        ColRate,
        ColPolarity,
        ColAddress,
        ColType,
        ColMessage,
        ColFragment,
        ColInfo,
        ColErrors,
        ColState,
        ColDate,            //!< Last in the enumeration so saved column states keep their indexes; shown before Time
        ColContact,         //!< The contact of the address book (Message Center): its icon and name
        ColumnCount
    };

    explicit PagingMessageTable(QWidget *parent = nullptr);

    //!< Columns shown when none were saved
    void setDefaultColumns(const QList<Column>& shown);
    //!< The order, the sizes and the hidden columns, as saved by the settings of the user
    QByteArray columns() const;
    bool setColumns(const QByteArray& state);
    //!< Rows kept, the oldest go first
    void setMaxRows(int rows) { m_maxRows = rows; }
    //!< The messages on one line, cut at the edge of their column (the whole text in the tool tip and the details): rows
    //!< of one height. Else they wrap to the width of their column
    void setSingleLineMessages(bool single);
    bool singleLineMessages() const { return m_singleLineMessages; }
    //!< The address column shows the contact of the address book (its icon and name) when the message has one
    void setContactInAddress(bool contact) { m_contactInAddress = contact; }
    //!< Actions of the owner of the table added to the menu of a row, for its message
    void setRowMenuExtender(const std::function<void(QMenu&, const MessageRecord&)>& extender) { m_rowMenuExtender = extender; }
    //!< The record is a row, or updates the row of its id. Newest at the bottom: the view follows while it is at the
    //!< bottom. While a row is selected or the view is not at the bottom, a new record waits (the rows shown do not
    //!< move) and a button says how many: they come when it is pressed, or the view is at the bottom again with no
    //!< row selected. A table being made again (updates disabled) takes them at once. State: the text of the state
    //!< column (queued, sent, ...), none for the received messages
    void addRecord(const MessageRecord& record, const QString& state = QString());
    //!< The records waiting are shown, the view follows again
    void showWaiting();
    int waiting() const { return (int) m_waiting.size(); }
    //!< Sizes the columns to their usual content (when no sizes were saved)
    void sizeColumns();
    //!< The id of the record of the selected row, empty if none; selects the row of an id
    QString selectedId() const;
    void selectId(const QString& id);
    //!< The addresses of the record of a row: its address and the members of a group message
    QStringList rowAddresses(int row) const;
    //!< The row of the record of an id, -1 if none
    int findRow(const QString& id) const;
    //!< The background of every cell of the row of an id (a notification that matched)
    void setRowBackground(const QString& id, const QColor& color);
    //!< Plain text of the record and the chips of its protocol fields (HTML), and the tool tip of the chips
    static void recordInfo(const MessageRecord& record, QString& plain, QString& html, QString& tip);
    //!< The chips of the protocol fields of a record: the label, the HTML and the tool tip of each
    static void recordChips(const MessageRecord& record, QStringList& texts, QStringList& chips, QStringList& tips);
    //!< The text of the row of the record
    static QString recordText(const MessageRecord& record);
    //!< The text as HTML, coloured by the status of each character (. received, c corrected, X lost); escaped only
    //!< when the status is not one for each character
    static QString statusHtml(const QString& text, const QString& charStatus);
    //!< The audio output of the voice files, by its name in AudioDeviceManager
    void setAudioDevice(const QString& deviceName);
    const QString& audioDevice() const;
    //!< Plays the voice file of a row, or stops it if it is being played
    void toggleVoice(int row);
    //!< A window with every field of the record of a row: the common fields, protocolData and the raw payload
    void showDetails(int row);
    //!< The window of every field of a record, from its JSON (MessageRecord::toJson)
    static void showRecord(const QString& json, QWidget *parent);
    //!< For a view of records on a model: the HTML of a cell in this role is drawn (chips, colours), and its text
    //!< stays plain; the delegate that draws it
    static const int m_htmlRole = Qt::UserRole + 1;
    static QStyledItemDelegate *createHtmlDelegate(QObject *parent);
    //!< The chip of a message type (ALPHA, NUM, TONE...), HTML
    static QString typeChipHtml(const QString& type);
    //!< Shows the Message Center window, adding the feature first if there is none
    static void openMessageCenter();
    //!< The Message Center button of a paging channel: green while there is a Message Center (every one takes the
    //!< messages of all the paging channels), its icon otherwise; a click opens it (openMessageCenter)
    static void setupMessageCenterButton(QToolButton *button);
    //!< The route dialog of the Message Center filled from a message: its protocol, polarity, type and address, decoded
    //!< as the codepage (vendor, model and codepage as in the records; empty: as received) and the type (empty: as
    //!< received). Nothing without a Message Center
    static void createRoute(const MessageRecord& record, const QVariantMap& charset, const QString& type);
    //!< A window with the message decoded with every codepage of its protocol (and as numeric), a row each
    void showAllCodepages(const MessageRecord& record);
    //!< A window with the members of a group message: searched, copied, an address filter on one of them
    void showGroupMembers(const MessageRecord& record);

signals:
    //!< The user moved, resized, showed or hid a column
    void columnsChanged();
    //!< The user chose another audio output for the voice files
    void audioDeviceChanged(const QString& deviceName);
    //!< The user decoded a received message again (another codepage or type): its row shows it
    void recordRedecoded(const MessageRecord& record);
    //!< The user asked for the messages of an address (a member of a group): the address filter of the window
    void addressFilterRequested(const QString& address);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    //!< The tool tip of the chip under the mouse in the info column
    bool viewportEvent(QEvent *event) override;

private:
    int m_maxRows = 1000;
    bool m_contactInAddress = false;
    bool m_singleLineMessages = false;
    std::function<void(QMenu&, const MessageRecord&)> m_rowMenuExtender;
    //! New records that wait while the view does not follow (a row selected, or scrolled up), with their state
    std::vector<std::pair<MessageRecord, QString>> m_waiting;
    QToolButton *m_waitingButton;

    //! Follows the newest rows: off when the user scrolls up, on when the user scrolls to the bottom or shows the
    //! records waiting
    bool m_following = true;
    //! The view does not follow: a row is selected or the user scrolled up
    bool holding() const;
    //! A new record as a row; the oldest rows go over the maximum
    void appendRecord(const MessageRecord& record, const QString& state);
    void showWaitingButton();
    //!< The row of each id (its time cell), kept by the model when rows go; an id not there has no row
    mutable QHash<QString, QPersistentModelIndex> m_rowsById;
    QList<Column> m_defaultColumns;
    QMenu *m_columnMenu;
    PagingVoicePlayer *m_player;

    void fillRow(int row, const MessageRecord& record, const QString& state);
    void fitNarrowColumns();
    void showColumnMenu(const QPoint& pos);
    void showRowMenu(const QPoint& pos);
    void setVoiceCell(int row);
    void displayPlayer();
    void selectAudioDevice(const QPoint& pos);
    static FeatureGUI *messageCenterGui();
};

#endif // SDRGUI_GUI_PAGINGMESSAGETABLE_H_
