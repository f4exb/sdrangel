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

#include <QApplication>
#include <memory>

#include <QDateTime>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QUuid>
#include <QDateTimeEdit>
#include <QPointer>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QColorDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/flowlayout.h"
#include "gui/pagingmessagetable.h"

#include "radiopagingpage.h"
#include "messagecentergui.h"
#include "messagecentercontacts.h"

namespace {

const int idRole = Qt::UserRole;

QTableWidget *makeTable(const QStringList& headers, QWidget *parent)
{
    QTableWidget *table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    // Each column as wide as its contents (a number, yes or no, a name), the last one takes the width left
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumHeight(60);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // One line a row: a long text ends with an ellipsis, its tooltip has all of it
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    return table;
}

void setRow(QTableWidget *table, int row, const QStringList& cells)
{
    for (int c = 0; c < cells.size(); c++)
    {
        QTableWidgetItem *item = table->item(row, c);

        if (!item)
        {
            item = new QTableWidgetItem();
            table->setItem(row, c, item);
        }

        item->setText(cells[c]);
        item->setToolTip(cells[c]);
    }
}

QString filterSummary(const MessageCenterFilter& f)
{
    QStringList parts;

    if (!f.m_contacts.isEmpty()) {
        parts << "contact " + f.m_contacts.join(", ");
    }
    if (!f.m_protocols.isEmpty()) {
        parts << f.m_protocols.join("/").toUpper();
    }
    if (f.m_direction >= 0) {
        parts << (f.m_direction == (int) MessageRecord::DirectionRX ? "RX" : "TX");
    }
    if (f.m_kind >= 0) {
        parts << MessageRecord::kindToString((MessageRecord::Kind) f.m_kind);
    }
    if (!f.m_speeds.isEmpty())
    {
        QStringList speeds;

        for (int speed : f.m_speeds) {
            speeds << QString::number(speed);
        }

        parts << speeds.join("/") + " bps";
    }
    if ((f.m_frequencyMin >= 0) || (f.m_frequencyMax >= 0))
    {
        parts << QString("%1 to %2 Hz")
            .arg(f.m_frequencyMin >= 0 ? QString::number(f.m_frequencyMin) : QString("0"))
            .arg(f.m_frequencyMax >= 0 ? QString::number(f.m_frequencyMax) : QString("any"));
    }
    if (f.m_polarity >= 0) {
        parts << (f.m_polarity == (int) MessageRecord::PolarityInverted ? "inverted" : "normal");
    }
    if (!f.m_channels.isEmpty()) {
        parts << "channel " + f.m_channels.join(",");
    }
    if (!f.m_address.isEmpty()) {
        parts << "address /" + f.m_address + "/";
    }
    if (!f.m_msgType.isEmpty()) {
        parts << f.m_msgType;
    }
    if (!f.m_text.isEmpty()) {
        parts << "text has /" + f.m_text + "/";
    }
    if (!f.m_sources.isEmpty()) {
        parts << "from " + f.m_sources.join(",");
    }
    if (f.m_hideDuplicates) {
        parts << "no duplicates";
    }
    if (f.m_voiceOnly) {
        parts << "voice files";
    }
    if (!f.m_quality.isEmpty()) {
        parts << ((f.m_quality == "notDamaged") ? QString("not damaged") : f.m_quality);
    }
    if (f.m_hideFragments) {
        parts << "no fragments";
    }
    for (const MessageCenterContact::Condition& c : f.m_conditions) {
        parts << c.text();
    }

    return parts.isEmpty() ? QString("everything") : parts.join(", ");
}

// The saved columns of the table of the Messages tab start with it
const QByteArray logColumnsTag("MCM3:");

// A button with an icon of the project and its tool tip, as the buttons of the log of the Pager demodulator
QToolButton *iconButton(QWidget *parent, const char *icon, const QString& tip)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(QIcon(icon));
    button->setToolTip(tip);
    return button;
}

} // namespace

// ---------------------------------------------------------------------------
// Filter widget
// ---------------------------------------------------------------------------

MessageCenterFilterWidget::MessageCenterFilterWidget(QWidget *parent) :
    QWidget(parent)
{
    // The filters in a line that wraps at the width of the window (two lines in the default width of the Message
    // Center, one when it is wide enough), each at its size, as the settings lines of the channels
    FlowLayout *layout = new FlowLayout(this, 0, 6, 3);

    m_contact = new QComboBox(this);
    m_contact->addItem("Any contact");
    m_contact->setToolTip("The messages of a contact of the address book (the Contacts tab): of any of its addresses");
    m_contact->setVisible(false);
    // Protocols and types shown by name, kept as the records have them; the types are those of the protocol chosen
    m_protocol = new QComboBox(this);
    m_protocol->addItem("Any protocol", QString());
    m_protocol->addItem("POCSAG", QString("pocsag"));
    m_protocol->addItem("FLEX", QString("flex"));
    m_protocol->addItem("GSC", QString("gsc"));
    m_direction = new QComboBox(this);
    m_direction->addItems({"RX and TX", "RX", "TX"});
    m_polarity = new QComboBox(this);
    m_polarity->addItems({"Any polarity", "Normal", "Inverted"});
    m_type = new QComboBox(this);
    fillTypes(QString());
    m_channel = new QLineEdit(this);
    m_channel->setPlaceholderText("channel name");
    m_address = new QLineEdit(this);
    m_address->setPlaceholderText("regular expression");
    m_text = new QLineEdit(this);
    m_text->setPlaceholderText("regular expression");
    m_source = new QComboBox(this);
    m_source->addItems({"Any source", "channels", "api"});
    m_source->setVisible(false);
    m_hideDuplicates = new QCheckBox("Hide duplicates", this);
    m_voiceOnly = new QCheckBox("Voice files", this);
    // How well the message came through: a field every message has
    m_quality = new QComboBox(this);
    m_quality->addItem("Any quality", QString());
    m_quality->addItem("Not damaged", QString("notDamaged"));
    m_quality->addItem("Clean", QString("clean"));
    m_quality->addItem("Corrected", QString("corrected"));
    m_quality->addItem("Damaged", QString("damaged"));
    m_quality->setToolTip("How well the message came through:\n"
        "Clean: no bit errors, every check passed (FLEX checksum and signature, GSC blocks)\n"
        "Corrected: bit errors, all corrected; nothing lost\n"
        "Damaged: something lost: uncorrectable errors, a failed check, an incomplete message\n"
        "Not damaged: clean or corrected, or nothing to check (a tone)");
    // Conditions on any field, those of a protocol too (checksum, signature, blocking length...)
    m_conditionsButton = new QToolButton(this);
    m_conditionsButton->setText("Conditions...");
    m_conditionsButton->setToolTip("Conditions on any field of the messages, those of their protocol too (FLEX checksum "
        "and signature, phase, frame, blocking length...): all of them met");
    connect(m_conditionsButton, &QToolButton::clicked, this, [this]() {
        emit fieldsWanted();
        editConditions();
    });

    m_protocol->setToolTip("Protocol of the messages: any, POCSAG, FLEX or GSC");
    m_direction->setToolTip("Received (RX), transmitted (TX) or both");
    m_polarity->setToolTip("Polarity the message was received or sent with");
    m_type->setToolTip("Message type as the decoder or encoder names it");
    m_channel->setToolTip("Name of the channel (its title), or the transmitter of a page (exact). Empty: any channel");
    m_address->setToolTip("Regular expression the whole address must match, as the filters of the other channels:\n"
        "e.g. 1234567 for that RIC, 12345.. for 1234500 to 1234599, 001001/. for a GSC cap code. Empty: any address");
    m_text->setToolTip("Regular expression found anywhere in the text, not case sensitive: e.g. fire, or fire|smoke.\n"
        "^ and $ for the start and the end. Its captures are ${1}, ${2}... in the texts of the actions. Empty: any text");
    m_source->setToolTip("Where the message comes from: the channels, the API (ingest), or a source of I/O");
    m_hideDuplicates->setToolTip("Hide the messages marked as duplicates (copies received again)");
    m_voiceOnly->setToolTip("Only the GSC voice pages that have a sound file");

    // The text fields with their names (a placeholder alone is hard to see), at a width that fits an expression
    auto labelled = [this](const QString& name, QWidget *field, QLineEdit *edit, int chars) {
        QWidget *w = new QWidget(this);
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(3);

        if (!name.isEmpty())
        {
            QLabel *label = new QLabel(name, w);
            label->setToolTip(edit->toolTip());
            l->addWidget(label);
        }

        if (field) {
            l->addWidget(field);
        }

        edit->setFixedWidth(edit->fontMetrics().averageCharWidth() * chars);
        l->addWidget(edit);
        return w;
    };

    layout->addWidget(m_contact);
    layout->addWidget(m_protocol);
    layout->addWidget(m_direction);
    layout->addWidget(m_polarity);
    layout->addWidget(m_type);
    layout->addWidget(m_quality);
    layout->addWidget(m_hideDuplicates);
    layout->addWidget(m_voiceOnly);
    layout->addWidget(m_source);
    layout->addWidget(labelled("Channel", nullptr, m_channel, 14));
    layout->addWidget(labelled("Address", nullptr, m_address, 14));
    layout->addWidget(labelled("Text", nullptr, m_text, 16));
    layout->addWidget(m_conditionsButton);

    for (QComboBox *combo : {m_contact, m_protocol, m_direction, m_polarity, m_type, m_source, m_quality}) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MessageCenterFilterWidget::changed);
    }
    connect(m_protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        fillTypes(m_protocol->currentData().toString());
    });

    // A regular expression that is not valid is shown in red with its error, and matches nothing
    auto validate = [](QLineEdit *edit, bool anchored, const QString& help) {
        QRegularExpression expression(anchored ? QRegularExpression::anchoredPattern(edit->text()) : edit->text());
        bool valid = edit->text().isEmpty() || expression.isValid();
        edit->setStyleSheet(valid ? QString() : QString("QLineEdit { color: red; }"));
        edit->setToolTip(valid ? help : expression.errorString());
    };

    for (QLineEdit *edit : {m_address, m_text})
    {
        const QString help = edit->toolTip();
        const bool anchored = (edit != m_text);
        connect(edit, &QLineEdit::textChanged, this, [validate, edit, anchored, help]() { validate(edit, anchored, help); });
    }

    for (QLineEdit *edit : {m_channel, m_address, m_text}) {
        connect(edit, &QLineEdit::editingFinished, this, &MessageCenterFilterWidget::changed);
    }
    for (QCheckBox *check : {m_hideDuplicates, m_voiceOnly}) {
        connect(check, &QCheckBox::toggled, this, &MessageCenterFilterWidget::changed);
    }
}

void MessageCenterFilterWidget::fillTypes(const QString& protocol)
{
    // The types the decoders and encoders give each protocol, as they name them
    static const QList<QPair<QString, QString>> names = {
        {"alphanumeric", "Alphanumeric"}, {"numeric", "Numeric"}, {"special_numeric", "Special numeric"},
        {"numbered_numeric", "Numbered numeric"}, {"numbered_special_numeric", "Numbered special numeric"},
        {"binary", "Binary"}, {"secure", "Secure"}, {"short_message", "Short message"},
        {"short_instruction", "Short instruction"}, {"tone", "Tone"}, {"voice", "Voice"}
    };
    static const QMap<QString, QStringList> byProtocol = {
        {"pocsag", {"alphanumeric", "numeric", "tone"}},
        {"flex", {"alphanumeric", "numeric", "special_numeric", "numbered_numeric", "numbered_special_numeric", "binary",
            "secure", "short_message", "short_instruction", "tone"}},
        {"gsc", {"alphanumeric", "numeric", "tone", "voice"}}
    };
    const QString current = m_type->currentData().toString();
    const QStringList types = byProtocol.value(protocol);

    m_type->blockSignals(true);
    m_type->clear();
    m_type->addItem("Any type", QString());

    for (const auto& name : names)
    {
        if (types.isEmpty() || types.contains(name.first)) {
            m_type->addItem(name.second, name.first);
        }
    }

    m_type->setCurrentIndex(std::max(0, m_type->findData(current)));
    m_type->blockSignals(false);

    if (m_type->currentData().toString() != current) {
        emit changed();
    }
}

MessageCenterFilter MessageCenterFilterWidget::filter() const
{
    MessageCenterFilter f = m_base;

    // A list of the filter set whose first entry the widget still shows is kept whole
    auto list = [](const QStringList& base, const QString& shown) {
        if (shown.isEmpty()) {
            return QStringList();
        }
        return (!base.isEmpty() && (base.first() == shown)) ? base : QStringList(shown);
    };

    // An item of several contacts has them as its data
    QStringList contacts = m_contact->currentData().toStringList();

    if (m_contact->currentIndex() <= 0) {
        f.m_contacts.clear();
    } else if (!contacts.isEmpty()) {
        f.m_contacts = contacts;
    } else {
        f.m_contacts = QStringList(m_contact->currentText());
    }

    f.m_protocols = list(m_base.m_protocols, m_protocol->currentData().toString());
    f.m_direction = -1;
    f.m_polarity = -1;
    f.m_msgType.clear();
    f.m_channels = list(m_base.m_channels, m_channel->text().trimmed());
    f.m_sources = m_sourcesShown ? list(m_base.m_sources, m_source->currentIndex() > 0 ? m_source->currentText() : QString())
        : m_base.m_sources;

    if (m_direction->currentIndex() > 0) {
        f.m_direction = (m_direction->currentIndex() == 1) ? (int) MessageRecord::DirectionRX : (int) MessageRecord::DirectionTX;
    }
    if (m_polarity->currentIndex() > 0) {
        f.m_polarity = (m_polarity->currentIndex() == 1) ? (int) MessageRecord::PolarityNormal : (int) MessageRecord::PolarityInverted;
    }
    f.m_msgType = m_type->currentData().toString();

    f.m_address = m_address->text().trimmed();
    f.m_text = m_text->text();
    f.m_hideDuplicates = m_hideDuplicates->isChecked();
    f.m_voiceOnly = m_voiceOnly->isChecked();
    f.m_quality = m_quality->currentData().toString();
    f.m_conditions = m_conditions;
    return f;
}

void MessageCenterFilterWidget::showConditions()
{
    QStringList texts;

    for (const MessageCenterContact::Condition& c : m_conditions) {
        texts.append(c.text());
    }

    m_conditionsButton->setText(m_conditions.isEmpty() ? QString("Conditions...") : QString("Conditions (%1)...").arg(m_conditions.size()));
    m_conditionsButton->setStyleSheet(m_conditions.isEmpty() ? QString() : QString("QToolButton { color: #66bb6a; }"));
    m_conditionsButton->setToolTip(m_conditions.isEmpty()
        ? QString("Conditions on any field of the messages, those of their protocol too (FLEX checksum and signature, phase, "
            "frame, blocking length...): all of them met")
        : QString("All of them met:\n%1").arg(texts.join("\n")));
}

// A row a condition: the field (those of the log, or one typed), the operator, the value; Add and Remove
void MessageCenterFilterWidget::editConditions()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Conditions");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *help = new QLabel("Conditions on any field of the messages, all of them met. The details of a message "
        "(double-click it) show its fields.", &dialog);
    help->setWordWrap(true);
    layout->addWidget(help);
    QTableWidget *table = new QTableWidget(0, 3, &dialog);
    table->setHorizontalHeaderLabels({"Field", "Operator", "Value"});
    table->setColumnWidth(0, table->fontMetrics().averageCharWidth() * 34);
    table->setColumnWidth(1, table->fontMetrics().averageCharWidth() * 14);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table);
    QLabel *error = new QLabel(&dialog);
    error->setStyleSheet("QLabel { color: #ff7777; }");
    error->setWordWrap(true);
    layout->addWidget(error);

    // The fields by group: common, then each protocol the channels describe, then the others of the log. An item is
    // "key - name", its data the key and its values; a key can be typed too
    const int valuesRole = Qt::UserRole + 1;
    auto fillFields = [this, valuesRole](QComboBox *field) {
        static const QList<QPair<QString, QString>> groups = {{"", "Common"}, {"pocsag", "POCSAG"}, {"flex", "FLEX"},
            {"gsc", "GSC"}, {"other", "Other fields of the log"}};
        QStringList protocols;

        for (const MessageCenterFieldInfo& f : m_fieldInfos)
        {
            if (!protocols.contains(f.m_protocol)) {
                protocols.append(f.m_protocol);
            }
        }

        QStandardItemModel *model = qobject_cast<QStandardItemModel*>(field->model());

        for (const QString& protocol : protocols)
        {
            auto group = std::find_if(groups.begin(), groups.end(), [&protocol](const QPair<QString, QString>& g) { return g.first == protocol; });
            field->addItem((group != groups.end()) ? group->second : protocol.toUpper());

            if (model)
            {
                QStandardItem *header = model->item(field->count() - 1);
                header->setFlags(header->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
                QFont font = header->font();
                font.setBold(true);
                header->setFont(font);
            }

            for (const MessageCenterFieldInfo& f : m_fieldInfos)
            {
                if (f.m_protocol == protocol)
                {
                    field->addItem(f.m_name.isEmpty() ? QString("  %1").arg(f.m_key) : QString("  %1 - %2").arg(f.m_key, f.m_name), f.m_key);
                    field->setItemData(field->count() - 1, f.m_values, valuesRole);
                }
            }
        }
    };
    // The key of the text of the field: before " - ", without the indent of the list
    auto fieldKey = [](const QString& text) { return text.section(" - ", 0, 0).trimmed(); };
    auto addRow = [this, table, fillFields, fieldKey, valuesRole](const MessageCenterContact::Condition& c) {
        const int row = table->rowCount();
        table->insertRow(row);
        QComboBox *field = new QComboBox(table);
        field->setEditable(true);
        field->setInsertPolicy(QComboBox::NoInsert);
        fillFields(field);
        const int index = c.m_field.isEmpty() ? -1 : field->findData(c.m_field);

        if (index >= 0) {
            field->setCurrentIndex(index);
        } else {
            field->setEditText(c.m_field);
        }

        field->setToolTip("Field of the message: the common ones, those of each protocol as the channels describe them, "
            "and the others of the messages of the log; or a key typed in (the details of a message show its fields)");
        QComboBox *op = new QComboBox(table);
        MessageCenterContactDialog::fillOperators(op, c.m_operator);
        // The value: the values of the field to choose from, or one typed in
        QComboBox *value = new QComboBox(table);
        value->setEditable(true);
        value->setInsertPolicy(QComboBox::NoInsert);
        value->setEnabled(MessageCenterContact::Condition::takesValue(c.m_operator));
        auto showValues = [this, field, value, fieldKey, valuesRole]() {
            const QString current = value->currentText();
            QStringList values = field->currentData(valuesRole).toStringList();

            // A key typed in: the values the descriptions give it
            if (values.isEmpty())
            {
                const QString key = fieldKey(field->currentText());

                for (const MessageCenterFieldInfo& f : m_fieldInfos)
                {
                    if (f.m_key == key)
                    {
                        for (const QString& v : f.m_values)
                        {
                            if (!values.contains(v)) {
                                values.append(v);
                            }
                        }
                    }
                }
            }

            value->clear();
            value->addItems(values);
            value->setEditText(current);
        };
        showValues();
        value->setEditText(c.m_value);
        connect(field, &QComboBox::currentTextChanged, value, showValues);
        // The key at the start of the text in view, not the end of its name
        field->lineEdit()->setCursorPosition(0);
        connect(field, QOverload<int>::of(&QComboBox::activated), field, [field]() {
            QTimer::singleShot(0, field, [field]() { field->lineEdit()->setCursorPosition(0); });
        });
        connect(op, QOverload<int>::of(&QComboBox::currentIndexChanged), value, [op, value]() {
            value->setEnabled(MessageCenterContact::Condition::takesValue((MessageCenterContact::Condition::Operator) op->currentData().toInt()));
        });
        table->setCellWidget(row, 0, field);
        table->setCellWidget(row, 1, op);
        table->setCellWidget(row, 2, value);
    };
    auto rowCondition = [table, fieldKey](int row) {
        MessageCenterContact::Condition c;
        const QComboBox *field = qobject_cast<QComboBox*>(table->cellWidget(row, 0));
        const QComboBox *op = qobject_cast<QComboBox*>(table->cellWidget(row, 1));
        const QComboBox *value = qobject_cast<QComboBox*>(table->cellWidget(row, 2));
        c.m_field = field ? fieldKey(field->currentText()) : QString();
        c.m_operator = op ? (MessageCenterContact::Condition::Operator) op->currentData().toInt() : MessageCenterContact::Condition::Is;
        c.m_value = !value || !MessageCenterContact::Condition::takesValue(c.m_operator) ? QString()
            : (c.m_operator == MessageCenterContact::Condition::Matches) ? value->currentText() : value->currentText().trimmed();
        return c;
    };

    for (const MessageCenterContact::Condition& c : m_conditions) {
        addRow(c);
    }

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *add = new QPushButton("Add", &dialog);
    QPushButton *remove = new QPushButton("Remove", &dialog);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->addWidget(box);
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, &dialog, [addRow]() { addRow(MessageCenterContact::Condition()); });
    connect(remove, &QPushButton::clicked, &dialog, [table]() {
        if (table->currentRow() >= 0) {
            table->removeRow(table->currentRow());
        }
    });
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dialog, [&dialog, table, error, rowCondition]() {
        // A row without a field is dropped; a condition that cannot match says why, the dialog stays
        for (int row = 0; row < table->rowCount(); row++)
        {
            const MessageCenterContact::Condition c = rowCondition(row);
            const QString why = c.m_field.isEmpty() ? QString() : c.check();

            if (!why.isEmpty())
            {
                error->setText(why);
                return;
            }
        }

        dialog.accept();
    });

    if (m_conditions.isEmpty()) {
        addRow(MessageCenterContact::Condition());
    }

    dialog.resize(620, 260);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_conditions.clear();

    for (int row = 0; row < table->rowCount(); row++)
    {
        const MessageCenterContact::Condition c = rowCondition(row);

        if (!c.m_field.isEmpty()) {
            m_conditions.append(c);
        }
    }

    showConditions();
    emit changed();
}

