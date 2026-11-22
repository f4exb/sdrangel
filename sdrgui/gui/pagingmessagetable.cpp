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

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QToolButton>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>
#include <QTreeWidget>
#include <QIcon>
#include <QVBoxLayout>

#include <algorithm>

#include "dsp/dspengine.h"
#include "feature/feature.h"
#include "feature/featuregui.h"
#include "feature/featureset.h"
#include "maincore.h"
#include "feature/featurewebapiutils.h"
#include "gui/audioselectdialog.h"
#include "gui/dialogpositioner.h"
#include "radiopagingpage.h"
#include "radiopagingrecode.h"
#include "pagingvoiceplayer.h"
#include "pagingmessagetable.h"

namespace {

// The cells with chips and colours keep their HTML in this role; the display text stays plain (copy, width)
const int htmlRole = PagingMessageTable::m_htmlRole;
const int voiceFileRole = Qt::UserRole + 2;     // On the time item of a voice message: the file recorded
const int jsonRole = Qt::UserRole + 3;          // On the time item: the record as JSON
const int tipRole = Qt::UserRole + 4;           // On the message item: its tool tip without the voice file
const int chipTextsRole = Qt::UserRole + 5;     // On the info item: the label of each chip
const int chipTipsRole = Qt::UserRole + 6;      // On the info item: the tool tip of each chip
const int marksIconRole = Qt::UserRole + 7;     // On the message item: the icons of its marks (a voice message: its play icon)

// The messages wrap to the width of their column, unless the table shows them on one line
bool messagesWrap(const QObject *view)
{
    const PagingMessageTable *table = qobject_cast<const PagingMessageTable*>(view);
    return !table || !table->singleLineMessages();
}

class HtmlDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QString html = index.data(htmlRole).toString();

        if (html.isEmpty())
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();

        // The icons of the marks side by side: at their width, not squeezed into the square of an icon
        if ((index.column() == PagingMessageTable::ColMessage) && !opt.icon.isNull())
        {
            const QSize size = opt.icon.availableSizes().value(0);

            // initStyleOption fitted it into the square: its height is the side of the square
            const int side = std::max(opt.decorationSize.width(), opt.decorationSize.height());

            if (size.isValid() && (size.height() > 0)) {
                opt.decorationSize = QSize(side * size.width() / size.height(), side);
            }
        }
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        // The text beside the icon, if the cell has one. The message wraps to the width of its column
        QRect textRect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        // Selected: the text in the selected text colour of SDRangel (black on its orange), as its other tables; the
        // marks of the errors in darker colours, underlined, that stay readable on the orange
        const bool selected = opt.state.testFlag(QStyle::State_Selected);

        if (selected)
        {
            html.replace("color:#ff5252;", "color:#8b0000;text-decoration:underline;");
            html.replace("color:#ff5252\"", "color:#8b0000;text-decoration:underline\"");
            html.replace("color:#ffca28\"", "color:#5c3a00;text-decoration:underline\"");
            // The light text of the rule sentences: the keywords, the values, the counts
            html.replace("color:#88aacc\"", "color:#0d2a55\"");
            html.replace("color:#e0e0e0\"", "color:#000000\"");
            html.replace("color:#888888\"", "color:#4a2a00\"");
            html.replace("color:#777777\"", "color:#4a2a00\"");
        }

        QTextDocument doc;
        doc.setDocumentMargin(1);
        doc.setDefaultFont(opt.font);
        doc.setHtml(html);
        if ((index.column() == PagingMessageTable::ColMessage) && messagesWrap(parent())) {
            doc.setTextWidth(textRect.width());
        }
        painter->save();
        // Never over the next cells: a text higher than its row (until the row is sized again) is cut
        painter->setClipRect(opt.rect);
        painter->translate(textRect.left(), opt.rect.top() + (opt.rect.height() - doc.size().height()) / 2);
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = opt.palette;
        // The colour group the style draws the plain cells of the row with (FastItemDelegate does the same)
        const QPalette::ColorGroup group = opt.state.testFlag(QStyle::State_Enabled)
            ? (opt.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive) : QPalette::Disabled;
        context.palette.setColor(QPalette::Text, opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        context.clip = QRectF(0, 0, textRect.width(), opt.rect.height());
        painter->setClipRect(context.clip, Qt::IntersectClip);
        doc.documentLayout()->draw(painter, context);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QString html = index.data(htmlRole).toString();

        if (html.isEmpty()) {
            return QStyledItemDelegate::sizeHint(option, index);
        }

        // The message: as high as its text wrapped to the width of its column
        // (less the width of its icon, the marks)
        const QTableView *view = qobject_cast<const QTableView *>(parent());
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const int iconWidth = icon.isNull() ? 0 : icon.availableSizes().value(0, QSize(16, 16)).width() + 4;
        const int textWidth = (view && (index.column() == PagingMessageTable::ColMessage) && messagesWrap(parent()))
            ? std::max(20, view->columnWidth(index.column()) - 8 - iconWidth) : -1;

        // Laying out HTML is slow: the size of the same HTML at the same width and font is kept (the chips and the
        // colours of thousands of rows are a few hundred different cells)
        const QString key = QString::number(textWidth) + '|' + option.font.key() + '|' + html;
        auto cached = m_sizes.constFind(key);

        if (cached != m_sizes.constEnd()) {
            return cached.value();
        }

        QTextDocument doc;
        doc.setDocumentMargin(1);
        doc.setDefaultFont(option.font);
        doc.setHtml(html);
        int width = (int) doc.idealWidth() + 4;

        if (textWidth > 0) {
            doc.setTextWidth(textWidth);
        }

        QSize size(width, std::max((int) doc.size().height(), QStyledItemDelegate::sizeHint(option, index).height()));

        if (m_sizes.size() > m_maxSizes) {
            m_sizes.clear();
        }

        m_sizes.insert(key, size);
        return size;
    }

private:
    static const int m_maxSizes = 20000;
    mutable QHash<QString, QSize> m_sizes;
};

QString chip(const QString& text, const QString& fg, const QString& bg)
{
    return QString("<span style=\"color:%1;background-color:%2;font-weight:bold\">&nbsp;%3&nbsp;</span>")
        .arg(fg, bg, text.toHtmlEscaped());
}

QString typeChip(const QString& type)
{
    static const QMap<QString, QPair<QString, QString>> labels = {
        {"alphanumeric", {"ALPHA", "#a5d6a7|#1b5e20"}},
        {"numeric", {"NUM", "#90caf9|#1a237e"}},
        {"tone", {"TONE", "#ce93d8|#4a148c"}},
        {"voice", {"VOICE", "#81d4fa|#01579b"}},
        {"secure", {"SEC", "#cfd8dc|#37474f"}},
        {"special_numeric", {"SPEC", "#90caf9|#1a237e"}},
        {"numbered_numeric", {"NNUM", "#90caf9|#1a237e"}},
        {"numbered_special_numeric", {"NSPEC", "#90caf9|#1a237e"}},
        {"binary", {"BIN", "#cfd8dc|#37474f"}},
        {"short_message", {"SMSG", "#cfd8dc|#37474f"}},
        {"short_instruction", {"SI", "#cfd8dc|#37474f"}}
    };
    auto it = labels.find(type);
    QString label = (it != labels.end()) ? it->first : type.toUpper();
    QStringList colours = ((it != labels.end()) ? it->second : QString("#cfd8dc|#37474f")).split('|');
    return chip(label, colours[0], colours[1]);
}

// Info chips, all alike: bold on the same background, KEY:value with the key grey and the value coloured by its
// meaning, a flag one word in its colour. The plain text is the label, character for character
enum ChipTone {
    ToneValue,      //!< A value or a flag that is neither good nor bad
    ToneGood,       //!< A check that passed, a better copy
    ToneWarn,       //!< Something to look at: truncated, duplicate, a copy, a check not done
    ToneBad,        //!< A check that failed, messages missed
    ToneGroup       //!< A group message or assignment
};

QString fieldChip(const QString& label, int tone)
{
    static const char *colours[] = {"#d1c4e9", "#66bb6a", "#ffb74d", "#ff5252", "#80cbc4"};
    const QString colour = colours[tone];
    const int colon = label.indexOf(':');
    const QString body = (colon < 0)
        ? QString("<span style=\"color:%1\">%2</span>").arg(colour, label.toHtmlEscaped())
        : QString("<span style=\"color:#8a8a8a\">%1</span><span style=\"color:%2\">%3</span>")
            .arg(label.left(colon + 1).toHtmlEscaped(), colour, label.mid(colon + 1).toHtmlEscaped());
    return QString("<span style=\"background-color:#2e2e2e;font-weight:bold\">&nbsp;%1&nbsp;</span>").arg(body);
}

QString infoChip(const QString& text)
{
    return fieldChip(text, ToneValue);
}

// A check result (ok, failed, incomplete) as the value of a chip and its tone
QString checkValue(const QString& check)
{
    return (check == "ok") ? "ok" : (check == "failed") ? "bad" : "?";
}

int checkTone(const QString& check)
{
    return (check == "ok") ? ToneGood : (check == "failed") ? ToneBad : ToneWarn;
}

const char *headers[PagingMessageTable::ColumnCount] = {
    "Time", "Dir", "Channel", "Proto", "Rate", "Pol", "Address", "Type", "Message", "Fragment", "Info", "Quality", "State", "Date",
    "Contact"
};

// The icon of a contact: a built-in one (:name) is a resource of the Message Center, else an image file. Kept: the
// same few icons are on many rows
QIcon contactIcon(const QString& icon)
{
    static QHash<QString, QIcon> icons;

    if (icon.isEmpty()) {
        return QIcon();
    }

    auto it = icons.constFind(icon);

    if (it != icons.constEnd()) {
        return it.value();
    }

    const QString path = (icon.startsWith(':') && !icon.startsWith(":/")) ? QString(":/messagecenter/icons/%1.svg").arg(icon.mid(1)) : icon;
    QIcon result(path);
    icons.insert(icon, result);
    return result;
}

