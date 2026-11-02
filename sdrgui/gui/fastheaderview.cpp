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

#include <QAbstractItemModel>
#include <QPainter>
#include <QPolygon>

#include "gui/fastheaderview.h"

FastHeaderView::FastHeaderView(Qt::Orientation orientation, QWidget *parent) :
    QHeaderView(orientation, parent)
{
    setSectionsClickable(true);
    setHighlightSections(false);
}

void FastHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid()) {
        return;
    }

    const QPalette &pal = palette();
    painter->fillRect(rect, pal.brush(QPalette::Button));

    // A single separator on the trailing edge, where Fusion draws its outline
    painter->setPen(pal.color(QPalette::Mid));
    if (orientation() == Qt::Horizontal) {
        painter->drawLine(rect.right(), rect.top() + 2, rect.right(), rect.bottom() - 2);
    } else {
        painter->drawLine(rect.left() + 2, rect.bottom(), rect.right() - 2, rect.bottom());
    }
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const QAbstractItemModel *m = model();
    if (!m) {
        return;
    }

    QRect content = rect.adjusted(4, 0, -4, 0);

    // The sort arrow, drawn as a small triangle rather than by the style
    if (isSortIndicatorShown() && (sortIndicatorSection() == logicalIndex)
        && (orientation() == Qt::Horizontal) && (content.width() > 12))
    {
        const int size = 6;
        const int x = content.right() - size;
        const int y = content.center().y() - (size / 4);
        QPolygon arrow;
        if (sortIndicatorOrder() == Qt::AscendingOrder) {
            arrow << QPoint(x, y + (size / 2)) << QPoint(x + size, y + (size / 2)) << QPoint(x + size / 2, y - (size / 2));
        } else {
            arrow << QPoint(x, y - (size / 2)) << QPoint(x + size, y - (size / 2)) << QPoint(x + size / 2, y + (size / 2));
        }
        painter->setBrush(pal.color(QPalette::ButtonText));
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(arrow);
        content.setRight(x - 3);
    }

    if (content.width() <= 0) {
        return;
    }

    const QVariant text = m->headerData(logicalIndex, orientation(), Qt::DisplayRole);
    if (!text.isValid()) {
        return;
    }

    const QVariant alignment = m->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole);
    int flags = alignment.isValid() ? alignment.toInt()
                                    : (int) ((orientation() == Qt::Horizontal ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);

    painter->setPen(pal.color(QPalette::ButtonText));
    painter->setBrush(Qt::NoBrush);
    painter->setFont(font());
    painter->drawText(content, flags,
                      fontMetrics().elidedText(text.toString(), Qt::ElideRight, content.width()));
}