void MessageCenterFilterWidget::setFilter(const MessageCenterFilter& f)
{
    m_base = f;

    // Several contacts are one item of the list, with them as its data; a contact the address book no longer has is
    // kept in the list too: the filter keeps it
    if (f.m_contacts.size() > 1)
    {
        const QString all = f.m_contacts.join(", ");

        if (m_contact->findText(all) < 0) {
            m_contact->addItem(all, f.m_contacts);
        }

        m_contact->setCurrentIndex(m_contact->findText(all));
    }
    else
    {
        if (!f.m_contacts.isEmpty() && (m_contact->findText(f.m_contacts.first()) < 0)) {
            m_contact->addItem(f.m_contacts.first());
        }

        m_contact->setCurrentIndex(f.m_contacts.isEmpty() ? 0 : std::max(0, m_contact->findText(f.m_contacts.first())));
    }

    m_contact->setVisible(m_contact->count() > 1);
    m_protocol->setCurrentIndex(f.m_protocols.isEmpty() ? 0 : std::max(0, m_protocol->findData(f.m_protocols.first().toLower())));
    fillTypes(m_protocol->currentData().toString());
    m_direction->setCurrentIndex(f.m_direction < 0 ? 0 : (f.m_direction == (int) MessageRecord::DirectionRX ? 1 : 2));
    m_polarity->setCurrentIndex(f.m_polarity < 0 ? 0 : (f.m_polarity == (int) MessageRecord::PolarityNormal ? 1 : 2));
    // A type the protocol list does not have (set through the WebAPI) is kept in the list
    if (!f.m_msgType.isEmpty() && (m_type->findData(f.m_msgType) < 0)) {
        m_type->addItem(f.m_msgType, f.m_msgType);
    }

    m_type->setCurrentIndex(f.m_msgType.isEmpty() ? 0 : std::max(0, m_type->findData(f.m_msgType)));
    m_channel->setText(f.m_channels.isEmpty() ? QString() : f.m_channels.first());
    m_address->setText(f.m_address);
    m_text->setText(f.m_text);
    m_hideDuplicates->setChecked(f.m_hideDuplicates);
    m_voiceOnly->setChecked(f.m_voiceOnly);
    m_quality->setCurrentIndex(std::max(0, m_quality->findData(f.m_quality)));
    m_conditions = f.m_conditions;
    showConditions();

    if (!f.m_sources.isEmpty() && (m_source->findText(f.m_sources.first()) < 0)) {
        m_source->addItem(f.m_sources.first());
    }

    m_source->setCurrentIndex(f.m_sources.isEmpty() ? 0 : std::max(0, m_source->findText(f.m_sources.first())));
    emit changed();     // The line edits tell their changes when edited only
}

void MessageCenterFilterWidget::setContacts(const QList<MessageCenterContact>& contacts)
{
    m_contact->blockSignals(true);
    const QString current = m_contact->currentIndex() > 0 ? m_contact->currentText() : QString();
    const QVariant currentData = m_contact->currentData();
    m_contact->clear();
    m_contact->addItem("Any contact");

    for (const MessageCenterContact& contact : contacts) {
        m_contact->addItem(MessageCenterContactIcons::icon(contact.m_icon), contact.m_name);
    }
    if (!current.isEmpty() && (m_contact->findText(current) < 0)) {
        m_contact->addItem(current, currentData);
    }

    m_contact->setCurrentIndex(current.isEmpty() ? 0 : std::max(0, m_contact->findText(current)));
    m_contact->setVisible(m_contact->count() > 1);
    m_contact->blockSignals(false);
}

void MessageCenterFilterWidget::showSources(const QStringList& sources)
{
    m_source->blockSignals(true);
    QString current = m_source->currentText();
    m_source->clear();
    m_source->addItems(QStringList({"Any source", "channels", "api"}) + sources);
    m_source->setCurrentIndex(std::max(0, m_source->findText(current)));
    m_source->blockSignals(false);
    m_source->setVisible(true);
    m_sourcesShown = true;
}

// ---------------------------------------------------------------------------
// GUI
// ---------------------------------------------------------------------------

MessageCenterGUI* MessageCenterGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    return new MessageCenterGUI(pluginAPI, featureUISet, feature);
}

void MessageCenterGUI::destroy()
{
    delete this;
}

void MessageCenterGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray MessageCenterGUI::serialize() const
{
    return m_settings.serialize();
}

bool MessageCenterGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }

    resetToDefaults();
    return false;
}

void MessageCenterGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

QWidget *MessageCenterGUI::makeRollup(const QString& title, bool expanding)
{
    QWidget *widget = new QWidget(getRollupContents());
    widget->setWindowTitle(title);

    // A section with a table takes the height the window is given: the window can be resized vertically
    if (expanding) {
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    return widget;
}

MessageCenterGUI::MessageCenterGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    m_center(reinterpret_cast<MessageCenter*>(feature)),
    m_doApplySettings(true),
    m_shownLogged(-1)
{
    (void) pluginAPI;
    (void) featureUISet;

    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/messagecenter/readme.md";
    m_settings = m_center->settings();
    m_settings.setRollupState(&m_rollupState);

    // Five sections, one for each stage of a message (where it comes from, which ones, what to do, where it goes),
    // as tabs on the left side (as the APRS and Meshtastic windows): they take width, not the height a small screen
    // lacks. The options in a dialog opened from the Messages tab
    RollupContents *rollupContents = getRollupContents();
    QWidget *main = makeRollup("Message Center", true);
    QVBoxLayout *mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(main);
    m_tabs->setTabPosition(QTabWidget::West);
    m_tabs->setDocumentMode(true);
    // Names only, as the side tabs of APRS and DATV: an icon makes each tab higher, the five no longer fit
    auto tab = [this](const QString& title, const QString& tip) {
        QWidget *w = new QWidget(m_tabs);
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_tabs->addTab(w, title);
        m_tabs->setTabToolTip(m_tabs->count() - 1, tip);
        return w;
    };
    // A section of two parts, one above the other, the user sets their share
    auto split = [](QWidget *page, QWidget *&top, QWidget *&bottom) {
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        QSplitter *splitter = new QSplitter(Qt::Vertical, page);
        top = new QWidget(splitter);
        bottom = new QWidget(splitter);
        splitter->addWidget(top);
        splitter->addWidget(bottom);
        splitter->setChildrenCollapsible(false);
        layout->addWidget(splitter);
    };
    QWidget *top = nullptr;
    QWidget *bottom = nullptr;

    buildLog(tab(tr("Messages"), tr("The messages received and sent: live, or searched in the history")));
    split(tab(tr("Send"), tr("Send a page to a contact or an address, and follow the pages sent")), top, bottom);
    buildCompose(top);
    buildQueue(bottom);
    buildContacts(tab(tr("Contacts"), tr("The address book: a name, an icon and a colour for the addresses of "
        "a pager or a station,\nshown beside their messages, chosen in the filters, and how to send them a page")));
    QWidget *rules = tab(tr("Rules"), tr("What happens to each message: when (which ones), what changes "
        "(a mark, a codepage), and where it is sent"));
    buildRoutes(rules);
    split(tab(tr("Connections"), tr("Where messages come from and go: the channels, and the files, UDP, "
        "commands, speech and Map")), top, bottom);
    buildEndpoints(top);
    buildFiles(bottom);
    mainLayout->addWidget(m_tabs);
    // What each rule finds in the log, counted again when the rules are looked at
    connect(m_tabs, &QTabWidget::currentChanged, this, [this, rules](int index) {
        if (m_tabs->widget(index) == rules) {
            refreshRoutes();
        }
    });

    m_optionsDialog = new QDialog(this);
    m_optionsDialog->setWindowTitle(tr("Message Center options"));
    QVBoxLayout *optionsLayout = new QVBoxLayout(m_optionsDialog);
    QWidget *options = new QWidget(m_optionsDialog);
    buildOptions(options);
    optionsLayout->addWidget(options);
    QDialogButtonBox *optionsButtons = new QDialogButtonBox(QDialogButtonBox::Close, m_optionsDialog);
    connect(optionsButtons, &QDialogButtonBox::rejected, m_optionsDialog, &QDialog::hide);
    optionsLayout->addWidget(optionsButtons);
    new DialogPositioner(m_optionsDialog, true);
    rollupContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    sizeToContents();

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    m_center->setMessageQueueToGUI(&m_inputMessageQueue);
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    refresh();
    m_resizer.enableChildMouseTracking();
}

MessageCenterGUI::~MessageCenterGUI()
{
}

void MessageCenterGUI::buildLog(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_logFilter = new MessageCenterFilterWidget(container);
    connect(m_logFilter, &MessageCenterFilterWidget::fieldsWanted, this, [this]() { m_logFilter->setFields(fieldInfos()); });
    m_logTable = new PagingMessageTable(container);
    m_logTable->setMinimumHeight(80);
    m_logTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logTable->setMaxRows(m_maxLogRows);
    // The message always in view: a few columns by default (who is the contact when there is one), the others in
    // the menu of the header
    m_logTable->setDefaultColumns({PagingMessageTable::ColDate, PagingMessageTable::ColTime, PagingMessageTable::ColDirection,
        PagingMessageTable::ColProtocol, PagingMessageTable::ColPolarity, PagingMessageTable::ColAddress, PagingMessageTable::ColType,
        PagingMessageTable::ColMessage});
    m_logTable->setContactInAddress(true);
    m_logTable->setSingleLineMessages(true);
    m_logTable->setRowMenuExtender([this](QMenu& menu, const MessageRecord& record) { messageMenu(menu, record); });
    m_logTable->setToolTip("The messages received and sent. Click the icon of a voice message to play it.\n"
        "Double-click a message for all its fields; right click it for what to do with it (page it again, add a contact,\n"
        "only its pager). Right click the header to choose the columns");
    buildQueryBar(container);
    layout->addWidget(m_queryBar);
    layout->addWidget(m_logFilter);
    layout->addWidget(m_logTable);

    // Icon buttons, as those of the log of the Pager demodulator
    QHBoxLayout *buttons = new QHBoxLayout();
    QToolButton *import = iconButton(container, ":/load.png", "Read the messages of a file into the log: a JSON lines file, "
        "or a CSV log of a message center\nor of the Pager demodulator. They are not routed: no action is taken on them again");
    QToolButton *exportLog = iconButton(container, ":/save.png", "Save the messages the filters select to a CSV file\n"
        "(it can be imported again, here or in another message center)");
    QToolButton *clear = iconButton(container, ":/bin.png", "Remove all the messages of the log, and of the history file when it is kept");
    // The options of the log and the history (and the claim and offline times of the channels), in a dialog
    QPushButton *options = new QPushButton(tr("Options..."), container);
    options->setToolTip(tr("Options: log size, history (size and days kept), claim time of the pages, time to offline "
        "of the channels"));
    connect(options, &QPushButton::clicked, this, [this]() {
        m_optionsDialog->show();
        m_optionsDialog->raise();
    });
    buttons->addWidget(import);
    buttons->addWidget(exportLog);
    buttons->addStretch(1);
    buttons->addWidget(options);
    buttons->addWidget(clear);
    connect(import, &QToolButton::clicked, this, &MessageCenterGUI::importFile);
    connect(exportLog, &QToolButton::clicked, this, &MessageCenterGUI::exportLog);
    layout->addLayout(buttons);

    connect(clear, &QToolButton::clicked, this, [this]() {
        // The history (days of messages) goes only when asked
        QMessageBox box(QMessageBox::Question, "Clear the log", "Remove the messages of the log?",
            QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::No);
        QCheckBox *history = nullptr;

        if (m_settings.m_historyEnabled)
        {
            history = new QCheckBox(QString("Also delete the history (%1 MB in %2)")
                .arg(m_center->historyBytes() / (1024.0 * 1024.0), 0, 'f', 1).arg(MessageCenter::historyFilePath()), &box);
            box.setCheckBox(history);
        }

        if (box.exec() == QMessageBox::Yes) {
            m_center->getInputMessageQueue()->push(MessageCenter::MsgClearLog::create(history && history->isChecked()));
        }
    });
    connect(m_logFilter, &MessageCenterFilterWidget::changed, this, [this]() {
        if (m_queryMode)
        {
            runQuery(true);
        }
        else
        {
            m_shownLogged = -1;
            refreshLog();
        }
    });
    connect(m_logTable, &PagingMessageTable::columnsChanged, this, [this]() {
        m_settings.m_logColumns = logColumnsTag + m_logTable->columns();
        applySettings(QStringList("logColumns"));
    });
    connect(m_logTable, &PagingMessageTable::audioDeviceChanged, this, [this](const QString& deviceName) {
        m_settings.m_voicePlaybackDeviceName = deviceName;
        applySettings(QStringList("voicePlaybackDeviceName"));
    });
    // A message decoded again replaces its record in the log
    connect(m_logTable, &PagingMessageTable::recordRedecoded, this, [this](const MessageRecord& record) {
        m_center->getInputMessageQueue()->push(MessageCenter::MsgRedecoded::create(record));
    });
    // A member of a group: its messages in the log (the group messages it is a member of too)
    connect(m_logTable, &PagingMessageTable::addressFilterRequested, this, [this](const QString& address) {
        MessageCenterFilter f = m_logFilter->filter();
        f.m_address = QRegularExpression::escape(address);
        m_logFilter->setFilter(f);
    });
}

void MessageCenterGUI::buildEndpoints(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_endpointsTable = makeTable({"Channel", "Role", "Status", "Protocols", "Speeds", "Polarity", "Queue", "Seen"}, container);
    m_endpointsTable->setToolTip("The channels that announced themselves");
    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *remove = new QPushButton("Remove", container);
    remove->setToolTip("Remove the channel selected from the list; a channel there comes back when it announces itself.\n"
        "The pages of a transmitter removed go back to the queue");
    QPushButton *removeOffline = new QPushButton("Remove offline", container);
    removeOffline->setToolTip("Remove the channels that are offline");
    buttons->addWidget(remove);
    buttons->addWidget(removeOffline);
    buttons->addStretch();
    layout->addWidget(m_endpointsTable);
    layout->addLayout(buttons);

    connect(remove, &QPushButton::clicked, this, [this]() {
        const QString key = selectedId(m_endpointsTable);

        if (!key.isEmpty()) {
            m_center->getInputMessageQueue()->push(MessageCenter::MsgRemoveEndpoints::create(QStringList(key), false));
        }
    });
    connect(removeOffline, &QPushButton::clicked, this, [this]() {
        m_center->getInputMessageQueue()->push(MessageCenter::MsgRemoveEndpoints::create(QStringList(), true));
    });
}

void MessageCenterGUI::buildQueue(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_queueTable = makeTable({"Page", "Transmitter", "Protocol", "Address", "Text", "State", "Tries", "Retries", "Reason"}, container);
    // The text and the reason share the width left and are cut short: the state stays in view
    m_queueTable->horizontalHeader()->setStretchLastSection(false);
    m_queueTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_queueTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *requeue = new QPushButton("Requeue", container);
    requeue->setToolTip("A dead page goes back to pending with its attempts reset");
    QPushButton *cancel = new QPushButton("Cancel", container);
    cancel->setToolTip("Withdraw a page nobody has claimed");
    QPushButton *clear = new QPushButton("Clear sent", container);
    buttons->addWidget(requeue);
    buttons->addWidget(cancel);
    buttons->addWidget(clear);
    buttons->addStretch();
    layout->addWidget(m_queueTable);
    layout->addLayout(buttons);

    connect(requeue, &QPushButton::clicked, this, [this]() { pageAction((int) MessageCenter::MsgPageAction::Requeue); });
    connect(cancel, &QPushButton::clicked, this, [this]() { pageAction((int) MessageCenter::MsgPageAction::Cancel); });
    connect(clear, &QPushButton::clicked, this, [this]() { pageAction((int) MessageCenter::MsgPageAction::ClearSent); });
}

void MessageCenterGUI::buildCompose(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);

    // To a contact: its send to gives the transmitter and the address of the page composed below
    QHBoxLayout *toLayout = new QHBoxLayout();
    QLabel *toLabel = new QLabel("To", container);
    m_toContact = new QComboBox(container);
    m_toContact->setToolTip("The transmitter and the address below, or a contact of the address book: its send to for the page "
        "(its type, protocol...) gives them");
    m_toWhere = new QLabel(container);
    m_toWhere->setToolTip("Where the page goes: the transmitter, protocol and address the send to of the contact gives");
    QToolButton *editContactButton = iconButton(container, ":/edit.png", "Edit the contact chosen (its send to)");
    QToolButton *contactsButton = iconButton(container, ":/listing.png", "The contacts (the address book)");
    QToolButton *addContactButton = iconButton(container, ":/plus.png", "A new contact of the transmitter and address below: "
        "a rule for the messages of the address, a send to of the transmitter");
    toLayout->addWidget(toLabel);
    toLayout->addWidget(m_toContact);
    toLayout->addWidget(editContactButton);
    toLayout->addWidget(addContactButton);
    toLayout->addWidget(contactsButton);

    m_addContact = addContactButton;
    connect(addContactButton, &QToolButton::clicked, this, [this, addContactButton]() {
        RadioPagingPage page = m_composer->page();
        page.m_transmitter = m_composer->transmitter();
        const MessageCenterContact preset = contactOfPage(page);

        // A new contact, or the address added to one (its rule and its send to after the others)
        QMenu menu(this);
        QAction *newContact = menu.addAction(QIcon(":/plus.png"), "New contact...");

        if (!m_settings.m_contacts.isEmpty()) {
            menu.addSection("Add to");
        }

        QMap<QAction*, int> addTo;

        for (int i = 0; i < m_settings.m_contacts.size(); i++) {
            addTo[menu.addAction(MessageCenterContactIcons::icon(m_settings.m_contacts[i].m_icon), m_settings.m_contacts[i].m_name + "...")] = i;
        }

        QAction *chosen = menu.exec(addContactButton->mapToGlobal(QPoint(0, addContactButton->height())));

        if (!chosen) {
            return;
        }

        QString name;

        if (chosen == newContact)
        {
            const int before = m_settings.m_contacts.size();
            editContact(-1, &preset);

            if (m_settings.m_contacts.size() > before) {
                name = m_settings.m_contacts.last().m_name;
            }
        }
        else
        {
            const int index = addTo.value(chosen);
            MessageCenterContact merged = m_settings.m_contacts[index];
            merged.m_rules.append(preset.m_rules);
            merged.m_sendTo.append(preset.m_sendTo);
            editContact(index, &merged);
            name = m_settings.m_contacts[index].m_name;
        }

        // The page goes to it, if the contact has it (it was not cancelled)
        if (!name.isEmpty())
        {
            destinationEdited();
            updateAddContact();
        }
    });
    toLayout->addWidget(m_toWhere, 1);
    layout->addLayout(toLayout);

    connect(editContactButton, &QToolButton::clicked, this, [this]() {
        const QString name = m_toContact->currentData().toString();

        for (int i = 0; i < m_settings.m_contacts.size(); i++)
        {
            if (!name.isEmpty() && (m_settings.m_contacts[i].m_name == name))
            {
                editContact(i);
                return;
            }
        }

        editContact(-1);    // No contact chosen: a new one
    });
    connect(contactsButton, &QToolButton::clicked, this, [this]() {
        for (int i = 0; i < m_tabs->count(); i++)
        {
            if (m_tabs->tabText(i) == tr("Contacts")) {
                m_tabs->setCurrentIndex(i);
            }
        }
    });
    connect(m_toContact, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        transmitterChanged();       // The protocols of the contact, or of the transmitter
        showContactPage();
    });

    // The transmitters say what they can send; the page is checked by the one that takes it (its mode, numbering)
    m_composer = new PagingComposer(container);
    m_composer->setNetworkMode(true);
    m_composer->setAddressHintShown(false);     // In the tool tip of the address: the form does not move as it is typed
    m_composer->setSendText("Post the page");
    m_composer->setSendOnOwnRow();
    m_composer->setTransmitters(QStringList());
    layout->addWidget(m_composer);
    // What the transmitter cannot send (copies of a one-shot transmitter), and the result of the last page posted:
    // below the form, they do not move it
    m_transmitterNote = new QLabel(container);
    m_transmitterNote->setWordWrap(true);
    m_transmitterNote->setStyleSheet("QLabel { color: #aaaaaa; }");
    m_transmitterNote->setVisible(false);
    layout->addWidget(m_transmitterNote);
    m_sendResult = new QLabel(container);
    m_sendResult->setWordWrap(true);
    m_sendResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sendResult->setToolTip("The last page posted: waiting, sent, or why it failed. Its row in Messages has it too");
    layout->addWidget(m_sendResult);
    // Its lines together at the top of the tab, not spread over its height
    layout->addStretch(1);

    connect(m_composer, &PagingComposer::selectionChanged, this, &MessageCenterGUI::transmitterChanged);
    connect(m_composer, &PagingComposer::selectionChanged, this, &MessageCenterGUI::showContactPage);
    connect(m_composer, &PagingComposer::destinationEdited, this, &MessageCenterGUI::destinationEdited);
    connect(m_composer, &PagingComposer::selectionChanged, this, &MessageCenterGUI::updateAddContact);
    connect(m_composer, &PagingComposer::sendRequested, this, &MessageCenterGUI::post);
}

