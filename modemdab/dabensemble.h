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

#ifndef MODEMDAB_DABENSEMBLE_H_
#define MODEMDAB_DABENSEMBLE_H_

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "export.h"
#include "dabconvolutional.h"
#include "dabtables.h"

namespace modemdab
{

// FIC: depuncturing, Viterbi, energy dispersal and FIB CRCs (EN 300 401 clauses 5.2, 10.2, 11.2)
class MODEMDAB_API DABFICDecoder
{
public:
    DABFICDecoder();

    // Decode one FIC block (2304 soft bits, one CIF) into three FIBs. Returns how many have a
    // valid CRC. bitErrors and codedBits estimate the channel bit error rate.
    int decodeBlock(
        const float *soft,
        std::array<std::array<uint8_t, ModeI::FIBBytes>, ModeI::FIBsPerCIF>& fibs,
        std::array<bool, ModeI::FIBsPerCIF>& valid,
        int& bitErrors,
        int& codedBits
    );
    // The Viterbi output of the last block, before energy dispersal is removed (768 bits)
    const std::vector<uint8_t>& lastBits() const { return m_bits; }

private:
    DABViterbi m_viterbi;
    ProtectionProfile m_profile;
    std::vector<uint8_t> m_bits;
};

struct MODEMDAB_API DABSubchannel
{
    int id = -1;
    int startCU = 0;
    int sizeCU = 0;
    bool uep = true;            // Short form (table 8), else EEP
    int tableIndex = 0;         // UEP
    bool eepSetB = false;       // EEP option 1
    int protectionLevel = 0;    // UEP 1..5; EEP 1..4
    int bitrate = 0;            // kbit/s
    int fecScheme = 0;          // FIG 0/14

    ProtectionProfile profile() const;
    std::string protection() const;   // "UEP 3", "EEP 3-A"
    bool operator==(const DABSubchannel& other) const;
};

struct MODEMDAB_API DABUserApplication
{
    int type = 0;               // TS 101 756 table 16 (0x002 SlideShow, 0x44a Journaline...)
    bool xpad = false;          // Carried in the X-PAD of an audio component
    int xpadAppType = -1;
    bool dataGroups = true;
    int dscty = -1;
    std::vector<uint8_t> data;
};

struct MODEMDAB_API DABServiceComponent
{
    enum Transport
    {
        StreamAudio = 0,
        StreamData = 1,
        PacketData = 3
    };

    int transport = StreamAudio;
    int ascty = -1;             // Audio: 0 DAB (MPEG Layer II), 63 DAB+
    int dscty = -1;             // Data: 60 MOT, 5 TDC...
    int subchannelId = -1;
    int scid = -1;              // Packet mode
    int scids = -1;             // From FIG 0/8 (0 for the primary component)
    bool primary = false;
    bool ca = false;
    int packetAddress = -1;
    bool dataGroups = true;
    int language = -1;
    std::string label;
    std::vector<DABUserApplication> userApplications;

    bool isAudio() const { return transport == StreamAudio; }
    bool isDABPlus() const { return (transport == StreamAudio) && (ascty == 63); }
};

struct MODEMDAB_API DABService
{
    uint32_t sid = 0;
    bool dataService = false;   // 32-bit SId
    int caId = 0;
    std::string label;
    std::string shortLabel;
    int pty = -1;
    bool ptyDynamic = false;
    int ecc = -1;
    std::vector<DABServiceComponent> components;

    const DABServiceComponent *primaryComponent() const;
};

struct MODEMDAB_API DABDateTime
{
    bool valid = false;
    int mjd = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int milliseconds = 0;

    // Gregorian date from the MJD
    void date(int& year, int& month, int& day) const;
};

struct MODEMDAB_API DABEnsembleInfo
{
    int eid = -1;
    std::string label;
    std::string shortLabel;
    int ecc = -1;
    bool haveLTO = false;
    int ltoHalfHours = 0;
    int internationalTable = 1;
    bool alarm = false;
    int changeFlags = 0;
    int occurrenceChange = -1;
    int servicesInConfiguration = -1;   // FIG 0/7
    int reconfigurationCount = -1;
    DABDateTime time;
};

// Announcement types are bits of the ASu and ASw flags (TS 101 756 table 14): bit 0 alarm,
// 1 road traffic, 2 transport, 3 warning/service, 4 news, 5 weather, 6 event, 7 special
// event, 8 programme information, 9 sport, 10 financial.
enum DABAnnouncementType
{
    DABAnnouncementAlarm = 0,
    DABAnnouncementTypes = 11
};

// The announcements a service may be interrupted by (FIG 0/18)
struct MODEMDAB_API DABAnnouncementSupport
{
    uint16_t flags = 0;         // ASu
    std::vector<int> clusters;
};

// An announcement in progress (FIG 0/19)
struct MODEMDAB_API DABAnnouncementSwitch
{
    int cluster = -1;           // 0xff for alarms
    uint16_t flags = 0;         // ASw: a single type
    bool newFlag = false;
    int subchannelId = -1;      // The sub-channel carrying it
    int type() const;           // The type of the set bit, -1 for none
};

// Services linked to a service of the ensemble (FIG 0/6, clause 8.1.15)
struct MODEMDAB_API DABLinkedId
{
    enum Kind
    {
        Service = 0,            // DAB SId
        RDSPI = 1,              // PI code
        DRMService = 3          // DRM or AMSS service identifier (24 bits)
    };

