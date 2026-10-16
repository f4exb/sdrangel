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

#include <QMutexLocker>

#include "dtv/dtvapps.h"

namespace {

// PMT stream types
constexpr int m_streamPrivateSections = 0x05;   //!< As the AIT's
constexpr int m_streamDsmccB = 0x0B;            //!< DSM-CC U-N messages: object carousels
constexpr int m_streamDsmccD = 0x0D;            //!< DSM-CC, any type

// Descriptors
constexpr int m_carouselIdentifierDescriptor = 0x13;
constexpr int m_associationTagDescriptor = 0x14;
constexpr int m_streamIdentifierDescriptor = 0x52;
constexpr int m_dataBroadcastIdDescriptor = 0x66;
constexpr int m_applicationSignallingDescriptor = 0x6F;

// AIT descriptors (TS 102 809 5.3.5, 5.3.6)
constexpr int m_applicationDescriptor = 0x00;
constexpr int m_applicationNameDescriptor = 0x01;
constexpr int m_transportProtocolDescriptor = 0x02;
constexpr int m_simpleApplicationLocationDescriptor = 0x15;
constexpr int m_simpleApplicationBoundaryDescriptor = 0x17;

constexpr int m_protocolObjectCarousel = 0x0001;
constexpr int m_protocolHTTP = 0x0003;

bool isDsmcc(int streamType)
{
    return (streamType == m_streamDsmccB) || (streamType == m_streamDsmccD);
}

// An application's transport: by its transport_protocol_descriptor
struct Transport
{
    int m_protocol = 0;
    int m_label = 0;
    QStringList m_urlBases;
    int m_componentTag = -1;
    int m_remoteNetworkId = -1;
    int m_remoteTransportStreamId = -1;
    int m_remoteServiceId = -1;
};

Transport transportProtocol(const uint8_t *d, int length)
{
    Transport transport;

    if (length < 3) {
        return transport;
    }

    transport.m_protocol = (d[0] << 8) | d[1];
    transport.m_label = d[2];
    int i = 3;

    if (transport.m_protocol == m_protocolObjectCarousel)
    {
        if (i >= length) {
            return transport;
        }

        const bool remote = d[i] & 0x80;
        i++;

        if (remote)
        {
            if (i + 6 > length) {
                return transport;
            }

            transport.m_remoteNetworkId = (d[i] << 8) | d[i + 1];
            transport.m_remoteTransportStreamId = (d[i + 2] << 8) | d[i + 3];
            transport.m_remoteServiceId = (d[i + 4] << 8) | d[i + 5];
            i += 6;
        }

        if (i < length) {
            transport.m_componentTag = d[i];
        }
    }
    else if (transport.m_protocol == m_protocolHTTP)
    {
        // URL bases, each with its extensions (which aren't needed to start the application)
        while (i < length)
        {
            const int baseLength = d[i];

            if (i + 1 + baseLength > length) {
                break;
            }

            transport.m_urlBases.append(QString::fromLatin1((const char *) d + i + 1, baseLength));
            i += 1 + baseLength;

            if (i >= length) {
                break;
            }

            const int extensions = d[i];
            i++;

            for (int e = 0; (e < extensions) && (i < length); e++) {
                i += 1 + d[i];
            }
        }
    }

    return transport;
}

void applyTransport(DTVApps::App& app, const Transport& transport)
{
    if (transport.m_protocol == m_protocolHTTP)
    {
        for (const QString& base : transport.m_urlBases)
        {
            if (!app.m_urlBases.contains(base)) {
                app.m_urlBases.append(base);
            }
        }
    }
    else if ((transport.m_protocol == m_protocolObjectCarousel) && (app.m_componentTag < 0))
    {
        app.m_componentTag = transport.m_componentTag;
        app.m_remoteNetworkId = transport.m_remoteNetworkId;
        app.m_remoteTransportStreamId = transport.m_remoteTransportStreamId;
        app.m_remoteServiceId = transport.m_remoteServiceId;
    }
}

} // namespace

