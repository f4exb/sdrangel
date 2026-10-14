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

#ifndef INCLUDE_DVBDEMODEPG_H
#define INCLUDE_DVBDEMODEPG_H

#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include <QElapsedTimer>
#include <QMutex>
#include <QString>

#include "dtv/dtvguide.h"
#include "dtv/dtvsections.h"

// Electronic programme guide, from the service information in the transport stream (EN 300 468): the Event
// Information Tables (now and next, and schedules of up to 8 days) of the services of this and other transport
// streams of the network, their names from the SDTs, logical channel numbers from the NITs, and the time from the
// TDT: the guide (DTVGuide) for DVB. Fed with every packet of the transport stream, in the demodulator's thread, and read from others
class DVBDemodEPG : public DTVGuide
{
public:
    DVBDemodEPG();
    void clear();
    //! A 188-byte transport stream packet
    void feed(const uint8_t *packet);

    //! UTC, seconds since 1970, from the TDT and the time since it was received. 0 until received
    qint64 getTime() const override;
    int getTransportStreamId() const override;
    int getNetworkId() const override;   //!< original_network_id of the transport stream being received, -1 if not known
    //! The services: one for each service id and name of a network (the UK carries a service in every region's
    //! transport stream), that of the transport stream received, else one whose multiplex is known, else that with the
    //! most programmes. Services with the same ids but other names are each listed
    std::vector<Service> getServices() const override;
    std::vector<TransportStream> getTransportStreams() const;
    //! False if its frequency isn't known
    bool getTransportStream(int networkId, int transportStreamId, TransportStream& transportStream) const;
    //! The transport stream being received is at frequency (Hz), as the device is tuned: kept for it, if the NIT
    //! doesn't give it (as the UK's for DVB-T2 multiplexes), so its services can be tuned to from others. Replaces a
    //! frequency learned before if it differs by more than 1 MHz (the multiplex has moved), never the NIT's
    void setFrequency(qint64 frequency, int bandwidth);
    //! The same for a satellite multiplex, at a symbol rate, DVB-S2 or DVB-S
    //! The multiplex being received is at this frequency, with this symbol rate. rollOff: from the BBHEADER, as in a
    //! satellite delivery system descriptor, -1 if not known. isi: the input stream it's in, -1 for a single one
    void setSatelliteFrequency(qint64 frequency, int symbolRate, bool s2, int rollOff, int isi);
    //! The device has been retuned: the transport stream being received isn't known until its SDT or EIT arrives
    void retuned();
    //! The same programme as a service's on now on another service, as in HD, from the event linkage of its event
    //! (the present event, or the schedule's at the time; just the present one, if the time isn't known). False if
    //! none, or it isn't in this area
    bool getSimulcast(int networkId, int serviceId, Service& simulcast) const override;
    //! A service, as getServices() gives it (or removed). False if not in the guide. A service id can be used in more
    //! than one transport stream of a network: these, given only the network's and service's ids, take the
    //! transport stream being received's, else one with a name, else any
    bool getService(int networkId, int serviceId, Service& service) const override;
    //! A programme of a service's schedule, by its id. False if not in the guide
    bool getEvent(int networkId, int serviceId, int eventId, Event& event) const override;
    //! The running status of a service's programme, if it's the present or following event of the service's
    //! transport stream being received (whose table's kept up to date): 0 to 4 (as Event's), -1 if not known
    int getRunningStatus(int networkId, int serviceId, int eventId) const;
    //! Changes whenever what the guide gives does (its services, events, multiplexes, or the one received), so views
    //! can skip rebuilding what hasn't changed
    quint64 getRevision() const override { return m_revision; }
    //! Of a service in the transport stream being received (or the first with the id, if not known yet)
    bool getNowNext(int serviceId, Event& now, Event& next) const override;
    //! Of any service in the guide
    bool getNowNext(int networkId, int serviceId, Event& now, Event& next) const override;
    //! A service's schedule, in order, from the TDT time less an hour (or all, if no time)
    std::vector<Event> getEvents(int networkId, int serviceId) const override;
    //! The same for many services (network and service ids) at once, in their order
    std::vector<std::vector<Event>> getEvents(const std::vector<std::pair<int, int>>& services) const override;
    //! For each service (network and service ids), a value that changes whenever what getEvents() gives for it may
    //! (but for the time passing), so views can skip fetching schedules that haven't changed. 0 if not in the guide
    std::vector<quint64> getServiceRevisions(const std::vector<std::pair<int, int>>& services) const override;
    int getEventCount() const override;
    bool isModified() const;    //!< Since the last save or load

