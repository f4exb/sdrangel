///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 The Qt Company Ltd.
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include <QtWidgets>

#include "featurelayout.h"

FeatureLayout::FeatureLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FeatureLayout::FeatureLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FeatureLayout::~FeatureLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0))) {
        delete item;
    }
}

void FeatureLayout::addItem(QLayoutItem *item)
{
    itemList.append(item);
}

int FeatureLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0) {
        return m_hSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }
}

int FeatureLayout::verticalSpacing() const
{
    if (m_vSpace >= 0) {
        return m_vSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }
}

bool FeatureLayout::hasHeightForWidth() const
{
    return true;
}

int FeatureLayout::heightForWidth(int width) const
{
    QSize size;
    if (m_orientation == Qt::Horizontal) {
        size = doLayoutHorizontally(QRect(0, 0, width, 0), true);
    } else {
        size = doLayoutVertically(QRect(0, 0, width, 0), true);
    }
    return size.height();
}

int FeatureLayout::count() const
{
    return itemList.size();
}

QLayoutItem *FeatureLayout::itemAt(int index) const
{
    return itemList.value(index);
}

QLayoutItem *FeatureLayout::takeAt(int index)
{
    if (index >= 0 && index < itemList.size()) {
        return itemList.takeAt(index);
    }
    return nullptr;
}

void FeatureLayout::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
}

Qt::Orientations FeatureLayout::expandingDirections() const
{
    return Qt::Horizontal | Qt::Vertical;
}

void FeatureLayout::setGeometry(const QRect &rect)
{
    m_prevGeometry = rect;
    QLayout::setGeometry(rect);
    if (m_orientation == Qt::Horizontal) {
        doLayoutHorizontally(rect, false);
    } else {
        doLayoutVertically(rect, false);
    }
}

// Calculate preferred size
QSize FeatureLayout::sizeHint() const
{
    QSize size;
    if (m_orientation == Qt::Horizontal) {
        size = doLayoutHorizontally(m_prevGeometry, true);
    } else {
        size = doLayoutVertically(m_prevGeometry, true);
    }
    return size;
}

QSize FeatureLayout::minimumSize() const
{
    QSize size;
    if (m_orientation == Qt::Horizontal) {
        size = doLayoutHorizontally(m_prevGeometry, true);
    } else {
        size = doLayoutVertically(m_prevGeometry, true);
    }
    return size;
}

QSize FeatureLayout::doLayoutHorizontally(const QRect &rect, bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineWidth = 0;
    int spaceX = 0;
    int spaceY = 0;

    // Calculate space available for columns of widgets
    int maxWidthForColums = effectiveRect.width();
    if (itemList.size() > 0) {
        maxWidthForColums -= itemList[0]->minimumSize().width();
    }
    int minHeight = 0;

    int i = 0;
    for (QLayoutItem *item : qAsConst(itemList))
    {
        // Splitter is item 0, so skip
        if (i != 0)
        {
            const QWidget *wid = item->widget();
            spaceX = horizontalSpacing();
            if (spaceX == -1) {
                spaceX = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
            }
            spaceY = verticalSpacing();
            if (spaceY == -1) {
                spaceY = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
            }

            // Layout in vertical columns
            int nextY = y + item->sizeHint().height() + spaceY;
            int nextX = x + lineWidth + spaceX + item->sizeHint().width();
            if (nextY - spaceY > effectiveRect.bottom() && lineWidth > 0 && nextX < maxWidthForColums)
            {
                minHeight = qMax(minHeight, y);
                y = effectiveRect.y();
                x = x + lineWidth + spaceX;
                nextY = y + item->sizeHint().height() + spaceY;
                lineWidth = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
            }

            y = nextY;
            lineWidth = qMax(lineWidth, item->sizeHint().width());
            minHeight = qMax(minHeight, y);
        }
        i++;
    }

    if (itemList.size() > 0)
    {
        // Now layout splitter
        QLayoutItem *item = itemList[0];
        y = effectiveRect.y();
        x = x + lineWidth + spaceX;

        if (!testOnly)
        {
            // Use all available space
            int splitterWidth = rect.width() - right - x;
            int splitterHeight = rect.height() - bottom - y;
            splitterWidth = qMax(splitterWidth, item->minimumSize().width());
            splitterHeight = qMax(splitterHeight, item->minimumSize().height());
            item->setGeometry(QRect(QPoint(x, y), QSize(splitterWidth, splitterHeight)));
        }
        lineWidth = item->minimumSize().width();
        y = y + item->minimumSize().height() + spaceY;
    }
    minHeight = qMax(minHeight, y);

    QSize size(x + lineWidth + right, minHeight - spaceY + bottom);
    return size;
}

QSize FeatureLayout::doLayoutVertically(const QRect &rect, bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;
    int spaceX = 0;
    int spaceY = 0;

    // Calculate space available for rows of widgets
    int maxHeightForRows = effectiveRect.height();
    if (itemList.size() > 0) {
        maxHeightForRows -= itemList[0]->minimumSize().height();
    }
    int minWidth = 0;

    int i = 0;
    for (QLayoutItem *item : qAsConst(itemList))
    {
        // Splitter is item 0, so skip
        if (i != 0)
        {
            const QWidget *wid = item->widget();
            spaceX = horizontalSpacing();
            if (spaceX == -1) {
                spaceX = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
            }
            spaceY = verticalSpacing();
            if (spaceY == -1) {
                spaceY = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
            }

            int nextX = x + item->sizeHint().width() + spaceX;
            if (nextX - spaceX > effectiveRect.right() && lineHeight > 0)
            {
                x = effectiveRect.x();
                y = y + lineHeight + spaceY;
                nextX = x + item->sizeHint().width() + spaceX;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
            }

            x = nextX;
            lineHeight = qMax(lineHeight, item->sizeHint().height());
            minWidth = qMax(minWidth, x);
        }
        i++;
    }

    if (itemList.size() > 0)
    {
        // Now layout splitter
        QLayoutItem *item = itemList[0];
        x = effectiveRect.x();
        y = y + lineHeight + spaceY;

        if (!testOnly)
        {
            // Use all available space
            int splitterWidth = rect.width() - right - x;
            int splitterHeight = rect.height() - bottom - y;
            splitterWidth = qMax(splitterWidth, item->minimumSize().width());
            splitterHeight = qMax(splitterHeight, item->minimumSize().height());
            item->setGeometry(QRect(QPoint(x, y), QSize(splitterWidth, splitterHeight)));
        }
        lineHeight = item->minimumSize().height();
        x = x + item->minimumSize().width() + spaceX;
    }
    minWidth = qMax(minWidth, x);

    QSize size(minWidth - spaceX + right, y + lineHeight - rect.y() + bottom);
    return size;
}

int FeatureLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent) {
        return -1;
    } else if (parent->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout *>(parent)->spacing();
    }
}
