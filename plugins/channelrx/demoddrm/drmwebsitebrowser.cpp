///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextFragment>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#else
#include <QTextCodec>
#endif

#include "drmwebsitebrowser.h"

QImage DRMWebsiteImages::image(QTextDocument *document, const QTextFormat& format)
{
    const QVariant data = document->resource(QTextDocument::ImageResource, QUrl(format.toImageFormat().name()));

    if (data.userType() == QMetaType::QImage) {
        return data.value<QImage>();
    }

    if (data.userType() == QMetaType::QPixmap) {
        return data.value<QPixmap>().toImage();
    }

    if (data.userType() == QMetaType::QByteArray) {
        return QImage::fromData(data.toByteArray());
    }

    return QImage();
}

QSizeF DRMWebsiteImages::intrinsicSize(QTextDocument *document, int position, const QTextFormat& format)
{
    (void) position;
    const QTextImageFormat imageFormat = format.toImageFormat();
    const QImage picture = image(document, format);
    const QSizeF natural = picture.isNull() ? QSizeF(1, 1)
        : QSizeF(picture.width() / picture.devicePixelRatio(), picture.height() / picture.devicePixelRatio());
    const bool hasWidth = imageFormat.hasProperty(QTextFormat::ImageWidth);
    const bool hasHeight = imageFormat.hasProperty(QTextFormat::ImageHeight);

    // As written (width and height), else scaled to the one given
    if (hasWidth && hasHeight) {
        return QSizeF(imageFormat.width(), imageFormat.height());
    }

    if (hasWidth && (natural.width() > 0)) {
        return QSizeF(imageFormat.width(), natural.height() * imageFormat.width() / natural.width());
    }

    if (hasHeight && (natural.height() > 0)) {
        return QSizeF(natural.width() * imageFormat.height() / natural.height(), imageFormat.height());
    }

    return natural;
}

void DRMWebsiteImages::drawObject(QPainter *painter, const QRectF& rect, QTextDocument *document, int position,
    const QTextFormat& format)
{
    (void) position;
    const QImage picture = image(document, format);

    if (!picture.isNull()) {
        painter->drawImage(rect, picture);
    }
}

DRMWebsiteBrowser::DRMWebsiteBrowser(QWidget *parent) :
    QTextBrowser(parent),
    m_images(new DRMWebsiteImages(this))
{
    setOpenExternalLinks(false);
    useImageHandler();

    // Pages are written for a web browser's defaults, whatever SDRangel's theme
    QPalette page = palette();
    page.setColor(QPalette::Base, Qt::white);
    page.setColor(QPalette::Text, Qt::black);
    page.setColor(QPalette::Link, QColor(0x00, 0x00, 0xee));
    page.setColor(QPalette::LinkVisited, QColor(0x55, 0x1a, 0x8b));
    setPalette(page);

    setHtml(tr("<p><i>No Broadcast Website</i></p>"));
}

QUrl DRMWebsiteBrowser::fileUrl(const std::string& name)
{
    QUrl url;
    url.setScheme(QStringLiteral("bws"));
    url.setPath(QStringLiteral("/") + QString::fromUtf8(name.data(), static_cast<int>(name.size())), QUrl::DecodedMode);
    return url;
}

void DRMWebsiteBrowser::setWebsite(std::shared_ptr<const modemdrm::MOTWebsite> website)
{
    // Another generation is another website (a service change the GUI did
    // not see the website cleared for): its pages and history start again
    const bool another = !website || !m_website || (website->generation != m_website->generation);
    m_website = std::move(website);

    if (another)
    {
        m_page = QUrl();
        m_used.clear();
        clearHistory();
    }

    if (!m_website)
    {
        m_page = QUrl();
        m_used.clear();
        clearHistory();
        setHtml(tr("<p><i>No Broadcast Website</i></p>"));
        return;
    }

    if (m_page.isEmpty())
    {
        showStartPage();
        return;
    }

    // Show the page again only when a file it uses arrived or changed, keeping
    // the reader's place
    const bool changed = std::any_of(m_used.begin(), m_used.end(), [this](const auto& used) {
        return use(*m_website, QUrl(used.first)) != used.second;
    });

    if (changed)
    {
        const int scroll = verticalScrollBar()->value();
        reload();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        pageShown(); // Qt 5's reload does not reach setSource(const QUrl&)
#endif
        verticalScrollBar()->setValue(scroll);
    }
}

void DRMWebsiteBrowser::showStartPage()
{
    if (!m_website) {
        setHtml(tr("<p><i>No Broadcast Website</i></p>"));
    } else if (m_website->startPage.empty()) {
        setHtml(tr("<p><i>Waiting for the website's directory...</i></p>"));
    } else {
        setSource(fileUrl(m_website->startPage));
    }
}

void DRMWebsiteBrowser::home()
{
    showStartPage();
}

