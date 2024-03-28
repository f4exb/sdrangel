///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#include <QColor>
#include <QColor>
#include <QToolButton>
#include <QDialog>
#include <QTableWidget>

#include "tablecolorchooser.h"
#include "colordialog.h"

static QString rgbToColor(quint32 rgb)
{
    QColor color = QColor::fromRgba(rgb);
    return QString("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

static QString backgroundCSS(quint32 rgb)
{
    // Must specify a border, otherwise we end up with a gradient instead of solid background
    return QString("QToolButton { background-color: rgb(%1); border: none; }").arg(rgbToColor(rgb));
}

static QString noColorCSS()
{
    return "QToolButton { background-color: black; border: none; }";
}

TableColorChooser::TableColorChooser(QTableWidget *table, int row, int col, bool noColor, quint32 color) :
    m_noColor(noColor),
    m_color(color)
{
    m_colorButton = new QToolButton(table);
    m_colorButton->setFixedSize(22, 22);
    if (!m_noColor)
    {
        m_colorButton->setStyleSheet(backgroundCSS(m_color));
    }
    else
    {
        m_colorButton->setStyleSheet(noColorCSS());
        m_colorButton->setText("-");
    }
    table->setCellWidget(row, col, m_colorButton);
    connect(m_colorButton, &QToolButton::clicked, this, &TableColorChooser::on_color_clicked);
}

void TableColorChooser::on_color_clicked()
{
    ColorDialog dialog(QColor::fromRgba(m_color), m_colorButton);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_noColor = dialog.noColorSelected();
        if (!m_noColor)
        {
            m_colorButton->setText("");
            m_color = dialog.selectedColor().rgba();
            m_colorButton->setStyleSheet(backgroundCSS(m_color));
        }
        else
        {
            m_colorButton->setText("-");
            m_colorButton->setStyleSheet(noColorCSS());
        }
    }
}