// The icons of the marks of a message side by side, in one icon (kept: the same few combinations on many rows)
QIcon marksIcon(const QStringList& names, int size)
{
    static QHash<QString, QIcon> icons;

    if (names.isEmpty()) {
        return QIcon();
    }

    const QString key = QString::number(size) + '|' + names.join('|');
    auto it = icons.constFind(key);

    if (it != icons.constEnd()) {
        return it.value();
    }

    QPixmap pixmap(names.size() * (size + 2) - 2, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    for (int i = 0; i < names.size(); i++) {
        contactIcon(names[i]).paint(&painter, QRect(i * (size + 2), 0, size, size));
    }

    painter.end();
    QIcon result(pixmap);
    icons.insert(key, result);
    return result;
}

} // namespace

PagingMessageTable::PagingMessageTable(QWidget *parent) :
    QTableWidget(0, ColumnCount, parent),
    m_columnMenu(new QMenu(this)),
    m_player(new PagingVoicePlayer(this))
{
    QStringList labels;

    for (int i = 0; i < ColumnCount; i++) {
        labels.append(headers[i]);
    }

    setHorizontalHeaderLabels(labels);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new HtmlDelegate(this));
    // A text longer than its column (a long channel name) is cut with an ellipsis, not wrapped; the message wraps in
    // its delegate
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);
    // Row numbers, as in the Pager demodulator
    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    horizontalHeader()->setSectionsMovable(true);
    // The date before the time
    horizontalHeader()->moveSection(horizontalHeader()->visualIndex(ColDate), horizontalHeader()->visualIndex(ColTime));
    // The contact before the address
    horizontalHeader()->moveSection(horizontalHeader()->visualIndex(ColContact), horizontalHeader()->visualIndex(ColAddress));
    horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &PagingMessageTable::showColumnMenu);
    connect(horizontalHeader(), &QHeaderView::sectionMoved, this, &PagingMessageTable::columnsChanged);
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, &PagingMessageTable::columnsChanged);
    // The messages wrap to the width of their column: the rows follow it
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int, int) {
        if ((logicalIndex == ColMessage) && !m_singleLineMessages) {
            resizeRowsToContents();
        }
    });
    connect(this, &QWidget::customContextMenuRequested, this, &PagingMessageTable::showRowMenu);
    connect(this, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { showDetails(row); });
    connect(m_player, &PagingVoicePlayer::stateChanged, this, &PagingMessageTable::displayPlayer);

    m_defaultColumns = {ColDate, ColTime, ColProtocol, ColAddress, ColType, ColMessage, ColInfo};
    fitNarrowColumns();
    setDefaultColumns(m_defaultColumns);

    // The records that wait while the view does not follow: a button in the corner of the view. They come when it is
    // pressed, or when the view is at the bottom with no row selected
    m_waitingButton = new QToolButton(viewport());
    m_waitingButton->setIcon(QIcon(":/arrow_down.png"));
    m_waitingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_waitingButton->setToolTip(tr("New messages wait while a row is selected or the view is scrolled up, so that the rows\n"
        "do not move. Click to show them and follow the newest again"));
    m_waitingButton->hide();
    connect(m_waitingButton, &QToolButton::clicked, this, [this]() {
        clearSelection();
        m_following = true;
        showWaiting();
    });
    auto release = [this]() {
        if (!m_waiting.empty() && !holding()) {
            showWaiting();
        }
    };
    // Following the newest is a state the user changes: scrolling up stops it, scrolling to the bottom starts it again.
    // Not read from the position: rows added move the bottom a moment later, which is no scroll of the user. The
    // action is applied after the signal: the position is read then
    connect(verticalScrollBar(), &QScrollBar::actionTriggered, this, [this, release]() {
        QTimer::singleShot(0, this, [this, release]() {
            const QScrollBar *sb = verticalScrollBar();
            m_following = (sb->value() >= sb->maximum());
            release();
        });
    });
    // Rows added, made higher, the table resized: a table that follows stays at the bottom
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this]() {
        if (m_following && !selectionModel()->hasSelection()) {
            scrollToBottom();
        }
    });
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, release);
}

bool PagingMessageTable::holding() const
{
    return selectionModel()->hasSelection() || !m_following;
}

void PagingMessageTable::showWaitingButton()
{
    m_waitingButton->setText(tr("%n new", "", (int) m_waiting.size()));
    m_waitingButton->adjustSize();
    m_waitingButton->move(viewport()->width() - m_waitingButton->width() - 4, viewport()->height() - m_waitingButton->height() - 4);
    m_waitingButton->setVisible(!m_waiting.empty());
    m_waitingButton->raise();
}

void PagingMessageTable::resizeEvent(QResizeEvent *event)
{
    // A table following the newest messages keeps following them when its size changes (the window laid out when it
    // opens, made higher)
    QTableWidget::resizeEvent(event);

    if (m_following && !selectionModel()->hasSelection()) {
        QTimer::singleShot(0, this, [this]() { scrollToBottom(); });
    }
    if (!m_waiting.empty()) {
        showWaitingButton();
    }
}

void PagingMessageTable::showWaiting()
{
    std::vector<std::pair<MessageRecord, QString>> waiting;
    waiting.swap(m_waiting);

    // One that has a row by now (the table was made again meanwhile) updates it
    for (const auto& w : waiting)
    {
        const int row = findRow(w.first.m_id);

        if (row >= 0) {
            fillRow(row, w.first, w.second);
        } else {
            appendRecord(w.first, w.second);
        }
    }

    m_waitingButton->hide();
    scrollToBottom();
}

void PagingMessageTable::setDefaultColumns(const QList<Column>& shown)
{
    m_defaultColumns = shown;

    for (int i = 0; i < ColumnCount; i++) {
        setColumnHidden(i, !shown.contains((Column) i));
    }
}

QByteArray PagingMessageTable::columns() const
{
    return horizontalHeader()->saveState();
}

bool PagingMessageTable::setColumns(const QByteArray& state)
{
    if (state.isEmpty()) {
        return false;
    }

    QSignalBlocker blocker(horizontalHeader());
    bool restored = horizontalHeader()->restoreState(state);
    fitNarrowColumns();
    return restored;
}

// The narrow columns (direction, polarity) are as wide as their widest content, whatever was saved. A fixed width:
// a column sized to its contents measures all its rows at each row added
void PagingMessageTable::fitNarrowColumns()
{
    auto chipWidth = [this](const QString& html) {
        QTextDocument doc;
        doc.setDocumentMargin(1);
        doc.setDefaultFont(font());
        doc.setHtml(html);
        return (int) doc.idealWidth() + 4;
    };
    // A cell of plain text as the item delegate sizes it (the style's margins), and the header with its sort space
    auto textWidth = [this](const QString& text) {
        QStyleOptionViewItem opt;
        opt.initFrom(this);
        opt.font = font();
        opt.features = QStyleOptionViewItem::HasDisplay;
        opt.text = text;
        return style()->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), this).width() + 4;
    };
    auto headerWidth = [this](int column) {
        const QTableWidgetItem *header = horizontalHeaderItem(column);
        QStyleOptionHeader opt;
        opt.initFrom(horizontalHeader());
        opt.text = header ? header->text() : QString();
        return style()->sizeFromContents(QStyle::CT_HeaderSection, &opt, QSize(), horizontalHeader()).width() + 4;
    };
    const int direction = std::max({chipWidth(chip("QU", "#1a1a0d", "#cccc66")) + 8, chipWidth(chip("NS", "#2a0d0d", "#ff7777")) + 8,
        textWidth("RX"), textWidth("TX"), headerWidth(ColDirection)});
    const int polarity = std::max({chipWidth(chip("I", "#2a1f00", "#ffcc44")) + 8, chipWidth(chip("N", "#0d1a2a", "#88aacc")) + 8,
        headerWidth(ColPolarity)});

    horizontalHeader()->setSectionResizeMode(ColDirection, QHeaderView::Fixed);
    horizontalHeader()->setSectionResizeMode(ColPolarity, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(ColDirection, direction);
    horizontalHeader()->resizeSection(ColPolarity, polarity);
}

// The columns sized to the usual content: a row of the longest values, taken out again
void PagingMessageTable::sizeColumns()
{
    int row = rowCount();
    setRowCount(row + 1);

    const char *cells[ColumnCount] = {
        "00:00:00", "TX", "Channel alias", "POCSAG", "3200/4", "I", "0001234567A", "VOICE",
        "ABCEDGHIJKLMNOPQRSTUVWXYZABCDEFGHIJ", "REASM 99", "PH:A C:14 F:127 N:64 R:1 ADDR:long K:ok S:ok DUP:same", "X99", "transmitted",
        "Wed Sep 30 2026", "WW Fire station 3"
    };

    for (int c = 0; c < ColumnCount; c++) {
        setItem(row, c, new QTableWidgetItem(cells[c]));
    }

    // The info column is as wide as its chips are drawn (bold, with padding), not as their plain text
    QStringList infoChips;
    for (const QString& t : QString(cells[ColInfo]).split(' ')) {
        infoChips.append(infoChip(t));
    }
    item(row, ColInfo)->setData(htmlRole, infoChips.join(" "));

    QSignalBlocker blocker(horizontalHeader());
    resizeColumnsToContents();
    removeRow(row);
    fitNarrowColumns();
}

int PagingMessageTable::findRow(const QString& id) const
{
    if (id.isEmpty()) {
        return -1;
    }

    // The rows are added by addRecord, which keeps their index; a row removed (or all of them) leaves its index
    // not valid
    auto it = m_rowsById.find(id);

    if (it == m_rowsById.end()) {
        return -1;
    }

    const int row = it->isValid() ? it->row() : -1;
    const QTableWidgetItem *item = (row >= 0) ? this->item(row, ColTime) : nullptr;

    if (!item || (item->data(Qt::UserRole).toString() != id))
    {
        m_rowsById.erase(it);
        return -1;
    }

    return row;
}