// The page composed, to the contact chosen: the fields its send to gives. False and why if there is none for it
bool MessageCenterGUI::contactPage(RadioPagingPage& page, QString& error) const
{
    const QString name = m_toContact->currentData().toString();

    for (const MessageCenterContact& contact : m_settings.m_contacts)
    {
        if (contact.m_name != name) {
            continue;
        }

        // The fields of the page as a post has them, that the conditions of the send to look at
        QJsonObject message;
        message["contact"] = name;
        message["protocol"] = page.m_protocol;
        message["type"] = page.m_type.isEmpty() ? QString("alphanumeric") : page.m_type;
        message["text"] = page.m_text;
        message["priority"] = page.m_priority ? 1 : 0;
        message["speed"] = page.m_speed;

        if (page.m_polarity >= 0) {
            message["polarity"] = (page.m_polarity == 1) ? "inverted" : "normal";
        }

        QJsonObject sent;

        if (!contact.pageFor(message, sent, &error)) {
            return false;
        }

        page.m_transmitter = sent.value("transmitter").toString();
        page.m_protocol = sent.value("protocol").toString(page.m_protocol).toLower();
        page.m_address = sent.value("address").toVariant().toString();
        page.m_function = sent.value("function").toInt(page.m_function);

        // The polarity of the pager (the same address in the other polarity is another pager)
        if (sent.contains("polarity"))
        {
            const QString polarity = sent.value("polarity").toString().toLower();
            page.m_polarity = (polarity == "normal") ? 0 : (polarity == "inverted") ? 1 : -1;
        }

        return true;
    }

    error = QString("No contact %1").arg(name);
    return false;
}

// The fields of the composer that the send to of the contact chosen gives for the page (its type): filled and locked
void MessageCenterGUI::showContactPage()
{
    if (m_fillingContact) {
        return;     // The composer telling the fields just set
    }

    if (m_toContact->currentIndex() <= 0)
    {
        m_toWhere->clear();
        return;
    }

    RadioPagingPage page = m_composer->page();
    QString error;

    if (contactPage(page, error))
    {
        m_fillingContact = true;
        m_composer->setDestination(page.m_transmitter, page.m_protocol, page.m_address, page.m_function, page.m_polarity);
        m_fillingContact = false;
        m_toWhere->setText(QString("by %1").arg(page.m_transmitter));
        m_toWhere->setStyleSheet(QString());
    }
    else
    {
        m_toWhere->setText(error);
        m_toWhere->setStyleSheet("QLabel { color: red; }");
    }
}

// The transmitter, address, function or polarity changed by hand: the page goes where the fields say; the contact of
// that destination (one of its send to) is shown, if there is one
void MessageCenterGUI::destinationEdited()
{
    RadioPagingPage page = m_composer->page();
    page.m_transmitter = m_composer->transmitter();
    const QString known = contactOfDestination(page);
    const int index = known.isEmpty() ? 0 : std::max(0, m_toContact->findData(known));

    if (m_toContact->currentIndex() != index)
    {
        {
            QSignalBlocker blocker(m_toContact);
            m_toContact->setCurrentIndex(index);
        }

        transmitterChanged();       // The protocols of the contact or of the transmitter
    }

    m_toWhere->setText(known.isEmpty() ? QString() : QString("by %1").arg(page.m_transmitter));
    m_toWhere->setStyleSheet(QString());
    updateAddContact();
}

// Something to add to the contacts: an address that is one, of a transmitter, that no contact has as its destination
void MessageCenterGUI::updateAddContact()
{
    if (!m_addContact) {
        return;
    }

    RadioPagingPage page = m_composer->page();
    page.m_transmitter = m_composer->transmitter();
    const QString error = page.checkAddress();
    const QString known = error.isEmpty() ? contactOfDestination(page) : QString();
    const bool addable = error.isEmpty() && !page.m_transmitter.isEmpty() && known.isEmpty();

    m_addContact->setEnabled(addable);
    m_addContact->setToolTip(addable ? QString("Add the transmitter and the address below to the contacts: a new contact, or to one")
        : !error.isEmpty() ? QString("Nothing to add: %1").arg(error)
        : page.m_transmitter.isEmpty() ? QString("Nothing to add: no transmitter")
        : QString("Nothing to add: it is the destination of %1").arg(known));
}

void MessageCenterGUI::buildRoutes(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    // How the routes work: in the tool tip of the table (and the readme, from the help button of the window)
    // A rule a row, as a sentence: when (the messages its filter selects, and how many of the log it finds), change
    // (marks, decoding), send to (the other sinks), stop
    m_routesTable = makeTable({"#", "Rule"}, container);
    m_routesTable->setItemDelegateForColumn(1, PagingMessageTable::createHtmlDelegate(m_routesTable));
    m_routesTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_routesTable->setToolTip("Every message is checked against all the rules, in their number order (#), not only the first.\n"
        "Each rule that selects it (when) changes it (marks, decoding) and sends it to its places: the log, the queue,\n"
        "or the files, UDP, commands, speech and map of Connections. A rule with stop ends the check for the messages\n"
        "it selects. The log is a place too: a message no rule sends to it is not logged (rule 1, All messages to the\n"
        "log, does it by default). finds: the messages of the log the rule selects.\n"
        "Double-click a rule to edit it");
    QHBoxLayout *buttons = new QHBoxLayout();
    QToolButton *add = iconButton(container, ":/plus.png", "Add a rule");
    QToolButton *edit = iconButton(container, ":/edit.png", "Edit the rule selected");
    QToolButton *remove = iconButton(container, ":/bin.png", "Remove the rule selected");
    QToolButton *up = iconButton(container, ":/arrow_up.png", "Move the rule up: it is checked earlier");
    QToolButton *down = iconButton(container, ":/arrow_down.png", "Move the rule down: it is checked later");
    QToolButton *exportRouting = iconButton(container, ":/save.png", "Save the rules, the sinks and the sources to a JSON file");
    QToolButton *importRouting = iconButton(container, ":/load.png", "Replace the rules, the sinks and the sources with those of a JSON file");
    QPushButton *show = new QPushButton("Show...", container);
    show->setToolTip("The messages of the log the rule selected finds, in a window: newest first, 100 a page");
    connect(show, &QPushButton::clicked, this, [this]() {
        const int row = m_routesTable->currentRow();

        if ((row >= 0) && (row < (int) m_settings.m_routes.size())) {
            showMatches(m_settings.m_routes[row].m_filter, m_settings.m_routes[row].m_name);
        }
    });
    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(remove);
    buttons->addWidget(up);
    buttons->addWidget(down);
    buttons->addWidget(show);
    buttons->addStretch();

    // The presets: the routes and the sinks of each, in place of those set (after a question)
    QToolButton *presets = new QToolButton(container);
    presets->setText("Presets");
    presets->setToolTip("Replace the rules and the sinks with a preset: Empty (every message to the log, as a new\n"
        "Message Center) or Emergency (an example: an icon on the messages of a kind, by a few words of their text)");
    presets->setPopupMode(QToolButton::InstantPopup);
    QMenu *presetsMenu = new QMenu(presets);

    for (const QString& name : MessageCenterSettings::presetNames())
    {
        connect(presetsMenu->addAction(name), &QAction::triggered, this, [this, name]() {
            std::vector<MessageCenterEngine::Route> routes;
            QList<MessageCenterFileSetting> sinks;

            if (!MessageCenterSettings::preset(name, routes, sinks)) {
                return;
            }

            const QString question = QString("Replace the rules (%1) and the sinks (%2) with those of the preset %3 (rules: %4, "
                "sinks: %5)? The sources and the contacts stay.").arg(m_settings.m_routes.size()).arg(m_settings.m_sinks.size())
                .arg(name).arg(routes.size()).arg(sinks.size());

            if (QMessageBox::question(this, "Preset", question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                return;
            }

            m_settings.m_routes = routes;
            m_settings.m_sinks = sinks;
            refreshRoutes();
            refreshFiles();
            applySettings({"routes", "sinks"});
        });
    }

    presets->setMenu(presetsMenu);
    buttons->addWidget(presets);
    buttons->addWidget(importRouting);
    buttons->addWidget(exportRouting);
    layout->addWidget(m_routesTable);
    layout->addLayout(buttons);

    connect(add, &QToolButton::clicked, this, &MessageCenterGUI::addRoute);
    connect(edit, &QToolButton::clicked, this, &MessageCenterGUI::editSelectedRoute);
    connect(remove, &QToolButton::clicked, this, &MessageCenterGUI::removeRoute);
    connect(up, &QToolButton::clicked, this, [this]() { moveRoute(-1); });
    connect(down, &QToolButton::clicked, this, [this]() { moveRoute(1); });
    connect(exportRouting, &QToolButton::clicked, this, &MessageCenterGUI::exportRouting);
    connect(importRouting, &QToolButton::clicked, this, &MessageCenterGUI::importRouting);
    connect(m_routesTable, &QTableWidget::cellDoubleClicked, this, &MessageCenterGUI::editSelectedRoute);
}

void MessageCenterGUI::buildContacts(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_contactsTable = makeTable({"Contact", "Rules", "Note"}, container);
    m_contactsTable->setIconSize(QSize(fontMetrics().height(), fontMetrics().height()));
    m_contactsTable->horizontalHeader()->setStretchLastSection(false);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_contactsTable->setToolTip("The address book: the messages of the addresses of a contact have its name, icon and colour\n"
        "in the log, and the filters choose them by it (Any contact). A pager has several addresses\n"
        "(POCSAG functions, FLEX capcodes): one contact for all. Double-click a contact to edit it");
    QHBoxLayout *buttons = new QHBoxLayout();
    QToolButton *add = iconButton(container, ":/plus.png", "Add a contact");
    QToolButton *edit = iconButton(container, ":/edit.png", "Edit the contact selected");
    QToolButton *remove = iconButton(container, ":/bin.png", "Remove the contact selected");
    QToolButton *exportButton = iconButton(container, ":/save.png", "Save the contacts to a JSON file");
    QToolButton *importButton = iconButton(container, ":/load.png", "Add the contacts of a JSON file (those of the same name are replaced)");
    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(remove);
    buttons->addStretch();
    buttons->addWidget(importButton);
    buttons->addWidget(exportButton);
    layout->addWidget(m_contactsTable);
    layout->addLayout(buttons);

    connect(add, &QToolButton::clicked, this, [this]() { editContact(-1); });
    connect(edit, &QToolButton::clicked, this, [this]() { editContact(m_contactsTable->currentRow()); });
    connect(remove, &QToolButton::clicked, this, &MessageCenterGUI::removeContact);
    connect(exportButton, &QToolButton::clicked, this, &MessageCenterGUI::exportContacts);
    connect(importButton, &QToolButton::clicked, this, &MessageCenterGUI::importContacts);
    connect(m_contactsTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { editContact(row); });
}

void MessageCenterGUI::refreshContacts()
{
    const int selected = m_contactsTable->currentRow();
    m_contactsTable->setRowCount(m_settings.m_contacts.size());

    for (int i = 0; i < m_settings.m_contacts.size(); i++)
    {
        const MessageCenterContact& c = m_settings.m_contacts[i];
        QStringList rules;

        for (const MessageCenterContact::Rule& rule : c.m_rules) {
            rules.append(MessageCenterContact::ruleText(rule));
        }

        setRow(m_contactsTable, i, {c.m_name, rules.join(" | "), c.m_note});
        QTableWidgetItem *name = m_contactsTable->item(i, 0);
        name->setIcon(MessageCenterContactIcons::icon(c.m_icon));
        name->setForeground(c.m_color ? QBrush(QColor::fromRgba(c.m_color)) : QBrush());
        m_contactsTable->item(i, 1)->setToolTip(rules.join("\nor "));
    }

    if ((selected >= 0) && (selected < m_contactsTable->rowCount())) {
        m_contactsTable->selectRow(selected);
    }

    m_logFilter->setContacts(m_settings.m_contacts);

    // The contacts a page can go to: those with a send to
    const QString to = m_toContact->currentData().toString();
    QSignalBlocker blocker(m_toContact);
    m_toContact->clear();
    m_toContact->addItem("the address below", QString());

    for (const MessageCenterContact& c : m_settings.m_contacts)
    {
        if (!c.m_sendTo.isEmpty()) {
            m_toContact->addItem(MessageCenterContactIcons::icon(c.m_icon), c.m_name, c.m_name);
        }
    }

    m_toContact->setCurrentIndex(std::max(0, m_toContact->findData(to)));
    blocker.unblock();
    showContactPage();
    updateAddContact();
}

MessageCenterContact MessageCenterGUI::contactOfPage(const RadioPagingPage& page) const
{
    MessageCenterContact contact;
    MessageCenterContact::Rule rule;
    auto condition = [](const QString& field, const QString& value) {
        MessageCenterContact::Condition c;
        c.m_field = field;
        c.m_value = value;
        return c;
    };

    // The messages of its address as the receivers record them: POCSAG the RIC and function, FLEX the capcode (the
    // address of a capcode form), GSC the address and its number
    rule.append(condition("protocol", page.m_protocol));

    if (page.m_protocol == "pocsag")
    {
        rule.append(condition("ric", QString::number(page.m_address.toULongLong())));
        rule.append(condition("function", QString::number(page.m_function)));
    }
    else if (page.m_protocol == "flex")
    {
        qulonglong capcode = 0;
        page.flexCapcode(capcode);
        rule.append(condition("capcode", QString::number(capcode)));
    }
    else if (page.m_protocol == "gsc")
    {
        QString address;
        int function;
        page.gscAddress(address, function);
        rule.append(condition("address", QString("%1/%2").arg(address).arg(function + 1)));
    }

    // The same address in the other polarity is another pager
    if (page.m_polarity >= 0) {
        rule.append(condition("polarity", (page.m_polarity == 1) ? "inverted" : "normal"));
    }

    contact.m_rules.append(rule);

    MessageCenterContact::SendTo sendTo;
    sendTo.m_page["transmitter"] = page.m_transmitter;
    sendTo.m_page["protocol"] = page.m_protocol;
    sendTo.m_page["address"] = page.m_address;

    if (!RadioPagingPage::functions(page.m_protocol).isEmpty()) {
        sendTo.m_page["function"] = page.m_function;
    }
    if (page.m_polarity >= 0) {
        sendTo.m_page["polarity"] = (page.m_polarity == 1) ? "inverted" : "normal";
    }

    contact.m_sendTo.append(sendTo);
    contact.m_icon = ":pager";
    return contact;
}

QString MessageCenterGUI::contactOfDestination(const RadioPagingPage& page) const
{
    const QString polarity = (page.m_polarity == 1) ? "inverted" : (page.m_polarity == 0) ? "normal" : QString();
    const bool functions = !RadioPagingPage::functions(page.m_protocol).isEmpty();

    for (const MessageCenterContact& c : m_settings.m_contacts)
    {
        for (const MessageCenterContact::SendTo& s : c.m_sendTo)
        {
            const QJsonObject& p = s.m_page;

            // Numbers as numbers (a RIC with or without its leading zeros)
            const QString address = p.value("address").toVariant().toString().trimmed();
            bool a1, a2;
            const qulonglong n1 = address.toULongLong(&a1);
            const qulonglong n2 = page.m_address.trimmed().toULongLong(&a2);
            const bool sameAddress = (a1 && a2) ? (n1 == n2) : (address == page.m_address.trimmed());

            if ((p.value("transmitter").toString() == page.m_transmitter) && (p.value("protocol").toString().toLower() == page.m_protocol)
                && sameAddress && (!functions || (p.value("function").toInt(0) == page.m_function))
                && (p.value("polarity").toString().toLower() == polarity)) {
                return c.m_name;
            }
        }
    }

    return QString();
}

// The fields of the last messages of the log, the usual ones first, then the others by name; their fields in sample,
// if given
QStringList MessageCenterGUI::logFieldNames(std::vector<QJsonObject> *sample) const
{
    QSet<QString> fieldSet;
    const std::deque<MessageRecord>& log = m_center->engine().log();
    const size_t count = std::min<size_t>(log.size(), 2000);

    for (size_t i = log.size() - count; i < log.size(); i++)
    {
        QJsonObject fields = MessageCenterContact::recordFields(log[i]);

        for (const QString& key : fields.keys())
        {
            const QJsonValue::Type type = fields.value(key).type();

            if ((type != QJsonValue::Object) && (key != "protocolData") && !key.startsWith("contact")) {
                fieldSet.insert(key);
            }
        }

        if (sample) {
            sample->push_back(fields);
        }
    }

    QStringList fields = {"protocol", "address", "ric", "function", "capcode", "group_capcodes", "msgType", "channel",
        "frequency", "mode", "quality", "checksum", "signature", "errorsCorrected", "errorsUncorrectable"};
    QStringList seen = fieldSet.values();
    std::sort(seen.begin(), seen.end());

    for (const QString& field : seen)
    {
        if (!fields.contains(field)) {
            fields.append(field);
        }
    }

    return fields;
}

QList<MessageCenterFieldInfo> MessageCenterGUI::fieldInfos() const
{
    QList<MessageCenterFieldInfo> out;
    QSet<QString> described;
    auto add = [&out, &described](const QString& protocol, const QString& key, const QString& name, const QStringList& values) {
        if (!described.contains(protocol + "|" + key))
        {
            described.insert(protocol + "|" + key);
            out.append(MessageCenterFieldInfo{key, name, protocol, values});
        }
    };

    // Common to every message (the fields of the record)
    add("", "protocol", "Protocol", {"pocsag", "flex", "gsc"});
    add("", "msgType", "Message type", {"alphanumeric", "numeric", "tone", "voice", "binary", "secure", "short_message"});
    add("", "quality", "Quality", {"clean", "corrected", "damaged", "none"});
    add("", "direction", "Direction", {"rx", "tx"});
    add("", "polarity", "Polarity", {"normal", "inverted", "unknown"});
    add("", "duplicate", "Duplicate", {"none", "dup", "dup+"});
    add("", "errorsCorrected", "Bit errors corrected", {});
    add("", "errorsUncorrectable", "Codewords that could not be corrected", {});
    add("", "ber", "Bit error rate", {});
    add("", "address", "Address", {});
    add("", "text", "Text", {});
    add("", "mode", "Speed and levels", {});
    add("", "channel", "Channel", {});
    add("", "frequency", "Frequency (Hz)", {});
    add("", "contact", "Contact", {});

    // Those the channels describe, by protocol
    for (const MessageEndpoint& e : m_center->engine().endpoints())
    {
        for (const QVariant& v : e.m_fields)
        {
            const QVariantMap f = v.toMap();
            add(f.value("protocol").toString(), f.value("key").toString(), f.value("name").toString(), f.value("values").toStringList());
        }
    }

    // The others of the log: no name
    QSet<QString> keys;

    for (const MessageCenterFieldInfo& f : out) {
        keys.insert(f.m_key);
    }

    for (const QString& key : logFieldNames())
    {
        if (!keys.contains(key)) {
            out.append(MessageCenterFieldInfo{key, QString(), QString("other"), {}});
        }
    }

    return out;
}

void MessageCenterGUI::editContact(int index, const MessageCenterContact *preset)
{
    const bool existing = (index >= 0) && (index < m_settings.m_contacts.size());
    QStringList others;

    for (int i = 0; i < m_settings.m_contacts.size(); i++)
    {
        if (i != index) {
            others.append(m_settings.m_contacts[i].m_name);
        }
    }

    // The fields of the messages of the log (the conditions are on them), and the last messages to show what each rule
    // finds
    std::vector<QJsonObject> sample;
    const QStringList fields = logFieldNames(&sample);

    // The transmitters that announced themselves
    QStringList transmitters;

    for (const MessageEndpoint& e : m_center->engine().endpoints())
    {
        if ((e.m_role == MessageEndpoint::Transmitter) && !transmitters.contains(e.m_channel)) {
            transmitters.append(e.m_channel);
        }
    }

    MessageCenterContactDialog dialog(preset ? *preset : existing ? m_settings.m_contacts[index] : MessageCenterContact(), others, fields,
        sample, transmitters, this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList keys("contacts");

    if (existing)
    {
        // A contact renamed is still the one its page rules page
        const QString oldName = m_settings.m_contacts[index].m_name;
        m_settings.m_contacts[index] = dialog.contact();
        const QString newName = m_settings.m_contacts[index].m_name;

        for (MessageCenterEngine::Route& rule : m_settings.m_routes)
        {
            if ((oldName != newName) && (rule.m_action == MessageCenterEngine::Route::ActionPage)
                && (rule.m_pageTo == MessageCenterEngine::Route::PageContact) && (rule.m_target == oldName))
            {
                rule.m_target = newName;

                if (!keys.contains("routes")) {
                    keys.append("routes");
                }
            }
        }
    }
    else
    {
        m_settings.m_contacts.append(dialog.contact());
    }

    refreshContacts();
    m_contactsTable->selectRow(existing ? index : m_settings.m_contacts.size() - 1);

    if (keys.contains("routes")) {
        refreshRoutes();
    }

    applySettings(keys);
}

void MessageCenterGUI::removeContact()
{
    const int row = m_contactsTable->currentRow();

    if ((row < 0) || (row >= m_settings.m_contacts.size())) {
        return;
    }

    const QString name = m_settings.m_contacts[row].m_name;
    m_settings.m_contacts.removeAt(row);

    // The rules that paged a contact that is gone go too, as those of a sink removed: they would do nothing
    auto& rules = m_settings.m_routes;
    const size_t before = rules.size();
    rules.erase(std::remove_if(rules.begin(), rules.end(), [&name](const MessageCenterEngine::Route& rule) {
        return (rule.m_action == MessageCenterEngine::Route::ActionPage) && (rule.m_pageTo == MessageCenterEngine::Route::PageContact)
            && (rule.m_target == name);
    }), rules.end());
    refreshContacts();

    if (rules.size() != before)
    {
        refreshRoutes();
        applySettings({"contacts", "routes"});
    }
    else
    {
        applySettings(QStringList("contacts"));
    }
}

void MessageCenterGUI::exportContacts()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export the contacts", "contacts.json", "JSON (*.json)");

    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    const QByteArray json = QJsonDocument::fromJson(MessageCenterContact::listToJson(m_settings.m_contacts)).toJson();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || (file.write(json) < 0)) {
        QMessageBox::warning(this, "Export the contacts", QString("Cannot write %1").arg(path));
    }
}