QString DTVApps::App::url(int originalNetworkId, int transportStreamId) const
{
    QString location = m_location;

    if (!m_urlBases.isEmpty())
    {
        const QString& base = m_urlBases.first();

        if (base.endsWith('/') && location.startsWith('/')) {
            location = location.mid(1);
        }

        return base + location;
    }

    if (m_componentTag >= 0)
    {
        // dvb://original_network_id.transport_stream_id.service_id.component_tag/path, in hex
        const bool remote = m_remoteServiceId >= 0;
        const int networkId = remote ? m_remoteNetworkId : originalNetworkId;
        const int tsId = remote ? m_remoteTransportStreamId : transportStreamId;
        const int serviceId = remote ? m_remoteServiceId : m_serviceId;

        if (location.startsWith('/')) {
            location = location.mid(1);
        }

        return QString("dvb://%1.%2.%3.%4/%5")
            .arg(networkId >= 0 ? QString::number(networkId, 16) : QString())
            .arg(tsId >= 0 ? QString::number(tsId, 16) : QString())
            .arg(serviceId, 0, 16)
            .arg(m_componentTag, 0, 16)
            .arg(location);
    }

    return location;
}

QString DTVApps::App::controlCodeName() const
{
    switch (m_controlCode)
    {
    case 1: return "AUTOSTART";
    case 2: return "PRESENT";
    case 3: return "DESTROY";
    case 4: return "KILL";
    case 5: return "PREFETCH";
    case 6: return "REMOTE";
    case 7: return "DISABLED";
    case 8: return "PLAYBACK_AUTOSTART";
    default: return QString::number(m_controlCode);
    }
}

QString DTVApps::App::typeName() const
{
    switch (m_applicationType)
    {
    case 0x0001: return "DVB-J";
    case 0x0002: return "DVB-HTML";
    case m_hbbtvApplicationType: return "HbbTV";
    default: return QString("0x%1").arg(m_applicationType, 4, 16, QChar('0'));
    }
}

QString DTVApps::App::idText() const
{
    return QString("%1.%2").arg(m_organisationId, 0, 16).arg(m_applicationId, 0, 16);
}

QString DTVApps::Carousel::kindName() const
{
    QStringList kinds;

    if (m_dataBroadcastId == m_mhegDataBroadcastId) {
        kinds.append("MHEG-5");
    }

    if ((m_dataBroadcastId == m_hbbtvDataBroadcastId) || m_hbbtvApp) {
        kinds.append("HbbTV");
    }

    return kinds.isEmpty() ? QString("DSM-CC") : kinds.join(", ");
}

DTVApps::DTVApps() :
    m_wanted(8192, false),
    m_transportStreamId(-1),
    m_originalNetworkId(-1),
    m_carouselService(0),
    m_carouselServiceApplied(0),
    m_revision(0)
{
    updatePids();
}

DTVApps::~DTVApps()
{
}

void DTVApps::reset()
{
    QMutexLocker locker(&m_mutex);
    m_assemblers.clear();
    m_transportStreamId = -1;
    m_originalNetworkId = -1;
    m_programs.clear();
    m_services.clear();
    m_aits.clear();
    m_carousels.clear();
    m_carouselServices.clear();
    m_carouselPids.clear();
    updatePids();
    m_revision++;
}

void DTVApps::setCarouselService(int serviceId)
{
    m_carouselService = serviceId;
}

void DTVApps::feed(const uint8_t *p)
{
    if ((p[0] != 0x47) || (p[1] & 0x80)) { // Lost sync, or uncorrectable errors
        return;
    }

    const int pid = ((p[1] & 0x1F) << 8) | p[2];
    QMutexLocker locker(&m_mutex);  // Uncontended but for reads, which are infrequent

    if (m_carouselService != m_carouselServiceApplied) {
        updateCarousels();
    }

    if (m_wanted[pid]) {
        packet(pid, p);
    }
}