void PagingMessageTable::setRowBackground(const QString& id, const QColor& color)
{
    int row = findRow(id);

    if (row < 0) {
        return;
    }

    for (int col = 0; col < columnCount(); col++)
    {
        if (QTableWidgetItem *cellItem = item(row, col)) {
            cellItem->setBackground(QBrush(color));
        }
    }
}

QString PagingMessageTable::selectedId() const
{
    int row = currentRow();
    return ((row >= 0) && item(row, ColTime)) ? item(row, ColTime)->data(Qt::UserRole).toString() : QString();
}

QStringList PagingMessageTable::rowAddresses(int row) const
{
    MessageRecord record;
    const QTableWidgetItem *timeItem = item(row, ColTime);

    if (!timeItem || !MessageRecord::fromJson(QJsonDocument::fromJson(timeItem->data(jsonRole).toString().toUtf8()).object(), record))
    {
        const QTableWidgetItem *address = item(row, ColAddress);
        return address ? QStringList(address->text()) : QStringList();
    }

    return record.addresses();
}

void PagingMessageTable::selectId(const QString& id)
{
    int row = findRow(id);

    if (row >= 0) {
        selectRow(row);
    }
}

QString PagingMessageTable::recordText(const MessageRecord& record)
{
    const QVariantMap& d = record.m_protocolData;

    if (d.contains("voice_end"))
    {
        QString text = QString("voice ended: %1 after %2 s").arg(d.value("voice_end").toString())
            .arg(d.value("voice_seconds").toDouble(), 0, 'f', 1);
        return text;
    }
    if (record.m_text.isEmpty() && (record.m_msgType == "voice"))
    {
        // The voice sent, recorded by its transmitter
        return d.contains("voice_seconds") ? QString("voice, %1 s").arg(d.value("voice_seconds").toDouble(), 0, 'f', 1)
            : QString("(voice)");
    }
    if (record.m_text.isEmpty() && ((record.m_msgType == "tone") || d.value("tone_only").toBool())) {
        return QString("(tone only)");
    }
    // A FLEX message vector of no words: there is nothing but the vector
    if (record.m_text.isEmpty() && d.value("no_words").toBool()) {
        return QString("(type:%1, no message words)").arg(record.m_msgType);
    }
    // A FLEX short message without digits (STD-43A Table 3.9.2-1): type 1 is a source alone (the pager shows the
    // message it has for it), type 2 a source with a message number and the retrieval flag, type 3 is reserved
    if (record.m_text.isEmpty() && (record.m_msgType == "short_message") && d.contains("short_type"))
    {
        const int type = d.value("short_type").toInt();
        const int source = d.value("short_source").toInt();

        if (type == 1) {
            return QString("(type:source source:%1)").arg(source);
        }
        if (type == 2)
        {
            return QString("(type:numbered source:%1 number:%2%3)").arg(source).arg(d.value("message_number").toInt())
                .arg(d.value("retrieval").toBool() ? " retrieval" : "");
        }
        if (type == 3) {
            return QString("(type:reserved(3) data:0x%1)").arg(record.m_rawData.toHex().constData());
        }
    }
    // A FLEX short instruction: what it tells the pager of the address
    if (record.m_text.isEmpty() && d.contains("instruction_type"))
    {
        const int type = d.value("instruction_type").toInt();

        if (type == 0)
        {
            const int slot = d.value("instruction_slot").toInt();
            return QString("(added to group slot %1, temporary address %2, group message in frame %3)")
                .arg(slot).arg(2029568 + slot).arg(d.value("instruction_frame").toInt(), 3, 10, QChar('0'));
        }
        if (type == 1) {
            return QString("(system event, flags %1)").arg(d.value("event_flags").toUInt(), 3, 16, QChar('0'));
        }

        // Types 2 to 7 are reserved (STD-43A Table 3.9.1-1): the 11 data bits d3-d13 as they are
        const unsigned bits = (d.value("instruction_data").toUInt() >> 3) & 0x7ff;
        return QString("(short instruction type %1, reserved, data 0x%2)").arg(type).arg(bits, 3, 16, QChar('0'));
    }

    return record.m_text;
}

QString PagingMessageTable::statusHtml(const QString& text, const QString& charStatus)
{
    if (charStatus.size() != text.size()) {
        return text.toHtmlEscaped();
    }

    QString html;
    int i = 0;

    while (i < text.size())
    {
        const QChar s = charStatus[i];
        int j = i;

        while ((j < text.size()) && (charStatus[j] == s)) {
            j++;
        }

        const QString run = text.mid(i, j - i).toHtmlEscaped();

        if (s == 'X') {
            html += QString("<span style=\"color:#ff5252;font-weight:bold;background-color:rgba(255,82,82,0.18)\">%1</span>").arg(run);
        } else if (s == 'u') {
            html += QString("<span style=\"color:#ff5252\">%1</span>").arg(run);
        } else if (s == 'c') {
            html += QString("<span style=\"color:#ffca28\">%1</span>").arg(run);
        } else {
            html += run;
        }

        i = j;
    }

    return html;
}