    //! Cache, so the guide is complete straight away. Events that ended over an hour before now (UTC, seconds since
    //! 1970, 0 for the computer's clock) aren't loaded
    bool save(const QString& fileName);
    bool load(const QString& fileName, qint64 now = 0);

    //! Freesat's regional channel numbers: those of this region id (from its bouquet association table's region
    //! descriptor), or 0 for the default region's (65535). Services numbered in the default region with a number
    //! the region gives another are hidden, as receivers do
    void setFreesatRegion(int region);
    //! Freesat's regions, id and name, of the bouquet the channel numbers are taken from
    std::vector<std::pair<int, QString>> getFreesatRegions() const;

    //! The UK (original_network_id 0x233A) uses the D-Book's names, which differ from EN 300 468's from 6
    static QString genreText(int genre, int networkId);
    QString genreName(int genre, int networkId) const override { return genreText(genre, networkId); }
    static QString defaultCacheFileName();

private:
    static constexpr int m_freesatSource = 0x100;  //!< Of a table id: from Freesat's own PIDs, kept apart from the standard tables
    static constexpr int m_nitPid = 0x10;
    static constexpr int m_sdtPid = 0x11;
    static constexpr int m_eitPid = 0x12;
    static constexpr int m_timePid = 0x14;
    // Freesat (the UK's free to air satellite service, on Astra 2): its SDT and BAT for the whole network, and its EIT
    // with the whole network's 7 day schedule, on PIDs of their own: 3002 and 3003 on the home transponder (11.428 GHz H,
    // transport stream 2315), 3841 and 3842 on others. EN 300 468 tables, with the actual transport stream's table ids
    // for others' too, the same text compression as the UK's DVB-T, and Freesat's own descriptors for channel numbers
    static constexpr int m_freesatSiPid = 3002;
    static constexpr int m_freesatEitPid = 3003;
    static constexpr int m_freesatSiPidOther = 3841;
    static constexpr int m_freesatEitPidOther = 3842;
    static constexpr int m_freesatChannelTag = 0xD3;      //!< In the BAT's transport stream loop: services' channel numbers by region
    static constexpr int m_freesatRegionTag = 0xD4;       //!< In the BAT's first loop: regions' names
    static constexpr int m_freesatDefaultRegion = 0xFFFF;
    static const int m_freesatBouquets[3]; //!< England HD, G2 and SD, preferred in turn, then any

    static constexpr int m_expiryInterval = 600;  // Seconds between removing old events
    static constexpr int m_keepEnded = 3600;      // Seconds events are kept after they end
    static constexpr int m_timeJump = 300;        // Seconds a TDT may differ from the time expected, without the next to confirm it
    static constexpr int m_timeAbsent = 120;      // Seconds without a TDT before events are expired without one
    static constexpr qint64 m_expiryCheckMs = 60000;        // Milliseconds between checks for that
    static constexpr qint64 m_versionTimeout = 1800000;   // Milliseconds a section's version is remembered without receiving it
    static constexpr int m_longEvent = 86400;     // Seconds: an event longer doesn't replace the events it overlaps
    static constexpr int m_mjd2000 = 51544;       // 1 January 2000: earlier (or from 2100) times are taken as corrupt
    static constexpr int m_mjd2100 = 88069;

