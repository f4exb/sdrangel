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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>

#include "maincore.h"
#include "device/deviceenumerator.h"
#include "plugin/pluginmanager.h"
#include "plugin/plugininterface.h"

#include "mcpdocs.h"

MCPDocs::MCPDocs() :
    m_loaded(false)
{
}

const QList<MCPDocs::Doc>& MCPDocs::list()
{
    load();
    return m_docs;
}

const MCPDocs::Doc *MCPDocs::find(const QString& typeIn, const QString& kindIn)
{
    load();
    QString type = normalize(typeIn);
    QString kind = kindIn.trimmed().toLower();

    // Exact id first, then displayed name, then readme title, so that "ADS-B" or "RTL-SDR" also resolve
    for (int pass = 0; pass < 3; pass++)
    {
        for (const Doc& doc : m_docs)
        {
            if (!kind.isEmpty() && (doc.kind != kind)) {
                continue;
            }

            bool hit;

            if (pass == 0)
            {
                hit = normalize(doc.id) == type;

                for (const QString& alias : doc.aliases) {
                    hit = hit || (normalize(alias) == type);
                }
            }
            else if (pass == 1)
            {
                hit = normalize(doc.name) == type;
            }
            else
            {
                hit = normalize(doc.title).startsWith(type);
            }

            if (hit) {
                return &doc;
            }
        }
    }

    return nullptr;
}

QString MCPDocs::text(const Doc& doc, const QString& section)
{
    QString markdown = toMarkdown(readFile(doc.path));

    if (section.trimmed().isEmpty()) {
        return markdown;
    }

    // Return the section whose heading contains the requested text, up to the next heading
    // of the same or a higher level
    QString wanted = section.trimmed().toLower();
    QStringList lines = markdown.split('\n');
    int start = -1;
    int startLevel = 0;

    for (int i = 0; i < lines.size(); i++)
    {
        int level;
        QString heading = headingText(lines[i], level);

        if (level == 0) {
            continue;
        }

        if (start < 0)
        {
            if (heading.toLower().contains(wanted))
            {
                start = i;
                startLevel = level;
            }
        }
        else if (level <= startLevel)
        {
            return lines.mid(start, i - start).join('\n').trimmed();
        }
    }

    if (start >= 0) {
        return lines.mid(start).join('\n').trimmed();
    }

    return QString();
}

QJsonObject MCPDocs::index()
{
    load();
    QJsonArray docs;

    for (const Doc& doc : m_docs)
    {
        QJsonObject d;
        d["kind"] = doc.kind;
        d["id"] = doc.id;

        if (!doc.aliases.isEmpty()) {
            d["aliases"] = QJsonArray::fromStringList(doc.aliases);
        }

        if (!doc.direction.isEmpty()) {
            d["direction"] = doc.direction;
        }

        d["name"] = doc.name;
        d["title"] = doc.title;
        d["uri"] = uri(doc);
        d["headings"] = QJsonArray::fromStringList(doc.headings);
        docs.append(d);
    }

    QJsonObject result;
    result["count"] = docs.count();
    result["docs"] = docs;
    return result;
}

// Hand written guide compiled in beside the plugin readmes
QString MCPDocs::guide()
{
    QString text = readFile(":/mcpdocs/bandguide.md");

    if (text.isEmpty()) {
        return QString("The receiving guide is not available in this build.");
    }

    return text;
}

