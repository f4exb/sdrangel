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

#ifndef SDRGUI_GUI_WEBSITEBROWSER_H_
#define SDRGUI_GUI_WEBSITEBROWSER_H_

#include <map>
#include <memory>
#include <string>

#include <QTextBrowser>
#include <QTextObjectInterface>
#include <QUrl>

#include "modemdr/mot.h"
#include "export.h"

// Draws a website page's images from the document's resources only. Qt's own
// handler first looks for a high-DPI variant of the image's name as a file,
// and loads the name as a file when the resource does not decode: a name
// like //host/share/a.png would reach the network.
class SDRGUI_API WebsiteImages : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    explicit WebsiteImages(QObject *parent = nullptr) : QObject(parent) {}
    QSizeF intrinsicSize(QTextDocument *document, int position, const QTextFormat& format) override;
    void drawObject(QPainter *painter, const QRectF& rect, QTextDocument *document, int position,
        const QTextFormat& format) override;
    static QImage image(QTextDocument *document, const QTextFormat& format);
};

// Shows a MOT Broadcast Website (ETSI TS 101 498), of DAB or DRM: pages, images and style sheets come from the
// files received (as bws:/path URLs), and links between them work. Links
// out of the website are ignored, and nothing is ever read from the local
// file system or the network: images are drawn by WebsiteImages, and no
// text of the page is shown outside it (tooltips, What's This).
class SDRGUI_API WebsiteBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    explicit WebsiteBrowser(QWidget *parent = nullptr);

    // A new snapshot of the website: shows its start page when no page is
    // shown, else the page shown again if a file it uses arrived or changed.
    void setWebsite(std::shared_ptr<const modemdr::MOTWebsite> website);
    void showStartPage();
    // The URL of a file of the website.
    static QUrl fileUrl(const std::string& name);
    QVariant loadResource(int type, const QUrl& name) override;
    // Colours written as bare hex digits (bgcolor=0088b8), which web browsers
    // accept and Qt does not, with their "#"; only within tags.
    static QByteArray fixColours(const QByteArray& html);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    using QTextBrowser::setSource;
#endif

public slots:
    void backward() override;
    void forward() override;
    void home() override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5's hook for a new source (its doSetSource is not virtual)
    void setSource(const QUrl& name) override;
#endif

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void doSetSource(const QUrl& name, QTextDocument::ResourceType type) override;
#endif
    // A title attribute's tooltip would be shown as rich text outside the
    // browser, where Qt reads images as local files: no tooltips
    bool event(QEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    // A URL of the website: bws:, or a receiver's own web server.
    static bool local(const QUrl& url);
    // The name, in the website, of the file a URL names (received or listed),
    // else empty.
    static std::string websiteName(const modemdr::MOTWebsite& website, const QUrl& url);
    QUrl resolved(const QUrl& url) const;
    // What a URL of the website gives: the file's data, else whether it is listed.
    struct Use
    {
        const void *data = nullptr;
        bool listed = false;
        bool operator!=(const Use& other) const { return (data != other.data) || (listed != other.listed); }
    };
    static Use use(const modemdr::MOTWebsite& website, const QUrl& url);
    // HTML text for Qt, decoded here: the character set its BOM or a meta
    // tag in its first 1,024 bytes declares, if Qt has it, else UTF-8 if
    // valid, else Latin-1 (so Qt 5 and Qt 6 show a page alike)
    static QString htmlText(const QByteArray& html);
    // The image handler on the document's layout, whatever Qt registered
    void useImageHandler();
    // A new source: the website's own URL for it, shown
    void showSource(const QUrl& name, QTextDocument::ResourceType type);
    // After a page is shown (a link, a reload or the history): its URL, and its
    // images loaded now (Qt loads them as it paints) so that every file it uses
    // is known even while the tab is hidden
    void pageShown();

    std::shared_ptr<const modemdr::MOTWebsite> m_website;
    WebsiteImages *m_images;
    QUrl m_page;
    std::map<QString, Use> m_used; // the shown page's files, by URL
};

#endif // SDRGUI_GUI_WEBSITEBROWSER_H_