void MessageCenterGUI::importContacts()
{
    const QString path = QFileDialog::getOpenFileName(this, "Import contacts", QString(), "JSON (*.json);;All files (*)");

    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    QString error;
    const QList<MessageCenterContact> contacts = file.open(QIODevice::ReadOnly)
        ? MessageCenterContact::listFromJson(file.readAll(), &error) : QList<MessageCenterContact>();

    if (!error.isEmpty() || contacts.isEmpty())
    {
        QMessageBox::warning(this, "Import contacts", error.isEmpty() ? QString("%1 has no contacts").arg(path) : error);
        return;
    }

    // A contact of the same name is replaced, the others added
    int replaced = 0;

    for (const MessageCenterContact& c : contacts)
    {
        auto it = std::find_if(m_settings.m_contacts.begin(), m_settings.m_contacts.end(),
            [&c](const MessageCenterContact& other) { return other.m_name == c.m_name; });

        if (it != m_settings.m_contacts.end())
        {
            *it = c;
            replaced++;
        }
        else
        {
            m_settings.m_contacts.append(c);
        }
    }

    refreshContacts();
    applySettings(QStringList("contacts"));
    QMessageBox::information(this, "Import contacts", QString("%1 contacts added, %2 replaced")
        .arg(contacts.size() - replaced).arg(replaced));
}

void MessageCenterGUI::moveRoute(int step)
{
    int row = m_routesTable->currentRow();
    int to = row + step;

    if ((row < 0) || (row >= (int) m_settings.m_routes.size()) || (to < 0) || (to >= (int) m_settings.m_routes.size())) {
        return;
    }

    std::swap(m_settings.m_routes[row], m_settings.m_routes[to]);
    refreshRoutes();
    m_routesTable->selectRow(to);
    applySettings(QStringList("routes"));
}

// The routing as one JSON object: routes, sinks and sources, as the WebAPI has them
void MessageCenterGUI::exportRouting()
{
    QString path = QFileDialog::getSaveFileName(this, "Export the routing", "messagecenter.json", "JSON (*.json)");

    if (path.isEmpty()) {
        return;
    }

    QJsonObject json;
    json["routes"] = QJsonDocument::fromJson(MessageCenterSettings::routesToJson(m_settings.m_routes)).array();
    json["sinks"] = QJsonDocument::fromJson(MessageCenterSettings::filesToJson(m_settings.m_sinks)).array();
    json["sources"] = QJsonDocument::fromJson(MessageCenterSettings::filesToJson(m_settings.m_sources)).array();
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || (file.write(QJsonDocument(json).toJson()) < 0)) {
        QMessageBox::warning(this, "Export the routing", QString("Cannot write %1").arg(path));
    }
}

void MessageCenterGUI::importRouting()
{
    QString path = QFileDialog::getOpenFileName(this, "Import the routing", QString(), "JSON (*.json);;All files (*)");

    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    QJsonParseError parseError;
    QJsonDocument document = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll(), &parseError) : QJsonDocument();

    if (!document.isObject())
    {
        QMessageBox::warning(this, "Import the routing", QString("%1 is not a routing file").arg(path));
        return;
    }

    QJsonObject json = document.object();
    MessageCenterSettings settings = m_settings;
    auto text = [&json](const char *key) { return QJsonDocument(json[key].toArray()).toJson(QJsonDocument::Compact); };
    settings.m_routes = MessageCenterSettings::routesFromJson(text("routes"));
    settings.m_sinks = MessageCenterSettings::filesFromJson(text("sinks"));
    settings.m_sources = MessageCenterSettings::filesFromJson(text("sources"));
    MessageCenterSettings::upgradeRoutes(settings.m_routes, settings.m_sinks);
    QString error = MessageCenter::checkRouting(settings);

    if (!error.isEmpty())
    {
        QMessageBox::warning(this, "Import the routing", error);
        return;
    }

    QString question = QString("Replace %1 rules, %2 sinks and %3 sources with %4 rules, %5 sinks and %6 sources?")
        .arg(m_settings.m_routes.size()).arg(m_settings.m_sinks.size()).arg(m_settings.m_sources.size())
        .arg(settings.m_routes.size()).arg(settings.m_sinks.size()).arg(settings.m_sources.size());

    if (QMessageBox::question(this, "Import the rules", question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_settings.m_routes = settings.m_routes;
    m_settings.m_sinks = settings.m_sinks;
    m_settings.m_sources = settings.m_sources;
    refreshRoutes();
    refreshFiles();
    applySettings({"routes", "sinks", "sources"});
}

void MessageCenterGUI::buildFiles(QWidget *container)
{
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_sinksTable = makeTable({"Sink", "Type", "Target", "Format", "Limit", "Taken", "Failed", "Waiting", "Rules"}, container);
    m_sinksTable->setToolTip("The destinations the rules send messages to: files, UDP, a command, speech, the Map; the log and\n"
        "the queue built in. Limit: at most so many a second, minute or hour, over it held (Waiting) or dropped (Failed).\n"
        "Double-click to edit");
    m_sourcesTable = makeTable({"Source", "Type", "Target"}, container);
    m_sourcesTable->setToolTip("The sources messages are read from, as if they came from the channels: files (read as they\n"
        "grow) and UDP. Double-click to edit");
    // The errors of the sinks and the sources: shown from the first one
    m_errors = new QPlainTextEdit(container);
    m_errors->setReadOnly(true);
    m_errors->setMaximumBlockCount(200);
    m_errors->setMaximumHeight(m_errors->fontMetrics().lineSpacing() * 3 + 2 * m_errors->frameWidth() + 8);
    m_errors->setToolTip("Errors of the sinks and the sources");
    m_errors->setVisible(false);
    QHBoxLayout *sinkButtons = new QHBoxLayout();
    QToolButton *addSink = iconButton(container, ":/plus.png", "Add a sink");
    QToolButton *removeSink = iconButton(container, ":/bin.png", "Remove the sink selected; the rules no longer send to it");
    sinkButtons->addWidget(addSink);
    sinkButtons->addWidget(removeSink);
    sinkButtons->addStretch();
    QHBoxLayout *sourceButtons = new QHBoxLayout();
    QToolButton *addSource = iconButton(container, ":/plus.png", "Add a source");
    QToolButton *removeSource = iconButton(container, ":/bin.png", "Remove the source selected");
    sourceButtons->addWidget(addSource);
    sourceButtons->addWidget(removeSource);
    sourceButtons->addStretch();
    layout->addWidget(m_sinksTable);
    layout->addLayout(sinkButtons);
    layout->addWidget(m_sourcesTable);
    layout->addLayout(sourceButtons);
    layout->addWidget(m_errors);

    connect(m_sinksTable, &QTableWidget::cellDoubleClicked, this, &MessageCenterGUI::editSelectedSink);
    connect(m_sourcesTable, &QTableWidget::cellDoubleClicked, this, &MessageCenterGUI::editSelectedSource);
    connect(addSink, &QToolButton::clicked, this, &MessageCenterGUI::addSink);
    connect(removeSink, &QToolButton::clicked, this, &MessageCenterGUI::removeSink);
    connect(addSource, &QToolButton::clicked, this, &MessageCenterGUI::addSource);
    connect(removeSource, &QToolButton::clicked, this, &MessageCenterGUI::removeSource);
}

void MessageCenterGUI::buildOptions(QWidget *container)
{
    QFormLayout *layout = new QFormLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);

    m_logSize = new QSpinBox(container);
    m_logSize->setRange(MessageCenterEngine::m_minLogSize, MessageCenterEngine::m_maxLogSize);
    m_logSize->setSingleStep(1000);
    m_logSize->setToolTip(QString("Messages kept in the log (in memory), %1 to %2; the oldest go first. The table shows the "
        "newest %3 the filters select").arg(MessageCenterEngine::m_minLogSize).arg(MessageCenterEngine::m_maxLogSize).arg(m_maxLogRows));
    m_claimTimeout = new QSpinBox(container);
    m_claimTimeout->setRange(1, 3600);
    m_claimTimeout->setSuffix(" s");
    m_claimTimeout->setToolTip("A page given to a transmitter that does not claim it in this time goes back to the queue");
    m_silentTime = new QSpinBox(container);
    m_silentTime->setRange(6, 3600);
    m_silentTime->setSuffix(" s");
    m_silentTime->setToolTip("A channel that does not announce itself for this time is offline");
    m_historyEnabled = new QCheckBox("Keep", container);
    m_historyEnabled->setToolTip(QString("Keep the log in the history file,\nso that it is there again when the message center "
        "starts:\n%1\nOne message center keeps the history").arg(MessageCenter::historyFilePath()));
    m_historyMaxMB = new QSpinBox(container);
    m_historyMaxMB->setRange(1, 10000);
    m_historyMaxMB->setSuffix(" MB");
    m_historyMaxMB->setToolTip("Size kept of the history: over it the oldest days go");
    m_historyDays = new QSpinBox(container);
    m_historyDays->setRange(1, 3650);
    m_historyDays->setSuffix(" days");
    m_historyDays->setToolTip("Days kept of the history: older days go");
    QWidget *history = new QWidget(container);
    QHBoxLayout *historyLayout = new QHBoxLayout(history);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->addWidget(m_historyEnabled);
    historyLayout->addWidget(m_historyMaxMB, 1);
    historyLayout->addWidget(m_historyDays, 1);

    layout->addRow("Log size", m_logSize);
    layout->addRow("History", history);
    layout->addRow("Claim time", m_claimTimeout);
    layout->addRow("Offline after", m_silentTime);

    connect(m_logSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_logSize = value;
        applySettings(QStringList("logSize"));
    });
    connect(m_historyEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.m_historyEnabled = checked;
        m_historyMaxMB->setEnabled(checked);
        m_historyDays->setEnabled(checked);
        applySettings(QStringList("historyEnabled"));
    });
    connect(m_historyMaxMB, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_historyMaxMB = value;
        applySettings(QStringList("historyMaxMB"));
    });
    connect(m_historyDays, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_historyDays = value;
        applySettings(QStringList("historyDays"));
    });
    connect(m_claimTimeout, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_claimTimeoutSec = value;
        applySettings(QStringList("claimTimeoutSec"));
    });
    connect(m_silentTime, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_silentSec = value;
        applySettings(QStringList("silentSec"));
    });
}

void MessageCenterGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList("rollupState"));
    sizeToContents();
}

void MessageCenterGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);
        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();
        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);
        applySettings({"title", "rgbColor", "useReverseAPI", "reverseAPIAddress", "reverseAPIPort",
            "reverseAPIFeatureSetIndex", "reverseAPIFeatureIndex"});
    }

    resetContextMenuType();
}

void MessageCenterGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings) {
        m_center->getInputMessageQueue()->push(MessageCenter::MsgConfigureMessageCenter::create(m_settings, settingsKeys, force));
    }
}

void MessageCenterGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    m_doApplySettings = false;
    m_logSize->setValue(m_settings.m_logSize);
    m_historyEnabled->setChecked(m_settings.m_historyEnabled);
    m_historyMaxMB->setValue(m_settings.m_historyMaxMB);
    m_historyMaxMB->setEnabled(m_settings.m_historyEnabled);
    m_historyDays->setValue(m_settings.m_historyDays);
    m_historyDays->setEnabled(m_settings.m_historyEnabled);
    m_claimTimeout->setValue(m_settings.m_claimTimeoutSec);
    m_silentTime->setValue(m_settings.m_silentSec);
    refreshRoutes();
    refreshContacts();
    refreshFiles();
    m_doApplySettings = true;

    // The columns the user chose; the first time the usual ones. Saved with the tag of the columns of the Messages
    // tab: those of the log of before (all the columns) give the usual ones once
    if (!m_settings.m_logColumns.startsWith(logColumnsTag)
        || !m_logTable->setColumns(m_settings.m_logColumns.mid(logColumnsTag.size()))) {
        m_logTable->sizeColumns();
    }
    m_logTable->setAudioDevice(m_settings.m_voicePlaybackDeviceName);
    getRollupContents()->restoreState(m_rollupState);
    getRollupContents()->arrangeRollups();
}

void MessageCenterGUI::handleInputMessages()
{
    Message *message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }

        delete message;
    }
}