QStringList MCPDocs::unmatchedReadmes()
{
    load();
    QStringList result;

    for (const Readme& readme : m_readmes)
    {
        if (!readme.used) {
            result.append(readme.group + "/" + readme.dir);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------

void MCPDocs::load()
{
    if (m_loaded) {
        return;
    }

    m_loaded = true;
    scanReadmes();

    PluginManager *pluginManager = MainCore::instance()->getPluginManager();

    if (!pluginManager) {
        return;
    }

    for (const auto& reg : *pluginManager->getRxChannelRegistrations()) {
        addDoc("channel", reg.m_channelId, "rx", reg.m_channelIdURI, reg.m_plugin->getPluginDescriptor().displayedName, {"channelrx"}, reg.m_plugin);
    }
    for (const auto& reg : *pluginManager->getTxChannelRegistrations()) {
        addDoc("channel", reg.m_channelId, "tx", reg.m_channelIdURI, reg.m_plugin->getPluginDescriptor().displayedName, {"channeltx"}, reg.m_plugin);
    }
    for (const auto& reg : *pluginManager->getMIMOChannelRegistrations()) {
        addDoc("channel", reg.m_channelId, "mimo", reg.m_channelIdURI, reg.m_plugin->getPluginDescriptor().displayedName, {"channelmimo"}, reg.m_plugin);
    }
    for (const auto& reg : pluginManager->getSourceDeviceRegistrations()) {
        addDoc("device", reg.m_deviceHardwareId, "rx", reg.m_deviceId, reg.m_plugin->getPluginDescriptor().displayedName, {"samplesource"}, reg.m_plugin);
    }
    for (const auto& reg : pluginManager->getSinkDeviceRegistrations()) {
        addDoc("device", reg.m_deviceHardwareId, "tx", reg.m_deviceId, reg.m_plugin->getPluginDescriptor().displayedName, {"samplesink"}, reg.m_plugin);
    }
    for (const auto& reg : pluginManager->getMIMODeviceRegistrations()) {
        addDoc("device", reg.m_deviceHardwareId, "mimo", reg.m_deviceId, reg.m_plugin->getPluginDescriptor().displayedName, {"samplemimo"}, reg.m_plugin);
    }
    for (const auto& reg : *pluginManager->getFeatureRegistrations()) {
        addDoc("feature", reg.m_featureId, "", reg.m_featureIdURI, reg.m_plugin->getPluginDescriptor().displayedName, {"feature"}, reg.m_plugin);
    }

    applyEnumeratedDeviceIds();
    addGuiDocs();

    qDebug("MCPDocs::load: %d readmes, %d matched to plugins, unmatched: %s",
        (int) m_readmes.size(), (int) m_docs.size(), qPrintable(unmatchedReadmes().join(", ")));
}

// Pages of the GUI's own, compiled in under :/mcpdocs/gui by the CMake list beside this one.
// The spectrum display is the one that matters most: its settings are the only ones a client
// changes that no plugin readme explains
void MCPDocs::addGuiDocs()
{
    struct Page { const char *id; const char *name; const char *file; QStringList aliases; };

    static const QList<Page> pages = {
        {"spectrum", "Spectrum display", "spectrum.md", {"GLSpectrum", "spectrum display", "spectrum component"}},
        {"spectrummarkers", "Spectrum markers", "spectrummarkers.md", {"markers"}},
        {"spectrummeasurements", "Spectrum measurements", "spectrummeasurements.md", {"measurements"}},
        {"spectrumcalibration", "Spectrum calibration", "spectrumcalibration.md", {"calibration"}},
        {"mainspectrum", "Main spectrum window", "mainspectrum.md", {}},
        {"audio", "Audio management", "audio.md", {}},
        {"configurations", "Configurations", "configurations.md", {}},
        {"deviceuserargs", "Device user arguments", "deviceuserargs.md", {}},
        {"transverter", "Transverter dialog", "transverterdialog.md", {}}
    };

    for (const Page& page : pages)
    {
        QString path = QString(":/mcpdocs/gui/%1").arg(page.file);

        if (!QFile::exists(path)) {
            continue; // not in this build
        }

        QString markdown = toMarkdown(readFile(path));
        Doc doc;
        doc.kind = "gui";
        doc.id = page.id;
        doc.aliases = page.aliases;
        doc.name = page.name;
        doc.title = page.name;
        doc.path = path;
        doc.headings = extractHeadings(markdown);
        doc.plugin = nullptr;

        // The page's own first heading, when it has one
        for (const QString& line : markdown.split('\n'))
        {
            int level;
            QString heading = headingText(line, level);

            if (level > 0)
            {
                doc.title = heading;
                break;
            }
        }

        m_docs.append(doc);
    }
}

void MCPDocs::scanReadmes()
{
    QDir root(":/mcpdocs");

    for (const QString& group : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QDir groupDir(root.filePath(group));

        for (const QString& dir : groupDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            QString path = groupDir.filePath(dir) + "/readme.md";

            if (!QFile::exists(path)) {
                continue;
            }

            Readme readme;
            readme.group = group;
            readme.dir = dir;
            readme.path = path;
            readme.used = false;

            // Title is the first heading
            for (const QString& line : readFile(path).split('\n'))
            {
                int level;
                QString heading = headingText(line, level);

                if (level > 0)
                {
                    readme.title = heading;
                    break;
                }
            }

            m_readmes.append(readme);
        }
    }
}

// The hardware id a device plugin registers with can differ from the one it enumerates devices under,
// and the enumerated one is what list_available_devices, set_device and describe_settings use.
// Make that the canonical id and keep the registration id as an alias.
void MCPDocs::applyEnumeratedDeviceIds()
{
    DeviceEnumerator *enumerator = DeviceEnumerator::instance();

    auto apply = [this](const QString& direction, const PluginInterface::SamplingDevice *device, PluginInterface *plugin)
    {
        for (Doc& doc : m_docs)
        {
            if ((doc.kind != "device") || (doc.direction != direction) || (doc.plugin != plugin)) {
                continue;
            }

            if (doc.id != device->hardwareId)
            {
                if (!doc.aliases.contains(doc.id)) {
                    doc.aliases.append(doc.id);
                }

                doc.aliases.removeAll(device->hardwareId);
                doc.id = device->hardwareId;
            }
        }
    };

    for (int i = 0; i < enumerator->getNbRxSamplingDevices(); i++) {
        apply("rx", enumerator->getRxSamplingDevice(i), enumerator->getRxPluginInterface(i));
    }
    for (int i = 0; i < enumerator->getNbTxSamplingDevices(); i++) {
        apply("tx", enumerator->getTxSamplingDevice(i), enumerator->getTxPluginInterface(i));
    }
    for (int i = 0; i < enumerator->getNbMIMOSamplingDevices(); i++) {
        apply("mimo", enumerator->getMIMOSamplingDevice(i), enumerator->getMIMOPluginInterface(i));
    }
}

void MCPDocs::addDoc(const QString& kind, const QString& id, const QString& direction, const QString& uri,
    const QString& displayedName, const QStringList& groups, PluginInterface *plugin)
{
    // Several device plugins register the same hardware id (e.g. for different device instances)
    for (const Doc& existing : m_docs)
    {
        if ((existing.kind == kind) && (existing.id == id) && (existing.direction == direction)) {
            return;
        }
    }

    const Readme *readme = match(groups, id, uri, displayedName);

    if (!readme)
    {
        qDebug("MCPDocs::addDoc: no readme for %s %s (%s)", qPrintable(kind), qPrintable(id), qPrintable(displayedName));
        return;
    }

    Doc doc;
    doc.kind = kind;
    doc.id = id;
    doc.direction = direction;
    doc.name = displayedName;
    doc.title = readme->title;
    doc.path = readme->path;
    doc.headings = extractHeadings(toMarkdown(readFile(readme->path)));
    doc.plugin = plugin;
    m_docs.append(doc);
}

// Directory names and ids differ (demodadsb vs ADSBDemod, rtlsdr vs RTLSDR, chanalyzer vs ChannelAnalyzer),
// so compare normalized forms with the demod/mod affixes removed, then fall back to the readme title.
const MCPDocs::Readme *MCPDocs::match(const QStringList& groups, const QString& id, const QString& uri, const QString& displayedName)
{
    // Plugins whose directory, id and title all differ
    static const QMap<QString, QString> overrides = {
        {"ChannelAnalyzer", "chanalyzer"},
        {"IEEE_802_15_4_Mod", "mod802.15.4"},
    };

    QStringList idCandidates;
    QString normId = normalize(id);
    idCandidates.append(normId);
    idCandidates.append(normalize(uri.section('.', -1)));

    if (overrides.contains(id)) {
        idCandidates.prepend(normalize(overrides[id]));
    }

    // Device plugins share a hardware id between input and output and live in
    // "<id>input" / "<id>output" directories
    idCandidates.append(normId + "input");
    idCandidates.append(normId + "output");

    for (const QString affix : {"demod", "mod"})
    {
        if (normId.endsWith(affix) && (normId.size() > affix.size())) {
            idCandidates.append(normId.left(normId.size() - affix.size()));
        }
    }

    auto dirCandidates = [](const QString& dir)
    {
        QStringList result;
        QString normDir = normalize(dir);
        result.append(normDir);

        for (const QString affix : {"demod", "mod"})
        {
            if (normDir.startsWith(affix) && (normDir.size() > affix.size())) {
                result.append(normDir.mid(affix.size()));
            }
        }

        return result;
    };

    // Pass 1: names
    for (Readme& readme : m_readmes)
    {
        if (!groups.contains(readme.group)) {
            continue;
        }

        for (const QString& d : dirCandidates(readme.dir))
        {
            if (idCandidates.contains(d))
            {
                readme.used = true;
                return &readme;
            }
        }
    }

    // Pass 2: displayed name against the readme title, e.g. "ADS-B Demodulator" and "ADS-B demodulator plugin"
    QString normName = normalize(displayedName);

    if (!normName.isEmpty())
    {
        for (Readme& readme : m_readmes)
        {
            if (!groups.contains(readme.group)) {
                continue;
            }

            QString normTitle = normalize(readme.title);

            if (normTitle.startsWith(normName) || normName.startsWith(normTitle))
            {
                readme.used = true;
                return &readme;
            }
        }
    }

    return nullptr;
}

QString MCPDocs::normalize(const QString& s)
{
    QString result;

    for (const QChar& c : s.toLower())
    {
        if (c.isLetterOrNumber()) {
            result.append(c);
        }
    }

    return result;
}

QString MCPDocs::readFile(const QString& path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

// Readmes mix HTML headings with markdown. Normalize headings to markdown and drop images,
// which cannot be served to the model.
QString MCPDocs::toMarkdown(const QString& raw)
{
    static const QRegularExpression htmlHeading("^\\s*<h([1-6])>(.*?)</h\\1>\\s*$");
    static const QRegularExpression image("^\\s*!\\[[^\\]]*\\]\\([^)]*\\)\\s*$");
    static const QRegularExpression htmlImage("^\\s*<img\\b[^>]*>\\s*$", QRegularExpression::CaseInsensitiveOption);
    QStringList out;

    for (const QString& line : raw.split('\n'))
    {
        QRegularExpressionMatch m = htmlHeading.match(line);

        if (m.hasMatch())
        {
            out.append(QString(m.captured(1).toInt(), '#') + " " + m.captured(2).trimmed());
            continue;
        }

        if (image.match(line).hasMatch() || htmlImage.match(line).hasMatch()) {
            continue;
        }

        out.append(line.endsWith('\r') ? line.chopped(1) : line);
    }

    return out.join('\n');
}

// Returns the heading text of a markdown heading line and its level, or level 0 for other lines
QString MCPDocs::headingText(const QString& line, int& level)
{
    static const QRegularExpression mdHeading("^(#{1,6})\\s+(.*?)\\s*#*\\s*$");
    static const QRegularExpression htmlHeading("^\\s*<h([1-6])>(.*?)</h\\1>\\s*$");
    QRegularExpressionMatch m = mdHeading.match(line);

    if (m.hasMatch())
    {
        level = m.captured(1).size();
        return m.captured(2);
    }

    m = htmlHeading.match(line);

    if (m.hasMatch())
    {
        level = m.captured(1).toInt();
        return m.captured(2).trimmed();
    }

    level = 0;
    return QString();
}

QStringList MCPDocs::extractHeadings(const QString& markdown)
{
    QStringList headings;

    for (const QString& line : markdown.split('\n'))
    {
        int level;
        QString heading = headingText(line, level);

        if (level > 0) {
            headings.append(QString(level, '#') + " " + heading);
        }
    }

    return headings;
}