// Chips of the protocol fields of a record, each KEY:value (the key the field's letter in the standard where it has
// one), in the same order for every message: where it was sent, its number, the address and the type details, the
// checks, then its state. A bit flag is shown with value 1 when set. Each chip has a tool tip saying what it is and its
// value
void PagingMessageTable::recordChips(const MessageRecord& record, QStringList& texts, QStringList& chips, QStringList& tips)
{
    const QVariantMap& d = record.m_protocolData;
    auto add = [&](const QString& text, const QString& tipText, int tone = ToneValue) {
        texts.append(text);
        chips.append(fieldChip(text, tone));
        tips.append(tipText);
    };
    auto checkTip = [](const QString& what, const QString& check) {
        if (check == "ok") {
            return tr("%1: ok, it matches the message received").arg(what);
        } else if (check == "failed") {
            return tr("%1: failed, the message received does not match it (a character is wrong)").arg(what);
        }
        return tr("%1: not checked, a word of the message was lost").arg(what);
    };
    auto check = [&](const QString& key, const QString& what, const QString& value) {
        if ((value == "ok") || (value == "failed") || (value == "incomplete")) {
            add(QString("%1:%2").arg(key, checkValue(value)), checkTip(what, value), checkTone(value));
        }
    };

    if (record.m_protocol == "pocsag")
    {
        if (d.contains("batch"))
        {
            int batch = d.value("batch").toInt() + 1;
            add(QString("B:%1").arg(batch), tr("Batch %1: the address codeword was in batch %1 of the transmission "
                "(a batch is a sync codeword and 16 codewords; the first batch after the preamble is 1)").arg(batch));
        }
        // How the message ended: by an idle or address codeword (M.584-2 1.2), nothing shown
        const QString end = d.value("end").toString();

        if (end == "cut")
        {
            add("END:cut", tr("End cut: the signal was lost in the middle of a batch, or two codewords in a row could not be "
                "decoded (M.584-2 2.5.1): the rest of the message may be missing"), ToneWarn);
        }
        else if (end == "batch")
        {
            add("END:batch", tr("End batch: the message filled the last batch and the transmission ended with it, without an "
                "idle codeword after it. M.584-2 1.2 says the last codeword of a transmission should be idle; the network does "
                "not send it. The message is whole"));
        }
    }
    else if (record.m_protocol == "flex")
    {
        // Where: phase, cycle, frame, copies of a multiple transmission
        if (d.contains("phase"))
        {
            QString phase = d.value("phase").toString();
            int cycle = d.value("cycle").toInt();
            int frame = d.value("frame").toInt();
            add(QString("PH:%1").arg(phase), tr("Phase %1: the phase the message was sent in (1600 bps has phase A, "
                "3200 bps A and C, 6400 bps A to D)").arg(phase));
            add(QString("C:%1").arg(cycle), tr("Cycle %1 of 0-14: the frame's cycle (15 cycles of 4 minutes make an hour)").arg(cycle));
            add(QString("F:%1").arg(frame), tr("Frame %1 of 0-127: the frame of its cycle (frames of 1.875 s)").arg(frame));
        }
        if (d.contains("transmissions"))
        {
            int copies = d.value("subframe_copies").toInt();
            int transmissions = d.value("transmissions").toInt();
            add(QString("SUB:%1/%2").arg(copies).arg(transmissions), tr("Subframe copies: the frame is sent %2 times "
                "(multiple transmission); %1 of the copies were received and combined").arg(copies).arg(transmissions));
        }

        // Number: N as the pager shows it (N + 1, STD-43A 3.10.1.3), the retrieval flag R, numbers missed
        if (d.contains("message_number") && (d.value("message_number").toInt() >= 0))
        {
            int number = d.value("message_number").toInt() + 1;
            bool retrieval = d.value("retrieval").toBool();
            add(QString("N:%1").arg(number), tr("Message number %1: the network numbers the messages of each address in "
                "sequence (sent 0-63, shown + 1 as pagers do); a skipped number is a missed message").arg(number));
            add(QString("R:%1").arg(retrieval ? 1 : 0), retrieval
                ? tr("Message retrieval flag 1 (the usual case): the numbers of this address are checked for skipped numbers")
                : tr("Message retrieval flag 0: a retransmission; its number is not checked for skipped numbers"));
        }
        if (d.contains("missed_numbers"))
        {
            QVariantList missed = d.value("missed_numbers").toList();
            QStringList numbers;

            for (const QVariant& n : missed) {
                numbers.append(QString::number(n.toInt() + 1));
            }

            QString text = (missed.size() > 3)
                ? QString("MISS:%1-%2").arg(numbers.first(), numbers.last())
                : QString("MISS:%1").arg(numbers.join(","));
            add(text, tr("Missed messages: %n number(s) of this address skipped before this message (%1), messages "
                "that were not received", "", missed.size()).arg(numbers.join(", ")), ToneBad);
        }

        // Address and type details
        // The address type, always shown (STD-43A 3.8): short, long, temporary (a group), network, operator messaging,
        // information service, reserved
        static const QMap<QString, QPair<QString, const char *>> addrTypes = {
            {"short", {"short", QT_TR_NOOP("Short address: the capcode takes one address word")}},
            {"long", {"long", QT_TR_NOOP("Long address: the capcode takes two address words")}},
            {"temporary", {"temp", QT_TR_NOOP("Temporary address: a group message, sent to the members of a group slot")}},
            {"network", {"network", QT_TR_NOOP("Network address: a message of the network to its pagers")}},
            {"operator", {"operator", QT_TR_NOOP("Operator messaging address")}},
            {"info_service", {"info", QT_TR_NOOP("Information service address")}},
            {"reserved", {"reserved", QT_TR_NOOP("Reserved address")}},
            {"unknown", {"unknown", QT_TR_NOOP("Address type not known")}}
        };
        QString addrType = d.value("addr_type").toString();
        if (addrTypes.contains(addrType)) {
            add(QString("ADDR:%1").arg(addrTypes.value(addrType).first), tr(addrTypes.value(addrType).second));
        }
        if (d.value("priority").toBool()) {
            add("PRI:1", tr("Priority address: sent ahead of the other addresses of the frame"));
        }
        if (d.value("maildrop").toBool()) {
            add("M:1", tr("Mail drop flag M: the message can be handled separately from ordinary messages"));
        }
        if (d.contains("group_slot"))
        {
            QStringList members = d.value("group_capcodes").toStringList();
            bool tracked = d.value("group_tracked").toBool();
            int slot = d.value("group_slot").toInt();
            QString g = QString("G:%1[%2]").arg(slot).arg(tracked ? QString::number(members.size()) : QString("?"));
            // The first members; the window of the members (right click) has them all
            static const int shownMembers = 20;
            QStringList shown = members.mid(0, shownMembers);

            if (members.size() > shownMembers) {
                shown.append(tr("and %1 more").arg(members.size() - shownMembers));
            }

            add(g, tracked ? tr("Group message, group slot %1 (temporary address %2), %3 members:\n%4").arg(slot)
                    .arg(2029568 + slot).arg(members.size()).arg(shown.join("\n"))
                : tr("Group message, group slot %1 (temporary address %2): the group assignment was not received, the members "
                    "are not known").arg(slot).arg(2029568 + slot),
                ToneGroup);
        }
        // A member added to a group: the slot and the frame of the group message
        if (d.contains("instruction_type") && (d.value("instruction_type").toInt() == 0))
        {
            int slot = d.value("instruction_slot").toInt();
            int frame = d.value("instruction_frame").toInt();
            add(QString("G:%1>%2").arg(slot).arg(frame), tr("%1 added to group slot %2 (temporary address %3); the group "
                "message comes in frame %4").arg(record.m_address).arg(slot).arg(2029568 + slot).arg(frame), ToneGroup);
        }
        // The other short instructions: what the type is and the data bits
        if (d.contains("instruction_type") && (d.value("instruction_type").toInt() != 0))
        {
            const int type = d.value("instruction_type").toInt();
            const unsigned data = d.value("instruction_data").toUInt();
            const unsigned bits = (data >> 3) & 0x7ff;
            const QString binary = QString("%1").arg(bits, 11, 2, QChar('0'));
            QString tip = (type == 1)
                ? tr("Short instruction type 1, system event notification (STD-43A Table 3.9.1-1): event flags d3-d13 %1 (0x%2)")
                    .arg(binary).arg(bits, 3, 16, QChar('0'))
                : tr("Short instruction type %1 (i2 i1 i0 = %2): reserved for future use in STD-43A Table 3.9.1-1, not decoded.\n"
                    "Data bits d3-d13: %3 (0x%4, %5); the 14 bits of the vector: 0x%6.\n"
                    "A network may use it for its own purpose; a word with errors corrected (see Errors) may also be damaged")
                    .arg(type).arg(type, 3, 2, QChar('0')).arg(binary).arg(bits, 3, 16, QChar('0')).arg(bits).arg(data, 4, 16, QChar('0'));
            add(QString("SI:%1").arg(type), tip);
        }
        if (d.contains("nid_service_area"))
        {
            QString traffic = QString("%1").arg(d.value("nid_traffic_flags").toInt(), 4, 2, QChar('0'));
            add(QString("NID:A%1/M%2/T%3").arg(d.value("nid_service_area").toInt()).arg(d.value("nid_multiplier").toInt()).arg(traffic),
                tr("Network ID (roaming): service area %1, multiplier %2, traffic flags %3").arg(d.value("nid_service_area").toInt())
                    .arg(d.value("nid_multiplier").toInt()).arg(traffic));
        }
        if (d.value("nid_change").toBool()) {
            add("NID:change", tr("Network ID change instruction"));
        } else if (d.value("nid_system_message").toBool()) {
            add("NID:message", tr("System message of the network ID"));
        }
        if (d.contains("hex_nibble_bits"))
        {
            bool lsb = (d.value("hex_bit_order").toString() == "lsb_first");
            int nibble = d.value("hex_nibble_bits").toInt();
            int bits = d.value("hex_bits").toInt();
            add(QString("HEX:%1/%2/%3").arg(nibble).arg(lsb ? "lsb" : "msb").arg(bits),
                tr("HEX/binary data: %1 bits a digit, %2 bit first, %3 bits").arg(nibble).arg(lsb ? tr("least significant") : tr("most significant")).arg(bits));
        }
        if (d.contains("blocking"))
        {
            int blocking = d.value("blocking").toInt();
            bool rtl = (d.value("display_direction").toString() == "rtl");
            add(QString("BLK:%1").arg(blocking), tr("Blocking length: the data is in blocks of %1 bits").arg(blocking));
            add(rtl ? "D:rtl" : "D:ltr", rtl ? tr("Display direction D: right to left") : tr("Display direction D: left to right"));
            if (d.value("header_message").toBool()) {
                add("H:1", tr("Header message H: a header to a following transparent message of the same message number"));
            }
            if (d.value("status_info").toBool()) {
                add("STAT:1", tr("Status information message"));
            }
        }
        if (d.contains("secure_type"))
        {
            QString t = QString("%1%2").arg(d.value("secure_type").toInt() >> 1 & 1).arg(d.value("secure_type").toInt() & 1);
            add(QString("T:%1").arg(t), tr("Secure message type bits t1 t0 = %1").arg(t));
        }
        if (d.contains("fragment_control") && (d.value("fragment_control").toString() != "00"))
        {
            QString uv = d.value("fragment_control").toString();
            add(QString("UV:%1").arg(uv), tr("Fragment control bits U V = %1 (enhanced fragmentation)").arg(uv));
        }
        if (d.value("registration_ack").toBool()) {
            add("REG:ack", tr("Registration acknowledgement"));
        }
        if (d.value("payload_unknown").toBool()) {
            add("LAYOUT:unknown", tr("The layout of this payload is not published: shown as data"));
        }

        // Checks
        // K is ok or not (incomplete in records of before: not ok); S is incomplete when characters are lost
        const QString k = d.value("checksum").toString();
        check("K", tr("Checksum K of the words of the fragment (a reassembled message: the worst of its fragments): ok or failed"),
            (k == "incomplete") ? QString("failed") : k);
        check("S", tr("Signature S (sum of the characters of the whole message); incomplete: characters lost"), d.value("signature").toString());

        // State
        if (d.value("nul_error").toBool()) {
            add("NUL:inside", tr("A NUL character inside the message, where only the padding at the end of a fragment may "
                "have it: probably a reception error"), ToneWarn);
        }
    }
    else if (record.m_protocol == "gsc")
    {
        if (d.contains("slot"))
        {
            int slot = d.value("slot").toInt() + 1;
            add(QString("SLOT:%1").arg(slot), tr("Address %1 of the transmission (the first address is 1)").arg(slot));
        }
        if (d.value("non_battery_saver").toBool()) {
            add("MODE:nbs", tr("Non battery saver transmission"));
        } else if (d.value("batch_mode").toBool()) {
            add("MODE:batch", tr("Batch mode: the transmission started with a complemented preamble"));
        }
        if (d.value("interleaved").toBool()) {
            add("INTL:1", tr("Interleaved: a tone only address sent in the alert time of a voice page"));
        }
        if (d.contains("checksum_passed")) {
            check("CK", tr("Checksum of the data blocks"), d.value("checksum_passed").toBool() ? "ok" : "failed");
        }
        if (d.value("end").toString() == "cut") {
            add("END:cut", tr("End cut: the transmission ended before the message did; the rest of it may be missing"), ToneWarn);
        }
    }

    // State: a transmitted copy of a message sent before, a duplicate
    if (d.contains("retransmission"))
    {
        int copy = d.value("retransmission").toInt();
        add(QString("COPY:%1").arg(copy), tr("Transmitted copy %1 of a message sent before (retransmission)").arg(copy), ToneWarn);
    }
    if (record.m_duplicate == MessageRecord::DuplicateDup) {
        add("DUP:same", tr("Duplicate: the same content was received before"), ToneWarn);
    } else if (record.m_duplicate == MessageRecord::DuplicateDupPlus) {
        add("DUP:better", tr("Duplicate with fewer errors: the same message was received before, this copy is better"), ToneGood);
    }
}

void PagingMessageTable::recordInfo(const MessageRecord& record, QString& plain, QString& html, QString& tip)
{
    QStringList texts, chips, tips;
    recordChips(record, texts, chips, tips);
    plain = texts.join(" ");
    html = chips.join(" ");
    tip = tips.join("\n");
}

