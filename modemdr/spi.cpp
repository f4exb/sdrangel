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
#include <array>
#include <cstdlib>

#include "spi.h"

namespace modemdr
{

namespace
{

// Tags (TS 102 371 annex D)
enum Tag
{
    CData = 0x01,
    EPG = 0x02,
    ServiceInformation = 0x03,
    TokenTable = 0x04,
    DefaultContentId = 0x05,    // EPG V1 (TS 102 371 V1): not used in V3, but still sent
    ShortName = 0x10,
    MediumName = 0x11,
    LongName = 0x12,
    MediaDescription = 0x13,
    Genre = 0x14,
    Location = 0x19,
    ShortDescription = 0x1a,
    LongDescription = 0x1b,
    Programme = 0x1c,
    Schedule = 0x21,
    Scope = 0x24,
    ServiceScope = 0x25,
    Ensemble = 0x26,
    Service = 0x28,
    ServiceBearer = 0x29,
    Multimedia = 0x2b,
    Time = 0x2c,
    LocationBearer = 0x2d
};

// MOT parameters of SPI objects (TS 102 371 table 10)
constexpr int ScopeIdParameter = 0x27;

constexpr int MaximumDepth = 16;
constexpr int MaximumElements = 200000;
// Of the strings an object expands to: a token is up to 255 bytes, so a few megabytes of tokens
// could otherwise expand to gigabytes
constexpr std::size_t MaximumTextBytes = 16 * 1024 * 1024;

using Tokens = std::array<std::string, 20>;

// String data: UTF-8, with tokens (clause 5.5) for bytes 0x01 to 0x13 other than tab, line
// feed and carriage return. Appended to s; false when the object's text would pass
// MaximumTextBytes.
bool expand(const uint8_t *data, std::size_t size, const Tokens& tokens, std::size_t& textBytes, std::string& s)
{
    for (std::size_t i = 0; i < size; i++)
    {
        const uint8_t c = data[i];
        if ((c >= 0x01) && (c <= 0x13) && (c != 0x09) && (c != 0x0a) && (c != 0x0d))
        {
            textBytes += tokens[c].size();
            if (textBytes > MaximumTextBytes) {
                return false;
            }
            s += tokens[c];
        }
        else if (c != 0x00)
        {
            if (++textBytes > MaximumTextBytes) {
                return false;
            }
            s.push_back(static_cast<char>(c));
        }
    }
    return true;
}

// One tag, length and value (clause 5.2): the length is 8 bits, or 0xfe and 16, or 0xff and 24
bool readTLV(const uint8_t *data, std::size_t size, std::size_t& position, int& tag, const uint8_t *&value, std::size_t& length)
{
    if (position + 2 > size) {
        return false;
    }
    tag = data[position];
    length = data[position + 1];
    position += 2;
    if (length == 0xfe)
    {
        if (position + 2 > size) {
            return false;
        }
        length = (data[position] << 8) | data[position + 1];
        position += 2;
    }
    else if (length == 0xff)
    {
        if (position + 3 > size) {
            return false;
        }
        length = (data[position] << 16) | (data[position + 1] << 8) | data[position + 2];
        position += 3;
    }
    if (position + length > size) {
        return false;
    }
    value = data + position;
    position += length;
    return true;
}

bool parseContent(const uint8_t *data, std::size_t size, SPIElement& element, Tokens& tokens, bool topLevel,
    int depth, int& elements, std::size_t& textBytes)
{
    std::size_t position = 0;

    while (position < size)
    {
        int tag;
        const uint8_t *value;
        std::size_t length;

        if (!readTLV(data, size, position, tag, value, length)) {
            return false;
        }

        if (tag == CData)
        {
            if (!expand(value, length, tokens, textBytes, element.cdata)) {
                return false;
            }
        }
        else if ((tag == DefaultContentId) && topLevel)
        {
            // The service a V1 schedule is for, as a bearer id: kept as an attribute
            element.attributes[tag].assign(value, value + length);
        }
        else if ((tag == TokenTable) && topLevel)
        {
            std::size_t p = 0;
            while (p + 2 <= length)
            {
                const int token = value[p];
                const std::size_t tokenLength = value[p + 1];
                p += 2;
                if (p + tokenLength > length) {
                    return false;
                }
                if (token < static_cast<int>(tokens.size())) {
                    tokens[token].assign(reinterpret_cast<const char *>(value + p), tokenLength);
                }
                p += tokenLength;
            }
        }
        else if (tag >= 0x80)
        {
            element.attributes[tag].assign(value, value + length);
            std::string& string = element.strings[tag];
            string.clear();
            if (!expand(value, length, tokens, textBytes, string)) {
                return false;
            }
        }
        else if ((tag >= 0x10) && (tag <= 0x7e))
        {
            if ((depth >= MaximumDepth) || (++elements > MaximumElements)) {
                return false;
            }
            SPIElement child;
            child.tag = tag;
            if (!parseContent(value, length, child, tokens, false, depth + 1, elements, textBytes)) {
                return false;
            }
            element.children.push_back(std::move(child));
        }
        // Others (the default language, unused tags) are skipped
    }
    return true;
}

int integer(const SPIElement& element, int tag, int defaultValue)
{
    const auto it = element.attributes.find(tag);
    if (it == element.attributes.end() || it->second.empty() || (it->second.size() > 4)) {
        return defaultValue;
    }
    int value = 0;
    for (uint8_t b : it->second) {
        value = (value << 8) | b;
    }
    return value;
}

std::string text(const SPIElement& element, int tag)
{
    const auto it = element.strings.find(tag);
    return it == element.strings.end() ? std::string() : it->second;
}

// Names, descriptions, logos and genres of a service or programme
void readText(const SPIElement& element, SPIText& text)
{
    for (const SPIElement& child : element.children)
    {
        switch (child.tag)
        {
        case ShortName:
            if (text.shortName.empty()) {
                text.shortName = child.cdata;
            }
            break;
        case MediumName:
            if (text.mediumName.empty()) {
                text.mediumName = child.cdata;
            }
            break;
        case LongName:
            if (text.longName.empty()) {
                text.longName = child.cdata;
            }
            break;
        case MediaDescription:
            for (const SPIElement& media : child.children)
            {
                if ((media.tag == ShortDescription) && text.shortDescription.empty())
                {
                    text.shortDescription = media.cdata;
                }
                else if ((media.tag == LongDescription) && text.longDescription.empty())
                {
                    text.longDescription = media.cdata;
                }
                else if (media.tag == Multimedia)
                {
                    SPILogo logo;
                    logo.mimeType = modemdr::text(media, 0x80);
                    logo.url = modemdr::text(media, 0x82);
                    logo.type = integer(media, 0x83, SPILogo::Unrestricted);
                    logo.width = integer(media, 0x84, 0);
                    logo.height = integer(media, 0x85, 0);
                    if (logo.type == SPILogo::ColourSquare)
                    {
                        logo.width = 32;
                        logo.height = 32;
                    }
                    else if (logo.type == SPILogo::ColourRectangle)
                    {
                        logo.width = 112;
                        logo.height = 32;
                    }
                    while (!logo.url.empty() && (logo.url.front() == '/')) {
                        logo.url.erase(logo.url.begin());
                    }
                    if (!logo.url.empty()) {
                        text.logos.push_back(logo);
                    }
                }
            }
            break;
        case Genre:
        {
            const auto href = child.attributes.find(0x80);
            if (href != child.attributes.end())
            {
                const std::string genre = SPIDecoder::decodeGenre(href->second);
                if (!genre.empty()) {
                    text.genres.push_back(genre);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void mergeService(std::vector<SPIService>& services, const SPIService& service)
{
    for (SPIService& existing : services)
    {
        const bool same = std::any_of(existing.bearers.begin(), existing.bearers.end(), [&service](const SPIBearer& a) {
            return std::any_of(service.bearers.begin(), service.bearers.end(), [&a](const SPIBearer& b) { return a.sid == b.sid; });
        });
        if (!same) {
            continue;
        }
        SPIText& t = existing.text;
        const SPIText& u = service.text;
        for (auto field : {&SPIText::shortName, &SPIText::mediumName, &SPIText::longName,
                &SPIText::shortDescription, &SPIText::longDescription})
        {
            if ((t.*field).empty()) {
                t.*field = u.*field;
            }
        }
        for (const SPILogo& logo : u.logos)
        {
            if (std::none_of(t.logos.begin(), t.logos.end(), [&logo](const SPILogo& l) { return l.url == logo.url; })) {
                t.logos.push_back(logo);
            }
        }
        return;
    }
    services.push_back(service);
}

} // anonymous namespace

std::string SPIText::name() const
{
    // Some broadcasters end a medium name cut to its 16 characters with "~": the long name is
    // then better
    const bool truncated = !mediumName.empty() && (mediumName.back() == '~');
    if (!mediumName.empty() && !(truncated && !longName.empty())) {
        return mediumName;
    }
    return longName.empty() ? (mediumName.empty() ? shortName : mediumName) : longName;
}

std::string SPIText::description() const
{
    return longDescription.empty() ? shortDescription : longDescription;
}

const SPIService *SPIData::service(uint32_t sid) const
{
    for (const SPIService& service : services)
    {
        for (const SPIBearer& bearer : service.bearers)
        {
            if (bearer.sid == sid) {
                return &service;
            }
        }
    }
    return nullptr;
}

const MOTFile *SPIData::logo(uint32_t sid, int width) const
{
    const SPIService *s = service(sid);
    const MOTFile *best = nullptr;
    int bestDistance = 0;

    if (!s) {
        return nullptr;
    }
    for (const SPILogo& logo : s->text.logos)
    {
        const auto file = images.find(logo.url);
        if ((file == images.end()) || !file->second.data) {
            continue;
        }
        const int distance = std::abs(logo.width - width);
        if (!best || (distance < bestDistance))
        {
            best = &file->second;
            bestDistance = distance;
        }
    }
    return best;
}

const SPIProgramme *SPIData::programmeAt(uint32_t sid, int64_t time) const
{
    const auto schedule = schedules.find(sid);
    if (schedule == schedules.end()) {
        return nullptr;
    }
    for (const SPIProgramme& programme : schedule->second)
    {
        if ((programme.start <= time) && (time < programme.start + programme.duration) && programme.onAir) {
            return &programme;
        }
    }
    return nullptr;
}

bool SPIElement::parse(const uint8_t *data, std::size_t size, SPIElement& element)
{
    std::size_t position = 0;
    const uint8_t *value;
    std::size_t length;
    int tag;

    if (!readTLV(data, size, position, tag, value, length) || ((tag != EPG) && (tag != ServiceInformation))) {
        return false;
    }
    element = SPIElement();
    element.tag = tag;
    Tokens tokens;
    int elements = 0;
    std::size_t textBytes = 0;
    return parseContent(value, length, element, tokens, true, 0, elements, textBytes);
}

const SPIElement *SPIElement::child(int tag) const
{
    for (const SPIElement& c : children)
    {
        if (c.tag == tag) {
            return &c;
        }
    }
    return nullptr;
}

bool SPIDecoder::decodeBearer(const std::vector<uint8_t>& id, SPIBearer& bearer, SPIDomain domain)
{
    // DRM: the 24-bit service identifier (clause 5.4.5.1.3)
    if (domain == SPIDomain::DRM)
    {
        if (id.size() < 3) {
            return false;
        }
        bearer = SPIBearer();
        bearer.sid = (static_cast<uint32_t>(id[0]) << 16) | (id[1] << 8) | id[2];
        return true;
    }

    // Flags: Ens (an ECC and EId follow), X-PAD (an X-PAD application type follows the SId),
    // a 32-bit SId, and the SCIdS
    if (id.empty()) {
        return false;
    }
    const bool ensemble = (id[0] & 0x40) != 0;
    const bool longSId = (id[0] & 0x10) != 0;
    std::size_t position = 1;

    bearer = SPIBearer();
    bearer.scids = id[0] & 0x0f;
    if (ensemble)
    {
        if (id.size() < position + 3) {
            return false;
        }
        bearer.ecc = id[position];
        bearer.eid = (id[position + 1] << 8) | id[position + 2];
        position += 3;
    }
    const std::size_t sidLength = longSId ? 4 : 2;
    if (id.size() < position + sidLength) {
        return false;
    }
    for (std::size_t i = 0; i < sidLength; i++) {
        bearer.sid = (bearer.sid << 8) | id[position + i];
    }
    return true;
}

bool SPIDecoder::decodeTime(const std::vector<uint8_t>& time, int64_t& utc)
{
    // MJD (17 bits), LTO and UTC flags, then hours and minutes; with the UTC flag (the long
    // form) seconds follow. The time is UTC: a local time offset only says how to show it.
    if (time.size() < 4) {
        return false;
    }
    const uint32_t v = (static_cast<uint32_t>(time[0]) << 24) | (time[1] << 16) | (time[2] << 8) | time[3];
    const int mjd = (v >> 14) & 0x1ffff;
    const bool longForm = (v & 0x800) != 0;
    const int hours = (v >> 6) & 0x1f;
    const int minutes = v & 0x3f;
    const int seconds = (longForm && (time.size() >= 5)) ? (time[4] >> 2) : 0;

    if ((hours > 23) || (minutes > 59) || (seconds > 59)) {
        return false;
    }
    utc = (static_cast<int64_t>(mjd) - 40587) * 86400 + hours * 3600 + minutes * 60 + seconds;
    return true;
}

std::string SPIDecoder::decodeGenre(const std::vector<uint8_t>& href)
{
    // A classification scheme, then up to three levels (clause 5.4.5.4)
    static const char *schemes[] = {
        nullptr, "IntentionCS", "FormatCS", "ContentCS", "IntendedAudienceCS", "OriginationCS",
        "ContentAlertCS", "MediaTypeCS", "AtmosphereCS"
    };

    if (href.empty()) {
        return "";
    }
    const int cs = href[0] & 0x0f;
    if ((cs < 1) || (cs > 8)) {
        return "";
    }
    std::string urn = std::string("urn:tva:metadata:cs:") + schemes[cs] + ":2005:" + std::to_string(cs);
    for (std::size_t i = 1; (i < href.size()) && (i <= 3); i++) {
        urn += "." + std::to_string(href[i]);
    }
    return urn;
}

bool SPIDecoder::decodeSI(const uint8_t *object, std::size_t size, SPIData& data, SPIDomain domain)
{
    SPIElement root;
    if (!SPIElement::parse(object, size, root) || (root.tag != ServiceInformation)) {
        return false;
    }

    // DAB: serviceInformation, ensemble, service. DRM: services directly.
    std::vector<const SPIElement *> services;
    for (const SPIElement& child : root.children)
    {
        if (child.tag == Service) {
            services.push_back(&child);
        }
        else if (child.tag == Ensemble)
        {
            for (const SPIElement& service : child.children)
            {
                if (service.tag == Service) {
                    services.push_back(&service);
                }
            }
        }
    }

    for (const SPIElement *element : services)
    {
        SPIService service;
        readText(*element, service.text);
        for (const SPIElement& child : element->children)
        {
            SPIBearer bearer;
            const auto id = child.attributes.find(0x80);
            if ((child.tag == ServiceBearer) && (id != child.attributes.end()) && decodeBearer(id->second, bearer, domain)) {
                service.bearers.push_back(bearer);
            }
        }
        if (!service.bearers.empty()) {
            mergeService(data.services, service);
        }
    }
    return true;
}

bool SPIDecoder::decodePI(const uint8_t *object, std::size_t size, const SPIBearer *scope, SPIData& data, SPIDomain domain)
{
    SPIElement root;
    if (!SPIElement::parse(object, size, root) || (root.tag != EPG)) {
        return false;
    }

    for (const SPIElement& schedule : root.children)
    {
        if (schedule.tag != Schedule) {
            continue;
        }

        // The service the schedule is for: its scope, else the object's
        SPIBearer scheduleBearer;
        bool haveScheduleBearer = false;
        if (const SPIElement *scopeElement = schedule.child(Scope))
        {
            for (const SPIElement& serviceScope : scopeElement->children)
            {
                const auto id = serviceScope.attributes.find(0x80);
                if ((serviceScope.tag == ServiceScope) && (id != serviceScope.attributes.end()) && !haveScheduleBearer) {
                    haveScheduleBearer = decodeBearer(id->second, scheduleBearer, domain);
                }
            }
        }
        if (!haveScheduleBearer && scope)
        {
            scheduleBearer = *scope;
            haveScheduleBearer = true;
        }
        const auto defaultContentId = root.attributes.find(DefaultContentId);
        if (!haveScheduleBearer && (defaultContentId != root.attributes.end())) {
            haveScheduleBearer = decodeBearer(defaultContentId->second, scheduleBearer, domain);
        }

        for (const SPIElement& element : schedule.children)
        {
            if (element.tag != Programme) {
                continue;
            }

            SPIProgramme programme;
            readText(element, programme.text);
            programme.onAir = integer(element, 0x84, 1) != 2;
            programme.recommended = integer(element, 0x83, 1) == 2;

            bool haveTime = false;
            SPIBearer bearer = scheduleBearer;
            bool haveBearer = haveScheduleBearer;
            for (const SPIElement& location : element.children)
            {
                if (location.tag != Location) {
                    continue;
                }
                for (const SPIElement& child : location.children)
                {
                    if ((child.tag == Time) && !haveTime)
                    {
                        const auto time = child.attributes.find(0x80);
                        haveTime = (time != child.attributes.end()) && decodeTime(time->second, programme.start);
                        programme.duration = integer(child, 0x81, 0);
                    }
                    else if (child.tag == LocationBearer)
                    {
                        // A programme on a particular service
                        const auto id = child.attributes.find(0x80);
                        SPIBearer locationBearer;
                        if ((id != child.attributes.end()) && decodeBearer(id->second, locationBearer, domain))
                        {
                            bearer = locationBearer;
                            haveBearer = true;
                        }
                    }
                }
            }
            if (!haveTime || !haveBearer) {
                continue;
            }

            std::vector<SPIProgramme>& programmes = data.schedules[bearer.sid];
            const auto same = std::find_if(programmes.begin(), programmes.end(),
                [&programme](const SPIProgramme& p) { return p.start == programme.start; });
            if (same != programmes.end()) {
                *same = programme;
            } else if (programmes.size() < MaximumProgrammes) {
                programmes.push_back(programme);
            }
        }
    }
    return true;
}

void SPIDecoder::reset()
{
    m_mot.reset();
    m_data = std::make_shared<SPIData>();
    m_invalidObjects = 0;
}

bool SPIDecoder::push(const std::vector<uint8_t>& dataGroup)
{
    if (!m_mot.push(dataGroup)) {
        return false;
    }
    rebuild();
    return true;
}

void SPIDecoder::rebuild()
{
    m_data = decode(m_mot.files(), &m_invalidObjects, m_domain);
}

std::shared_ptr<SPIData> SPIDecoder::decode(const std::map<std::string, MOTFile>& files, int *invalidObjects, SPIDomain domain)
{
    // SI and PI objects (content type 7, subtypes 0 and 1) and the images they refer to
    auto data = std::make_shared<SPIData>();
    int invalid = 0;

    for (const auto& entry : files)
    {
        const MOTFile& file = entry.second;
        if (!file.data) {
            continue;
        }
        if (file.contentType == 7) {
            data->objects[entry.first] = file;
        }
        if ((file.contentType == 7) && (file.contentSubtype == 0))
        {
            if (!decodeSI(file.data->data(), file.data->size(), *data, domain)) {
                invalid++;
            }
        }
        else if ((file.contentType == 7) && (file.contentSubtype == 1))
        {
            SPIBearer scope;
            const auto id = file.parameters.find(ScopeIdParameter);
            const bool haveScope = (id != file.parameters.end()) && decodeBearer(id->second, scope, domain);
            if (!decodePI(file.data->data(), file.data->size(), haveScope ? &scope : nullptr, *data, domain)) {
                invalid++;
            }
        }
        else if (file.contentType == 2)
        {
            data->images[entry.first] = file;
        }
    }

    for (auto& schedule : data->schedules)
    {
        std::sort(schedule.second.begin(), schedule.second.end(),
            [](const SPIProgramme& a, const SPIProgramme& b) { return a.start < b.start; });
    }
    if (invalidObjects) {
        *invalidObjects = invalid;
    }
    return data;
}

} // namespace modemdr
