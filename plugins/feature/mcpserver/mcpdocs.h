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

#ifndef INCLUDE_FEATURE_MCPDOCS_H_
#define INCLUDE_FEATURE_MCPDOCS_H_

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

class PluginInterface;

// Serves the plugin readme files (compiled into this plugin as resources under :/mcpdocs)
// to MCP clients, mapped to the ids of the plugins registered in this instance.
class MCPDocs
{
public:
    struct Doc
    {
        QString kind;         //!< "channel", "device" or "feature" for a plugin; "gui" for a page of the GUI's own, such as the spectrum display
        QString id;           //!< Type id as used by the tools: channelType, device hwType or featureType; a short name for a GUI page
        QStringList aliases;  //!< Other ids the plugin is known by (e.g. the registration id of a device when it differs from the enumerated hwType)
        QString direction;    //!< "rx", "tx" or "mimo" for channels and devices, empty for features
        QString name;         //!< Displayed name from the plugin descriptor
        QString title;        //!< First heading of the readme
        QString path;         //!< Resource path of the readme
        QStringList headings; //!< Section headings, in order, with "#" markers for level
        PluginInterface *plugin;
    };

    MCPDocs();

    const QList<Doc>& list();
    const Doc *find(const QString& type, const QString& kind);
    QString text(const Doc& doc, const QString& section = QString());
    QJsonObject index();
    QString guide(); //!< The curated receiving guide
    QStringList unmatchedReadmes(); //!< Readmes for which no registered plugin was found

    static QString uri(const Doc& doc) { return QString("sdrangel://docs/%1/%2").arg(doc.kind).arg(doc.id); }

private:
    struct Readme
    {
        QString group;   //!< channelrx, samplesource, feature...
        QString dir;     //!< Plugin directory name
        QString path;
        QString title;
        bool used;
    };

    bool m_loaded;
    QList<Doc> m_docs;
    QList<Readme> m_readmes;

    void load();
    void scanReadmes();
    const Readme *match(const QStringList& groups, const QString& id, const QString& uri, const QString& displayedName);
    void addDoc(const QString& kind, const QString& id, const QString& direction, const QString& uri, const QString& displayedName, const QStringList& groups, PluginInterface *plugin);
    void addGuiDocs(); //!< The GUI's own pages, which no plugin owns
    void applyEnumeratedDeviceIds();
    static QString normalize(const QString& s);
    static QString readFile(const QString& path);
    static QString toMarkdown(const QString& raw);
    static QStringList extractHeadings(const QString& markdown);
    static QString headingText(const QString& line, int& level);
};

#endif // INCLUDE_FEATURE_MCPDOCS_H_