    static constexpr quint32 m_cacheMagic = 0x44564245; // "DVBE"
    // 2 adds the transport streams, 3 the event links, 4 the transport streams' flags and the channels' visibility
    static constexpr quint32 m_cacheVersion = 7;  // 7: events' CRIDs

    static int bcd2(uint8_t b);
    static qint64 dvbTime(const uint8_t *t);
    static int bcdDuration(const uint8_t *d);
    static qint64 bcd(const uint8_t *d, int digits);
    // Keys of the sections whose versions are kept, and of EIT sections (see dvbdemodepg.cpp)
    static uint64_t nitSectionKey(int table, int networkId, int section);
    static uint64_t batSectionKey(int bouquetId, int section);
    static int batSectionBouquet(uint64_t sectionKey);
    static uint64_t sdtSectionKey(bool freesat, int table, int networkId, int transportStreamId, int section);
    static int eitSourceTable(int table, bool freesat);
    static int eitVersionKey(int sourceTable, int section);
    static int freesatBouquet(const std::set<int>& bouquets);

    struct StoredEvent
    {
        Event m_event;
        int m_table = 0;            //!< Table and section it came from, to remove it if they change, -1 if cached
        int m_section = 0;
    };

    //! A section's version_number, and when it was last received (m_clock, ms): one not received for a while is read
    //! again whatever its version, as it may have changed 32 times (5 bits) since, as while tuned elsewhere
    struct SeenVersion
    {
        int m_version = -1;
        qint64 m_seen = 0;
    };

    struct ServiceData
    {
        Service m_info;
        std::map<qint64, StoredEvent> m_schedule;  //!< By start
        Event m_present;
        Event m_following;
        std::map<int, SeenVersion> m_versions;      //!< By eitVersionKey()
        quint64 m_changed = 0;                      //!< m_revision when its schedule, present event or info last changed
    };

    //! A complete version of an SDT, its sections and services as they arrive, to find the services left out
    struct SdtTable
    {
        int m_version = -1;
        int m_lastSection = 0;
        std::set<int> m_sections;
        std::set<int> m_serviceIds;
    };

    static uint32_t key(int networkId, int id) { return ((uint32_t) networkId << 16) | (uint32_t) (id & 0xFFFF); }
    //! A service's: original network, transport stream (0xFFFF if not known) and service ids
    static uint64_t serviceKey(int networkId, int transportStreamId, int serviceId) {
        return ((uint64_t) (networkId & 0xFFFF) << 32) | ((uint64_t) (transportStreamId & 0xFFFF) << 16) | (uint64_t) (serviceId & 0xFFFF);
    }
    ServiceData& service(int networkId, int transportStreamId, int serviceId);
    //! By the network's and service's ids, as getService() takes them
    const ServiceData *lookup(int networkId, int serviceId) const;
    void eventsLocked(const ServiceData *data, qint64 t, std::vector<Event>& events) const;
    //! A service's schedule, present event or info changed
    void changed(ServiceData& data) { data.m_changed = ++m_revision; }
    //! For choosing among services with the same ids: not removed, named, its multiplex known, then most programmes
    int rank(const ServiceData& data) const;
    const ServiceData *findService(int serviceId) const;
    //! A service with the guide's derived fields (in the transport stream received, events, its multiplex)
    Service describe(const ServiceData& data) const;
    bool nowNextLocked(const ServiceData *data, Event& now, Event& next) const;
    void nit(const uint8_t *s, int length);
    //! freesat: from Freesat's PIDs, which carry other transport streams' tables with the actual ones' table ids
    void sdt(const uint8_t *s, int length, bool freesat = false);
    void eit(const uint8_t *s, int length, bool freesat = false);
    void bat(const uint8_t *s, int length);
    //! dropped: services (serviceKey()) a BAT no longer numbers: their channel numbers are removed, if no other gives one
    void assignFreesatChannels(const std::set<uint64_t>& dropped = std::set<uint64_t>());
    void insertEvent(ServiceData& data, const Event& event, int table, int section);
    void replaceSegment(ServiceData& data, int table, int section, int lastSection, int version);
    void deliverySystem(int networkId, int transportStreamId, bool actual, const uint8_t *d, int length, int tag);
    void satelliteDeliverySystem(int networkId, int transportStreamId, bool actual, const uint8_t *d, int length);
    void time(const uint8_t *s, int length);
    bool parseEvent(const uint8_t *s, int length, Event& event);
    //! Remove events that ended an hour before now, if it's time to (or now is before the last time)
    void expire(qint64 now);
    //! Without the TDT for a while (or at all), expire() by the time extrapolated from the last, else the computer's clock
    void periodicExpiry();
    qint64 timeLocked() const;
    bool newVersion(uint64_t sectionKey, int version);

