///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                                //
// written by Robin Getz <rgetz503@gmail.com>                                        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include "releaseupdatewidget.h"

#ifdef ENABLE_RELEASE_CHECK

#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QTimer>

#include "releaseupdatechecker.h"
#include "releaseupdateservice.h"

ReleaseUpdateWidget::ReleaseUpdateWidget(QWidget *parent) :
    QLabel(parent),
    m_checker(nullptr),
    m_blinkTimer(new QTimer(this)),
    m_blink(false)
{
    hide();
    setTextFormat(Qt::RichText);
    setCursor(Qt::PointingHandCursor);
    setTextInteractionFlags(Qt::LinksAccessibleByMouse);

    ReleaseUpdateService *service = createReleaseUpdateService(this);

    if (!service) {
        return;
    }

    m_checker = new ReleaseUpdateChecker(
        service,
        qApp->applicationVersion(),
        this
    );

    connect(
        this,
        &QLabel::linkActivated,
        this,
        &ReleaseUpdateWidget::onLinkActivated
    );

    connect(
        m_checker,
        &ReleaseUpdateChecker::updateAvailabilityChanged,
        this,
        &ReleaseUpdateWidget::onReleaseUpdateAvailabilityChanged
    );

    connect(
        m_blinkTimer,
        &QTimer::timeout,
        this,
        &ReleaseUpdateWidget::updateBlink
    );

    m_blinkTimer->setInterval(500);
}

void ReleaseUpdateWidget::setVisible(bool visible)
{
    QLabel::setVisible(visible && m_wantVisible);
}

void ReleaseUpdateWidget::updateNotificationText()
{
    if (!m_checker) {
        return;
    }

    const QPalette palette = this->palette();
    const QString color = palette.color(QPalette::Link).name();

    setText(
        QString("<a href=\"update\" style=\"color:%1;\">%2</a>")
            .arg(
                color,
                m_checker->updateNotificationText().toHtmlEscaped()
            )
    );
}

void ReleaseUpdateWidget::updateBlink()
{
    if (!m_checker || !m_checker->updateAvailable()) {
        return;
    }

    m_blink = !m_blink;

    const QPalette palette = this->palette();
    const QString color = palette.color(
        m_blink ? QPalette::LinkVisited : QPalette::Link
    ).name();

    setText(
        QString("<a href=\"update\" style=\"color:%1;\">%2</a>")
            .arg(
                color,
                m_checker->updateNotificationText().toHtmlEscaped()
            )
    );
}

void ReleaseUpdateWidget::onReleaseUpdateAvailabilityChanged(bool available)
{
    m_blink = false;
    m_wantVisible = available;

    if (available) {
        updateNotificationText();
        setVisible(true);
        m_blinkTimer->start();
    }
    else {
        m_blinkTimer->stop();
        clear();
        setVisible(false);
    }
}

void ReleaseUpdateWidget::onLinkActivated(const QString& link)
{
    if (m_checker && link == "update") {
        m_checker->openUpdate();
    }
}

#else // ENABLE_RELEASE_CHECK

ReleaseUpdateWidget::ReleaseUpdateWidget(QWidget *parent) :
    QLabel(parent),
    m_checker(nullptr),
    m_blinkTimer(nullptr),
    m_blink(false)
{
    hide();
}

#endif // ENABLE_RELEASE_CHECK
