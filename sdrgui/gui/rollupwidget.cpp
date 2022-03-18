///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#include "gui/rollupwidget.h"
#include "settings/rollupstate.h"
#include "ui_glspectrumgui.h"

RollupWidget::RollupWidget(QWidget* parent) :
    QWidget(parent),
    m_highlighted(false),
    m_contextMenuType(ContextMenuNone),
    m_streamIndicator("S"),
    m_channelWidget(true),
    m_newHeight(0)
{
    setMinimumSize(250, 150);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setBackgroundRole(QPalette::Window);

    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    // Vorgaben aus der Palette
    m_titleColor = palette().highlight().color();
    m_titleTextColor = palette().highlightedText().color();
}

// QByteArray RollupWidget::saveState(int version) const
// {
//     QByteArray state;
//     QDataStream stream(&state, QIODevice::WriteOnly);
//     int count = 0;

//     for (int i = 0; i < children().count(); ++i)
//     {
//         QWidget* r = qobject_cast<QWidget*>(children()[i]);

//         if (r) {
//             count++;
//         }
//     }

//     stream << VersionMarker;
//     stream << version;
//     stream << count;

//     for (int i = 0; i < children().count(); ++i)
//     {
//         QWidget* r = qobject_cast<QWidget*>(children()[i]);

//         if (r)
//         {
//             stream << r->objectName();

//             if (r->isHidden()) {
//                 stream << (int) 0;
//             } else {
//                 stream << (int) 1;
//             }
//         }
//     }

//     return state;
// }

void RollupWidget::saveState(RollupState &state) const
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

// bool RollupWidget::restoreState(const QByteArray& state, int version)
// {
//     if (state.isEmpty()) {
//         return false;
//     }

//     QByteArray sd = state;
//     QDataStream stream(&sd, QIODevice::ReadOnly);
//     int marker, v;
//     stream >> marker;
//     stream >> v;

//     if ((stream.status() != QDataStream::Ok) || (marker != VersionMarker) || (v != version)) {
//         return false;
//     }

//     int count;
//     stream >> count;

//     if (stream.status() != QDataStream::Ok) {
//         return false;
//     }

//     for (int i = 0; i < count; ++i)
//     {
//         QString name;
//         int visible;

//         stream >> name;
//         stream >> visible;

//         if (stream.status() != QDataStream::Ok) {
//             return false;
//         }

//         for (int j = 0; j < children().count(); ++j)
//         {
//             QWidget* r = qobject_cast<QWidget*>(children()[j]);

//             if (r)
//             {
//                 if (r->objectName() == name)
//                 {
//                     if (visible) {
//                         r->show();
//                     } else {
//                         r->hide();
//                     }

//                     break;
//                 }
//             }
//         }
//     }

//     return true;
// }

void RollupWidget::restoreState(const RollupState& state)
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

void RollupWidget::setTitleColor(const QColor& c)
{
    m_titleColor = c;
    float l = 0.2126*c.redF() + 0.7152*c.greenF() + 0.0722*c.blueF();
    m_titleTextColor = l < 0.5f ? Qt::white : Qt::black;
    update();
}

void RollupWidget::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted)
    {
        m_highlighted = highlighted;
        update();
    }
}

int RollupWidget::arrangeRollups()
{
    QFontMetrics fm(font());
    int pos;

    // First calculate minimum height needed, to determine how much extra space
    // we have that can be split between expanding widgets
    pos = fm.height() + 4;
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
                pos += h + 5;
            }
        }
    }

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
    pos = fm.height() + 4;
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

void RollupWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QColor frame = palette().highlight().color();

    // Eigenbau
    QFontMetrics fm(font());

    p.setRenderHint(QPainter::Antialiasing, true);

    // Ecken (corners)
    p.setPen(Qt::NoPen);
    p.setBrush(palette().base());
    p.drawRect(0, 0, 5, 5);
    p.drawRect(width() - 5, 0, 5, 5);
    p.drawRect(0, height() - 5, 5, 5);
    p.drawRect(width() - 5, height() - 5, 5, 5);

    // Rahmen (frame)
    p.setPen(m_highlighted ? Qt::white : frame);
    p.setBrush(palette().window());
    QRectF r(rect());
    r.adjust(0.5, 0.5, -0.5, -0.5);
    p.drawRoundedRect(r, 3.0, 3.0, Qt::AbsoluteSize);

    // Titel-Hintergrund (Title background)
    p.setPen(Qt::NoPen);
    p.setBrush(m_titleColor);
    QPainterPath path;
    path.moveTo(1.5, fm.height() + 2.5);
    path.lineTo(width() - 1.5, fm.height() + 2.5);
    path.lineTo(width() - 1.5, 3.5);
    path.arcTo(QRectF(width() - 3.5, 0, 2.5, 2.5), 270, -90);
    path.lineTo(3.5, 1.5);
    path.arcTo(QRectF(1.5, 2.5, 2.5, 2.5), 90, 90);
    p.drawPath(path);

    // Titel-Abschlusslinie (Title closing line)
    p.setPen(frame);
    p.drawLine(QPointF(0.5, 2 + fm.height() + 1.5), QPointF(width() - 1.5, 2 + fm.height() + 1.5));

    // Aktiv-Button links
    p.setPen(QPen(palette().windowText().color(), 1.0));
    p.setBrush(palette().light());
    p.drawRoundedRect(QRectF(3.5, 3.5, fm.ascent(), fm.ascent()), 2.0, 2.0, Qt::AbsoluteSize);
    p.setPen(QPen(Qt::white, 1.0));
    p.drawText(QRectF(3.5, 2.5, fm.ascent(), fm.ascent()),  Qt::AlignCenter, "c");

    if (m_channelWidget)
    {
        // Stromkanal-Button links (Current channel)
        p.setPen(QPen(palette().windowText().color(), 1.0));
        p.setBrush(palette().light());
        p.drawRoundedRect(QRectF(5.5 + fm.ascent(), 2.5, fm.ascent() + 2.0, fm.ascent() + 2.0), 2.0, 2.0, Qt::AbsoluteSize);
        p.setPen(QPen(Qt::white, 1.0));
        p.drawText(QRectF(5.5 + fm.ascent(), 2.5, fm.ascent() + 2.0, fm.ascent() + 2.0),  Qt::AlignCenter, m_streamIndicator);
    }

    // Help button
    if (!m_helpURL.isEmpty())
    {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(palette().windowText().color(), 1.0));
        p.setBrush(palette().light());
        r = QRectF(width() - 2*(3.5 + fm.ascent()), 3.5, fm.ascent(), fm.ascent());
        p.drawRoundedRect(r, 2.0, 2.0, Qt::AbsoluteSize);
        p.drawText(QRectF(width() - 2*(3.5 + fm.ascent()), 5, fm.ascent(), fm.ascent() - 2), Qt::AlignCenter, "?");
    }

    //p.drawLine(r.topLeft() + QPointF(1, 1), r.bottomRight() + QPointF(-1, -1));
    //p.drawLine(r.bottomLeft() + QPointF(1, -1), r.topRight() + QPointF(-1, 1));

    // Schließen-Button rechts (Close button on the right)
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(palette().windowText().color(), 1.0));
    p.setBrush(palette().light());
    r = QRectF(width() - 3.5 - fm.ascent(), 3.5, fm.ascent(), fm.ascent());
    p.drawRoundedRect(r, 2.0, 2.0, Qt::AbsoluteSize);
    p.setPen(QPen(palette().windowText().color(), 1.5));
    p.drawLine(r.topLeft() + QPointF(1, 1), r.bottomRight() + QPointF(-1, -1));
    p.drawLine(r.bottomLeft() + QPointF(1, -1), r.topRight() + QPointF(-1, 1));

    // Titel
    //p.setPen(palette().highlightedText().color());
    p.setPen(m_titleTextColor);
    p.drawText(QRect(2 + 2*fm.height() + 2, 2, width() - 6 - 3*fm.height(), fm.height()),
        fm.elidedText(windowTitle(), Qt::ElideMiddle, width() - 6 - 3*fm.height(), 0));

    // Rollups
    int pos = fm.height() + 4;

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

        pos += paintRollup(qobject_cast<QWidget*>(*w), pos, &p, n == c.end(), frame);
    }
}

int RollupWidget::paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frame)
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
            p->setPen(frame);
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
        p->setPen(frame);
        p->drawLine(QPointF(1.5, pos + fm.height() + rollup->height() + 6.5),
                    QPointF(width() - 1.5, pos + fm.height() + rollup->height() + 6.5));
        height += rollup->height() + 4;
    }

    return height;
}

void RollupWidget::resizeEvent(QResizeEvent* size)
{
    m_newHeight = size->size().height();
    arrangeRollups();
    QWidget::resizeEvent(size);
}

void RollupWidget::mousePressEvent(QMouseEvent* event)
{
    QFontMetrics fm(font());

    // menu box left
    if (QRectF(3.5, 3.5, fm.ascent(), fm.ascent()).contains(event->pos()))
    {
        m_contextMenuType = ContextMenuChannelSettings;
        emit customContextMenuRequested(event->globalPos());
        return;
    }

    if (m_channelWidget)
    {
        // Stream channel menu left
        if (QRectF(5.5 + fm.ascent(), 2.5, fm.ascent() + 2.0, fm.ascent() + 2.0).contains(event->pos()))
        {
            m_contextMenuType = ContextMenuStreamSettings;
            emit customContextMenuRequested(event->globalPos());
            return;
        }
    }

    // help button
    if(!m_helpURL.isEmpty() && QRectF(width() - 2*(3.5 + fm.ascent()), 3.5, fm.ascent(), fm.ascent()).contains(event->pos()))
    {
        QString url;
        if (m_helpURL.startsWith("http")) {
            url = m_helpURL;
        } else {
            url = QString("https://github.com/f4exb/sdrangel/blob/master/%1").arg(m_helpURL); // Something like "plugins/channelrx/chanalyzer/readme.md"
        }
        QDesktopServices::openUrl(QUrl(url));
        return;
    }

    // close button right
    if(QRectF(width() - 3.5 - fm.ascent(), 3.5, fm.ascent(), fm.ascent()).contains(event->pos())) {
        close();
        return;
    }

    // check if we need to change a rollup widget
    int pos = fm.height() + 4;

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

bool RollupWidget::event(QEvent* event)
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

    return QWidget::event(event);
}

bool RollupWidget::eventFilter(QObject* object, QEvent* event)
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

void RollupWidget::setStreamIndicator(const QString& indicator)
{
    m_streamIndicator = indicator;
    update();
}

bool RollupWidget::isRollupChild(QWidget *childWidget)
{
    return (qobject_cast<QDialog*>(childWidget) == nullptr); // exclude Dialogs from rollups
}