void DRMWebsiteBrowser::backward()
{
    // The history is restored without doSetSource
    m_used.clear();
    QTextBrowser::backward();
    pageShown();
}

void DRMWebsiteBrowser::forward()
{
    m_used.clear();
    QTextBrowser::forward();
    pageShown();
}

QUrl DRMWebsiteBrowser::resolved(const QUrl& url) const
{
    return url.isRelative() ? (m_page.isEmpty() ? fileUrl(std::string()) : m_page).resolved(url) : url;
}

bool DRMWebsiteBrowser::local(const QUrl& url)
{
    // bws: URLs, and pages written for a receiver's own web server
    const QString host = url.host().toLower();
    return (url.scheme() == QLatin1String("bws"))
        || (((url.scheme() == QLatin1String("http")) || (url.scheme() == QLatin1String("https")))
            && ((host == QLatin1String("127.0.0.1")) || (host == QLatin1String("localhost"))));
}

std::string DRMWebsiteBrowser::websiteName(const modemdrm::MOTWebsite& website, const QUrl& url)
{
    if (!local(url)) {
        return std::string();
    }

    QString path = url.path(QUrl::FullyDecoded);

    while (path.startsWith('/')) {
        path.remove(0, 1);
    }

    // The root is the start page
    if (path.isEmpty()) {
        return website.startPage;
    }

    // The path names the file (TS 101 498-1 annex A.1.1), else is a directory
    // that gives its DirectoryIndex. Pages written for a receiver's web server
    // may add leading directories (http://127.0.0.1/bws/page.html), which are
    // dropped for those URLs only.
    const QByteArray bytes = path.toUtf8();
    std::string variant(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    const bool receiverServer = url.scheme() != QLatin1String("bws");
    const auto known = [&website](const std::string& name) {
        return website.files.count(name) || website.listed.count(name);
    };

    for (;;)
    {
        if (variant.empty()) {
            return website.startPage; // a receiver's web server's root, http://127.0.0.1/bws/
        }

        const bool directory = variant.back() == '/';

        if (!directory && known(variant)) {
            return variant;
        }

        const std::string index = variant + (directory ? "" : "/") + website.indexName;

        if (known(index)) {
            return index;
        }

        const auto slash = variant.find('/');

        if (!receiverServer || (slash == std::string::npos)) {
            break;
        }

        variant.erase(0, slash + 1U);
    }

    return std::string();
}

DRMWebsiteBrowser::Use DRMWebsiteBrowser::use(const modemdrm::MOTWebsite& website, const QUrl& url)
{
    Use result;
    const std::string found = websiteName(website, url);

    if (!found.empty())
    {
        const auto file = website.files.find(found);
        result.data = file == website.files.end() ? nullptr : file->second.data.get();
        result.listed = true;
    }

    return result;
}

QByteArray DRMWebsiteBrowser::fixColours(const QByteArray& html)
{
    // As Latin-1, which keeps every byte whatever the page's character set
    static const QRegularExpression tag(QStringLiteral("<[^<>]*>"));
    static const QRegularExpression bareHex(
        QStringLiteral("\\b((?:bg)?color|text|link|vlink|alink)(\\s*=\\s*)([\"']?)([0-9a-fA-F]{6}|[0-9a-fA-F]{3})(?=[\"'\\s>/])"),
        QRegularExpression::CaseInsensitiveOption);
    const QString text = QString::fromLatin1(html);
    QString fixed;
    fixed.reserve(text.size() + 64);
    int position = 0;
    QRegularExpressionMatchIterator tags = tag.globalMatch(text);

    while (tags.hasNext())
    {
        const QRegularExpressionMatch match = tags.next();
        fixed += text.mid(position, match.capturedStart() - position);
        QString element = match.captured();
        element.replace(bareHex, QStringLiteral("\\1\\2\\3#\\4"));
        fixed += element;
        position = match.capturedEnd();
    }

    fixed += text.mid(position);
    return fixed.toLatin1();
}

QString DRMWebsiteBrowser::htmlText(const QByteArray& html)
{
    // A page that starts with a <qt type=detail> tag would be shown in a
    // What's This window, outside the browser: a comment comes first
    const QString start = QStringLiteral("<!-- -->");

    // A byte order mark, else the character set a meta tag declares where Qt
    // looks for one (the first 1,024 bytes), if Qt has that character set
    QByteArray name;

    if (html.startsWith("\xef\xbb\xbf")) {
        name = "UTF-8";
    } else if (html.startsWith("\xff\xfe")) {
        name = "UTF-16LE";
    } else if (html.startsWith("\xfe\xff")) {
        name = "UTF-16BE";
    }
    else
    {
        static const QRegularExpression meta(QStringLiteral("<meta[^>]*charset\\s*=\\s*[\"']?([A-Za-z0-9_:.\\-]+)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = meta.match(QString::fromLatin1(html.left(1024)));

        if (match.hasMatch()) {
            name = match.captured(1).toLatin1();
        }
    }

    if (!name.isEmpty())
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringDecoder decoder(name.constData());

        if (decoder.isValid())
        {
            const QString text = decoder.decode(html);

            if (!decoder.hasError()) {
                return start + text;
            }
        }
#else
        if (QTextCodec *codec = QTextCodec::codecForName(name)) {
            return start + codec->toUnicode(html);
        }
#endif
    }

    const QString utf8 = QString::fromUtf8(html);
    return start + (utf8.toUtf8() == html ? utf8 : QString::fromLatin1(html));
}

QVariant DRMWebsiteBrowser::loadResource(int type, const QUrl& name)
{
    const QUrl url = resolved(name);

    // Qt decodes data: URLs itself
    if (url.scheme() == QLatin1String("data")) {
        return QVariant();
    }

    const std::string found = m_website ? websiteName(*m_website, url) : std::string();

    if (m_website) {
        m_used[url.toString(QUrl::RemoveFragment)] = use(*m_website, url);
    }

    if (!found.empty())
    {
        const auto file = m_website->files.find(found);

        if ((file != m_website->files.end()) && !file->second.data->empty())
        {
            const QByteArray data(reinterpret_cast<const char *>(file->second.data->data()), static_cast<int>(file->second.data->size()));

            if (type == QTextDocument::HtmlResource) {
                return htmlText(fixColours(data));
            }

            // An image decoded here: Qt loads one that does not decode from
            // its name, as a file
            if (type == QTextDocument::ImageResource)
            {
                const QImage image = QImage::fromData(data);

                if (!image.isNull()) {
                    return image;
                }
            }
            else if (type == QTextDocument::MarkdownResource) {
                // As a page: never a What's This window
                return QStringLiteral("<!-- -->\n") + QString::fromUtf8(data);
            } else {
                return data;
            }
        }
    }

    // Never a null result: Qt would then read the URL as a local file
    if (type == QTextDocument::HtmlResource)
    {
        // A page the directory lists but the carousel has not sent yet, else one
        // the website does not have
        if (!found.empty()) {
            return tr("<p><i>Waiting for this page...</i></p>");
        }

        return tr("<p><i>This page is not in the website.</i></p><p><a href=\"bws:/\">Start page</a></p>");
    }

    if (type == QTextDocument::ImageResource)
    {
        QImage blank(1, 1, QImage::Format_ARGB32);
        blank.fill(Qt::transparent);
        return blank;
    }

    return QString(QLatin1Char(' '));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void DRMWebsiteBrowser::doSetSource(const QUrl& name, QTextDocument::ResourceType type)
{
    showSource(name, type);
}
#else
void DRMWebsiteBrowser::setSource(const QUrl& name)
{
    showSource(name, QTextDocument::UnknownResource);
}
#endif

void DRMWebsiteBrowser::showSource(const QUrl& name, QTextDocument::ResourceType type)
{
    const QUrl url = resolved(name);

    // Links out of the website are ignored
    if (!local(url)) {
        return;
    }

    // As the website's own URL, so that the page's relative links resolve in
    // it, with its fragment; a fragment of the page shown only scrolls to it
    const std::string found = m_website ? websiteName(*m_website, url) : std::string();
    QUrl page = found.empty() ? url : fileUrl(found);
    page.setFragment(url.fragment());

    if (page.adjusted(QUrl::RemoveFragment) != m_page.adjusted(QUrl::RemoveFragment)) {
        m_used.clear();
    }

    m_page = page;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QTextBrowser::doSetSource(page, type);
#else
    QTextBrowser::setSource(page, type);
#endif
    pageShown();
}

bool DRMWebsiteBrowser::event(QEvent *event)
{
    if ((event->type() == QEvent::ToolTip) || (event->type() == QEvent::WhatsThis))
    {
        event->accept();
        return true;
    }

    return QTextBrowser::event(event);
}

bool DRMWebsiteBrowser::viewportEvent(QEvent *event)
{
    if ((event->type() == QEvent::ToolTip) || (event->type() == QEvent::WhatsThis))
    {
        event->accept();
        return true;
    }

    return QTextBrowser::viewportEvent(event);
}

void DRMWebsiteBrowser::useImageHandler()
{
    document()->documentLayout()->registerHandler(QTextFormat::ImageObject, m_images);
}

void DRMWebsiteBrowser::pageShown()
{
    if (source().isValid()) {
        m_page = source();
    }

    useImageHandler();

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next())
    {
        for (QTextBlock::iterator fragment = block.begin(); !fragment.atEnd(); ++fragment)
        {
            const QTextCharFormat format = fragment.fragment().charFormat();

            if (format.isImageFormat()) {
                document()->resource(QTextDocument::ImageResource, QUrl(format.toImageFormat().name()));
            }
        }
    }
}