// The cells of a row from a record. A duplicate is shown like any message, with its DUP or DUP+ flag in the info
void PagingMessageTable::fillRow(int row, const MessageRecord& record, const QString& state)
{
    const QVariantMap& d = record.m_protocolData;

    auto cell = [&](int col, const QString& plain, const QString& html = QString(), const QString& tip = QString()) {
        QTableWidgetItem *item = this->item(row, col);

        if (!item)
        {
            item = new QTableWidgetItem();
            setItem(row, col, item);
        }

        item->setText(plain);
        item->setData(htmlRole, html.isEmpty() ? QVariant() : QVariant(html));
        item->setToolTip(tip);
        return item;
    };

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(record.m_timestamp);
    cell(ColDate, dateTime.date().toString(Qt::TextDate), QString(), dateTime.toString(Qt::ISODate));
    QTableWidgetItem *timeItem = cell(ColTime, dateTime.toString("HH:mm:ss"), QString(), dateTime.toString(Qt::ISODate));
    timeItem->setData(Qt::UserRole, record.m_id);
    timeItem->setData(voiceFileRole, d.value("voice_file").toString());
    timeItem->setData(jsonRole, QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact)));

    // RX received, TX sent by its channel; a page of the Message Center not sent yet (its record has no channel
    // until the transmitter's record replaces it): QU waiting in the queue, NS not sent (dead, cancelled or no
    // longer queued, as after a restart)
    if (record.m_direction != MessageRecord::DirectionTX)
    {
        cell(ColDirection, "RX", QString(), tr("Received"));
    }
    else if (!record.m_channel.isEmpty() || !d.contains("transmitter"))
    {
        cell(ColDirection, "TX", QString(), tr("Transmitted"));
    }
    else
    {
        const QString pageState = d.value("page_state").toString();
        const QString reason = d.value("page_reason").toString();
        const QString transmitter = d.value("transmitter").toString();
        const bool queued = (pageState == "pending") || (pageState == "queued") || (pageState == "claimed") || (pageState == "transmitted");
        QString tip = queued ? tr("In the queue of the Message Center for %1: %2").arg(transmitter, pageState)
            : (pageState == "dead") ? tr("Not sent by %1: it failed").arg(transmitter)
            : (pageState == "cancelled") ? tr("Not sent by %1: cancelled").arg(transmitter)
            : tr("Not sent by %1: no longer in the queue (Message Center restarted)").arg(transmitter);

        if (!reason.isEmpty()) {
            tip += QString(" (%1)").arg(reason);
        }

        cell(ColDirection, queued ? "QU" : "NS", queued ? chip("QU", "#1a1a0d", "#cccc66") : chip("NS", "#2a0d0d", "#ff7777"), tip);
    }
    cell(ColChannel, record.m_channel);

    // The contact of the address book: its icon and its name, in its colour
    {
        const QString name = d.value("contact").toString();
        const QString icon = d.value("contact_icon").toString();
        const quint32 color = (quint32) d.value("contact_color").toULongLong();
        QString html;

        if (!name.isEmpty() && (color != 0)) {
            html = QString("<span style=\"color:%1\">%2</span>").arg(QColor::fromRgba(color).name(), name.toHtmlEscaped());
        }

        // The icon beside the name (the delegate draws the text beside it)
        QTableWidgetItem *contactItem = cell(ColContact, name, html, name.isEmpty() ? QString() : tr("Contact of the address book: %1").arg(name));
        contactItem->setIcon(name.isEmpty() ? QIcon() : contactIcon(icon));
    }
    cell(ColProtocol, record.m_protocol.toUpper());
    cell(ColRate, record.m_mode);

    bool inverted = (record.m_polarity == MessageRecord::PolarityInverted);
    if (record.m_polarity == MessageRecord::PolarityUnknown) {
        cell(ColPolarity, QString());
    } else {
        cell(ColPolarity, inverted ? "I" : "N", inverted ? chip("I", "#2a1f00", "#ffcc44") : chip("N", "#0d1a2a", "#88aacc"),
            inverted ? tr("Inverted polarity") : tr("Normal polarity"));
    }

    // Who: the contact of the address book when there is one (in its colour), else the address
    {
        const QString name = d.value("contact").toString();

        if (m_contactInAddress && !name.isEmpty())
        {
            const quint32 color = (quint32) d.value("contact_color").toULongLong();
            const QString html = (color != 0)
                ? QString("<span style=\"color:%1\">%2</span>").arg(QColor::fromRgba(color).name(), name.toHtmlEscaped())
                : QString();
            QTableWidgetItem *addressItem = cell(ColAddress, name, html, tr("%1, address %2").arg(name, record.m_address));
            addressItem->setIcon(contactIcon(d.value("contact_icon").toString()));
        }
        else
        {
            cell(ColAddress, record.m_address);
        }
    }
    cell(ColType, record.m_msgType, typeChip(record.m_msgType), record.m_msgType);

    // Message, with the alternative decode in grey italics. With the status of each character: received in the
    // colour of the text, corrected amber, lost red on a red tint (shown as '?' or <...>)
    QString text = recordText(record);
    QString messageHtml = statusHtml(text, d.value("char_status").toString());
    QString messageTip = text;
    QString altText = d.value("alt_text").toString();

    if ((record.m_rawBits > 0) && !altText.isEmpty())
    {
        QString altType = d.value("alt_type").toString();
        messageHtml += QString(" <i style=\"color:#888888\">[%1: %2]</i>").arg(altType.toHtmlEscaped(), altText.toHtmlEscaped());
        messageTip += QString("\n%1: %2").arg(altType, altText);
    }

    // The marks of the routes (Message Center): their icons before the message, the colour of its text and of the row
    // (the last mark that has one)
    QStringList markIcons;
    QColor markText;
    QColor markBackground;

    for (const QVariant& m : d.value("marks").toList())
    {
        const QVariantMap mark = m.toMap();

        if (!mark.value("icon").toString().isEmpty()) {
            markIcons.append(mark.value("icon").toString());
        }
        if (mark.value("color").toUInt() != 0) {
            markText = QColor::fromRgba(mark.value("color").toUInt());
        }
        if (mark.value("background").toUInt() != 0) {
            markBackground = QColor::fromRgba(mark.value("background").toUInt());
        }
    }

    if (markText.isValid()) {
        messageHtml = QString("<span style=\"color:%1\">%2</span>").arg(markText.name(), messageHtml);
    }

    // A page of the Message Center that did not go: why, after its text, where there is room for it
    if (d.contains("page_state") && record.m_channel.isEmpty())
    {
        const QString pageState = d.value("page_state").toString();
        const QString reason = d.value("page_reason").toString();

        if (((pageState == "dead") || (pageState == "cancelled")) && !reason.isEmpty())
        {
            messageHtml += QString(" <span style=\"color:#ff7777\">(%1)</span>").arg(reason.toHtmlEscaped());
            messageTip += QString("\n%1").arg(tr("Not sent: %1").arg(reason));
        }
    }

    QTableWidgetItem *messageItem = cell(ColMessage, text, messageHtml, messageTip);
    messageItem->setData(tipRole, messageTip);
    messageItem->setData(marksIconRole, marksIcon(markIcons, iconSize().height() > 0 ? iconSize().height() : 16));
    setVoiceCell(row);

    // Fragment: the n-th fragment received, or the reassembled message of n fragments
    if (d.value("is_fragment").toBool())
    {
        QString f = QString::number(d.value("fragment_count").toInt());
        cell(ColFragment, f, chip(f, "#3a2a00", "#ffb74d"), tr("Fragment %1 received").arg(f));
    }
    else if (d.value("fragmented").toBool())
    {
        int n = d.value("fragment_count").toInt();
        bool complete = d.value("complete").toBool();
        QString f = QString("%1 %2").arg(complete ? "REASM" : "INCOMPLETE").arg(n);
        cell(ColFragment, f, complete ? chip(f, "#0d2a3a", "#4fc3f7") : chip(f, "#2a1000", "#ff8a65"),
            complete ? tr("Reassembled from %1 fragments").arg(n) : tr("%1 fragments, fragments missing or timed out").arg(n));
    }
    else {
        cell(ColFragment, QString());
    }

    QStringList chipTexts, chipHtml, chipTips;
    recordChips(record, chipTexts, chipHtml, chipTips);
    QTableWidgetItem *infoItem = cell(ColInfo, chipTexts.join(" "), chipHtml.join(" "), chipTips.join("\n"));
    infoItem->setData(chipTextsRole, chipTexts);
    infoItem->setData(chipTipsRole, chipTips);

    // Quality of a received message: Xn uncorrectable codewords, ~n corrected bits, ok clean; damaged when its bits were
    // all corrected but a check failed (the corrections were wrong: FLEX checksum K, signature S). None for a message sent
    const MessageRecord::Quality quality = RadioPagingPage::quality(record);

    if ((record.m_direction == MessageRecord::DirectionTX) || (quality == MessageRecord::QualityNone))
    {
        cell(ColErrors, QString());
    }
    else
    {
        QString errors, color;
        const QString reason = RadioPagingPage::qualityReason(record);

        if (quality == MessageRecord::QualityDamaged)
        {
            errors = (record.m_errorsUncorrectable > 0) ? QString("X%1").arg(record.m_errorsUncorrectable) : QString("damaged");
            color = "#ff5252";
        }
        else if (quality == MessageRecord::QualityCorrected)
        {
            errors = QString("~%1").arg(std::max(0, record.m_errorsCorrected));
            color = "#ffca28";
        }
        else
        {
            errors = "ok";
            color = "#66bb6a";
        }

        const QString name = (quality == MessageRecord::QualityDamaged) ? tr("Damaged")
            : (quality == MessageRecord::QualityCorrected) ? tr("Corrected") : tr("Clean");
        cell(ColErrors, errors, QString("<span style=\"color:%1;font-weight:bold\">%2</span>").arg(color, errors),
            reason.isEmpty() ? tr("%1: no bit errors, every check passed").arg(name) : QString("%1: %2").arg(name, reason));
    }

    // The state: as given, else that of a page of the Message Center (waiting, failed with its reason...), else sent for
    // a message a transmitter sent
    if (!state.isEmpty())
    {
        cell(ColState, state);
    }
    else if (d.contains("page_state") && record.m_channel.isEmpty())
    {
        const QString pageState = d.value("page_state").toString();
        const QString reason = d.value("page_reason").toString();
        const QString shown = ((pageState == "pending") || (pageState == "queued")) ? tr("waiting")
            : (pageState == "claimed") ? tr("claimed")
            : (pageState == "transmitted") ? tr("on air")
            : (pageState == "sent") ? tr("sent")
            : (pageState == "cancelled") ? tr("cancelled")
            : (pageState == "dead") ? tr("failed")
            : tr("not sent");
        // The reason in the tool tip, and after the text of the message
        const QString tip = reason.isEmpty() ? shown : QString("%1: %2").arg(shown, reason);
        const bool bad = (pageState == "dead") || (pageState == "cancelled") || (pageState.isEmpty());
        cell(ColState, shown, bad ? QString("<span style=\"color:#ff7777\">%1</span>").arg(shown.toHtmlEscaped()) : QString(), tip);
    }
    else if (record.m_direction == MessageRecord::DirectionTX)
    {
        cell(ColState, tr("sent"));
    }
    else
    {
        cell(ColState, QString());
    }

    // The colour of the row of its marks, on every cell (none: a row decoded again loses the one it had)
    if (markBackground.isValid() || d.contains("marks"))
    {
        QBrush brush = markBackground.isValid()
            ? QBrush(QColor(markBackground.red(), markBackground.green(), markBackground.blue(), 110)) : QBrush();

        for (int col = 0; col < columnCount(); col++)
        {
            if (QTableWidgetItem *cellItem = item(row, col)) {
                cellItem->setBackground(brush);
            }
        }
    }

    // The height of the row follows its cells (a message decoded again may need more or fewer lines); one line each
    // when the messages are on one line
    if (!m_singleLineMessages) {
        resizeRowToContents(row);
    }
}

