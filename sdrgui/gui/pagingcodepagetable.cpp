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

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "pagingcodepagetable.h"

namespace {

const int rows = 16;

// What a cell shows: the glyph; the hex code for a code without one; the names of the space and the controls
QString cellText(const RadioPagingCharset::Codepage& codepage, int code)
{
    const char *glyph = codepage.glyph((unsigned) code);

    if (!glyph) {
        return QString("%1").arg(code, 2, 16, QChar('0')).toUpper();
    }

    const QString text = QString::fromUtf8(glyph);

    if (text == " ") {
        return "SP";
    } else if (text == "\r") {
        return "CR";
    } else if (text == "\n") {
        return "LF";
    }

    return text;
}

} // namespace

PagingCodepageTable::PagingCodepageTable(QWidget *parent) :
    QFrame(parent, Qt::Popup),
    m_codepage(nullptr)
{
    setFrameShape(QFrame::StyledPanel);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(2);
    m_title = new QLabel(this);
    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_title);
    layout->addWidget(m_table);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (m_codepage)
        {
            emit codeSelected(codeText(*m_codepage, column * rows + row));
            hide();
        }
    });
}

QString PagingCodepageTable::codeText(const RadioPagingCharset::Codepage& codepage, int code)
{
    const char *glyph = codepage.glyph((unsigned) code);

    // A text line holds neither CR nor LF
    if (!glyph || (std::string(glyph) == "\r") || (std::string(glyph) == "\n")) {
        return QString("<0x%1>").arg(code, 2, 16, QChar('0')).toUpper().replace("<0X", "<0x");
    }

    return QString::fromUtf8(glyph);
}

void PagingCodepageTable::setCodepage(const RadioPagingCharset::Codepage& codepage)
{
    const int columns = codepage.codes() / rows;
    QStringList columnLabels;
    QStringList rowLabels;

    m_codepage = &codepage;
    m_title->setText(tr("%1: click a character to insert it").arg(QString::fromUtf8(codepage.m_label)));
    m_table->clear();
    m_table->setRowCount(rows);
    m_table->setColumnCount(columns);

    for (int c = 0; c < columns; c++) {
        columnLabels.append(QString("%1x").arg(c, 1, 16).toUpper());
    }
    for (int r = 0; r < rows; r++) {
        rowLabels.append(QString("x%1").arg(r, 1, 16).toUpper());
    }

    m_table->setHorizontalHeaderLabels(columnLabels);
    m_table->setVerticalHeaderLabels(rowLabels);

    for (int c = 0; c < columns; c++)
    {
        for (int r = 0; r < rows; r++)
        {
            const int code = c * rows + r;
            QTableWidgetItem *item = new QTableWidgetItem(cellText(codepage, code));
            const std::vector<std::string> glyphs = codepage.glyphs((unsigned) code);
            QString tip = QString("0x%1").arg(code, 2, 16, QChar('0')).toUpper().replace("0X", "0x");

            if (glyphs.empty()) {
                tip += tr(": no glyph, inserted as <0x%1>").arg(code, 2, 16, QChar('0')).toUpper();
            } else {
                tip += QString(": %1").arg(cellText(codepage, code));
            }
            if (glyphs.size() > 1)
            {
                QStringList aliases;

                for (size_t i = 1; i < glyphs.size(); i++) {
                    aliases.append(QString::fromUtf8(glyphs[i].c_str()));
                }

                tip += tr(", also typed as %1").arg(aliases.join(" "));
            }

            item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(tip);

            if (glyphs.empty()) {
                item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
            }

            m_table->setItem(r, c, item);
        }
    }

    m_table->resizeColumnsToContents();
    m_table->resizeRowsToContents();
    int width = m_table->verticalHeader()->width() + 4;
    int height = m_table->horizontalHeader()->height() + 4;

    for (int c = 0; c < columns; c++) {
        width += m_table->columnWidth(c);
    }
    for (int r = 0; r < rows; r++) {
        height += m_table->rowHeight(r);
    }

    m_table->setFixedSize(width, height);
    adjustSize();
}

void PagingCodepageTable::popup(QWidget *below)
{
    move(below->mapToGlobal(QPoint(0, below->height())));
    show();
}
