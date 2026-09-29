///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
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
#include <QDesktopServices>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTextBlock>
#include <QToolTip>
#include <QUrl>
#include <QDebug>

#include "feature/featurewebapiutils.h"
#include "util/callsign.h"
#include "util/maidenhead.h"
#include "acronymview.h"

AcronymView::AcronymView(QWidget* parent) :
    QPlainTextEdit(parent)
{
    setMouseTracking(true);
    setReadOnly(true);
}

bool AcronymView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        const QString url = urlAt(helpEvent->pos());
        const QString word = wordAt(helpEvent->pos());
        QString acronym = word.toUpper();
        // Remove trailing digits from METAR
        while (!acronym.isEmpty() && acronym.back().isDigit()) {
            acronym.chop(1);
        }
        if (!url.isEmpty())
        {
            QToolTip::showText(
                helpEvent->globalPos(),
                tr("%1 - Web link (click to open)").arg(url));
        }
        else if (!acronym.isEmpty() && m_acronym.contains(acronym))
        {
            QToolTip::showText(helpEvent->globalPos(), QString("%1 - %2").arg(acronym, m_acronym.value(acronym)));
        }
        else if (Maidenhead::isMaidenhead(word))
        {
            QToolTip::showText(
                helpEvent->globalPos(),
                tr("%1 - Maidenhead locator (click to find on Map)").arg(word.toUpper()));
        }
        else
        {
            QString country;
            const QString callsign = callsignAt(helpEvent->pos(), &country);

            if (!callsign.isEmpty())
            {
                QToolTip::showText(
                    helpEvent->globalPos(),
                    tr("%1 - Amateur radio callsign from %2 (click to look up on QRZ)")
                        .arg(callsign, country));
                return true;
            }

            if (!word.isEmpty()) {
                qDebug() << "AcronymView::event: No tooltip for " << word;
            }
            QToolTip::hideText();
        }
        return true;
    }
    return QPlainTextEdit::event(event);
}