void PagingMessageTable::addRecord(const MessageRecord& record, const QString& state)
{
    int row = findRow(record.m_id);

    if (row >= 0)
    {
        // The end of a voice message completes its row
        if (record.m_protocolData.contains("voice_end") && record.m_text.isEmpty())
        {
            QTableWidgetItem *timeItem = item(row, ColTime);
            QTableWidgetItem *messageItem = item(row, ColMessage);
            QString file = record.m_protocolData.value("voice_file").toString();
            QString text = recordText(record);

            timeItem->setData(voiceFileRole, file);
            messageItem->setText(text);
            messageItem->setToolTip(text);
            messageItem->setData(tipRole, text);
            messageItem->setData(htmlRole, QVariant());
            setVoiceCell(row);
            return;
        }

        fillRow(row, record, state);
        return;
    }

    // A new record while the view does not follow waits, the rows shown stay where they are (one already waiting is
    // updated there). A table being made again takes them at once
    if (updatesEnabled() && (holding() || !m_waiting.empty()))
    {
        auto it = std::find_if(m_waiting.begin(), m_waiting.end(),
            [&record](const std::pair<MessageRecord, QString>& w) { return !record.m_id.isEmpty() && (w.first.m_id == record.m_id); });

        if (it != m_waiting.end())
        {
            *it = std::make_pair(record, state);
        }
        else
        {
            m_waiting.emplace_back(record, state);

            // Never more than a table of them: the oldest would go at once
            if ((int) m_waiting.size() > m_maxRows) {
                m_waiting.erase(m_waiting.begin());
            }
        }

        if (holding())
        {
            showWaitingButton();
            return;
        }

        showWaiting();
        return;
    }

    appendRecord(record, state);
}

void PagingMessageTable::appendRecord(const MessageRecord& record, const QString& state)
{
    int row = rowCount();
    setRowCount(row + 1);
    fillRow(row, record, state);

    if (!record.m_id.isEmpty())
    {
        // The ids of the rows gone (the table emptied, the oldest rows dropped) are forgotten from time to time
        if (m_rowsById.size() > 2 * std::max(m_maxRows, rowCount()))
        {
            for (auto it = m_rowsById.begin(); it != m_rowsById.end();) {
                it = it->isValid() ? std::next(it) : m_rowsById.erase(it);
            }
        }

        m_rowsById.insert(record.m_id, QPersistentModelIndex(model()->index(row, ColTime)));
    }

    while (rowCount() > m_maxRows) {
        removeRow(0);
    }

    if (m_following && !selectionModel()->hasSelection()) {
        scrollToBottom();
    }
}

void PagingMessageTable::setAudioDevice(const QString& deviceName)
{
    m_player->setAudioDevice(deviceName);
}

const QString& PagingMessageTable::audioDevice() const
{
    return m_player->audioDevice();
}

// The message cell of a voice message with a file: a play icon (stop while it is played), and the file in the
// tool tip: its path, length and format
void PagingMessageTable::setVoiceCell(int row)
{
    QTableWidgetItem *timeItem = item(row, ColTime);
    QTableWidgetItem *messageItem = item(row, ColMessage);

    if (!timeItem || !messageItem) {
        return;
    }

    QString file = timeItem->data(voiceFileRole).toString();
    QString tip = messageItem->data(tipRole).toString();

    if (file.isEmpty())
    {
        messageItem->setIcon(messageItem->data(marksIconRole).value<QIcon>());
        messageItem->setToolTip(tip);
        return;
    }

    bool playing = m_player->isPlaying() && (m_player->file() == file);
    messageItem->setIcon(QIcon(playing ? ":/stop.png" : ":/play.png"));
    messageItem->setToolTip(QString("%1\n\n%2\n\n%3").arg(tip, PagingVoicePlayer::fileInfo(file),
        playing ? tr("Click the icon to stop. Right click: the audio output")
                : tr("Click the icon to play. Right click: the audio output")));
}

// The icons of the voice files: the one played has the stop icon
void PagingMessageTable::displayPlayer()
{
    for (int row = 0; row < rowCount(); row++)
    {
        QTableWidgetItem *timeItem = item(row, ColTime);

        if (timeItem && !timeItem->data(voiceFileRole).toString().isEmpty()) {
            setVoiceCell(row);
        }
    }
}

void PagingMessageTable::toggleVoice(int row)
{
    QTableWidgetItem *timeItem = item(row, ColTime);
    QString file = timeItem ? timeItem->data(voiceFileRole).toString() : QString();

    if (file.isEmpty()) {
        return;
    }

    if (m_player->isPlaying() && (m_player->file() == file))
    {
        m_player->stop();
        return;
    }

    QString error;

    if (!m_player->play(file, error)) {
        QMessageBox::information(this, tr("Voice message"), error);
    }
}

namespace {

// A JSON value as one line of a tree: objects and arrays as branches
void addJsonItem(QTreeWidgetItem *parent, const QString& key, const QJsonValue& value)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent, QStringList(key));

    if (value.isObject())
    {
        const QJsonObject object = value.toObject();

        for (auto it = object.begin(); it != object.end(); ++it) {
            addJsonItem(item, it.key(), it.value());
        }
        item->setExpanded(true);
    }
    else if (value.isArray())
    {
        const QJsonArray array = value.toArray();

        for (int i = 0; i < array.size(); i++) {
            addJsonItem(item, QString::number(i), array.at(i));
        }
        item->setText(1, QString("[%1]").arg(array.size()));
    }
    else
    {
        QString text = value.isNull() ? QString("null") : value.isBool() ? QString(value.toBool() ? "true" : "false")
            : value.isDouble() ? QString::number(value.toDouble(), 'g', 15) : value.toString();
        item->setText(1, text);
        item->setToolTip(1, text);
    }
}

} // namespace

// The window of the Message Center: the feature window at its index in feature set 0 (there is one feature set),
// nullptr if there is none
FeatureGUI *PagingMessageTable::messageCenterGui()
{
    int featureSetIndex = -1;
    int featureIndex = -1;

    if (!FeatureWebAPIUtils::getFeature(featureSetIndex, featureIndex, "sdrangel.feature.messagecenter")) {
        return nullptr;
    }

    for (QWidget *widget : QApplication::allWidgets())
    {
        FeatureGUI *gui = qobject_cast<FeatureGUI*>(widget);

        if (gui && (gui->getIndex() == featureIndex)) {
            return gui;
        }
    }

    return nullptr;
}

void PagingMessageTable::openMessageCenter()
{
    FeatureGUI *gui = messageCenterGui();

    if (!gui)
    {
        // Added by the main message queue: its window shows when it is made
        FeatureWebAPIUtils::openMessageCenter();
        return;
    }

    gui->show();
    gui->showNormal();
    gui->raise();
    gui->activateWindow();
}

void PagingMessageTable::setupMessageCenterButton(QToolButton *button)
{
    button->setIcon(QIcon(":/sms.png"));
    button->setText(QString());

    // The Message Centers there are now, counted again when a feature comes or goes (after it is gone)
    auto update = [button]()
    {
        int count = 0;

        for (const FeatureSet *featureSet : MainCore::instance()->getFeatureeSets())
        {
            for (int index = 0; index < featureSet->getNumberOfFeatures(); index++)
            {
                const Feature *feature = featureSet->getFeatureAt(index);

                if (feature && (feature->getURI() == "sdrangel.feature.messagecenter")) {
                    count++;
                }
            }
        }

        // On and off as the indicators of the other channels (NFM, BFM)
        button->setStyleSheet(count > 0 ? QString("QToolButton { background-color : green; }") : QString("QToolButton { background:rgb(79,79,79); }"));
        button->setToolTip(count == 0
            ? tr("Message Center: there is none. Click to add one and open it.\n"
                 "It keeps the messages of all the paging channels with their history, sends pages to the transmitters,\n"
                 "and has the contacts and the rules (what to do with each message)")
            : (count == 1
                ? tr("Message Center: it takes the messages of this channel. Click to open it.\n"
                     "It keeps the messages of all the paging channels with their history, sends pages to the transmitters,\n"
                     "and has the contacts and the rules (what to do with each message)")
                : tr("Message Centers: %1, each takes the messages of this channel. Click to open the first one").arg(count)));
    };

    update();
    QObject::connect(button, &QToolButton::clicked, button, []() { openMessageCenter(); });
    QObject::connect(MainCore::instance(), &MainCore::featureAdded, button, [button, update]() { QTimer::singleShot(0, button, update); });
    QObject::connect(MainCore::instance(), &MainCore::featureRemoved, button, [button, update]() { QTimer::singleShot(0, button, update); });
}