void DTVApps::packet(int pid, const uint8_t *p)
{
    std::unique_ptr<DTVSectionAssembler>& assembler = m_assemblers[pid];

    if (!assembler) {
        assembler.reset(new DTVSectionAssembler());
    }

    assembler->packet(p, [this, pid](const uint8_t *s, int length) {
        // The long form, with a CRC; current, not a table to apply next
        if ((length >= 12) && (s[1] & 0x80) && (s[5] & 0x01)) {
            section(pid, s, length);
        }
    });
}

void DTVApps::section(int pid, const uint8_t *s, int length)
{
    const int table = s[0];

    if ((pid == m_patPid) && (table == m_patTable))
    {
        pat(s, length);
    }
    else if ((pid == m_sdtPid) && (table == m_sdtActual))
    {
        const int originalNetworkId = (s[8] << 8) | s[9];

        if (originalNetworkId != m_originalNetworkId)
        {
            m_originalNetworkId = originalNetworkId;
            m_revision++;
        }
    }
    else if (table == m_pmtTable)
    {
        pmt(pid, s, length);
    }
    else if (table == m_aitTable)
    {
        ait(pid, s, length);
    }
    else if ((table == m_dsmccInfoTable) || (table == m_dsmccDataTable))
    {
        // A DSI on a carousel stream of a service whose carousels are received starts a carousel. The DSI's
        // message id (0x1006) follows the section header and the protocol discriminator and type
        if ((table == m_dsmccInfoTable) && (m_carousels.count(pid) == 0) && (length >= 12)
            && (s[10] == 0x10) && (s[11] == 0x06))
        {
            for (const auto& service : m_services)
            {
                if (!carouselWanted(service.first)) {
                    continue;
                }

                for (const Stream& stream : service.second.m_streams)
                {
                    if ((stream.m_pid == pid) && isDsmcc(stream.m_type) && (m_carousels.count(pid) == 0))
                    {
                        m_carousels[pid].reset(new DTVCarousel(pid));
                        m_carouselServices[pid] = service.first;
                        m_carouselPids[pid].insert(pid);
                    }
                }
            }
        }

        auto fed = m_carouselPids.find(pid);

        if (fed == m_carouselPids.end()) {
            return;
        }

        bool tagsChanged = false;

        for (int carouselPid : fed->second)
        {
            DTVCarousel *carousel = m_carousels[carouselPid].get();
            const size_t tags = carousel->getAssociationTags().size();

            if (carousel->section(s, length)) {
                m_revision++;
            }

            tagsChanged |= carousel->getAssociationTags().size() != tags;
        }

        if (tagsChanged) {
            updateCarousels();  // Its DIIs or DDBs may be on other PIDs
        }
    }
}

// program_association_section: the transport stream id, and the PMTs' PIDs
void DTVApps::pat(const uint8_t *s, int length)
{
    const int transportStreamId = (s[3] << 8) | s[4];

    if ((m_transportStreamId >= 0) && (transportStreamId != m_transportStreamId))
    {
        // Another transport stream
        m_originalNetworkId = -1;
        m_programs.clear();     // So the PIDs read are those of the new one's
        m_services.clear();
        m_aits.clear();
        m_carousels.clear();
        m_carouselServices.clear();
        m_carouselPids.clear();
        m_revision++;
    }

    m_transportStreamId = transportStreamId;
    std::map<int, int> programs;

    for (int i = 8; i + 4 <= length - 4; i += 4)
    {
        const int program = (s[i] << 8) | s[i + 1];
        const int pid = ((s[i + 2] & 0x1F) << 8) | s[i + 3];

        if (program != 0) { // Not the NIT
            programs[program] = pid;
        }
    }

    if (programs != m_programs)
    {
        m_programs = programs;

        for (auto it = m_services.begin(); it != m_services.end();)
        {
            auto program = m_programs.find(it->first);

            if ((program == m_programs.end()) || (program->second != it->second.m_pmtPid)) {
                it = m_services.erase(it);
            } else {
                ++it;
            }
        }

        updateCarousels();
        m_revision++;
    }
}