bool MessageCenterGUI::handleMessage(const Message& message)
{
    // Settings changed from the WebAPI or the MCP server (routes, sinks, sources...): shown at once
    if (MessageCenter::MsgConfigureMessageCenter::match(message))
    {
        const MessageCenter::MsgConfigureMessageCenter& cfg = (const MessageCenter::MsgConfigureMessageCenter&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        m_settings.setRollupState(&m_rollupState);
        displaySettings();
        return true;
    }
    if (MessageCenter::MsgUpdated::match(message))
    {
        refresh();
        return true;
    }
    if (MessageCenter::MsgSpeak::match(message))
    {
        speak(((const MessageCenter::MsgSpeak&) message).getText());
        return true;
    }
    return false;
}

void MessageCenterGUI::refresh()
{
    refreshLog();
    showLiveCount();    // A count put off (at most every 2 s) is made now
    refreshEndpoints();
    refreshQueue();
    refreshComposeTransmitters();
    refreshFiles();         // The counts of the sinks

    for (const QString& error : m_center->takeErrors()) {
        m_errors->appendPlainText(QString("%1 %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(error));
        m_errors->setVisible(true);
    }
}

// The newest messages of the log that the filter selects; made again when the log changed
// Used by reference (captured, std::max): defined once
const int MessageCenterGUI::m_maxLogRows;
const int MessageCenterGUI::m_pageRows;

// The bar above the filters: Live or Query, and the range, the pages and the refresh of a query
void MessageCenterGUI::buildQueryBar(QWidget *container)
{
    m_queryBar = new QWidget(container);
    QHBoxLayout *bar = new QHBoxLayout(m_queryBar);
    bar->setContentsMargins(0, 0, 0, 0);

    m_liveButton = new QToolButton(m_queryBar);
    m_liveButton->setText("Live");
    m_liveButton->setCheckable(true);
    m_liveButton->setChecked(true);
    m_liveButton->setToolTip(QString("The newest %1 messages the filters select, as they come").arg(m_maxLogRows));
    m_queryButton = new QToolButton(m_queryBar);
    m_queryButton->setText("Query");
    m_queryButton->setCheckable(true);
    m_queryButton->setToolTip(QString("The messages of a time range the filters select, over the history: a page of %1, "
        "newest first,\nand how many there are. They stay as they are until Run, a page or the auto-refresh").arg(m_pageRows));

    m_range = new QComboBox(m_queryBar);
    m_range->addItem("Last 5 min", 5 * 60);
    m_range->addItem("Last hour", 3600);
    m_range->addItem("Last day", 24 * 3600);
    m_range->addItem("Last 7 days", 7 * 24 * 3600);
    m_range->addItem("All", 0);
    m_range->addItem("From - to", -1);
    m_range->setCurrentIndex(2);
    m_range->setToolTip("The time range of the messages: from now, fixed when the query is run (the pages keep it)");
    m_from = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(-3600), m_queryBar);
    m_to = new QDateTimeEdit(QDateTime::currentDateTime(), m_queryBar);

    for (QDateTimeEdit *edit : {m_from, m_to})
    {
        edit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
        edit->setCalendarPopup(true);
        edit->setVisible(false);
    }

    QPushButton *run = new QPushButton("Run", m_queryBar);
    run->setToolTip("Run the query: the range from now, the first page");
    m_refresh = new QComboBox(m_queryBar);
    m_refresh->addItem("No refresh", 0);
    m_refresh->addItem("Every 5 s", 5);
    m_refresh->addItem("Every 10 s", 10);
    m_refresh->addItem("Every 30 s", 30);
    m_refresh->addItem("Every 60 s", 60);
    m_refresh->setToolTip("Run the query again by itself (the range from now, the first page)");
    m_newer = new QToolButton(m_queryBar);
    m_newer->setText("Newer");
    m_newer->setToolTip(QString("The %1 newer messages").arg(m_pageRows));
    m_older = new QToolButton(m_queryBar);
    m_older->setText("Older");
    m_older->setToolTip(QString("The %1 older messages").arg(m_pageRows));
    m_queryStatus = new QLabel(m_queryBar);
    m_refreshTimer = new QTimer(this);
    // The filters take room: shown when asked
    m_filtersButton = new QToolButton(m_queryBar);
    m_filtersButton->setText(tr("Filters"));
    m_filtersButton->setCheckable(true);
    m_filtersButton->setToolTip(tr("Show the filters of the messages: protocol, direction, polarity, type, channel, "
        "address, text, a field"));
    m_logFilter->setVisible(false);
    connect(m_filtersButton, &QToolButton::toggled, m_logFilter, &QWidget::setVisible);
    m_showFragments = new QCheckBox(tr("Fragments"), m_queryBar);
    m_showFragments->setToolTip(tr("A FLEX message sent in fragments: each fragment as a row of its own as well (they "
        "come before the whole message)"));
    connect(m_showFragments, &QCheckBox::toggled, this, [this]() {
        if (m_queryMode) {
            runQuery(false);
        } else {
            m_shownLogged = -1;     // All the rows again
            refreshLog();
        }
    });

    bar->addWidget(m_filtersButton);
    bar->addWidget(m_liveButton);
    bar->addWidget(m_queryButton);
    bar->addWidget(m_range);
    bar->addWidget(m_from);
    bar->addWidget(m_to);
    bar->addWidget(run);
    bar->addWidget(m_refresh);
    bar->addWidget(m_queryStatus);
    bar->addStretch(1);
    bar->addWidget(m_showFragments);
    bar->addWidget(m_newer);
    bar->addWidget(m_older);

    connect(m_liveButton, &QToolButton::clicked, this, [this]() { setLogMode(false); });
    connect(m_queryButton, &QToolButton::clicked, this, [this]() { setLogMode(true); });
    connect(m_range, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const bool custom = m_range->currentData().toInt() < 0;
        m_from->setVisible(m_queryMode && custom);
        m_to->setVisible(m_queryMode && custom);
        runQuery(true);
    });
    connect(run, &QPushButton::clicked, this, [this]() { runQuery(true); });
    connect(m_newer, &QToolButton::clicked, this, [this]() {
        m_queryOffset = std::max(0, m_queryOffset - m_pageRows);
        runQuery(false);
    });
    connect(m_older, &QToolButton::clicked, this, [this]() {
        m_queryOffset += m_pageRows;
        runQuery(false);
    });
    connect(m_refresh, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const int seconds = m_refresh->currentData().toInt();

        if (seconds > 0) {
            m_refreshTimer->start(seconds * 1000);
        } else {
            m_refreshTimer->stop();
        }
    });
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        // Not over a query being made, nor while a row is looked at
        if (m_queryMode && !m_queryBusy && m_logTable->selectedId().isEmpty())
        {
            m_queryOffset = 0;
            runQuery(true);
        }
    });

    setLogMode(false);
}

void MessageCenterGUI::setLogMode(bool query)
{
    m_queryMode = query;
    m_liveButton->setChecked(!query);
    m_queryButton->setChecked(query);
    const bool custom = m_range->currentData().toInt() < 0;

    // The status says what is shown in both: live, or what the query found
    for (QWidget *w : std::initializer_list<QWidget*>{m_range, m_refresh, m_newer, m_older}) {
        w->setVisible(query);
    }
    for (QWidget *w : m_queryBar->findChildren<QPushButton*>()) {
        w->setVisible(query);
    }

    m_from->setVisible(query && custom);
    m_to->setVisible(query && custom);

    if (query)
    {
        m_queryOffset = 0;
        runQuery(true);
    }
    else
    {
        m_refreshTimer->stop();
        m_refresh->setCurrentIndex(0);
        m_shownLogged = -1;
        refreshLog();
    }
}

void MessageCenterGUI::runQuery(bool resolve)
{
    if (!m_queryMode) {
        return;
    }

    if (resolve)
    {
        const int seconds = m_range->currentData().toInt();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_queryOffset = 0;

        if (seconds > 0)
        {
            m_queryFrom = now - (qint64) seconds * 1000;
            m_queryTo = now;
        }
        else if (seconds == 0)
        {
            m_queryFrom = -1;
            m_queryTo = -1;
        }
        else
        {
            m_queryFrom = m_from->dateTime().toMSecsSinceEpoch();
            m_queryTo = m_to->dateTime().toMSecsSinceEpoch();
        }
    }

    // The files are read in a thread of the pool; the result comes back here. A newer query drops the older result
    const int number = ++m_queryRunning;
    const MessageCenterFilter filter = logFilter();
    const qint64 from = m_queryFrom;
    const qint64 to = m_queryTo;
    const int offset = m_queryOffset;
    MessageCenter *center = m_center;
    QPointer<MessageCenterGUI> self(this);
    m_queryBusy = true;
    m_queryStatus->setText("Running...");

    QThreadPool::globalInstance()->start([=]() {
        MessageCenterArchiveResult result = center->queryMessages(from, to, filter, offset, m_pageRows);

        QMetaObject::invokeMethod(qApp, [self, number, result]() {
            if (self) {
                self->showQueryResult(number, result);
            }
        }, Qt::QueuedConnection);
    });
}

void MessageCenterGUI::showQueryResult(int number, const MessageCenterArchiveResult& result)
{
    if ((number != m_queryRunning) || !m_queryMode) {
        return;
    }

    m_queryBusy = false;
    m_queryTotal = result.m_total;
    QString selected = m_logTable->selectedId();

    // The page newest at the bottom, as the live log
    m_logTable->setUpdatesEnabled(false);
    m_logTable->setRowCount(0);

    for (auto it = result.m_records.rbegin(); it != result.m_records.rend(); ++it) {
        m_logTable->addRecord(*it);
    }

    m_logTable->selectId(selected);
    m_logTable->setUpdatesEnabled(true);
    m_logTable->scrollToBottom();

    const int first = result.m_records.empty() ? 0 : m_queryOffset + 1;
    const int last = m_queryOffset + (int) result.m_records.size();
    m_queryStatus->setText(QString("%1 found, %2-%3").arg(result.m_total).arg(first).arg(last));
    m_queryStatus->setToolTip(QString("%1 messages the query selects; %2 to %3 of them shown, newest first%4").arg(result.m_total)
        .arg(first).arg(last).arg(m_center->engine().archive() ? QString(" (over the history)") : QString(" (no history: over the log)")));
    m_newer->setEnabled(m_queryOffset > 0);
    m_older->setEnabled(last < result.m_total);
}

void MessageCenterGUI::refreshLog()
{
    if (m_queryMode) {
        return;     // A query stays as it is until it is run again
    }

    const MessageCenterEngine& engine = m_center->engine();

    if (engine.logged() == m_shownLogged) {
        return;
    }

    // The records stored or changed since the table was shown: their rows only. Making all the rows again at each
    // change of a large log froze the GUI
    QStringList changed;

    if ((m_shownLogged >= 0) && engine.changedSince(m_shownLogged, changed))
    {
        const MessageCenterFilter filter = logFilter();
        QSet<QString> done;

        m_shownLogged = engine.logged();

        for (const QString& id : changed)
        {
            if (done.contains(id)) {
                continue;
            }

            done.insert(id);
            const MessageRecord *record = engine.find(id);

            if (record && filter.matches(*record)) {
                m_logTable->addRecord(*record);
            }
            else
            {
                // A record the filter no longer selects (decoded again), or gone from the log
                const int row = m_logTable->findRow(id);

                if (row >= 0) {
                    m_logTable->removeRow(row);
                }
            }
        }

        showLiveCount();
        return;
    }

    m_shownLogged = engine.logged();
    std::vector<MessageRecord> records = engine.select(logFilter());
    m_liveMatching = (int) records.size();      // Counted as selected
    m_countedLogged = engine.logged();
    m_countTimer.start();
    size_t first = (records.size() > (size_t) m_maxLogRows) ? records.size() - m_maxLogRows : 0;
    QString selected = m_logTable->selectedId();

    m_logTable->setUpdatesEnabled(false);
    m_logTable->setRowCount(0);

    for (size_t i = first; i < records.size(); i++) {
        m_logTable->addRecord(records[i]);
    }

    m_logTable->selectId(selected);
    m_logTable->setUpdatesEnabled(true);
    m_logTable->scrollToBottom();
    showLiveCount();
}

// The filters of the Messages tab. A fragment of a FLEX message is a record of its own (the receiver reports each
// fragment, then the whole message): shown only when asked, a message is one row. In the filter, so that a page of a
// query and its totals count the rows shown
MessageCenterFilter MessageCenterGUI::logFilter() const
{
    MessageCenterFilter f = m_logFilter->filter();
    f.m_hideFragments = !m_showFragments->isChecked();
    return f;
}

// The menu of a message: page its address again (the Send tab filled), add its sender as a contact, show only the
// messages of its pager; a page of the queue: requeue it (failed) or cancel it (waiting)
void MessageCenterGUI::messageMenu(QMenu& menu, const MessageRecord& record)
{
    if ((record.m_kind != MessageRecord::KindMessage) || record.m_address.isEmpty()) {
        return;
    }

    // The destination of the record as a page: the address digits, the function, the polarity
    RadioPagingPage page;
    const QVariantMap& d = record.m_protocolData;
    QString digits;

    for (const QChar c : record.m_address)
    {
        if (!c.isDigit()) {
            break;
        }
        digits.append(c);
    }

    page.m_protocol = record.m_protocol;
    page.m_transmitter = m_composer->transmitter();
    page.m_address = (record.m_protocol == "pocsag") && d.contains("ric") ? d.value("ric").toString() : digits;
    page.m_function = (record.m_protocol == "pocsag") ? d.value("function").toInt()
        : (record.m_protocol == "gsc") ? std::max(0, d.value("address_number").toInt() - 1) : 0;
    page.m_polarity = (record.m_polarity == MessageRecord::PolarityNormal) ? 0
        : (record.m_polarity == MessageRecord::PolarityInverted) ? 1 : -1;

    if (!page.m_address.isEmpty())
    {
        connect(menu.addAction(QIcon(":/tx.png"), tr("Page this address...")), &QAction::triggered, this, [this, page]() {
            m_composer->setDestination(page.m_transmitter, page.m_protocol, page.m_address, page.m_function, page.m_polarity);

            for (int i = 0; i < m_tabs->count(); i++)
            {
                if (m_tabs->tabToolTip(i).startsWith(tr("Send a page"))) {
                    m_tabs->setCurrentIndex(i);
                }
            }
        });
        connect(menu.addAction(QIcon(":/star.png"), tr("Add as a contact...")), &QAction::triggered, this, [this, page]() {
            const MessageCenterContact preset = contactOfPage(page);
            editContact(-1, &preset);
        });
    }

    connect(menu.addAction(QIcon(":/funnel.png"), tr("Only this pager")), &QAction::triggered, this, [this, record]() {
        MessageCenterFilter f = m_logFilter->filter();
        f.m_address = QString("^%1$").arg(QRegularExpression::escape(record.m_address));
        m_logFilter->setFilter(f);
        m_filtersButton->setChecked(true);      // The filter shown, to be cleared
    });

    // A page of the Message Center that did not go, or is still waiting
    const QString pageState = d.value("page_state").toString();

    if ((record.m_direction == MessageRecord::DirectionTX) && record.m_channel.isEmpty() && !pageState.isEmpty())
    {
        if ((pageState == "dead") || (pageState == "cancelled"))
        {
            connect(menu.addAction(tr("Send again (requeue)")), &QAction::triggered, this, [this, record]() {
                m_center->getInputMessageQueue()->push(MessageCenter::MsgPageAction::create(record.m_id,
                    MessageCenter::MsgPageAction::Requeue));
            });
        }
        else if ((pageState == "pending") || (pageState == "queued"))
        {
            connect(menu.addAction(tr("Cancel the page")), &QAction::triggered, this, [this, record]() {
                m_center->getInputMessageQueue()->push(MessageCenter::MsgPageAction::create(record.m_id,
                    MessageCenter::MsgPageAction::Cancel));
            });
        }
    }
}

// Live: how many messages are shown, of how many in the log
void MessageCenterGUI::showLiveCount()
{
    if (m_queryMode) {
        return;
    }

    // The messages of the log the filters select: counted again when the log changed, at most every 2 s (a pass over
    // the log in memory, at most its size; a full refresh counts them as it selects)
    const MessageCenterEngine& engine = m_center->engine();

    if ((m_countedLogged != engine.logged()) && (!m_countTimer.isValid() || (m_countTimer.elapsed() >= 2000)))
    {
        const MessageCenterFilter filter = logFilter();
        m_liveMatching = (int) std::count_if(engine.log().begin(), engine.log().end(),
            [&filter](const MessageRecord& r) { return filter.matches(r); });
        m_countedLogged = engine.logged();
        m_countTimer.start();
    }

    const int shown = m_logTable->rowCount();
    const int inLog = (int) engine.log().size();
    m_queryStatus->setText(tr("Live: %1 of %2 matching (%3 in the log)").arg(shown).arg(m_liveMatching).arg(inLog));
    m_queryStatus->setToolTip(tr("The newest %1 messages the filters select, as they come: %2 shown, %3 of the log match "
        "the filters, %4 in the log (in memory: Options sets its size).\nQuery searches the history").arg(m_maxLogRows)
        .arg(shown).arg(m_liveMatching).arg(inLog));
}

void MessageCenterGUI::refreshEndpoints()
{
    static const char *statuses[] = {"offline", "idle", "busy"};
    const QMap<QString, MessageEndpoint>& endpoints = m_center->engine().endpoints();
    const QString selected = selectedId(m_endpointsTable);
    m_endpointsTable->setRowCount(endpoints.size());
    int row = 0;

    for (auto it = endpoints.constBegin(); it != endpoints.constEnd(); ++it)
    {
        const MessageEndpoint& e = it.value();
        QStringList speeds, polarities;

        for (int s : e.m_speeds) {
            speeds << QString::number(s);
        }
        for (int p : e.m_polarities) {
            polarities << ((p == (int) MessageRecord::PolarityInverted) ? "inverted" : "normal");
        }

        setRow(m_endpointsTable, row, {
            e.m_channel, e.m_role == MessageEndpoint::Transmitter ? "transmitter" : "receiver", statuses[(int) e.m_status],
            e.m_protocols.join(", "), speeds.join(", "), polarities.join(", "),
            e.m_role == MessageEndpoint::Transmitter ? QString("%1/%2").arg(e.m_queueDepth).arg(e.m_capacity) : QString(),
            QDateTime::fromMSecsSinceEpoch(e.m_lastSeen).toString("HH:mm:ss")
        });
        m_endpointsTable->item(row, 0)->setData(idRole, it.key());

        if (it.key() == selected) {
            m_endpointsTable->selectRow(row);
        }

        row++;
    }
}

void MessageCenterGUI::refreshQueue()
{
    std::vector<MessageCenterPage> pages = m_center->engine().pages();
    QString selected = selectedId(m_queueTable);
    m_queueTable->setRowCount((int) pages.size());

    for (int i = 0; i < (int) pages.size(); i++)
    {
        const MessageCenterPage& p = pages[i];
        setRow(m_queueTable, i, {
            p.m_record.m_id.left(8), p.transmitter(), p.m_record.m_protocol, p.m_record.m_address, p.m_record.m_text,
            MessageCenterPage::stateName(p.m_state), QString::number(p.m_attempts), QString::number(p.m_retransmits), p.m_reason
        });
        m_queueTable->item(i, 0)->setData(idRole, p.m_record.m_id);
        m_queueTable->item(i, 0)->setToolTip(p.m_record.m_id);

        if (p.m_record.m_id == selected) {
            m_queueTable->selectRow(i);
        }
    }
}

void MessageCenterGUI::refreshComposeTransmitters()
{
    QStringList aliases;

    for (const MessageEndpoint& e : m_center->engine().endpoints())
    {
        if ((e.m_role == MessageEndpoint::Transmitter) && (e.m_status != MessageEndpoint::Offline)) {
            aliases.append(e.m_channel);
        }
    }

    // The composer keeps its choice when the transmitters did not change
    m_composer->setTransmitters(aliases);
    // What the transmitter can send may have changed (its mode), and the page posted moved on
    applyCapabilities();
    showSendResult();
}