    DTVSectionAssembler m_nitSections;
    DTVSectionAssembler m_sdtSections;
    DTVSectionAssembler m_eitSections;
    DTVSectionAssembler m_timeSections;
    DTVSectionAssembler m_freesatSiSections[2];    //!< SDT and BAT, on the home transponder's PID, or others'
    DTVSectionAssembler m_freesatEitSections[2];   //!< The whole network's EIT, the same

    struct FreesatChannel
    {
        int m_bouquetId;
        int m_region;
        int m_channel;
    };

    unsigned int m_packets = 0; //!< Fed, to check now and then if events should be expired. The feeding thread's
    QElapsedTimer m_expiryCheck;    //!< The same

    mutable QMutex m_mutex;     //!< For the following
    std::map<uint64_t, ServiceData> m_services;     //!< By serviceKey()
    //! m_services by key(networkId, serviceId), in transport stream id order, for lookup()
    std::map<uint32_t, std::vector<ServiceData *>> m_servicesByIds;
    std::map<uint32_t, SdtTable> m_sdtTables;       //!< SDT actual's, by key(networkId, transportStreamId)
    std::map<uint32_t, TransportStream> m_transportStreams;    //!< By key(networkId, transportStreamId)
    std::map<uint64_t, SeenVersion> m_tableVersions;    //!< SDT, NIT and BAT sections, by their section keys
    //! What each NIT section's version (by its section key) last numbered: serviceKey() -> logical channel number,
    //! so those a new version leaves out lose their numbers
    std::map<uint64_t, std::map<uint64_t, int>> m_nitChannels;
    //! The same for the BAT sections: the services they number (serviceKey()), and the regions they name
    std::map<uint64_t, std::set<uint64_t>> m_batServices;
    std::map<uint64_t, std::set<int>> m_batRegions;
    std::map<uint64_t, std::vector<FreesatChannel>> m_freesatChannels; //!< By serviceKey(), from the BAT
    std::map<std::pair<int, int>, QString> m_freesatRegions;           //!< (bouquet_id, region) -> name
    int m_freesatRegion;        //!< 0 for the default region
    int m_networkId;            //!< Of the transport stream being received
    int m_transportStreamId;
    qint64 m_time;              //!< From the last TDT or TOT accepted, 0 if none
    QElapsedTimer m_timeSince;
    //! A TDT or TOT far from the time expected (m_time's, else the computer's clock), not used unless the next agrees:
    //! the TDT has no CRC, so may be corrupt. 0 if none
    qint64 m_pendingTime = 0;
    QElapsedTimer m_pendingSince;
    QElapsedTimer m_timeReceived;   //!< Since a TDT or TOT was last received, invalid if never
    QElapsedTimer m_clock;          //!< Monotonic, for SeenVersion
    qint64 m_lastExpiry;        //!< The time events were last expired at, 0 if never
    bool m_modified;
    std::atomic<quint64> m_revision;
};

#endif // INCLUDE_DVBDEMODEPG_H
