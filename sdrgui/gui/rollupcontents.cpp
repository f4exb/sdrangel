///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDialog>
#include <QDesktopServices>

#include "gui/rollupcontents.h"
#include "settings/rollupstate.h"
#include "ui_glspectrumgui.h"

RollupContents::RollupContents(QWidget* parent) :
    QWidget(parent),
    m_streamIndicator("S"),
    // m_channelWidget(true),
    m_newHeight(0)
{
    setMinimumSize(250, 150);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setBackgroundRole(QPalette::Window);

    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void RollupContents::saveState(RollupState &state) const
{
    QList<RollupState::RollupChildState>& childrenStates = state.getChildren();
    childrenStates.clear();

    for (const auto &child : children())
    {
        QWidget* r = qobject_cast<QWidget*>(child);

        if (r && isRollupChild(r)) {
            childrenStates.push_back({r->objectName(), r->isHidden()});
        }
    }
}

void RollupContents::restoreState(const RollupState& state)
{
    const QList<RollupState::RollupChildState>& childrenStates = state.getChildren();

    for (const auto &object : children())
    {
        QWidget* r = qobject_cast<QWidget*>(object);

        if (r && isRollupChild(r))
        {
            for (const auto &childState : childrenStates)
            {
                if (childState.m_objectName.compare(r->objectName()) == 0)
                {
                    if (childState.m_isHidden) {
                        r->hide();
                    } else {
                        r->show();
                    }

                    break;
                }
            }
        }
    }
}

bool RollupContents::hasExpandableWidgets()
{
    for (int i = 0; i < children().count(); ++i)
    {
        QWidget* r = qobject_cast<QWidget*>(children()[i]);

        if (r && isRollupChild(r) && !r->isHidden() && (r->sizePolicy().verticalPolicy() == QSizePolicy::Expanding)) {
            return true;
        }
    }

    return false;
}
int RollupContents::arrangeRollups()
{
    QFontMetrics fm(font());
    int pos;
    int minWidthHint = 0;

    // First calculate minimum height needed, to determine how much extra space
    // we have that can be split between expanding widgets
    pos = 2; // fm.height() + 4;
    int expandingChildren = 0;
    for (int i = 0; i < children().count(); ++i)
    {
        QWidget* r = qobject_cast<QWidget*>(children()[i]);

        if ((r != nullptr) && isRollupChild(r))
        {
            pos += fm.height() + 2;
            if (!r->isHidden())
            {
                if (r->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
                    expandingChildren++;
                }
                int h = 0;
                if (r->hasHeightForWidth()) {
                    h = r->heightForWidth(width() - 4);
                } else {
                    h = r->minimumSizeHint().height();
                }
                minWidthHint = std::max(minWidthHint, r->minimumSizeHint().width());
                pos += h + 5;
            }
        }
    }

    // We use minimumSizeHint for auto-calculated width, so
    // minimumWidth can be set by user in .ui file
    m_minimumSizeHint.setHeight(pos);
    m_minimumSizeHint.setWidth(minWidthHint);

    // However, we need to set minimumHeight, as the value in the .ui
    // files is typically set as the minimum height for when widget is unrolled
    // but when rolled-up, we want it to be smaller
    setMinimumHeight(pos);

    // Split extra space equally between widgets
    // If there's a remainder, we give it to the first widget
    // In the future, we should probably respect 'Vertical Stretch'
    int extraSpace;
    int firstExtra;
    if ((expandingChildren > 0) && (m_newHeight > pos))
    {
        int totalExtra = m_newHeight - pos;
        extraSpace = totalExtra / expandingChildren;
        firstExtra = totalExtra - (extraSpace * expandingChildren);
    }
    else
    {
        extraSpace = 0;
        firstExtra = 0;
    }

    // Now reposition and resize child widgets
    pos = 2; // fm.height() + 4;
    for (int i = 0; i < children().count(); ++i)
    {
        QWidget* r = qobject_cast<QWidget*>(children()[i]);

        if ((r != nullptr) && isRollupChild(r))
        {
            pos += fm.height() + 2;

            if (!r->isHidden())
            {
                r->move(2, pos + 3);

                int h = 0;
                if (r->hasHeightForWidth()) {
                    h = r->heightForWidth(width() - 4);
                } else {
                    h = r->minimumSizeHint().height();
                }
                if (r->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag)
                {
                    h += extraSpace;
                    h += firstExtra;
                    firstExtra = 0;
                }

                r->resize(width() - 4, h);
                pos += r->height() + 5;
            }
        }
    }

    if (expandingChildren == 0) {
        setMaximumHeight(pos);
    } else {
        setMaximumHeight(16777215);
    }
    updateGeometry();
    return pos;
}

void RollupContents::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QColor frameColor = palette().highlight().color().darker(125);
    // QColor frameColor = Qt::black;

    // Eigenbau
    QFontMetrics fm(font());

    p.setRenderHint(QPainter::Antialiasing, true);

    // Ecken (corners)
    // p.setPen(Qt::NoPen);
    // p.setBrush(palette().base());
    // p.drawRect(0, 0, 5, 5);
    // p.drawRect(width() - 5, 0, 5, 5);
    // p.drawRect(0, height() - 5, 5, 5);
    // p.drawRect(width() - 5, height() - 5, 5, 5);

    // Rahmen (frame)
    // p.setPen(m_highlighted ? Qt::white : frameColor);
    p.setBrush(palette().window());
    QRectF r(rect());
    // r.adjust(0.5, 0.5, -0.5, -0.5);
    // p.drawRoundedRect(r, 3.0, 3.0, Qt::AbsoluteSize);
    // p.drawRect(r);
    p.fillRect(r, palette().window());

    // Rollups
    int pos = 2; // fm.height() + 4;

    const QObjectList& c = children();
    QObjectList::ConstIterator w = c.begin();
    QObjectList::ConstIterator n = c.begin();

    for (n = c.begin(); n != c.end(); ++n)
    {
        if (qobject_cast<QWidget*>(*n) != nullptr) {
            break;
        }
    }

    for (w = n; w != c.end(); w = n)
    {
        if (n != c.end()) {
            ++n;
        }

        for (; n != c.end(); ++n)
        {
            if (qobject_cast<QWidget*>(*n) != nullptr) {
                break;
            }
        }

        pos += paintRollup(qobject_cast<QWidget*>(*w), pos, &p, n == c.end(), frameColor);
    }
}