// What the transmitter announced it can send sets what the form offers: copies and a send delay in network mode
// only, FLEX copies with the numbering of the messages. A transmitter that does not say: everything
void MessageCenterGUI::applyCapabilities()
{
    if (!m_transmitterNote) {
        return;     // The Send tab being made
    }

    const QString alias = m_composer->transmitter();
    QVariantMap capabilities;

    for (const MessageEndpoint& e : m_center->engine().endpoints())
    {
        if ((e.m_role == MessageEndpoint::Transmitter) && (e.m_channel == alias)) {
            capabilities = e.m_capabilities;
        }
    }

    const bool network = capabilities.value("network", true).toBool();
    const bool numbering = capabilities.value("flexNumbering", true).toBool();

    if ((network != m_shownNetwork) || (numbering != m_shownNumbering))
    {
        m_shownNetwork = network;
        m_shownNumbering = numbering;
        m_composer->setNetworkMode(network);
        m_composer->setFlexNumbering(numbering);
    }

    QString note;

    if (!network) {
        note = QString("%1 is in one-shot mode: no copies, no send delay (network mode only)").arg(alias);
    } else if (!numbering && m_composer->protocol() == "flex") {
        note = QString("%1 does not number FLEX messages: no copies of FLEX messages").arg(alias);
    }

    m_transmitterNote->setText(note);
    m_transmitterNote->setVisible(!note.isEmpty());
}

// The last page posted: where it is now, from the queue (or its row in the log once the queue let it go)
void MessageCenterGUI::showSendResult()
{
    if (m_lastPostId.isEmpty() || !m_sendResult) {
        return;
    }

    const MessageCenterEngine& engine = m_center->engine();
    const MessageCenterPage *page = engine.page(m_lastPostId);
    QString state;
    QString reason;

    if (page)
    {
        static const char *names[] = {"pending", "queued", "claimed", "transmitted", "sent", "dead"};
        state = names[page->m_state];
        reason = page->m_reason;
    }
    else if (const MessageRecord *record = engine.find(m_lastPostId))
    {
        state = record->m_channel.isEmpty() ? record->m_protocolData.value("page_state").toString() : QString("sent");
        reason = record->m_protocolData.value("page_reason").toString();
    }
    else
    {
        return;     // Not known yet: the message to the Message Center is on its way
    }

    const QString when = m_lastPostTime.toString("HH:mm:ss");
    QString text;
    bool failed = false;

    if ((state == "pending") || (state == "queued")) {
        text = QString("Posted %1 to %2: waiting for %3").arg(when, m_lastPostTo, m_lastPostTransmitter);
    } else if (state == "claimed") {
        text = QString("Posted %1 to %2: %3 is sending it").arg(when, m_lastPostTo, m_lastPostTransmitter);
    } else if ((state == "transmitted") || (state == "sent")) {
        text = QString("Posted %1 to %2: sent by %3").arg(when, m_lastPostTo, m_lastPostTransmitter);
    } else if (state == "cancelled") {
        text = QString("Posted %1 to %2: cancelled").arg(when, m_lastPostTo);
    } else {
        text = QString("Not sent (%1) to %2: %3").arg(when, m_lastPostTo, reason.isEmpty() ? QString("failed") : reason);
        failed = true;
    }

    m_sendResult->setText(text);
    m_sendResult->setStyleSheet(failed ? "QLabel { color: #ff7777; }" : "QLabel { color: #66bb6a; }");
}

// A page that cannot be posted: why, where the result goes
void MessageCenterGUI::showSendError(const QString& error)
{
    m_lastPostId.clear();
    m_sendResult->setText(QString("Not posted: %1").arg(error));
    m_sendResult->setStyleSheet("QLabel { color: #ff7777; }");
}

void MessageCenterGUI::refreshRoutes()
{
    const std::vector<MessageCenterEngine::Route>& routes = m_settings.m_routes;
    int selected = m_routesTable->currentRow();
    m_routesTable->setRowCount((int) routes.size());

    for (int i = 0; i < (int) routes.size(); i++)
    {
        const QString sentence = ruleSentence(routes[i]);
        QTextDocument plain;
        plain.setHtml(sentence);
        setRow(m_routesTable, i, {QString::number(i + 1), plain.toPlainText()});
        m_routesTable->item(i, 1)->setData(PagingMessageTable::m_htmlRole, sentence);
        m_routesTable->item(i, 1)->setToolTip(plain.toPlainText());
    }

    if ((selected >= 0) && (selected < (int) routes.size())) {
        m_routesTable->selectRow(selected);
    }
}