void PagingMessageTable::createRoute(const MessageRecord& record, const QVariantMap& charset, const QString& type)
{
    FeatureGUI *gui = messageCenterGui();

    if (!gui) {
        return;
    }

    gui->showNormal();
    // Queued: the menu or the dialog of the table closes first
    const QString json = QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact));
    QMetaObject::invokeMethod(gui, "createRouteFromRecord", Qt::QueuedConnection, Q_ARG(QString, json),
        Q_ARG(QVariantMap, charset), Q_ARG(QString, type));
}

// Every codepage of the protocol of a received message, and its numeric reading: a row each. The right click on a
// row makes a route of the Message Center that decodes the messages of the address so
void PagingMessageTable::showAllCodepages(const MessageRecord& record)
{
    struct Reading
    {
        RadioPagingCharset::Key m_key;
        QString m_type;
        QString m_label;
    };

    const QStringList types = RadioPagingRecode::types(record);
    const QString alphaType = types.contains("alphanumeric") ? QString("alphanumeric") : QString();
    QList<Reading> readings;

    for (const RadioPagingCharset::Codepage *codepage : RadioPagingCharset::codepages(record.m_protocol.toStdString()))
    {
        readings.append({codepage->key(), (alphaType != record.m_msgType) ? alphaType : QString(),
            QString::fromUtf8(codepage->m_label)});
    }
    if (types.contains("numeric")) {
        readings.append({RadioPagingCharset::Key(), QString("numeric"), tr("Numeric")});
    }

    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("%1 %2 in every codepage").arg(record.m_protocol.toUpper(), record.m_address));
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTableWidget *table = new QTableWidget((int) readings.size(), 2, dialog);
    table->setHorizontalHeaderLabels({tr("Codepage"), tr("Message")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    const RadioPagingCharset::Key current = RadioPagingPage::charsetKey(record.m_protocolData.value("charset"));

    for (int i = 0; i < (int) readings.size(); i++)
    {
        MessageRecord out;
        QString error;
        const bool done = RadioPagingRecode::redecode(record, readings[i].m_key, readings[i].m_type, out, error);
        QTableWidgetItem *label = new QTableWidgetItem(readings[i].m_label);
        QTableWidgetItem *text = new QTableWidgetItem(done ? out.m_text : error);

        // The reading of the message as it is shown
        if (((readings[i].m_key == current) && readings[i].m_type.isEmpty()) || (readings[i].m_type == record.m_msgType))
        {
            QFont bold = label->font();
            bold.setBold(true);
            label->setFont(bold);
        }
        if (!done) {
            text->setForeground(Qt::gray);
        }

        text->setToolTip(text->text());
        table->setItem(i, 0, label);
        table->setItem(i, 1, text);
    }

    table->resizeColumnToContents(0);
    const bool canRoute = messageCenterGui() != nullptr;

    connect(table, &QTableWidget::customContextMenuRequested, dialog, [table, readings, record, canRoute](const QPoint& pos) {
        QTableWidgetItem *clicked = table->itemAt(pos);

        if (!clicked) {
            return;
        }

        const Reading reading = readings[clicked->row()];
        QMenu menu(table);
        QAction *route = menu.addAction(tr("Create rule..."));
        route->setToolTip(tr("A rule of the Message Center for the messages of this address, decoded so"));
        route->setEnabled(canRoute);
        connect(route, &QAction::triggered, table, [record, reading]() {
            createRoute(record, reading.m_key.empty() ? QVariantMap() : RadioPagingPage::charsetData(reading.m_key), reading.m_type);
        });
        connect(menu.addAction(tr("Copy message")), &QAction::triggered, table, [table, clicked]() {
            QApplication::clipboard()->setText(table->item(clicked->row(), 1)->text());
        });
        menu.exec(table->viewport()->mapToGlobal(pos));
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(table);
    layout->addWidget(buttons);
    dialog->resize(640, 400);
    new DialogPositioner(dialog, true);
    dialog->show();
}

// Every field of the record of the row, as stored (the JSON of the record): the common fields, protocolData
// and the raw payload, also as bits

// The members of a group message, one capcode a row: a search, copy, and the messages of a member
void PagingMessageTable::showGroupMembers(const MessageRecord& record)
{
    const QVariantList members = record.m_protocolData.value("group_capcodes").toList();
    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Group slot %1 (%2): %3 members").arg(record.m_protocolData.value("group_slot").toInt())
        .arg(record.m_address).arg(members.size()));
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QLineEdit *search = new QLineEdit(dialog);
    search->setPlaceholderText(tr("Search"));
    search->setClearButtonEnabled(true);
    QTableWidget *table = new QTableWidget(members.size(), 2, dialog);
    table->setHorizontalHeaderLabels({tr("#"), tr("Capcode")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);

    for (int i = 0; i < members.size(); i++)
    {
        table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        table->setItem(i, 1, new QTableWidgetItem(members[i].toString()));
    }

    table->resizeColumnToContents(0);

    connect(search, &QLineEdit::textChanged, table, [table](const QString& text) {
        for (int i = 0; i < table->rowCount(); i++) {
            table->setRowHidden(i, !text.isEmpty() && !table->item(i, 1)->text().contains(text.trimmed()));
        }
    });
    connect(table, &QTableWidget::customContextMenuRequested, table, [this, table](const QPoint& pos) {
        QTableWidgetItem *clicked = table->itemAt(pos);

        if (!clicked) {
            return;
        }

        const QString capcode = table->item(clicked->row(), 1)->text();
        QMenu menu(table);
        connect(menu.addAction(tr("Messages of %1").arg(capcode)), &QAction::triggered, this, [this, capcode]() {
            emit addressFilterRequested(capcode);
        });
        connect(menu.addAction(tr("Copy")), &QAction::triggered, table, [capcode]() { QApplication::clipboard()->setText(capcode); });
        menu.exec(table->viewport()->mapToGlobal(pos));
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QPushButton *copyAll = buttons->addButton(tr("Copy all"), QDialogButtonBox::ActionRole);
    connect(copyAll, &QPushButton::clicked, dialog, [members]() {
        QStringList list;

        for (const QVariant& m : members) {
            list.append(m.toString());
        }

        QApplication::clipboard()->setText(list.join('\n'));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(search);
    layout->addWidget(table, 1);
    layout->addWidget(buttons);
    dialog->resize(320, 480);
    dialog->show();
}

void PagingMessageTable::setSingleLineMessages(bool single)
{
    m_singleLineMessages = single;

    // Rows of one line of text, all of one height; the last column (the message) takes the width left
    if (single)
    {
        verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    }
    else
    {
        verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    }

    horizontalHeader()->setSectionResizeMode(ColMessage, single ? QHeaderView::Stretch : QHeaderView::Interactive);
}

QStyledItemDelegate *PagingMessageTable::createHtmlDelegate(QObject *parent)
{
    return new HtmlDelegate(parent);
}

QString PagingMessageTable::typeChipHtml(const QString& type)
{
    return typeChip(type);
}

void PagingMessageTable::showDetails(int row)
{
    QTableWidgetItem *timeItem = item(row, ColTime);

    if (timeItem) {
        showRecord(timeItem->data(jsonRole).toString(), this);
    }
}

void PagingMessageTable::showRecord(const QString& json, QWidget *parent)
{
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
    QDialog *dialog = new QDialog(parent);
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTreeWidget *tree = new QTreeWidget(dialog);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QPushButton *copy = buttons->addButton(tr("Copy as JSON"), QDialogButtonBox::ActionRole);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Message %1 %2").arg(object.value("protocol").toString(), object.value("address").toString()));
    tree->setColumnCount(2);
    tree->setHeaderLabels({tr("Field"), tr("Value")});

    // A received message: how well it came through, and why, first
    MessageRecord record;

    if (MessageRecord::fromJson(object, record) && (record.m_direction == MessageRecord::DirectionRX))
    {
        const MessageRecord::Quality quality = RadioPagingPage::quality(record);
        const QString reason = RadioPagingPage::qualityReason(record);

        if (quality != MessageRecord::QualityNone)
        {
            const QString name = MessageRecord::qualityToString(quality);
            QTreeWidgetItem *item = new QTreeWidgetItem(tree->invisibleRootItem(),
                QStringList({tr("Quality"), reason.isEmpty() ? name : QString("%1: %2").arg(name, reason)}));
            item->setForeground(1, QColor((quality == MessageRecord::QualityDamaged) ? "#ff5252"
                : (quality == MessageRecord::QualityCorrected) ? "#ffca28" : "#66bb6a"));
            item->setToolTip(1, tr("Clean: no bit errors, every check passed. Corrected: bit errors, all corrected. "
                "Damaged: something lost (uncorrectable errors, a failed or incomplete check)"));
        }
    }

    // A group message: its members first, the list closed when it is long
    const QJsonArray members = object.value("protocolData").toObject().value("group_capcodes").toArray();

    if (!members.isEmpty())
    {
        QTreeWidgetItem *group = new QTreeWidgetItem(tree->invisibleRootItem(),
            QStringList({tr("Group members (%1)").arg(members.size()), tr("slot %1").arg(object.value("protocolData").toObject().value("group_slot").toInt())}));

        for (const QJsonValue& member : members) {
            new QTreeWidgetItem(group, QStringList({QString(), member.toString()}));
        }

        group->setExpanded(members.size() <= 20);
    }

    for (auto it = object.begin(); it != object.end(); ++it) {
        addJsonItem(tree->invisibleRootItem(), it.key(), it.value());
    }

    // The raw payload as bits, in groups of 8 from the first bit
    const QByteArray raw = QByteArray::fromHex(object.value("rawData").toString().toLatin1());
    const int rawBits = object.value("rawBits").toInt(-1);

    if (rawBits > 0)
    {
        QString bits;

        for (int i = 0; (i < rawBits) && (i / 8 < raw.size()); i++)
        {
            if ((i > 0) && (i % 8 == 0)) {
                bits += ' ';
            }
            bits += ((uint8_t) raw[i / 8] & (0x80 >> (i % 8))) ? '1' : '0';
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(tree->invisibleRootItem(), QStringList({tr("rawData (bits)"), bits}));
        item->setToolTip(1, bits);
    }

    tree->resizeColumnToContents(0);
    layout->addWidget(tree);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    connect(copy, &QPushButton::clicked, dialog, [json]() { QApplication::clipboard()->setText(json); });
    dialog->resize(640, 520);
    dialog->show();
}

// A click on the icon of a voice message plays or stops it; elsewhere the click selects as usual
// Over the info column: the tool tip of the chip under the mouse (its label and what it means). The chips are laid out
// as the delegate draws them: the HTML in the text rectangle of the cell, centred vertically
bool PagingMessageTable::viewportEvent(QEvent *event)
{
    if (event->type() != QEvent::ToolTip) {
        return QTableWidget::viewportEvent(event);
    }

    QHelpEvent *help = static_cast<QHelpEvent *>(event);
    QModelIndex index = indexAt(help->pos());
    QTableWidgetItem *it = index.isValid() ? item(index.row(), index.column()) : nullptr;
    QStringList texts = it ? it->data(chipTextsRole).toStringList() : QStringList();
    QStringList tips = it ? it->data(chipTipsRole).toStringList() : QStringList();

    if (texts.isEmpty() || (texts.size() != tips.size())) {
        return QTableWidget::viewportEvent(event);
    }

    QStyleOptionViewItem opt;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    initViewItemOption(&opt);
#else
    opt = viewOptions();
#endif
    opt.rect = visualRect(index);
    opt.features = QStyleOptionViewItem::HasDisplay;
    QRect textRect = style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, this);

    QTextDocument doc;
    doc.setDocumentMargin(1);
    doc.setDefaultFont(opt.font);
    doc.setHtml(it->data(htmlRole).toString());
    QPointF p(help->pos().x() - textRect.left(), help->pos().y() - (opt.rect.top() + (opt.rect.height() - doc.size().height()) / 2));
    int pos = doc.documentLayout()->hitTest(p, Qt::ExactHit);

    // Characters of the document: a non breaking space, the label, a non breaking space, then a space between chips
    int start = 0;

    for (int i = 0; (pos >= 0) && (i < texts.size()); i++)
    {
        int end = start + texts[i].size() + 2;

        if (pos < end)
        {
            QToolTip::showText(help->globalPos(), QString("%1\n%2").arg(texts[i], tips[i]), viewport(), visualRect(index));
            return true;
        }

        start = end + 1;
    }

    return QTableWidget::viewportEvent(event);
}

void PagingMessageTable::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        QModelIndex index = indexAt(event->pos());

        if (index.isValid() && (index.column() == ColMessage))
        {
            QTableWidgetItem *messageItem = item(index.row(), ColMessage);
            QRect rect = visualRect(index);
            int iconWidth = iconSize().isValid() ? iconSize().width() : style()->pixelMetric(QStyle::PM_SmallIconSize);

            if (messageItem && !messageItem->icon().isNull() && (event->pos().x() < rect.left() + iconWidth + 6))
            {
                toggleVoice(index.row());
                event->accept();
                return;
            }
        }
    }

    QTableWidget::mousePressEvent(event);
}

// The audio output of the voice files, in the dialog of the audio outputs of SDRangel
void PagingMessageTable::selectAudioDevice(const QPoint& pos)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_player->audioDevice());
    audioSelect.move(pos);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected && (audioSelect.m_audioDeviceName != m_player->audioDevice()))
    {
        m_player->setAudioDevice(audioSelect.m_audioDeviceName);
        emit audioDeviceChanged(audioSelect.m_audioDeviceName);
    }
}

// Right click in the header: the columns to show
void PagingMessageTable::showColumnMenu(const QPoint& pos)
{
    m_columnMenu->clear();

    for (int i = 0; i < ColumnCount; i++)
    {
        QAction *action = m_columnMenu->addAction(headers[i]);
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
        connect(action, &QAction::triggered, this, [this, i](bool checked) {
            setColumnHidden(i, !checked);
            emit columnsChanged();
        });
    }

    m_columnMenu->popup(horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Right click in a row: copy
void PagingMessageTable::showRowMenu(const QPoint& pos)
{
    QTableWidgetItem *clicked = itemAt(pos);

    if (!clicked) {
        return;
    }

    int row = clicked->row();
    QMenu menu(this);

    connect(menu.addAction(tr("Details...")), &QAction::triggered, this, [this, row]() { showDetails(row); });
    connect(menu.addAction(tr("Copy cell")), &QAction::triggered, this, [clicked]() {
        QApplication::clipboard()->setText(clicked->text());
    });
    connect(menu.addAction(tr("Copy row")), &QAction::triggered, this, [this, row]() {
        QStringList cells;

        for (int c = 0; c < ColumnCount; c++)
        {
            if (!isColumnHidden(c) && item(row, c)) {
                cells.append(item(row, c)->text());
            }
        }

        QApplication::clipboard()->setText(cells.join('\t'));
    });
    connect(menu.addAction(tr("Copy as JSON")), &QAction::triggered, this, [this, row]() {
        QApplication::clipboard()->setText(item(row, ColTime)->data(jsonRole).toString());
    });

    // The voice file of the row, and the audio output of the voice files
    QString file = item(row, ColTime) ? item(row, ColTime)->data(voiceFileRole).toString() : QString();
    menu.addSeparator();

    if (!file.isEmpty())
    {
        bool playing = m_player->isPlaying() && (m_player->file() == file);
        connect(menu.addAction(QIcon(playing ? ":/stop.png" : ":/play.png"), playing ? tr("Stop voice") : tr("Play voice")),
            &QAction::triggered, this, [this, row]() { toggleVoice(row); });
    }

    QPoint global = viewport()->mapToGlobal(pos);
    connect(menu.addAction(tr("Audio output...")), &QAction::triggered, this, [this, global]() { selectAudioDevice(global); });

    // A received message decoded again: as the other type, with another codepage of its protocol
    MessageRecord record;
    const QJsonObject json = QJsonDocument::fromJson(item(row, ColTime)->data(jsonRole).toString().toUtf8()).object();
    const bool parsed = MessageRecord::fromJson(json, record);

    // The members of a group message, all of them
    if (parsed && !record.m_protocolData.value("group_capcodes").toList().isEmpty())
    {
        connect(menu.addAction(tr("Group members (%1)...").arg(record.m_protocolData.value("group_capcodes").toList().size())),
            &QAction::triggered, this, [this, record]() { showGroupMembers(record); });
    }

    // The actions of the owner of the table on the message (the Message Center: page again, contact, filter)
    if (parsed && m_rowMenuExtender)
    {
        menu.addSeparator();
        m_rowMenuExtender(menu, record);
        menu.addSeparator();
    }

    // A route of the Message Center filled from the message
    if (parsed && (record.m_kind == MessageRecord::KindMessage) && !record.m_address.isEmpty())
    {
        QAction *route = menu.addAction(tr("Create rule..."));
        route->setToolTip(tr("A rule of the Message Center for the messages of this address"));
        route->setEnabled(messageCenterGui() != nullptr);
        connect(route, &QAction::triggered, this, [record]() { createRoute(record, QVariantMap(), QString()); });
    }

    if (parsed && !RadioPagingRecode::types(record).isEmpty())
    {
        QMenu *decode = menu.addMenu(tr("Decode as"));
        connect(decode->addAction(tr("All codepages...")), &QAction::triggered, this, [this, record]() { showAllCodepages(record); });
        decode->addSeparator();
        const QString state = item(row, ColState) ? item(row, ColState)->text() : QString();
        auto redecode = [this, record, state](const RadioPagingCharset::Key& key, const QString& type) {
            MessageRecord out;
            QString error;

            if (!RadioPagingRecode::redecode(record, key, type, out, error))
            {
                qWarning() << "PagingMessageTable: decode again:" << error;
                return;
            }

            int r = findRow(out.m_id);

            if (r >= 0) {
                fillRow(r, out, state);
            }

            emit recordRedecoded(out);
        };

        for (const QString& type : RadioPagingRecode::types(record))
        {
            QAction *action = decode->addAction(type == "numeric" ? tr("Numeric") : tr("Alphanumeric"));
            action->setCheckable(true);
            action->setChecked(type == record.m_msgType);
            connect(action, &QAction::triggered, this, [redecode, type]() { redecode(RadioPagingCharset::Key(), type); });
        }

        decode->addSeparator();
        const RadioPagingCharset::Key current = RadioPagingPage::charsetKey(record.m_protocolData.value("charset"));
        QMap<QString, QMenu*> vendors;

        for (const RadioPagingCharset::Codepage *codepage : RadioPagingCharset::codepages(record.m_protocol.toStdString()))
        {
            const QString vendor = QString::fromUtf8(codepage->m_vendor);

            if (!vendors.contains(vendor)) {
                vendors[vendor] = decode->addMenu(vendor);
            }

            QAction *action = vendors[vendor]->addAction(QString::fromUtf8(codepage->m_label));
            const RadioPagingCharset::Key key = codepage->key();
            action->setCheckable(true);
            action->setChecked(key == current);
            connect(action, &QAction::triggered, this, [redecode, key]() { redecode(key, QString()); });
        }
    }

    menu.exec(global);
}