    int kind = Service;
    int ecc = -1;               // International links
    uint32_t id = 0;

    bool operator==(const DABLinkedId& other) const { return (kind == other.kind) && (ecc == other.ecc) && (id == other.id); }
};

struct MODEMDAB_API DABLinkageSet
{
    int lsn = 0;                // Linkage set number
    bool active = false;        // LA
    bool hard = false;          // The same programme, else related ones
    bool international = false;
    uint32_t keyService = 0;    // A service of the ensemble
    std::vector<DABLinkedId> ids;   // The other members
};

// Frequencies of an ensemble, or of a service on FM, DRM or AMSS (FIG 0/21, clause 8.1.8)
struct MODEMDAB_API DABFrequencyInformation
{
    enum RM
    {
        Ensemble = 0,           // id is the EId
        DRM = 6,                // id is the 24-bit service identifier
        FMRDS = 8,              // id is the PI code
        AMSS = 14               // id is the 24-bit service identifier
    };

    struct Frequency
    {
        int64_t hz = 0;
        bool adjacent = true;   // DAB: geographically adjacent
    };

    int rm = Ensemble;
    uint32_t id = 0;
    bool otherEnsemble = false;
    bool continuity = false;
    std::vector<Frequency> frequencies;
};

// Where a service of the ensemble can also be received: the same service in another ensemble
// (FIG 0/24), or a service linked to it (FIG 0/6), with its frequencies (FIG 0/21)
struct MODEMDAB_API DABServiceLink
{
    enum Kind
    {
        SameService,            // In other ensembles
        LinkedDAB,
        LinkedFM,
        LinkedDRM
    };

    int kind = SameService;
    uint32_t id = 0;            // SId, PI code or DRM/AMSS service identifier
    int ecc = -1;
    bool hard = false;          // Linked services: the same programme
    bool active = false;        // Linked services: the link is active
    std::string label;          // DAB services of this ensemble
    std::vector<int> eids;      // DAB: the ensembles carrying it (empty if this one)
    std::vector<int64_t> frequencies;    // Hz, where known
};

// The ensemble database, built from FIGs. MCI is kept for the current and next configurations,
// and switched at the signalled CIF.
class MODEMDAB_API DABEnsemble
{
public:
    DABEnsemble();

    void reset();
    // Parse the 30-byte data field of a FIB with a valid CRC. cifCount is the CIF the FIB
    // belongs to, if known (-1 otherwise); it is used for reconfigurations.
    void processFIB(const uint8_t *fib);
    // Called for each CIF with its count, to switch to the next configuration when due.
    void cif(int cifCount);

    // Incremented whenever the ensemble or service information changes.
    uint32_t version() const { return m_version; }
    const DABEnsembleInfo& info() const { return m_info; }
    // CIF count from the last FIG 0/0 (-1 if none yet)
    int lastCIFCount() const { return m_lastCIFCount; }
    bool cifCountUpdated() { bool u = m_cifCountUpdated; m_cifCountUpdated = false; return u; }

    std::vector<DABService> services() const;
    bool service(uint32_t sid, DABService& service) const;
    bool subchannel(int id, DABSubchannel& subchannel) const;
    std::vector<DABSubchannel> subchannels() const;
    bool northAmericanPTy() const;

    // Announcements a service supports (FIG 0/18)
    DABAnnouncementSupport announcementSupport(uint32_t sid) const;
    // Announcements in progress (FIG 0/19), by cluster. An alarm (cluster 0xff) applies only when
    // the ensemble's alarm flag is set (info().alarm).
    const std::map<int, DABAnnouncementSwitch>& announcements() const { return m_announcements; }
    // Service linking, frequency information and other ensembles' services
    std::vector<DABLinkageSet> linkageSets() const;
    std::vector<DABFrequencyInformation> frequencyInformation() const;
    // The ensembles that also carry a service (FIG 0/24), and whether that is another ensemble's
    // service
    std::vector<int> otherEnsembles(uint32_t sid) const;
    // Where a service can also be received, from all of these
    std::vector<DABServiceLink> serviceLinks(uint32_t sid) const;
    // The sub-channel's audio codec, from the services: 0 MPEG Layer II, 63 DAB+, -1 unknown
    int subchannelASCTy(int subchannelId) const;
    // The label of the service whose primary audio is in the sub-channel
    std::string subchannelServiceLabel(int subchannelId) const;

private:
    struct ComponentDescription
    {
        int transport;
        int type;           // ASCTy or DSCTy
        int subchannelId;
        int scid;
        bool primary;
        bool ca;
    };

