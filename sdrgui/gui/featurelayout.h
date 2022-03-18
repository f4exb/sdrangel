///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 The Qt Company Ltd.                                        //
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

#ifndef SDRGUI_GUI_FEATURELAYOUT_H
#define SDRGUI_GUI_FEATURELAYOUT_H

#include <QLayout>
#include <QRect>
#include <QStyle>

// A QLayout specifically for the Features window, that tries to make the most
// of available space, while allowing the user to resize some elements, on the
// assumption that there are two types of Feature UI
//  - Fixed size widgets like Rotator Controller
//  - Expanding widgets like Map
// The Feature window is split in to two parts (when horizontal orientation):
//  - Left hand side for fixed widgets which are stacked in vertical columns
//    to fit available height
//  - Right hand side is for expanding widgets inside a Splitter which allows
//    a user to manually set how much space is used for each Feature
// When vertical orientation, the fixed widgets are in columns at the top, with
// the expanding widgets underneath (this isn't quite as nice, as the widget
// heights vary more than the widths)
class FeatureLayout : public QLayout
{
public:
    explicit FeatureLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit FeatureLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FeatureLayout();

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;
    void setOrientation(Qt::Orientation);

private:
    QSize doLayoutVertically(const QRect &rect, bool testOnly) const;
    QSize doLayoutHorizontally(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
    Qt::Orientation m_orientation;
    QRect m_prevGeometry;
};

#endif // SDRGUI_GUI_FEATURELAYOUT_H