// TS_program_map_section: the streams of a service, with the AIT's and the carousels'
void DTVApps::pmt(int pid, const uint8_t *s, int length)
{
    const int program = (s[3] << 8) | s[4];
    auto it = m_programs.find(program);

    if ((it == m_programs.end()) || (it->second != pid)) {
        return;
    }

    const int version = (s[5] >> 1) & 0x1F;
    Service& service = m_services[program];

    if ((service.m_pmtPid == pid) && (service.m_pmtVersion == version)) {
        return;
    }

    service.m_pmtPid = pid;
    service.m_pmtVersion = version;
    service.m_streams.clear();
    const int end = length - 4;
    int i = 12 + (((s[10] & 0x0F) << 8) | s[11]);

    while (i + 5 <= end)
    {
        Stream stream;
        stream.m_type = s[i];
        stream.m_pid = ((s[i + 1] & 0x1F) << 8) | s[i + 2];
        const int descriptorsEnd = std::min(end, i + 5 + (((s[i + 3] & 0x0F) << 8) | s[i + 4]));
        i += 5;

        while (i + 2 <= descriptorsEnd)
        {
            const int tag = s[i];
            const int dl = std::min((int) s[i + 1], descriptorsEnd - i - 2);
            const uint8_t *d = s + i + 2;

            if ((tag == m_streamIdentifierDescriptor) && (dl >= 1)) {
                stream.m_componentTag = d[0];
            } else if ((tag == m_associationTagDescriptor) && (dl >= 2)) {
                stream.m_associationTags.push_back((d[0] << 8) | d[1]);
            } else if ((tag == m_carouselIdentifierDescriptor) && (dl >= 4)) {
                stream.m_carouselId = ((uint32_t) d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
            } else if ((tag == m_dataBroadcastIdDescriptor) && (dl >= 2)) {
                stream.m_dataBroadcastId = (d[0] << 8) | d[1];
            } else if (tag == m_applicationSignallingDescriptor) {
                stream.m_ait = true;
            }

            i += 2 + s[i + 1];
        }

        i = descriptorsEnd;

        // An AIT's stream is of private sections, with the application signalling descriptor (TS 102 809 5.3.5.1)
        if (stream.m_ait && (stream.m_type != m_streamPrivateSections)) {
            stream.m_ait = false;
        }

        service.m_streams.push_back(stream);
    }

    updateCarousels();
    m_revision++;
}

// application_information_section (TS 102 809 5.3.4): the applications, each with its descriptors, and descriptors
// common to them (as transports applications refer to by label)
void DTVApps::ait(int pid, const uint8_t *s, int length)
{
    const int applicationType = ((s[3] << 8) | s[4]) & 0x7FFF;   // After the test_application_flag
    const int version = (s[5] >> 1) & 0x1F;
    const int sectionNumber = s[6];
    AIT& table = m_aits[{pid, applicationType}];

    if (table.m_version != version)
    {
        table.m_version = version;
        table.m_sections.clear();
    }
    else if (table.m_sections.count(sectionNumber) != 0)
    {
        return;
    }

    const int end = length - 4;
    int i = 8;
    const int commonEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;
    std::map<int, Transport> commonTransports;

    for (; i + 2 <= commonEnd; i += 2 + s[i + 1])
    {
        if ((s[i] == m_transportProtocolDescriptor) && (i + 2 + s[i + 1] <= commonEnd))
        {
            const Transport transport = transportProtocol(s + i + 2, s[i + 1]);
            commonTransports[transport.m_label] = transport;
        }
    }

    i = commonEnd;

    if (i + 2 > end) {
        return;
    }

    const int loopEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;
    std::vector<App> apps;

    while (i + 9 <= loopEnd)
    {
        App app;
        app.m_organisationId = ((uint32_t) s[i] << 24) | (s[i + 1] << 16) | (s[i + 2] << 8) | s[i + 3];
        app.m_applicationId = (s[i + 4] << 8) | s[i + 5];
        app.m_applicationType = applicationType;
        app.m_controlCode = s[i + 6];
        const int descriptorsEnd = std::min(loopEnd, i + 9 + (((s[i + 7] & 0x0F) << 8) | s[i + 8]));
        i += 9;
        std::vector<int> labels;

        for (; i + 2 <= descriptorsEnd; i += 2 + s[i + 1])
        {
            const int tag = s[i];
            const int dl = s[i + 1];
            const uint8_t *d = s + i + 2;

            if (i + 2 + dl > descriptorsEnd) {
                break;
            }

            if (tag == m_applicationDescriptor)
            {
                // Profiles, then service_bound_flag, visibility, priority and transport protocol labels
                const int profilesLength = d[0];

                if (1 + profilesLength + 2 > dl) {
                    continue;
                }

                for (int p = 1; p + 5 <= 1 + profilesLength; p += 5) {
                    app.m_profiles.push_back((((d[p] << 8) | d[p + 1]) << 24) | (d[p + 2] << 16) | (d[p + 3] << 8) | d[p + 4]);
                }

                const int flags = d[1 + profilesLength];
                app.m_serviceBound = flags & 0x80;
                app.m_visibility = (flags >> 5) & 3;
                app.m_priority = d[2 + profilesLength];

                for (int l = 3 + profilesLength; l < dl; l++) {
                    labels.push_back(d[l]);
                }
            }
            else if ((tag == m_applicationNameDescriptor) && (dl >= 4))
            {
                // The first language's name
                const int nameLength = std::min((int) d[3], dl - 4);
                app.m_language = QString::fromLatin1((const char *) d, 3);
                app.m_name = dvbText(d + 4, nameLength);
            }
            else if (tag == m_transportProtocolDescriptor)
            {
                applyTransport(app, transportProtocol(d, dl));
            }
            else if (tag == m_simpleApplicationLocationDescriptor)
            {
                app.m_location = QString::fromLatin1((const char *) d, dl);
            }
            else if ((tag == m_simpleApplicationBoundaryDescriptor) && (dl >= 1))
            {
                for (int b = 1, n = 0; (n < d[0]) && (b < dl); n++)
                {
                    const int boundaryLength = std::min((int) d[b], dl - b - 1);
                    app.m_boundaries.append(QString::fromLatin1((const char *) d + b + 1, boundaryLength));
                    b += 1 + d[b];
                }
            }
        }

        i = descriptorsEnd;

        for (int label : labels)
        {
            auto transport = commonTransports.find(label);

            if (transport != commonTransports.end()) {
                applyTransport(app, transport->second);
            }
        }

        apps.push_back(app);
    }

    table.m_sections[sectionNumber] = apps;
    m_revision++;
}

bool DTVApps::carouselWanted(int serviceId) const
{
    return (m_carouselServiceApplied == -1) || ((m_carouselServiceApplied != 0) && (m_carouselServiceApplied == serviceId));
}

// The PID of a service's stream with an association tag (that of an association_tag_descriptor, or in DVB usually its
// component tag, TR 101 202 4.7.7.1), -1 if none has it
int DTVApps::resolveTag(int serviceId, int tag) const
{
    auto service = m_services.find(serviceId);

    if (service == m_services.end()) {
        return -1;
    }

    for (const Stream& stream : service->second.m_streams)
    {
        if (std::find(stream.m_associationTags.begin(), stream.m_associationTags.end(), tag) != stream.m_associationTags.end()) {
            return stream.m_pid;
        }
    }

    for (const Stream& stream : service->second.m_streams)
    {
        if ((stream.m_componentTag >= 0) && (stream.m_componentTag == (tag & 0xFF))) {
            return stream.m_pid;
        }
    }

    return -1;
}

// The carousels of the services chosen are kept, those of others dropped, and each is fed from the PIDs its taps
// point to too
void DTVApps::updateCarousels()
{
    m_carouselServiceApplied = m_carouselService;

    for (auto it = m_carousels.begin(); it != m_carousels.end();)
    {
        const int pid = it->first;
        const int serviceId = m_carouselServices[pid];
        bool keep = false;
        auto service = m_services.find(serviceId);

        if (carouselWanted(serviceId) && (service != m_services.end()))
        {
            for (const Stream& stream : service->second.m_streams) {
                keep |= (stream.m_pid == pid) && isDsmcc(stream.m_type);
            }
        }

        if (keep)
        {
            ++it;
        }
        else
        {
            m_carouselServices.erase(pid);
            it = m_carousels.erase(it);
            m_revision++;
        }
    }

    m_carouselPids.clear();

    for (const auto& carousel : m_carousels)
    {
        m_carouselPids[carousel.first].insert(carousel.first);

        for (int tag : carousel.second->getAssociationTags())
        {
            const int pid = resolveTag(m_carouselServices[carousel.first], tag);

            if (pid >= 0) {
                m_carouselPids[pid].insert(carousel.first);
            }
        }
    }

    updatePids();
}

void DTVApps::updatePids()
{
    std::vector<bool> wanted(8192, false);
    wanted[m_patPid] = true;
    wanted[m_sdtPid] = true;

    for (const auto& program : m_programs) {
        wanted[program.second] = true;
    }

    for (const auto& service : m_services)
    {
        const bool carousels = carouselWanted(service.first);

        for (const Stream& stream : service.second.m_streams)
        {
            // Every service's AIT; carousels of those chosen (to find their DSIs)
            if (stream.m_ait || (carousels && isDsmcc(stream.m_type))) {
                wanted[stream.m_pid] = true;
            }
        }
    }

    for (const auto& pid : m_carouselPids) {
        wanted[pid.first] = true;
    }

    for (auto it = m_assemblers.begin(); it != m_assemblers.end();)
    {
        if (!wanted[it->first]) {
            it = m_assemblers.erase(it);
        } else {
            ++it;
        }
    }

    m_wanted = wanted;
}

QString DTVApps::dvbText(const uint8_t *text, int length)
{
    // EN 300 468 Annex A: an optional character table first. Names are usually ASCII
    if ((length >= 1) && (text[0] == 0x15)) {
        return QString::fromUtf8((const char *) text + 1, length - 1);
    } else if ((length >= 3) && (text[0] == 0x10)) {
        return QString::fromLatin1((const char *) text + 3, length - 3);
    } else if ((length >= 1) && (text[0] < 0x20)) {
        return QString::fromLatin1((const char *) text + 1, length - 1);
    } else {
        return QString::fromLatin1((const char *) text, length);
    }
}

int DTVApps::getTransportStreamId() const
{
    QMutexLocker locker(&m_mutex);
    return m_transportStreamId;
}

int DTVApps::getOriginalNetworkId() const
{
    QMutexLocker locker(&m_mutex);
    return m_originalNetworkId;
}

std::vector<DTVApps::App> DTVApps::getApps(int serviceId) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<App> apps;

    for (const auto& service : m_services)
    {
        if ((serviceId != 0) && (service.first != serviceId)) {
            continue;
        }

        std::set<std::pair<uint32_t, int>> seen;

        for (const Stream& stream : service.second.m_streams)
        {
            if (!stream.m_ait) {
                continue;
            }

            for (const auto& table : m_aits)
            {
                if (table.first.first != stream.m_pid) {
                    continue;
                }

                for (const auto& section : table.second.m_sections)
                {
                    for (App app : section.second)
                    {
                        if (!seen.insert({app.m_organisationId, app.m_applicationId}).second) {
                            continue;
                        }

                        app.m_serviceId = service.first;

                        if ((app.m_componentTag >= 0) && (app.m_remoteServiceId < 0)) {
                            app.m_carouselPid = resolveTag(service.first, app.m_componentTag);
                        }

                        apps.push_back(app);
                    }
                }
            }
        }
    }

    return apps;
}

