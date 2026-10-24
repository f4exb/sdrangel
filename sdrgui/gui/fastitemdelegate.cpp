///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                              //
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

#include <QPainter>

#include "gui/fastitemdelegate.h"

void FastItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Check boxes, and anything else below, are left to the style: they are rare in the
    // tables this is used on, and the point here is the common cell, not completeness
    if (index.data(Qt::CheckStateRole).isValid())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    const QPalette::ColorGroup group = opt.state.testFlag(QStyle::State_Enabled)
        ? (opt.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
        : QPalette::Disabled;

    // Background. The view asks for alternating rows through the option rather than
    // painting them itself, so that has to be honoured here
    if (selected) {
        painter->fillRect(opt.rect, opt.palette.brush(group, QPalette::Highlight));
    } else if (opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(opt.rect, opt.backgroundBrush);
    } else if (opt.features.testFlag(QStyleOptionViewItem::Alternate)) {
        painter->fillRect(opt.rect, opt.palette.brush(group, QPalette::AlternateBase));
    }

    // A small inset, as the style leaves, so text does not touch the cell edge
    QRect content = opt.rect.adjusted(3, 0, -3, 0);
    if (content.width() <= 0) {
        return;
    }

    if (!opt.icon.isNull())
    {
        const QSize size = opt.icon.actualSize(opt.decorationSize);
        const QRect iconRect(content.left(),
                             content.top() + (content.height() - size.height()) / 2,
                             size.width(), size.height());
        opt.icon.paint(painter, iconRect, Qt::AlignLeft | Qt::AlignVCenter,
                       selected ? QIcon::Selected : QIcon::Normal);
        content.setLeft(iconRect.right() + 4);
    }

    if (!opt.text.isEmpty() && (content.width() > 0))
    {
        painter->setPen(opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        int alignment = opt.displayAlignment;
        if ((alignment & Qt::AlignHorizontal_Mask) == 0) {
            alignment |= Qt::AlignLeft;
        }
        if ((alignment & Qt::AlignVertical_Mask) == 0) {
            alignment |= Qt::AlignVCenter;
        }
        painter->setFont(opt.font);
        const QString text = opt.fontMetrics.elidedText(opt.text, opt.textElideMode, content.width());
        painter->drawText(content, alignment, text);
    }
}