// The messages of the log a filter selects, in a window of their own: the table of Messages, newest first, 100 a page
// with Newer and Older. The log as it is when the window opens. A child of parent: usable over a dialog being shown
void MessageCenterGUI::showMatches(const MessageCenterFilter& filter, const QString& title, QWidget *parent)
{
    // The messages found, oldest first, kept by the window
    std::shared_ptr<std::vector<MessageRecord>> found = std::make_shared<std::vector<MessageRecord>>(m_center->engine().select(filter));
    QDialog *window = new QDialog(parent ? parent : this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(title.isEmpty() ? QString("Messages found") : QString("Messages found: %1").arg(title));
    QVBoxLayout *layout = new QVBoxLayout(window);
    QLabel *summary = new QLabel(QString("When a message is %1").arg(filter.isEmpty() ? QString("anything") : filterSummary(filter)), window);
    summary->setWordWrap(true);
    PagingMessageTable *table = new PagingMessageTable(window);
    table->setDefaultColumns({PagingMessageTable::ColDate, PagingMessageTable::ColTime, PagingMessageTable::ColDirection,
        PagingMessageTable::ColProtocol, PagingMessageTable::ColPolarity, PagingMessageTable::ColAddress, PagingMessageTable::ColType,
        PagingMessageTable::ColMessage});
    table->setContactInAddress(true);
    table->setSingleLineMessages(true);
    table->setMaxRows(m_pageRows);
    QHBoxLayout *bar = new QHBoxLayout();
    QLabel *status = new QLabel(window);
    QToolButton *newer = new QToolButton(window);
    newer->setText("Newer");
    QToolButton *older = new QToolButton(window);
    older->setText("Older");
    bar->addWidget(status);
    bar->addStretch(1);
    bar->addWidget(newer);
    bar->addWidget(older);
    layout->addWidget(summary);
    layout->addWidget(table);
    layout->addLayout(bar);

    // A page: offset messages from the newest; shown oldest at the top, as the log
    std::shared_ptr<int> offset = std::make_shared<int>(0);
    auto showPage = [table, status, newer, older, offset, found]() {
        const int total = (int) found->size();
        const int last = total - *offset;                       // One past the newest of the page
        const int first = std::max(0, last - m_pageRows);
        table->setUpdatesEnabled(false);
        table->setRowCount(0);

        for (int i = first; i < last; i++) {
            table->addRecord((*found)[i]);
        }

        table->setUpdatesEnabled(true);
        table->scrollToBottom();
        status->setText(total == 0 ? QString("None in the log")
            : QString("%1 to %2 of %3, newest first").arg(*offset + 1).arg(*offset + (last - first)).arg(total));
        newer->setEnabled(*offset > 0);
        older->setEnabled(first > 0);
    };
    connect(newer, &QToolButton::clicked, window, [offset, showPage]() {
        *offset = std::max(0, *offset - m_pageRows);
        showPage();
    });
    connect(older, &QToolButton::clicked, window, [offset, found, showPage]() {
        if (*offset + m_pageRows < (int) found->size())
        {
            *offset += m_pageRows;
            showPage();
        }
    });
    window->resize(640, 420);
    window->show();
    // The widths of the columns of Messages (its sizes fit the chips), the others to their contents
    table->sizeColumns();

    for (int c = 0; c < PagingMessageTable::ColumnCount; c++)
    {
        if ((c != PagingMessageTable::ColMessage) && (m_logTable->columnWidth(c) > 0)) {
            table->setColumnWidth(c, m_logTable->columnWidth(c));
        }
    }

    showPage();
}

// A rule as a sentence, on three lines: its name and when (the filter, how many messages of the log it finds), the
// changes (marks, decoding), where it sends (and stop). Off: grey
QString MessageCenterGUI::ruleSentence(const MessageCenterEngine::Route& route) const
{
    typedef MessageCenterEngine::Route Rule;
    const QString key = route.m_enabled ? QString("<b style=\"color:#88aacc\">%1</b>") : QString("<b>%1</b>");
    const QString pill = route.m_enabled ? QString("<span style=\"color:#e0e0e0\">%1</span>") : QString("%1");

    // What it finds: the messages of the log it selects (wherever they came from), counted without copying them
    const MessageCenterFilter& filter = route.m_filter;
    const int finds = (int) std::count_if(m_center->engine().log().begin(), m_center->engine().log().end(),
        [&filter](const MessageRecord& r) { return filter.matches(r); });
    const QString when = route.m_filter.isEmpty() ? QString("%1 any message").arg(key.arg("when"))
        : QString("%1 a message is %2").arg(key.arg("when"), pill.arg(filterSummary(route.m_filter).toHtmlEscaped()));

    // Its action, as a sentence
    QString then;

    switch (route.m_action)
    {
    case Rule::ActionLog:
        then = QString("%1 it").arg(key.arg("log"));
        break;
    case Rule::ActionSend:
        then = QString("%1 %2").arg(key.arg("send to"), pill.arg(route.m_target.toHtmlEscaped()));
        break;
    case Rule::ActionPage:
        then = (route.m_pageTo == Rule::PageContact) ? QString("%1 %2").arg(key.arg("page"), pill.arg(route.m_target.toHtmlEscaped()))
            : (route.m_pageTo == Rule::PageMessageContact) ? QString("%1 its contact").arg(key.arg("page"))
            : QString("%1 it (queue it for its transmitter)").arg(key.arg("page"));
        break;
    case Rule::ActionMark:
    {
        QString mark;
        const QString icon = route.m_mark.value("icon").toString();

        if (!icon.isEmpty()) {
            mark += QString("<img src=\"%1\" width=\"14\" height=\"14\"> ").arg(MessageCenterContact::iconPath(icon).toHtmlEscaped());
        }
        for (const char *colour : {"color", "background"})
        {
            if (route.m_mark.value(colour).toUInt() != 0) {
                mark += QString("<span style=\"color:%1\">&#9632;</span> ").arg(QColor::fromRgba(route.m_mark.value(colour).toUInt()).name());
            }
        }

        then = QString("%1 it %2").arg(key.arg("mark"), mark);
        break;
    }
    case Rule::ActionDecode:
    {
        QStringList as;

        if (!route.m_decodeCharset.empty()) {
            as.append(RadioPagingPage::charsetText(route.m_decodeCharset));
        }
        if (!route.m_decodeType.isEmpty()) {
            as.append(route.m_decodeType);
        }

        then = QString("%1 %2").arg(key.arg("decode as"), pill.arg(as.join(" ").toHtmlEscaped()));
        break;
    }
    case Rule::ActionTransform:
        then = QString("%1 the text /%2/ into %3").arg(key.arg("transform"), pill.arg(route.m_pattern.toHtmlEscaped()),
            pill.arg(route.m_template.toHtmlEscaped()));
        break;
    case Rule::ActionReplace:
        then = QString("%1 /%2/ by %3%4").arg(key.arg("replace"), pill.arg(route.m_pattern.toHtmlEscaped()),
            pill.arg(route.m_template.toHtmlEscaped()), route.m_all ? QString() : QString(" (the first one)"));
        break;
    case Rule::ActionDrop:
        then = QString("%1 it: no rule after this one sees it").arg(key.arg("drop"));
        break;
    default:
        break;
    }

    QStringList lines;
    lines.append(QString("<b>%1</b>: %2 <span style=\"color:#888888\">(finds %3)</span>")
        .arg(route.m_name.toHtmlEscaped(), when).arg(finds));
    lines.append(then);
    const QString html = lines.join("<br>");
    return route.m_enabled ? html : QString("<span style=\"color:#777777\">(off) %1</span>").arg(html);
}

namespace {

// What a sink or a source works on, for its row
QString fileTarget(const MessageCenterFileSetting& f)
{
    const QString type = f.typeOrDefault();

    if (type == "udp") {
        return QString("%1:%2").arg(f.m_address.isEmpty() ? QString("127.0.0.1") : f.m_address).arg(f.m_port);
    }
    if ((type == "command") || (type == "speech")) {
        return f.m_text;
    }
    if (type == "map") {
        return "the position in the message";
    }

    return f.m_path;
}

QString fileFormat(const MessageCenterFileSetting& f)
{
    const QString type = f.typeOrDefault();

    if (type == "file") {
        return f.m_format == 0 ? "JSON lines" : f.m_format == 2 ? "Pager CSV" : "CSV";
    }
    if (type == "udp") {
        return f.m_format == 0 ? "Pager" : "JSON";
    }
    if (type == "csv") {
        return f.m_format == 1 ? "new rows only" : "all rows";
    }

    return QString();
}

} // namespace

// The built in sinks (log, queue) and sources (channels, api) come first: they are always there
void MessageCenterGUI::refreshFiles()
{
    const QMap<QString, MessageCenterEngine::SinkStat>& stats = m_center->engine().sinkStats();
    auto routesTo = [this](const QString& sink) {
        QStringList routes;

        // The rules that deliver to it: the log, the page, or a send to it
        for (int r = 0; r < (int) m_settings.m_routes.size(); r++)
        {
            const MessageCenterEngine::Route& rule = m_settings.m_routes[r];
            const bool to = (sink == "log") ? (rule.m_action == MessageCenterEngine::Route::ActionLog)
                : (sink == "queue") ? (rule.m_action == MessageCenterEngine::Route::ActionPage)
                : ((rule.m_action == MessageCenterEngine::Route::ActionSend) && (rule.m_target == sink));

            if (to) {
                routes.append(QString("%1 %2").arg(r + 1).arg(rule.m_name));
            }
        }

        return routes;
    };
    const int rulesColumn = 8;
    auto setSinkRow = [&](int row, const QStringList& cells, const QStringList& routes, bool builtIn) {
        setRow(m_sinksTable, row, cells + QStringList(routes.isEmpty() ? QString("none: takes nothing") : routes.join(", ")));
        m_sinksTable->item(row, rulesColumn)->setForeground(routes.isEmpty() ? QBrush(QColor(255, 120, 80)) : QBrush());

        for (int c = 0; c < m_sinksTable->columnCount(); c++) {
            m_sinksTable->item(row, c)->setForeground(builtIn && (c != rulesColumn) ? QBrush(QColor(150, 150, 150)) : m_sinksTable->item(row, c)->foreground());
        }
    };
    // The limit: N/s, N/min, N/h and what is done over it; the failed with the dropped among them
    auto limitText = [](const MessageCenterFileSetting& f) {
        if (f.m_rateCount <= 0) {
            return QString();
        }

        const QString per = (f.m_ratePeriod >= 3600) ? "h" : (f.m_ratePeriod >= 60) ? "min" : "s";
        return QString("%1/%2 %3").arg(f.m_rateCount).arg(per, f.m_rateHold ? "hold" : "drop");
    };
    auto failedText = [](const MessageCenterEngine::SinkStat& stat) {
        return (stat.m_dropped > 0) ? QString("%1 (%2 dropped)").arg(stat.m_failed).arg(stat.m_dropped) : QString::number(stat.m_failed);
    };

    m_sinksTable->setRowCount(m_builtInSinks + m_settings.m_sinks.size());
    MessageCenterEngine::SinkStat logStat = stats.value("log");
    MessageCenterEngine::SinkStat queueStat = stats.value("queue");
    setSinkRow(0, {"log", "log", "built in: Messages and the history (the rules that log)", "", "", QString::number(logStat.m_accepted),
        QString::number(logStat.m_failed), ""}, routesTo("log"), true);
    setSinkRow(1, {"queue", "queue", "built in: the pages to transmit (the rules that page)", "", "", QString::number(queueStat.m_accepted),
        QString::number(queueStat.m_failed), ""}, routesTo("queue"), true);

    for (int i = 0; i < m_settings.m_sinks.size(); i++)
    {
        const MessageCenterFileSetting& f = m_settings.m_sinks[i];
        MessageCenterEngine::SinkStat stat = stats.value(f.m_name);
        int row = m_builtInSinks + i;
        setSinkRow(row, {f.m_name, f.typeOrDefault(), fileTarget(f), fileFormat(f), limitText(f),
            QString::number(stat.m_accepted), failedText(stat), (stat.m_waiting > 0) ? QString::number(stat.m_waiting) : QString()},
            routesTo(f.m_name), false);
    }

    m_sourcesTable->setRowCount(m_builtInSources + m_settings.m_sources.size());
    setRow(m_sourcesTable, 0, {"channels", "channels", "built in: the Radio Paging demodulators and modulators"});
    setRow(m_sourcesTable, 1, {"api", "api", "built in: the ingest action of the WebAPI and of the MCP server"});

    for (int r = 0; r < m_builtInSources; r++)
    {
        for (int c = 0; c < m_sourcesTable->columnCount(); c++) {
            m_sourcesTable->item(r, c)->setForeground(QBrush(QColor(150, 150, 150)));
        }
    }

    for (int i = 0; i < m_settings.m_sources.size(); i++)
    {
        const MessageCenterFileSetting& f = m_settings.m_sources[i];
        setRow(m_sourcesTable, m_builtInSources + i, {f.m_name, f.typeOrDefault(),
            fileFormat(f).isEmpty() ? fileTarget(f) : QString("%1 (%2)").arg(fileTarget(f), fileFormat(f))});
    }
}

QString MessageCenterGUI::selectedId(QTableWidget *table) const
{
    int row = table->currentRow();
    return ((row >= 0) && table->item(row, 0)) ? table->item(row, 0)->data(idRole).toString() : QString();
}

void MessageCenterGUI::transmitterChanged()
{
    QString alias = m_composer->transmitter();
    QStringList protocols;

    if (m_fillingContact) {
        return;
    }

    // To a contact: the protocols its send to list sends with (any for one that sets none)
    if (m_toContact && (m_toContact->currentIndex() > 0))
    {
        for (const MessageCenterContact& c : m_settings.m_contacts)
        {
            if (c.m_name != m_toContact->currentData().toString()) {
                continue;
            }

            for (const MessageCenterContact::SendTo& s : c.m_sendTo)
            {
                const QString p = s.m_page.value("protocol").toString().toLower();

                if (p.isEmpty()) {
                    protocols = RadioPagingPage::protocols();
                } else if (!protocols.contains(p)) {
                    protocols.append(p);
                }
            }
        }

        m_composer->setProtocols(protocols.isEmpty() ? RadioPagingPage::protocols() : protocols);
        applyCapabilities();
        return;
    }

    for (const MessageEndpoint& e : m_center->engine().endpoints())
    {
        if ((e.m_role == MessageEndpoint::Transmitter) && (e.m_channel == alias)) {
            protocols = e.m_protocols;
        }
    }

    // A transmitter that did not say what it sends: any
    m_composer->setProtocols(protocols.isEmpty() ? RadioPagingPage::protocols() : protocols);
    applyCapabilities();
}

void MessageCenterGUI::post()
{
    RadioPagingPage page = m_composer->page();
    QString error;

    // To a contact: the transmitter and the address of its send to
    if (m_toContact->currentIndex() > 0)
    {
        if (!contactPage(page, error))
        {
            showSendError(error);
            return;
        }
    }
    else if (m_composer->transmitter().isEmpty())
    {
        showSendError("no transmitter has announced itself");
        return;
    }

    error = page.check();

    if (!error.isEmpty())
    {
        showSendError(error);
        return;
    }

    // Its id given here: the line below the form follows it
    if (page.m_id.isEmpty()) {
        page.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    m_lastPostId = page.m_id;
    m_lastPostTime = QDateTime::currentDateTime();
    m_lastPostTo = (m_toContact->currentIndex() > 0) ? m_toContact->currentData().toString()
        : QString("%1 %2").arg(page.m_protocol.toUpper(), page.m_address);
    m_lastPostTransmitter = page.m_transmitter;
    m_sendResult->setText(QString("Posted %1 to %2").arg(m_lastPostTime.toString("HH:mm:ss"), m_lastPostTo));
    m_sendResult->setStyleSheet(QString());
    m_center->getInputMessageQueue()->push(MessageCenter::MsgPostPage::create(page.toRecord()));
}

void MessageCenterGUI::pageAction(int action)
{
    m_center->getInputMessageQueue()->push(MessageCenter::MsgPageAction::create(selectedId(m_queueTable),
        (MessageCenter::MsgPageAction::Action) action));
}

// A route: a name, a filter, the sinks it sends to
void MessageCenterGUI::editRoute(int index, const MessageCenterEngine::Route *preset)
{
    bool isNew = (index < 0);
    MessageCenterEngine::Route route;

    if (!isNew) {
        route = m_settings.m_routes[index];
    } else if (preset) {
        route = *preset;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(isNew ? "New rule" : "Rule");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLineEdit *name = new QLineEdit(route.m_name, &dialog);
    name->setPlaceholderText("name");
    MessageCenterFilterWidget *filter = new MessageCenterFilterWidget(&dialog);
    filter->setContacts(m_settings.m_contacts);
    QStringList sources;

    for (const MessageCenterFileSetting& f : m_settings.m_sources) {
        sources.append(f.m_name);
    }

    filter->showSources(sources);
    filter->setFilter(route.m_filter);
    connect(filter, &MessageCenterFilterWidget::fieldsWanted, &dialog, [this, filter]() { filter->setFields(fieldInfos()); });
    QFormLayout *nameForm = new QFormLayout();
    nameForm->addRow("Name", name);
    layout->addLayout(nameForm);
    QLabel *when = new QLabel("<b>When</b> a message is (the filters; none: any message)", &dialog);
    when->setToolTip("The messages the rule is about: every filter set must match");
    layout->addWidget(when);
    layout->addWidget(filter);
    // What the filter finds in the log, as it is changed
    QLabel *finds = new QLabel(&dialog);
    finds->setStyleSheet("QLabel { color: #888888; }");
    finds->setToolTip("The messages of the log the filter selects now");
    auto count = [this, filter, finds]() {
        finds->setText(QString("Finds %1 messages of the log").arg(m_center->engine().select(filter->filter()).size()));
    };
    count();
    connect(filter, &MessageCenterFilterWidget::changed, &dialog, count);
    QPushButton *showFound = new QPushButton("Show...", &dialog);
    showFound->setToolTip("The messages of the log the filter selects now, in a window: newest first, 100 a page");
    connect(showFound, &QPushButton::clicked, &dialog, [this, filter, name, &dialog]() {
        showMatches(filter->filter(), name->text().trimmed(), &dialog);
    });
    QHBoxLayout *findsLine = new QHBoxLayout();
    findsLine->addWidget(finds);
    findsLine->addWidget(showFound);
    findsLine->addStretch(1);
    layout->addLayout(findsLine);

    // The action of the rule, and below it the fields of that action only
    typedef MessageCenterEngine::Route Rule;
    static const QList<QPair<Rule::Action, const char*>> actions = {
        {Rule::ActionLog, "Log: keep it in Messages and the history"},
        {Rule::ActionSend, "Send: to a destination of Connections"},
        {Rule::ActionPage, "Page: queue it for a transmitter"},
        {Rule::ActionMark, "Mark: an icon and colours in the log"},
        {Rule::ActionDecode, "Decode as: another codepage or type"},
        {Rule::ActionTransform, "Transform: a new text from a template"},
        {Rule::ActionReplace, "Replace: in the text, as sed does"},
        {Rule::ActionDrop, "Drop: no rule after this one sees it"}
    };
    QComboBox *action = new QComboBox(&dialog);

    for (const auto& a : actions) {
        action->addItem(a.second, (int) a.first);
    }

    action->setCurrentIndex(std::max(0, action->findData((int) route.m_action)));
    action->setToolTip("What the rule does with the messages it selects. The changes (mark, decode, transform, replace) are\n"
        "for the rules after it; the log, the sends and the pages are made at the end, with the message as the changes left it");
    QFormLayout *actionForm = new QFormLayout();
    actionForm->addRow(new QLabel("<b>Then</b>", &dialog), action);
    layout->addLayout(actionForm);
    QStackedWidget *pages = new QStackedWidget(&dialog);
    auto note = [&dialog](const QString& text) {
        QLabel *label = new QLabel(text, &dialog);
        label->setWordWrap(true);
        label->setStyleSheet("QLabel { color: #aaaaaa; }");
        return label;
    };
    auto form = [&dialog]() {
        QWidget *page = new QWidget(&dialog);
        QFormLayout *f = new QFormLayout(page);
        f->setContentsMargins(0, 0, 0, 0);
        return std::make_pair(page, f);
    };

    // Log, page, drop: what they do
    QWidget *logPage = note("Every message the rule selects is kept in Messages and in the history (when it is kept).");
    // Page: to the transmitter the message names, to a contact (a relay: the message paged by the send to of the
    // contact, e.g. POCSAG heard, paged as FLEX), or to the contact of the message
    auto pageForm = form();
    QWidget *pagePage = pageForm.first;
    QComboBox *pageTo = new QComboBox(pagePage);
    pageTo->addItem("The transmitter of the message", QStringList({"transmitter"}));
    pageTo->addItem("The contact of the message", QStringList({"messageContact"}));

    for (const MessageCenterContact& contact : m_settings.m_contacts) {
        pageTo->addItem(contact.m_name, QStringList({"contact", contact.m_name}));
    }

    const QStringList pageToNow = (route.m_pageTo == Rule::PageContact) ? QStringList({"contact", route.m_target})
        : QStringList({Rule::pageToName(route.m_pageTo)});
    pageTo->setCurrentIndex(std::max(0, pageTo->findData(pageToNow)));
    pageTo->setToolTip("Who the page goes to. A contact: the message is paged by the send to of the contact (its transmitter,\n"
        "protocol and address), e.g. a POCSAG message heard paged again as FLEX");
    pageForm.second->addRow("To", pageTo);
    QLabel *pageNote = qobject_cast<QLabel*>(note(QString()));
    pageForm.second->addRow(pageNote);
    auto showPageTo = [pageTo, pageNote]() {
        const QString to = pageTo->currentData().toStringList().value(0);
        pageNote->setText((to == "transmitter")
            ? "A message for a transmitter (sent through the API, or by a rule) becomes a page in the queue of the Send tab; "
              "the others are left alone."
            : (to == "messageContact")
            ? "The message is paged to its contact, by the send to of the contact; a message without a contact is not paged."
            : "The message is paged to this contact, by its send to (its transmitter, protocol and address). Select received "
              "messages only, or the page heard back is paged again.");
    };
    showPageTo();
    connect(pageTo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, showPageTo);
    QWidget *dropPage = note("The rules after this one do not see the messages it selects. What the rules before it did stays "
        "(their log, sends and pages are made).");

    // Send: a destination of Connections
    auto sendForm = form();
    QComboBox *target = new QComboBox(sendForm.first);

    for (const MessageCenterFileSetting& f : m_settings.m_sinks) {
        target->addItem(QString("%1 (%2)").arg(f.m_name, f.typeOrDefault()), f.m_name);
    }

    // The last item opens the editor of a new destination, selected when it is saved
    const QString newTarget = "\n new";
    target->addItem("New destination...", newTarget);
    target->setCurrentIndex(std::max(0, target->findData(route.m_target)));

    if (target->currentData() == newTarget) {
        target->setCurrentIndex(-1);    // No destination yet: none chosen
    }

    target->setToolTip("A file, UDP, a command, speech or the Map, as in the Connections tab; New destination... adds one");
    sendForm.second->addRow("To", target);
    std::shared_ptr<int> lastTarget = std::make_shared<int>(target->currentData() == newTarget ? -1 : target->currentIndex());
    connect(target, QOverload<int>::of(&QComboBox::activated), &dialog, [this, target, newTarget, lastTarget](int index) {
        if (target->itemData(index) != newTarget)
        {
            *lastTarget = index;
            return;
        }

        const QString name = editFile(true, -1, true);

        if (name.isEmpty())
        {
            target->setCurrentIndex(*lastTarget);   // -1: none chosen yet
            return;
        }

        const MessageCenterFileSetting& added = m_settings.m_sinks.back();
        target->insertItem(index, QString("%1 (%2)").arg(name, added.typeOrDefault()), name);
        target->setCurrentIndex(index);
        *lastTarget = index;
    });

    // Mark: an icon, the colour of the text, of the row
    auto markForm = form();
    QWidget *markRow = new QWidget(markForm.first);
    QHBoxLayout *markLayout = new QHBoxLayout(markRow);
    markLayout->setContentsMargins(0, 0, 0, 0);
    QToolButton *markIcon = new QToolButton(markRow);
    QToolButton *markText = new QToolButton(markRow);
    QToolButton *markBackground = new QToolButton(markRow);
    markIcon->setToolTip("The icon before the message: click to choose, right click for none");
    markText->setToolTip("The colour of the text of the message: click to choose, right click for none");
    markBackground->setToolTip("The colour of the row of the message: click to choose, right click for none");
    markLayout->addWidget(markIcon);
    markLayout->addWidget(markText);
    markLayout->addWidget(markBackground);
    markLayout->addStretch();
    markForm.second->addRow("Mark", markRow);
    QString iconValue = route.m_mark.value("icon").toString();
    quint32 textValue = route.m_mark.value("color").toUInt();
    quint32 backgroundValue = route.m_mark.value("background").toUInt();
    auto showMark = [&]() {
        QIcon icon = MessageCenterContactIcons::icon(iconValue);
        markIcon->setIcon(icon);
        markIcon->setText(icon.isNull() ? QString("Icon") : QString());
        markIcon->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly);

        for (auto pair : {qMakePair(markText, textValue), qMakePair(markBackground, backgroundValue)})
        {
            QToolButton *button = pair.first;
            const bool none = (pair.second == 0);
            button->setText(none ? QString(button == markText ? "Text colour" : "Row colour") : QString());
            button->setStyleSheet(none ? QString()
                : QString("QToolButton { background-color: %1; min-width: 2em; }").arg(QColor::fromRgba(pair.second).name()));
        }
    };
    showMark();

    for (QToolButton *button : {markIcon, markText, markBackground}) {
        button->setContextMenuPolicy(Qt::CustomContextMenu);
    }

    connect(markIcon, &QToolButton::clicked, &dialog, [&]() {
        MessageCenterIconPicker picker(iconValue, &dialog);

        if (picker.exec() == QDialog::Accepted)
        {
            iconValue = picker.icon();
            showMark();
        }
    });
    connect(markIcon, &QWidget::customContextMenuRequested, &dialog, [&]() { iconValue.clear(); showMark(); });
    const QPair<QToolButton*, quint32*> markColours[] = {qMakePair(markText, &textValue), qMakePair(markBackground, &backgroundValue)};

    for (const auto& pair : markColours)
    {
        QToolButton *button = pair.first;
        quint32 *value = pair.second;
        connect(button, &QToolButton::clicked, &dialog, [&, value, button]() {
            QColor chosen = QColorDialog::getColor(*value ? QColor::fromRgba(*value) : QColor(Qt::white), &dialog,
                button == markText ? QString("Colour of the text") : QString("Colour of the row"));

            if (chosen.isValid())
            {
                *value = chosen.rgba();
                showMark();
            }
        });
        connect(button, &QWidget::customContextMenuRequested, &dialog, [&, value]() { *value = 0; showMark(); });
    }

    // Transform and replace: an expression, a template, the case
    auto textForm = form();
    QLineEdit *pattern = new QLineEdit(route.m_pattern, textForm.first);
    pattern->setPlaceholderText("regular expression, e.g. Unit:(?<unit>\\w+) Job #(?<job>\\S+)");
    QLineEdit *tmpl = new QLineEdit(route.m_template, textForm.first);
    QCheckBox *caseInsensitive = new QCheckBox("Not case sensitive", textForm.first);
    caseInsensitive->setChecked(route.m_caseInsensitive);
    QCheckBox *all = new QCheckBox("Every match (else the first one only)", textForm.first);
    all->setChecked(route.m_all);
    QLabel *textHelp = note(QString());
    textForm.second->addRow("Find", pattern);
    textForm.second->addRow("Into", tmpl);
    textForm.second->addRow(QString(), caseInsensitive);
    textForm.second->addRow(QString(), all);
    textForm.second->addRow(textHelp);

    // Decode as: the codepages of every protocol, once each; a message of a protocol without it is left as it is
    auto decodePage = form();
    QFormLayout *decodeForm = decodePage.second;
    QComboBox *decodeCharset = new QComboBox(&dialog);
    decodeCharset->setToolTip("The messages the rule selects are decoded again with this codepage, for the rules after it.\n"
        "A message of a protocol that does not have it is left as it was received");
    decodeCharset->addItem("As received");
    QList<RadioPagingCharset::Key> keys;
    keys.append(RadioPagingCharset::Key());

    for (const char *protocol : {"pocsag", "flex", "gsc"})
    {
        for (const RadioPagingCharset::Codepage *codepage : RadioPagingCharset::codepages(protocol))
        {
            const RadioPagingCharset::Key key = codepage->key();

            if (!keys.contains(key))
            {
                keys.append(key);
                decodeCharset->addItem(RadioPagingPage::charsetText(key));
            }
        }
    }

    decodeCharset->setCurrentIndex(std::max(0, (int) keys.indexOf(route.m_decodeCharset)));
    QComboBox *decodeType = new QComboBox(&dialog);
    decodeType->addItem("As received", QString());
    decodeType->addItem("Alphanumeric", QString("alphanumeric"));
    decodeType->addItem("Numeric", QString("numeric"));
    decodeType->setCurrentIndex(std::max(0, decodeType->findData(route.m_decodeType)));

    // FLEX sends the type of a message with it: its messages are not read as another type
    auto showDecodeType = [filter, decodeType]() {
        const bool flex = filter->filter().m_protocols == QStringList("flex");

        if (flex) {
            decodeType->setCurrentIndex(0);
        }

        decodeType->setEnabled(!flex);
        decodeType->setToolTip(flex ? QString("FLEX sends the type of a message with it: its messages keep their type")
            : QString("The type POCSAG and GSC messages are decoded as. FLEX messages keep their type (it is sent with them)"));
    };

    showDecodeType();
    connect(filter, &MessageCenterFilterWidget::changed, &dialog, showDecodeType);
    decodeForm->addRow("Codepage", decodeCharset);
    decodeForm->addRow("Type", decodeType);

    // The page of each action; transform and replace share theirs
    const QMap<int, QWidget*> pageOf = {
        {Rule::ActionLog, logPage}, {Rule::ActionSend, sendForm.first}, {Rule::ActionPage, pagePage},
        {Rule::ActionMark, markForm.first}, {Rule::ActionDecode, decodePage.first}, {Rule::ActionTransform, textForm.first},
        {Rule::ActionReplace, textForm.first}, {Rule::ActionDrop, dropPage}
    };

    for (QWidget *page : {logPage, sendForm.first, pagePage, markForm.first, decodePage.first, textForm.first, dropPage}) {
        pages->addWidget(page);
    }

    auto showAction = [&]() {
        const int a = action->currentData().toInt();
        pages->setCurrentWidget(pageOf.value(a, logPage));

        // As high as the page of the action, not the highest one
        for (int i = 0; i < pages->count(); i++) {
            pages->widget(i)->setSizePolicy(QSizePolicy::Preferred, (i == pages->currentIndex()) ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        }

        pages->updateGeometry();

        // Higher when the page needs it (the filters wrap: their height is that of the width), never lower
        QTimer::singleShot(0, &dialog, [&dialog]() {
            const int needed = dialog.layout()->hasHeightForWidth() ? dialog.layout()->totalHeightForWidth(dialog.width())
                : dialog.layout()->totalSizeHint().height();

            if (needed > dialog.height()) {
                dialog.resize(dialog.width(), needed);
            }
        });
        const bool replace = (a == Rule::ActionReplace);
        all->setVisible(replace);
        textHelp->setText(replace
            ? QString("Each match of Find in the text (or the first one) is replaced by Into: \\1 or ${1} for a group, "
                "${name} for a named group, ${address}, ${channel}, ${time}... for a field of the message")
            : QString("When Find is found in the text, the text becomes Into: ${1} or ${name} for its groups, ${address}, "
                "${channel}, ${time}, ${text}... for a field of the message. The text before is kept (original_text)"));
    };
    showAction();
    connect(action, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, showAction);
    layout->addWidget(pages);
    QCheckBox *enabled = new QCheckBox("Enabled", &dialog);
    enabled->setChecked(route.m_enabled);
    QFormLayout *positionForm = new QFormLayout();

    // A new rule: where it goes, before rule 1 by default
    QComboBox *position = nullptr;

    if (isNew)
    {
        position = new QComboBox(&dialog);
        const std::vector<MessageCenterEngine::Route>& routes = m_settings.m_routes;

        for (int i = 0; i < (int) routes.size(); i++) {
            position->addItem(QString("Before %1 %2").arg(i + 1).arg(routes[i].m_name), i);
        }

        position->addItem(routes.empty() ? QString("First") : QString("After %1 %2").arg(routes.size()).arg(routes.back().m_name),
            (int) routes.size());
        position->setCurrentIndex(0);
        positionForm->addRow("Insert", position);
    }

    QLabel *error = new QLabel(&dialog);
    error->setStyleSheet("QLabel { color: #ff7777; }");
    error->setWordWrap(true);
    QPushButton *ok = new QPushButton("OK", &dialog);
    QPushButton *cancel = new QPushButton("Cancel", &dialog);
    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    layout->addWidget(enabled);
    layout->addLayout(positionForm);
    layout->addWidget(error);
    layout->addLayout(buttons);

    // The rule of the dialog: its action with what that action takes, the rest not kept
    auto ruleOfDialog = [&]() {
        Rule r = route;
        r.m_name = name->text().trimmed().isEmpty() ? QString("rule %1").arg(m_settings.m_routes.size() + 1) : name->text().trimmed();
        r.m_filter = filter->filter();
        r.m_enabled = enabled->isChecked();
        r.m_action = (Rule::Action) action->currentData().toInt();
        // "New destination..." is never a target: none chosen
        r.m_target = ((r.m_action == Rule::ActionSend) && (target->currentData() != newTarget)) ? target->currentData().toString() : QString();
        r.m_pageTo = Rule::PageTransmitter;

        if (r.m_action == Rule::ActionPage)
        {
            const QStringList to = pageTo->currentData().toStringList();
            r.m_pageTo = Rule::pageToOf(to.value(0));
            r.m_target = (r.m_pageTo == Rule::PageContact) ? to.value(1) : QString();
        }
        r.m_mark.clear();

        if (r.m_action == Rule::ActionMark)
        {
            if (!iconValue.isEmpty()) {
                r.m_mark.insert("icon", iconValue);
            }
            if (textValue != 0) {
                r.m_mark.insert("color", textValue);
            }
            if (backgroundValue != 0) {
                r.m_mark.insert("background", backgroundValue);
            }
        }

        const bool decode = (r.m_action == Rule::ActionDecode);
        r.m_decodeCharset = decode ? keys.value(decodeCharset->currentIndex()) : RadioPagingCharset::Key();
        r.m_decodeType = decode ? decodeType->currentData().toString() : QString();
        const bool text = (r.m_action == Rule::ActionTransform) || (r.m_action == Rule::ActionReplace);
        r.m_pattern = text ? pattern->text() : QString();
        r.m_template = text ? tmpl->text() : QString();
        r.m_caseInsensitive = text && caseInsensitive->isChecked();
        r.m_all = all->isChecked();
        return r;
    };

    // A rule that cannot work says why, the dialog stays
    connect(ok, &QPushButton::clicked, &dialog, [&]() {
        const Rule rule = ruleOfDialog();
        const QString why = rule.check();
        // The dialog is of this rule: its name before the reason is left out ("Rule rule 2: send to where")
        const QString prefix = QString("Rule %1: ").arg(rule.m_name);
        error->setText(why.startsWith(prefix) ? why.mid(prefix.size()) : why);

        if (why.isEmpty()) {
            dialog.accept();
        }
    });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    route = ruleOfDialog();

    int at = index;

    if (isNew)
    {
        at = std::min(std::max(0, position->currentData().toInt()), (int) m_settings.m_routes.size());
        m_settings.m_routes.insert(m_settings.m_routes.begin() + at, route);
    }
    else
    {
        m_settings.m_routes[index] = route;
    }

    refreshRoutes();
    m_routesTable->selectRow(at);
    applySettings(QStringList("routes"));
}

void MessageCenterGUI::addRoute()
{
    editRoute(-1);
}

void MessageCenterGUI::createRouteFromRecord(const QString& recordJson, const QVariantMap& charset, const QString& type)
{
    MessageRecord record;

    if (!MessageRecord::fromJson(QJsonDocument::fromJson(recordJson.toUtf8()).object(), record)) {
        return;
    }

    // The messages of the address: decoded so when a codepage or a type was chosen (All codepages), else marked
    MessageCenterEngine::Route route;
    route.m_decodeCharset = RadioPagingPage::charsetKey(charset);
    route.m_decodeType = type;
    route.m_action = (!route.m_decodeCharset.empty() || !type.isEmpty()) ? MessageCenterEngine::Route::ActionDecode
        : MessageCenterEngine::Route::ActionMark;
    route.m_filter.m_protocols = QStringList(record.m_protocol.toLower());
    route.m_filter.m_polarity = (record.m_polarity == MessageRecord::PolarityUnknown) ? -1 : (int) record.m_polarity;
    route.m_filter.m_msgType = record.m_msgType;
    route.m_filter.m_address = QRegularExpression::escape(record.m_address);
    route.m_name = QString("%1 %2").arg(record.m_protocol.toUpper(), record.m_address);

    if (!route.m_decodeCharset.empty())
    {
        const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::find(record.m_protocol.toStdString(), route.m_decodeCharset);
        route.m_name += " " + (codepage ? QString::fromUtf8(codepage->m_label) : RadioPagingPage::charsetText(route.m_decodeCharset));
    }
    if (!type.isEmpty()) {
        route.m_name += " " + type;
    }

    show();
    raise();
    activateWindow();
    editRoute(-1, &route);
}

void MessageCenterGUI::editSelectedRoute()
{
    int row = m_routesTable->currentRow();

    if ((row >= 0) && (row < (int) m_settings.m_routes.size())) {
        editRoute(row);
    }
}

void MessageCenterGUI::removeRoute()
{
    int row = m_routesTable->currentRow();

    if ((row >= 0) && (row < (int) m_settings.m_routes.size()))
    {
        m_settings.m_routes.erase(m_settings.m_routes.begin() + row);
        refreshRoutes();
        applySettings(QStringList("routes"));
    }
}

// A sink or a source: its type and what it works on
QString MessageCenterGUI::editFile(bool sink, int index, bool forRule)
{
    QList<MessageCenterFileSetting>& list = sink ? m_settings.m_sinks : m_settings.m_sources;
    const bool isNew = (index < 0) || (index >= list.size());
    MessageCenterFileSetting setting = isNew ? MessageCenterFileSetting() : list[index];
    const QString oldName = setting.m_name;

    QDialog dialog(this);
    dialog.setWindowTitle(QString("%1 %2").arg(isNew ? "New" : "Edit").arg(sink ? "sink" : "source"));
    QFormLayout *layout = new QFormLayout(&dialog);
    QLineEdit *name = new QLineEdit(setting.m_name, &dialog);
    QComboBox *type = new QComboBox(&dialog);
    type->addItems(sink ? MessageCenterFileSetting::sinkTypes() : MessageCenterFileSetting::sourceTypes());
    type->setCurrentIndex(std::max(0, type->findText(setting.typeOrDefault())));
    type->setToolTip(sink
        ? "file: written as JSON lines, CSV or the CSV log of the Pager demodulator. udp: a datagram for each message.\n"
          "command: run for each message. speech: said for each message. map: the position in the message, on the Map.\n"
          "A rule sends messages to it (its action Send); a mark is an action of a rule"
        : "file: a JSON lines file, read as it grows. csv: a CSV log of a message center or of the Pager\n"
          "demodulator, read as it grows. udp: datagrams in JSON or in the format of the Pager demodulator");
    QLineEdit *path = new QLineEdit(setting.m_path, &dialog);
    QPushButton *browse = new QPushButton("...", &dialog);
    QWidget *pathRow = new QWidget(&dialog);
    QHBoxLayout *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(path);
    pathLayout->addWidget(browse);
    QComboBox *format = new QComboBox(&dialog);
    QLineEdit *address = new QLineEdit(setting.m_address, &dialog);
    address->setPlaceholderText(sink ? "IP address to send to" : "127.0.0.1");
    address->setToolTip(sink ? "IP address the datagrams are sent to"
        : "IP address to listen on: 127.0.0.1 (the default) for this computer only, 0.0.0.0 for any");
    QSpinBox *port = new QSpinBox(&dialog);
    port->setRange(1, 65535);
    port->setValue(setting.m_port > 0 ? setting.m_port : 9999);
    QLineEdit *text = new QLineEdit(setting.m_text, &dialog);
    text->setToolTip("${address}, ${message}, ${protocol}, ${type}, ${channel}, ${mode}, ${id}, ${frequency}, ${time},\n"
        "${0}, ${1}... (the captures of the text expression of the rule) and ${field} (any field of the message) are replaced;\n"
        "${text:base64}, ${text:json} (a JSON string), ${text:url}: encoded. A command is split into the program and its\n"
        "arguments first (no shell): the message goes into the arguments only, each one as it is");
    layout->addRow("Name", name);
    layout->addRow("Type", type);
    layout->addRow("File", pathRow);
    layout->addRow("Format", format);
    layout->addRow("Address", address);
    layout->addRow("Port", port);
    layout->addRow("Text", text);

    // Command: how the message is given to it besides its arguments
    QWidget *passRow = new QWidget(&dialog);
    QHBoxLayout *passLayout = new QHBoxLayout(passRow);
    passLayout->setContentsMargins(0, 0, 0, 0);
    QCheckBox *passEnv = new QCheckBox("Environment", passRow);
    passEnv->setChecked(setting.m_commandEnv);
    passEnv->setToolTip("The fields of the message in environment variables: MC_ID, MC_TIME, MC_PROTOCOL, MC_ADDRESS,\n"
        "MC_TYPE, MC_TEXT, MC_CHANNEL, MC_FREQUENCY, MC_POLARITY, MC_QUALITY, MC_CONTACT and MC_RECORD (the record as JSON)");
    QCheckBox *passStdin = new QCheckBox("JSON on its input", passRow);
    passStdin->setChecked(setting.m_commandStdin);
    passStdin->setToolTip("The record as one line of JSON on the standard input of the command");
    passLayout->addWidget(passEnv);
    passLayout->addWidget(passStdin);
    passLayout->addStretch();

    // The limit: at most so many in a second, a minute or an hour; over it held or dropped
    QWidget *limitRow = new QWidget(&dialog);
    QHBoxLayout *limitLayout = new QHBoxLayout(limitRow);
    limitLayout->setContentsMargins(0, 0, 0, 0);
    QSpinBox *limitCount = new QSpinBox(limitRow);
    limitCount->setRange(0, 100000);
    limitCount->setSpecialValueText("No limit");
    limitCount->setValue(isNew ? 0 : setting.m_rateCount);
    QComboBox *limitPeriod = new QComboBox(limitRow);
    limitPeriod->addItem("a second", 1);
    limitPeriod->addItem("a minute", 60);
    limitPeriod->addItem("an hour", 3600);
    limitPeriod->setCurrentIndex(std::max(0, limitPeriod->findData(setting.m_ratePeriod)));
    QComboBox *limitMode = new QComboBox(limitRow);
    limitMode->addItem("then hold", true);
    limitMode->addItem("then drop", false);
    limitMode->setCurrentIndex(setting.m_rateHold ? 0 : 1);
    limitMode->setToolTip(QString("Hold: the messages over the limit wait and go as it allows (at most %1 wait, the oldest dropped "
        "over that).\nDrop: they are dropped (and counted)").arg(MessageCenterEngine::m_maxHeld));
    limitLayout->addWidget(limitCount);
    limitLayout->addWidget(limitPeriod);
    limitLayout->addWidget(limitMode);
    limitLayout->addStretch();
    auto showLimit = [limitCount, limitPeriod, limitMode]() {
        limitPeriod->setEnabled(limitCount->value() > 0);
        limitMode->setEnabled(limitCount->value() > 0);
    };
    showLimit();
    connect(limitCount, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, showLimit);

    if (sink)
    {
        layout->addRow("Pass", passRow);
        layout->addRow("Limit", limitRow);
    }

    // A new command: at most 5 runs a second
    if (isNew)
    {
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [type, limitCount]() {
            if ((type->currentText() == "command") && (limitCount->value() == 0)) {
                limitCount->setValue(5);
            }
        });
    }

    // The fields of the type. A row is shown or hidden with its label (QFormLayout::setRowVisible is Qt 6 only)
    auto setRowVisible = [layout](QWidget *field, bool visible) {
        field->setVisible(visible);

        if (QWidget *label = layout->labelForField(field)) {
            label->setVisible(visible);
        }
    };
    auto showFields = [&]() {
        const QString t = type->currentText();
        const bool file = (t == "file") || (t == "csv");
        const bool udp = (t == "udp");
        const bool hasText = (t == "command") || (t == "speech");
        setRowVisible(pathRow, file);
        setRowVisible(format, (sink && (t == "file")) || udp || (!sink && (t == "csv")));
        setRowVisible(address, udp);
        setRowVisible(port, udp);
        setRowVisible(text, hasText);

        if (sink) {
            setRowVisible(passRow, t == "command");
        }
        int current = format->currentIndex();
        format->blockSignals(true);
        format->clear();

        if (!sink && (t == "csv")) {
            format->addItems({"All rows", "New rows only"});
            format->setToolTip("All rows: the rows in the file are read, then those added. New rows only: the rows already\n"
                "there are skipped (a log the Pager demodulator writes to)");
        } else if (udp) {
            format->addItems({"Pager", "JSON"});
            format->setToolTip(sink ? "Pager: the fields of the Pager demodulator, each ended by a NUL. JSON: the record as one line"
                : "The datagrams are read in either format");
        } else {
            format->addItems({"JSON lines", "CSV", "Pager CSV"});
            format->setToolTip("JSON lines: one record a line, nothing lost. CSV: the columns of the record.\n"
                "Pager CSV: the log of the Pager demodulator (its columns, its messages only)");
        }

        format->setCurrentIndex(std::max(0, std::min(format->count() - 1, current)));
        format->blockSignals(false);
        text->setPlaceholderText(t == "command" ? "program argument ${message} ..." : "${message}");
    };

    format->addItems({"JSON lines", "CSV", "Pager CSV"});
    format->setCurrentIndex(std::max(0, std::min(format->count() - 1, setting.m_format)));
    showFields();
    format->setCurrentIndex(std::max(0, std::min(format->count() - 1, setting.m_format)));

    QPushButton *ok = new QPushButton("OK", &dialog);
    QPushButton *cancel = new QPushButton("Cancel", &dialog);
    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    // A sink takes only what rules send to it: the rules that do, and a new rule that sends every message to it
    QCheckBox *newRoute = nullptr;

    if (sink && !forRule)
    {
        QWidget *routesBox = new QWidget(&dialog);
        QVBoxLayout *routesLayout = new QVBoxLayout(routesBox);
        routesLayout->setContentsMargins(0, 0, 0, 0);
        QStringList sending;

        for (int r = 0; r < (int) m_settings.m_routes.size(); r++)
        {
            const MessageCenterEngine::Route& route = m_settings.m_routes[r];

            if (!isNew && (route.m_action == MessageCenterEngine::Route::ActionSend) && (route.m_target == oldName)) {
                sending.append(QString("%1 %2").arg(r + 1).arg(route.m_name));
            }
        }

        QLabel *sendingLabel = new QLabel(sending.isEmpty() ? QString("none: it takes nothing") : sending.join(", "), routesBox);
        sendingLabel->setWordWrap(true);
        routesLayout->addWidget(sendingLabel);
        newRoute = new QCheckBox("New rule: every message to this sink", routesBox);
        newRoute->setToolTip("Adds a rule without a filter that sends every message to this sink.\n"
            "Edit it in Rules to select the messages");
        newRoute->setChecked(m_settings.m_routes.empty() && isNew);
        routesLayout->addWidget(newRoute);
        routesBox->setToolTip("The rules that send messages to this sink. A sink no rule sends to takes nothing");
        layout->addRow("Rules", routesBox);
    }

    layout->addRow(buttons);

    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&]() { showFields(); });
    connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(browse, &QPushButton::clicked, &dialog, [&]() {
        QString file = sink ? QFileDialog::getSaveFileName(&dialog, "File", path->text())
            : QFileDialog::getOpenFileName(&dialog, "File", path->text());

        if (!file.isEmpty()) {
            path->setText(file);
        }
    });

    for (;;)
    {
        if (dialog.exec() != QDialog::Accepted) {
            return QString();
        }

        const QString t = type->currentText();
        setting.m_name = name->text().trimmed();
        setting.m_type = (t == "file") ? QString() : t;
        setting.m_path = ((t == "file") || (t == "csv")) ? path->text().trimmed() : QString();
        setting.m_format = (((t == "file") && sink) || (t == "udp") || (!sink && (t == "csv"))) ? format->currentIndex() : 0;
        setting.m_address = (t == "udp") ? address->text().trimmed() : QString();
        setting.m_port = (t == "udp") ? port->value() : 0;
        setting.m_text = ((t == "command") || (t == "speech")) ? text->text() : QString();
        setting.m_rateCount = sink ? limitCount->value() : 0;
        setting.m_ratePeriod = limitPeriod->currentData().toInt();
        setting.m_rateHold = limitMode->currentData().toBool();
        setting.m_commandEnv = (t == "command") && passEnv->isChecked();
        setting.m_commandStdin = (t == "command") && passStdin->isChecked();

        QString error = setting.check(sink);

        for (int i = 0; error.isEmpty() && (i < list.size()); i++)
        {
            if ((i != index) && (list[i].m_name == setting.m_name)) {
                error = QString("There is a %1 named %2 already").arg(sink ? "sink" : "source").arg(setting.m_name);
            }
        }

        if (error.isEmpty()) {
            break;
        }

        QMessageBox::information(this, sink ? "Sink" : "Source", error);
    }

    if (isNew) {
        list.append(setting);
    } else {
        list[index] = setting;
    }

    QStringList keys(sink ? "sinks" : "sources");

    // A sink renamed keeps its rules (they send to its new name); a new rule that sends every message to it
    if (sink)
    {
        bool routesChanged = false;

        for (MessageCenterEngine::Route& route : m_settings.m_routes)
        {
            if (!isNew && (oldName != setting.m_name) && (route.m_action == MessageCenterEngine::Route::ActionSend)
                && (route.m_target == oldName))
            {
                route.m_target = setting.m_name;
                routesChanged = true;
            }
        }

        if (newRoute && newRoute->isChecked())
        {
            MessageCenterEngine::Route route;
            route.m_name = QString("to %1").arg(setting.m_name);
            route.m_action = MessageCenterEngine::Route::ActionSend;
            route.m_target = setting.m_name;
            m_settings.m_routes.push_back(route);
            routesChanged = true;
        }

        if (routesChanged)
        {
            keys.append("routes");
            refreshRoutes();
        }
    }

    refreshFiles();
    applySettings(keys);
    return setting.m_name;
}