std::vector<DTVApps::Carousel> DTVApps::getCarousels(int serviceId) const
{
    // Of all services: a carousel shared by services may have its data_broadcast_id in some of their PMTs only, and
    // applications in it signalled in some of their AITs
    const std::vector<App> apps = getApps();
    QMutexLocker locker(&m_mutex);
    std::vector<Carousel> carousels;
    std::map<int, int> dataBroadcastIds;

    for (const auto& service : m_services)
    {
        for (const Stream& stream : service.second.m_streams)
        {
            if (stream.m_dataBroadcastId >= 0) {
                dataBroadcastIds[stream.m_pid] = stream.m_dataBroadcastId;
            }
        }
    }

    for (const auto& service : m_services)
    {
        if ((serviceId != 0) && (service.first != serviceId)) {
            continue;
        }

        for (const Stream& stream : service.second.m_streams)
        {
            if (!isDsmcc(stream.m_type)) {
                continue;
            }

            // Those with a carousel id, or an application, or a DSI: not those of only a carousel's modules
            bool app = false;
            bool hbbtv = false;

            for (const App& a : apps)
            {
                if (a.m_carouselPid == stream.m_pid)
                {
                    app |= a.m_serviceId == service.first;
                    hbbtv |= a.m_applicationType == m_hbbtvApplicationType;
                }
            }

            auto received = m_carousels.find(stream.m_pid);

            if ((stream.m_carouselId < 0) && !app && (received == m_carousels.end())) {
                continue;
            }

            Carousel carousel;
            carousel.m_serviceId = service.first;
            carousel.m_pid = stream.m_pid;
            carousel.m_componentTag = stream.m_componentTag;
            carousel.m_carouselId = stream.m_carouselId;
            auto dataBroadcastId = dataBroadcastIds.find(stream.m_pid);
            carousel.m_dataBroadcastId = dataBroadcastId != dataBroadcastIds.end() ? dataBroadcastId->second : -1;
            carousel.m_hbbtvApp = hbbtv;

            if (received != m_carousels.end())
            {
                const DTVCarousel *c = received->second.get();
                carousel.m_received = c->getCarouselId() >= 0;
                carousel.m_carouselId = carousel.m_received ? c->getCarouselId() : carousel.m_carouselId;
                carousel.m_progress = c->getProgress();
                carousel.m_complete = c->isComplete();

                for (const DTVCarousel::Entry& entry : c->getEntries()) {
                    carousel.m_files += (entry.m_kind == DTVCarousel::Kind::File) && entry.m_received ? 1 : 0;
                }
            }

            carousels.push_back(carousel);
        }
    }

    return carousels;
}

