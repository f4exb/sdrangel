///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_ROLLUPWIDGET_H
#define INCLUDE_ROLLUPWIDGET_H

#include <QWidget>
#include "export.h"

class RollupState;

class SDRGUI_API RollupWidget : public QWidget {
    Q_OBJECT

public:
    RollupWidget(QWidget* parent = nullptr);
    void setTitleColor(const QColor& c);
    void setHighlighted(bool highlighted);
    void setChannelWidget(bool channelWidget) { m_channelWidget = channelWidget; }

signals:
    void widgetRolled(QWidget* widget, bool rollDown);

protected:
    enum {
        VersionMarker = 0xff
    };

    enum ContextMenuType
    {
        ContextMenuNone,
        ContextMenuChannelSettings,
        ContextMenuStreamSettings
    };

    QColor m_titleColor;
    QColor m_titleTextColor;
    bool m_highlighted;
    ContextMenuType m_contextMenuType;
    QString m_streamIndicator;
    QString m_helpURL;

    int arrangeRollups();

    // QByteArray saveState(int version = 0) const;
    void saveState(RollupState& state) const;
    // bool restoreState(const QByteArray& state, int version = 0);
    void restoreState(const RollupState& state);

    void paintEvent(QPaintEvent*);
    int paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frame);

    void resizeEvent(QResizeEvent* size);
    void mousePressEvent(QMouseEvent* event);

    bool event(QEvent* event);
    bool eventFilter(QObject* object, QEvent* event);

    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }
    void setStreamIndicator(const QString& indicator);

private:
    static bool isRollupChild(QWidget *childWidget); //!< chidl is part of rollups (ex: not a dialog)
    bool m_channelWidget;
    int m_newHeight;
};

#endif // INCLUDE_ROLLUPWIDGET_H