void MessageCenterGUI::addSink()
{
    editFile(true, -1);
}

void MessageCenterGUI::addSource()
{
    editFile(false, -1);
}

void MessageCenterGUI::editSelectedSink()
{
    int row = m_sinksTable->currentRow() - m_builtInSinks;

    if ((row >= 0) && (row < m_settings.m_sinks.size())) {
        editFile(true, row);
    }
}

void MessageCenterGUI::editSelectedSource()
{
    int row = m_sourcesTable->currentRow() - m_builtInSources;

    if ((row >= 0) && (row < m_settings.m_sources.size())) {
        editFile(false, row);
    }
}

// The speech engine takes long to start on some systems: it is made when it is first needed
void MessageCenterGUI::speak(const QString& text)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (!m_speech) {
        m_speech = new QTextToSpeech(this);
    }

    m_speech->say(text);
#else
    m_errors->appendPlainText(QString("%1 Text to speech is not in this build: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(text));
    m_errors->setVisible(true);
#endif
}

void MessageCenterGUI::importFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Import messages", QString(), "Logs (*.jsonl *.json *.csv);;All files (*)");

    if (!path.isEmpty()) {
        m_center->getInputMessageQueue()->push(MessageCenter::MsgImportFile::create(path));
    }
}

void MessageCenterGUI::exportLog()
{
    QString path = QFileDialog::getSaveFileName(this, "Export messages", "messages.csv", "CSV (*.csv)");

    if (path.isEmpty()) {
        return;
    }

    const std::vector<MessageRecord> records = m_center->engine().select(logFilter());
    QFile file(path);
    bool written = file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && (file.write(MessageRecord::csvHeader().join(',').toUtf8() + "\n") >= 0);

    for (size_t i = 0; written && (i < records.size()); i++) {
        written = file.write(records[i].toCSV().toUtf8() + "\n") >= 0;
    }

    if (!written) {
        QMessageBox::warning(this, "Export messages", QString("Cannot write %1").arg(path));
    }
}

void MessageCenterGUI::removeSink()
{
    int row = m_sinksTable->currentRow() - m_builtInSinks;

    if ((row >= 0) && (row < m_settings.m_sinks.size()))
    {
        QString name = m_settings.m_sinks[row].m_name;
        m_settings.m_sinks.removeAt(row);

        // The rules that sent to a sink that is gone go too: they would do nothing
        auto& rules = m_settings.m_routes;
        rules.erase(std::remove_if(rules.begin(), rules.end(), [&name](const MessageCenterEngine::Route& rule) {
            return (rule.m_action == MessageCenterEngine::Route::ActionSend) && (rule.m_target == name);
        }), rules.end());

        refreshFiles();
        refreshRoutes();
        applySettings({"sinks", "routes"});
    }
}

void MessageCenterGUI::removeSource()
{
    int row = m_sourcesTable->currentRow() - m_builtInSources;

    if ((row >= 0) && (row < m_settings.m_sources.size()))
    {
        m_settings.m_sources.removeAt(row);
        refreshFiles();
        applySettings(QStringList("sources"));
    }
}