int RollupContents::paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frameColor)
{
    QFontMetrics fm(font());
    int height = 1;

    // Titel-Abschlusslinie
    if (!rollup->isHidden())
    {
        p->setPen(palette().dark().color());
        p->drawLine(QPointF(1.5, pos + fm.height() + 1.5), QPointF(width() - 1.5, pos + fm.height() + 1.5));
        p->setPen(palette().light().color());
        p->drawLine(QPointF(1.5, pos + fm.height() + 2.5), QPointF(width() - 1.5, pos + fm.height() + 2.5));
        height += 2;
    }
    else
    {
        if (!last)
        {
            p->setPen(frameColor);
            p->drawLine(QPointF(1.5, pos + fm.height() + 1.5), QPointF(width() - 1.5, pos + fm.height() + 1.5));
            height++;
        }
    }

    // Titel
    p->setPen(palette().windowText().color());
    p->drawText(QRect(2 + fm.height(), pos, width() - 4 - fm.height(), fm.height()),
        fm.elidedText(rollup->windowTitle(), Qt::ElideMiddle, width() - 4 - fm.height(), 0));
    height += fm.height();

    // Ausklapp-Icon
    p->setPen(palette().windowText().color());
    p->setBrush(palette().windowText());

    if (!rollup->isHidden())
    {
        QPolygonF a;
        a.append(QPointF(3.5, pos + 2));
        a.append(QPointF(3.5 + fm.ascent(), pos + 2));
        a.append(QPointF(3.5 + fm.ascent() / 2.0, pos + fm.height() - 2));
        p->drawPolygon(a);
    }
    else
    {
        QPolygonF a;
        a.append(QPointF(3.5, pos + 2));
        a.append(QPointF(3.5, pos + fm.height() - 2));
        a.append(QPointF(3.5 + fm.ascent(), pos + fm.height() / 2));
        p->drawPolygon(a);
    }

    // Inhalt
    if (!rollup->isHidden() && (!last))
    {
        // Rollup-Abschlusslinie
        p->setPen(frameColor);
        p->drawLine(QPointF(1.5, pos + fm.height() + rollup->height() + 6.5),
                    QPointF(width() - 1.5, pos + fm.height() + rollup->height() + 6.5));
        height += rollup->height() + 4;
    }

    return height;
}

void RollupContents::resizeEvent(QResizeEvent* size)
{
    m_newHeight = size->size().height();
    arrangeRollups();
    QWidget::resizeEvent(size);
}

void RollupContents::mousePressEvent(QMouseEvent* event)
{
    QFontMetrics fm(font());

    // check if we need to change a rollup widget
    int pos = 2; // fm.height() + 4;

    for (int i = 0; i < children().count(); ++i)
    {
        QWidget* r = qobject_cast<QWidget*>(children()[i]);

        if (r)
        {
            if ((event->y() >= pos) && (event->y() < (pos + fm.height() + 3)))
            {
                if (r->isHidden())
                {
                    r->show();
                    //emit widgetRolled(r, true);
                }
                else
                {
                    r->hide();
                    //emit widgetRolled(r, false);
                }

                arrangeRollups();
                repaint();
                return;
            }
            else
            {
                pos += fm.height() + 2;

                if (!r->isHidden()) {
                    pos += r->height() + 5;
                }
            }
        }
    }
}

bool RollupContents::event(QEvent* event)
{
    if (event->type() == QEvent::ChildAdded)
    {
        ((QChildEvent*)event)->child()->installEventFilter(this);
        arrangeRollups();
    }
    else if (event->type() == QEvent::ChildRemoved)
    {
        ((QChildEvent*)event)->child()->removeEventFilter(this);
        arrangeRollups();
    }
    else if (event->type() == QEvent::LayoutRequest)
    {
        arrangeRollups();
    }

    return QWidget::event(event);
}

bool RollupContents::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Show)
    {
        if (children().contains(object))
        {
            arrangeRollups();
            emit widgetRolled(qobject_cast<QWidget*>(object), true);
        }
    }
    else if (event->type() == QEvent::Hide)
    {
        if (children().contains(object))
        {
            arrangeRollups();
            emit widgetRolled(qobject_cast<QWidget*>(object), false);
        }
    }
    else if (event->type() == QEvent::WindowTitleChange)
    {
        if (children().contains(object)) {
            repaint();
        }
    }

    return QWidget::eventFilter(object, event);
}

bool RollupContents::isRollupChild(QWidget *childWidget)
{
    return (qobject_cast<QDialog*>(childWidget) == nullptr); // exclude Dialogs from rollups
}