std::vector<int> DTVApps::getServices() const
{
    QMutexLocker locker(&m_mutex);
    std::vector<int> services;

    for (const auto& service : m_services)
    {
        bool interactive = false;

        for (const Stream& stream : service.second.m_streams) {
            interactive |= stream.m_ait || isDsmcc(stream.m_type);
        }

        if (interactive) {
            services.push_back(service.first);
        }
    }

    return services;
}

int DTVApps::getComponentPid(int serviceId, int componentTag) const
{
    QMutexLocker locker(&m_mutex);
    return resolveTag(serviceId, componentTag);
}

bool DTVApps::isCarouselComplete(int pid) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_carousels.find(pid);
    return (it != m_carousels.end()) && it->second->isComplete();
}

std::vector<DTVCarousel::Entry> DTVApps::getCarouselEntries(int pid) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_carousels.find(pid);
    return it != m_carousels.end() ? it->second->getEntries() : std::vector<DTVCarousel::Entry>();
}

bool DTVApps::getCarouselFile(int pid, const QString& path, QByteArray& data) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_carousels.find(pid);
    return (it != m_carousels.end()) && it->second->getFile(path, data);
}

std::map<QString, QByteArray> DTVApps::getCarouselFiles(int pid) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_carousels.find(pid);
    return it != m_carousels.end() ? it->second->getFiles() : std::map<QString, QByteArray>();
}