QString AcronymView::wordAt(const QPoint& position) const
{
    QTextCursor cursor = cursorForPosition(position);
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

QString AcronymView::urlAt(const QPoint& position) const
{
    const QTextCursor cursor = cursorForPosition(position);
    const QTextBlock block = cursor.block();
    const QString text = block.text();
    const int positionInBlock = cursor.position() - block.position();

    if ((positionInBlock < 0) || (positionInBlock >= text.size()) || text[positionInBlock].isSpace()) {
        return QString();
    }

    int start = positionInBlock;
    int end = positionInBlock + 1;

    while ((start > 0) && !text[start - 1].isSpace()) {
        --start;
    }
    while ((end < text.size()) && !text[end].isSpace()) {
        ++end;
    }

    QString candidate = text.mid(start, end - start);
    const QString leadingPunctuation = QStringLiteral("([{<\"'");
    const QString trailingPunctuation = QStringLiteral(".,;:!?)]}>\"'");

    while (!candidate.isEmpty() && leadingPunctuation.contains(candidate.front())) {
        candidate.remove(0, 1);
    }
    while (!candidate.isEmpty() && trailingPunctuation.contains(candidate.back())) {
        candidate.chop(1);
    }

    if (   !candidate.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        && !candidate.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return QString();
    }

    const QUrl url(candidate, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty() ? url.toString() : QString();
}

QString AcronymView::maidenheadAt(const QPoint& position) const
{
    const QString word = wordAt(position);
    QString acronym = word;

    while (!acronym.isEmpty() && acronym.back().isDigit()) {
        acronym.chop(1);
    }

    return !m_acronym.contains(acronym) && Maidenhead::isMaidenhead(word) ? word.toUpper() : QString();
}

QString AcronymView::callsignAt(const QPoint& position, QString* country) const
{
    if (country) {
        country->clear();
    }

    const QString word = wordAt(position);
    QString acronym = word;

    while (!acronym.isEmpty() && acronym.back().isDigit()) {
        acronym.chop(1);
    }

    if (m_acronym.contains(acronym) || Maidenhead::isMaidenhead(word)) {
        return QString();
    }

    const QString callsign = word.toUpper();

    // Callsign::is_callsign expects at least two characters and deliberately
    // accepts a prefix match. Keep the surrounding check strict so ordinary
    // words containing a callsign-like fragment do not become QRZ links.
    if ((callsign.size() < 3) || (callsign.size() > 10)) {
        return QString();
    }

    static const QRegularExpression amateurCallsign(
        R"(^(?:[2-9][A-Z]{1,2}|[A-Z]{1,2})[0-9]{1,4}[A-Z]{1,4}$)");

    if (!amateurCallsign.match(callsign).hasMatch() || !Callsign::is_callsign(callsign)) {
        return QString();
    }

    const CountryDat::CountryInfo countryInfo = Callsign::instance()->getCountryInfo(callsign);

    if (countryInfo.country == CountryDat::nullCountry.country) {
        return QString();
    }

    if (country) {
        *country = countryInfo.country;
    }

    return callsign;
}

void AcronymView::mouseMoveEvent(QMouseEvent* event)
{
    QPlainTextEdit::mouseMoveEvent(event);
    const bool link =   !urlAt(event->pos()).isEmpty()
                     || !maidenheadAt(event->pos()).isEmpty()
                     || !callsignAt(event->pos()).isEmpty();
    viewport()->setCursor(link ? Qt::PointingHandCursor : Qt::IBeamCursor);
}

void AcronymView::mousePressEvent(QMouseEvent* event)
{
    m_pressedUrl = event->button() == Qt::LeftButton ? urlAt(event->pos()) : QString();
    m_pressedMaidenhead = m_pressedUrl.isEmpty() && (event->button() == Qt::LeftButton)
        ? maidenheadAt(event->pos()) : QString();
    m_pressedCallsign = m_pressedUrl.isEmpty() && (event->button() == Qt::LeftButton)
        ? callsignAt(event->pos()) : QString();
    QPlainTextEdit::mousePressEvent(event);
}

void AcronymView::mouseReleaseEvent(QMouseEvent* event)
{
    const QString url = event->button() == Qt::LeftButton ? urlAt(event->pos()) : QString();

    if (!url.isEmpty() && (url == m_pressedUrl))
    {
        m_pressedUrl.clear();
        m_pressedMaidenhead.clear();
        m_pressedCallsign.clear();
        QDesktopServices::openUrl(QUrl(url));
        event->accept();
        return;
    }

    const QString maidenhead = event->button() == Qt::LeftButton ? maidenheadAt(event->pos()) : QString();

    if (!maidenhead.isEmpty() && (maidenhead == m_pressedMaidenhead))
    {
        m_pressedUrl.clear();
        m_pressedMaidenhead.clear();
        m_pressedCallsign.clear();
        FeatureWebAPIUtils::mapFind(maidenhead);
        event->accept();
        return;
    }

    const QString callsign = event->button() == Qt::LeftButton ? callsignAt(event->pos()) : QString();

    if (!callsign.isEmpty() && (callsign == m_pressedCallsign))
    {
        m_pressedUrl.clear();
        m_pressedMaidenhead.clear();
        m_pressedCallsign.clear();
        QDesktopServices::openUrl(QUrl(QString("https://www.qrz.com/db/%1").arg(callsign)));
        event->accept();
        return;
    }

    m_pressedUrl.clear();
    m_pressedMaidenhead.clear();
    m_pressedCallsign.clear();
    QPlainTextEdit::mouseReleaseEvent(event);
}

void AcronymView::addAcronym(const QString& acronym, const QString& explanation)
{
    m_acronym.insert(acronym, explanation);
}

void AcronymView::addAcronyms(const QHash<QString, QString>& acronyms)
{
    m_acronym.insert(acronyms);
}
