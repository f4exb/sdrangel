///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QPixmap>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QVBoxLayout>

#include "dsemodaprssymboldialog.h"

namespace {

// The APRS feature's symbol set, cut into one file per symbol: table 0 is the primary table ('/')
// and table 1 the alternate ('\\'), indexed by the code character counted from '!'
QString resourcePath(int table, int index)
{
    return QString(":/aprs/aprs/aprs-symbols-24-%1-%2.png").arg(table).arg(index, 2, 10, QChar('0'));
}

QListWidget *symbolTable(int table, QWidget *parent)
{
    QListWidget *list = new QListWidget(parent);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(24, 24));
    list->setGridSize(QSize(32, 32));
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setUniformItemSizes(true);
    list->setSelectionMode(QAbstractItemView::SingleSelection);

    for (int i = 0; i < DSEModAPRSSymbolDialog::NbSymbols; i++)
    {
        char code = (char) ('!' + i);
        QListWidgetItem *item = new QListWidgetItem(list);
        QPixmap pixmap(resourcePath(table, i));

        if (pixmap.isNull()) {
            item->setText(QString(QChar(code)));   // no APRS feature: the character will do
        } else {
            item->setIcon(QIcon(pixmap));
        }

        item->setToolTip(QString("%1%2").arg(table == 0 ? '/' : '\\').arg(QChar(code)));
        item->setData(Qt::UserRole, (int) code);
    }

    return list;
}

}

DSEModAPRSSymbolDialog::DSEModAPRSSymbolDialog(const QString& symbol, QWidget *parent) :
    QDialog(parent),
    m_symbol(symbol)
{
    setWindowTitle("Select APRS symbol");

    m_tables = new QTabWidget(this);
    m_primary = symbolTable(0, m_tables);
    m_alternate = symbolTable(1, m_tables);
    m_tables->addTab(m_primary, "Primary /");
    m_tables->addTab(m_alternate, "Alternate \\");

    m_overlayLabel = new QLabel("Overlay", this);
    m_overlay = new QLineEdit(this);
    m_overlay->setMaxLength(1);
    m_overlay->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Za-z]?"), this));
    m_overlay->setToolTip("A digit or letter shown over an alternate table symbol, "
        "sent in place of the table character. Empty for the plain symbol");

    QHBoxLayout *overlayLayout = new QHBoxLayout();
    overlayLayout->addWidget(m_overlayLabel);
    overlayLayout->addWidget(m_overlay);
    overlayLayout->addStretch();

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    // A station may send no symbol at all, and leave the receiver to choose one from the position
    QPushButton *none = buttons->addButton("None", QDialogButtonBox::ResetRole);
    none->setToolTip("Send no symbol: the receiver chooses one from the position");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_tables);
    layout->addLayout(overlayLayout);
    layout->addWidget(buttons);

    connect(m_tables, &QTabWidget::currentChanged, this, &DSEModAPRSSymbolDialog::tableChanged);
    connect(m_primary, &QListWidget::itemClicked, this, &DSEModAPRSSymbolDialog::symbolClicked);
    connect(m_alternate, &QListWidget::itemClicked, this, &DSEModAPRSSymbolDialog::symbolClicked);
    connect(m_primary, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_alternate, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_overlay, &QLineEdit::textChanged, this, &DSEModAPRSSymbolDialog::overlayChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(none, &QPushButton::clicked, this, [this]() { m_symbol.clear(); accept(); });

    // What is set now, so that the dialog opens on the station's own symbol
    char symbolTableCharacter = symbol.size() >= 1 ? symbol.at(0).toLatin1() : '/';
    char symbolCode = symbol.size() >= 2 ? symbol.at(1).toLatin1() : '-';

    if (symbolTableCharacter != '/')
    {
        m_overlay->setText(symbolTableCharacter == '\\' ? "" : QString(QChar(symbolTableCharacter)));
        m_tables->setCurrentIndex(1);
    }

    select(symbolTableCharacter, symbolCode);
    tableChanged(m_tables->currentIndex());
    resize(12 * 32 + 40, 8 * 32 + 110);   // twelve symbols to a row, eight rows of them
}

QPixmap DSEModAPRSSymbolDialog::symbolPixmap(const QString& symbol)
{
    if (symbol.size() < 2) {
        return QPixmap();
    }

    char symbolTableCharacter = symbol.at(0).toLatin1();
    int index = symbol.at(1).toLatin1() - '!';

    if ((index < 0) || (index >= NbSymbols)) {
        return QPixmap();
    }

    // Anything but the primary table's character is the alternate table, overlay or not
    return QPixmap(resourcePath(symbolTableCharacter == '/' ? 0 : 1, index));
}

QListWidget *DSEModAPRSSymbolDialog::table(int index) const
{
    return index == 0 ? m_primary : m_alternate;
}

void DSEModAPRSSymbolDialog::select(char symbolTable, char symbolCode)
{
    int index = symbolCode - '!';

    if ((index < 0) || (index >= NbSymbols)) {
        return;
    }

    QListWidget *list = table(symbolTable == '/' ? 0 : 1);
    list->setCurrentRow(index);
    list->scrollToItem(list->item(index));
}

void DSEModAPRSSymbolDialog::tableChanged(int index)
{
    // The overlay belongs to the alternate table: the primary one has no room for it
    m_overlayLabel->setVisible(index == 1);
    m_overlay->setVisible(index == 1);

    QListWidgetItem *item = table(index)->currentItem();

    if (item) {
        symbolClicked(item);
    }
}

void DSEModAPRSSymbolDialog::symbolClicked(QListWidgetItem *item)
{
    char symbolCode = (char) item->data(Qt::UserRole).toInt();
    char symbolTable = '/';

    if (m_tables->currentIndex() == 1) {
        // (overlays are digits and upper case letters)
        symbolTable = m_overlay->text().isEmpty() ? '\\' : m_overlay->text().toUpper().at(0).toLatin1();
    }

    m_symbol = QString("%1%2").arg(QChar(symbolTable)).arg(QChar(symbolCode));
}

void DSEModAPRSSymbolDialog::overlayChanged(const QString& text)
{
    (void) text;
    QListWidgetItem *item = m_alternate->currentItem();

    if (item) {
        symbolClicked(item);
    }
}