    struct ServiceDescription
    {
        bool dataService = false;
        int caId = 0;
        std::vector<ComponentDescription> components;
    };

    struct PacketComponent
    {
        int subchannelId;
        int packetAddress;
        int dscty;
        bool dataGroups;
    };

    struct GlobalDefinition
    {
        bool longForm;
        int subchannelId;
        int scid;
    };

    struct MCI
    {
        std::map<int, DABSubchannel> subchannels;
        std::map<uint32_t, ServiceDescription> services;
        std::map<int, PacketComponent> packetComponents;            // By SCId
        std::map<std::pair<uint32_t, int>, GlobalDefinition> globalDefinitions;   // By SId, SCIdS
        std::map<std::pair<uint32_t, int>, std::vector<DABUserApplication>> userApplications;
        std::map<int, int> fecSchemes;

        bool empty() const { return subchannels.empty() && services.empty(); }
    };

    struct Label
    {
        std::string label;
        std::string shortLabel;
        bool extended = false;      // From FIG 2
    };

    struct ExtendedLabelSegments
    {
        int toggle = -1;
        int count = 0;
        std::array<std::vector<uint8_t>, 8> segments;
        std::array<bool, 8> received{};
    };

    MCI m_current;
    MCI m_next;
    DABEnsembleInfo m_info;
    Label m_ensembleLabel;
    std::map<uint32_t, Label> m_serviceLabels;
    std::map<std::pair<uint32_t, int>, Label> m_componentLabels;
    std::map<uint32_t, std::pair<int, bool>> m_programmeTypes;
    std::map<int, int> m_subchannelLanguages;
    std::map<int, int> m_scidLanguages;
    std::map<uint32_t, int> m_serviceECCs;
    std::map<std::pair<int, uint32_t>, ExtendedLabelSegments> m_extendedLabels;  // By extension, id

    // Service information databases, by their keys. The FIGs give each in parts: C/N 0 starts a
    // database entry and 1 continues it.
    struct LinkageKey
    {
        bool pd;
        bool hard;
        bool international;
        int lsn;
        bool operator<(const LinkageKey& o) const
        {
            return std::tie(pd, hard, international, lsn) < std::tie(o.pd, o.hard, o.international, o.lsn);
        }
    };
    struct OEServices
    {
        bool otherEnsemble = false;
        std::vector<int> eids;
    };
    std::map<uint32_t, DABAnnouncementSupport> m_announcementSupport;
    std::map<int, DABAnnouncementSwitch> m_announcements;
    static constexpr int64_t AnnouncementTimeoutCIFs = 5 * 1000 / 24;  // 5 s
    // Bounds on the databases, as FIGs are only protected by a CRC
    static constexpr std::size_t MaximumDatabaseEntries = 512;
    static constexpr std::size_t MaximumDatabaseIds = 64;
    std::map<int, int64_t> m_announcementLastCIF;   // By cluster: m_cifs when last received
    int64_t m_cifs;                                 // CIFs counted by cif()
    std::map<LinkageKey, DABLinkageSet> m_linkageSets;
    std::map<std::tuple<bool, int, uint32_t>, DABFrequencyInformation> m_frequencies;   // By OE, R&M, Id
    std::map<std::pair<bool, uint32_t>, OEServices> m_oeServices;                        // By P/D, SId
    uint32_t m_version;
    int m_lastCIFCount;
    bool m_cifCountUpdated;

    void changed() { m_version++; }
    MCI& mci(bool next) { return next ? m_next : m_current; }
    void fig0(const uint8_t *data, int length);
    void fig1(const uint8_t *data, int length);
    void fig2(const uint8_t *data, int length);
    void fig0_0(const uint8_t *data, int length);
    void fig0_1(const uint8_t *data, int length, bool next);
    void fig0_2(const uint8_t *data, int length, bool next, bool pd);
    void fig0_3(const uint8_t *data, int length, bool next);
    void fig0_5(const uint8_t *data, int length);
    void fig0_7(const uint8_t *data, int length);
    void fig0_8(const uint8_t *data, int length, bool next, bool pd);
    void fig0_9(const uint8_t *data, int length);
    void fig0_10(const uint8_t *data, int length);
    void fig0_13(const uint8_t *data, int length, bool next, bool pd);
    void fig0_14(const uint8_t *data, int length, bool next);
    void fig0_17(const uint8_t *data, int length);
    void fig0_6(const uint8_t *data, int length, bool cn, bool pd);
    void fig0_18(const uint8_t *data, int length);
    void fig0_19(const uint8_t *data, int length);
    void fig0_21(const uint8_t *data, int length, bool cn, bool oe);
    void fig0_24(const uint8_t *data, int length, bool cn, bool oe, bool pd);
    void setLabel(int extension, const uint8_t *id, int idLength, const std::string& label, const std::string& shortLabel, bool extended);
};

} // namespace modemdab

#endif // MODEMDAB_DABENSEMBLE_H_
