///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for features                                                              //
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

#ifndef INCLUDE_ROLLUPCONTENTS_H
#define INCLUDE_ROLLUPCONTENTS_H

#include <QWidget>
#include "export.h"

class RollupState;

class SDRGUI_API RollupContents : public QWidget {
    Q_OBJECT

public:
    RollupContents(QWidget* parent = nullptr);
    void saveState(RollupState& state) const;
    void restoreState(const RollupState& state);
    int arrangeRollups();
    bool hasExpandableWidgets();
    virtual QSize minimumSizeHint() const override { return m_minimumSizeHint; }

signals:
    void widgetRolled(QWidget* widget, bool rollDown);

protected:
    enum {
        VersionMarker = 0xff
    };

    QString m_streamIndicator;
    QString m_helpURL;

    void paintEvent(QPaintEvent*);
    int paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frameColor);

    void resizeEvent(QResizeEvent* size);
    void mousePressEvent(QMouseEvent* event);

    bool event(QEvent* event);
    bool eventFilter(QObject* object, QEvent* event);

private:
    static bool isRollupChild(QWidget *childWidget); //!< chidl is part of rollups (ex: not a dialog)
    // bool m_channelWidget;
    int m_newHeight;
    QSize m_minimumSizeHint;
};

#endif // INCLUDE_ROLLUPCONTENTS_H
