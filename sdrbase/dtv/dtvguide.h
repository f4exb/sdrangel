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

#ifndef INCLUDE_DTVGUIDE_H
#define INCLUDE_DTVGUIDE_H

#include <utility>
#include <vector>

#include <QList>
#include <QString>
#include <QStringList>

#include "export.h"

// A digital TV programme guide, as the guide's views (sdrgui/dtv) show it: the services of a network (or, for ATSC,
// a market) with their schedules, from a broadcast system's service information. DVB's EPG (EN 300 468's SDT, NIT
// and EIT) and ATSC's PSIP (A/65's VCT and EIT) each implement it. Services are identified by a network id and a
// service id (DVB's original_network_id and service_id; ATSC's transport stream id and program_number), events by
// their id in their service. Read from any thread
class SDRBASE_API DTVGuide
{
public:
    struct Event
    {
        int m_eventId = 0;
        qint64 m_start = 0;         //!< UTC, seconds since 1970
        int m_duration = 0;         //!< Seconds
        QString m_title;
        QString m_text;             //!< The short description, then any extended description
        //! Content nibbles (level 1 * 16 + level 2, EN 300 468 table 29), -1 if none. Other systems' genres are given
        //! as the nearest of these, with their own code from bit 8 (ATSC's A/65 table 6.20), which genreName() names
        int m_genre = -1;
        int m_minimumAge = 0;       //!< From the parental rating, 0 if none
        bool m_hd = false;
        bool m_subtitles = false;   //!< Or captions
        bool m_audioDescription = false;
        QString m_guidance;         //!< As "Contains strong language" (UK), or a rating (ATSC's content advisory)
        //! The same event on another service (as in HD), from an event linkage descriptor, -1 if none
        int m_linkNetworkId = -1;
        int m_linkServiceId = -1;
        //! From DVB's EIT (table 6): 0 undefined, 1 not running, 2 starts in a few seconds, 3 pausing, 4 running. Kept
        //! up to date by broadcasters that use it in the present and following table, to start and end recordings
        int m_runningStatus = 0;
        //! The content identifier descriptor's CRIDs (TS 102 323, as the UK's D-Book uses them), lower case, empty if
        //! none: the programme's (a repeat has its first showing's), and its series'. As broadcast: a relative CRID's
        //! authority is the service's (or its transport stream's, network's), the same for its events
        QString m_programmeCrid;
        QString m_seriesCrid;

        qint64 end() const { return m_start + m_duration; }
        bool isValid() const { return m_duration > 0; }
    };

    struct SDRBASE_API Service
    {
        int m_networkId = 0;        //!< DVB's original_network_id, ATSC's transport stream id
        int m_transportStreamId = -1;
        int m_serviceId = 0;        //!< MPEG program number
        QString m_name;
        QString m_provider;
        int m_type = 0;             //!< DVB's service_type: 1 TV, 2 radio, 0x16 to 0x19 HD TV, 0x0C data...
        int m_channel = -1;         //!< Logical channel number (ATSC's major channel number), -1 if none
        int m_minorChannel = -1;    //!< ATSC's minor channel number, -1 if none
        bool m_visible = true;      //!< The logical channel's visible_service_flag: false to hide it from lists
        bool m_actual = false;      //!< In the transport stream being received
        int m_events = 0;
        qint64 m_frequency = 0;     //!< Of its transport stream, Hz, 0 if not known
        //! Its transport stream is described by the NIT (or has been received), so it's in this area and can be
        //! searched for if its frequency isn't known, unlike other regions' (whose SI the UK also carries)
        bool m_multiplexKnown = false;
        //! Left out of its transport stream's SDT (a complete version of it): no longer broadcast there. Not listed by
        //! getServices(), and never m_actual
        bool m_removed = false;

        //! The number viewers select it by: "4", or ATSC's "7.1"; empty if none
        QString channelText() const;
        //! Order by channel number (major, then minor), those without last
        bool channelBefore(const Service& other) const;
    };

    //! A multiplex of the network, from DVB's NIT, or learned by tuning to it
    struct TransportStream
    {
        int m_networkId = 0;        //!< original_network_id
        int m_transportStreamId = 0;
        qint64 m_frequency = 0;     //!< Centre, Hz
        int m_bandwidth = 8000000;  //!< Hz
        bool m_t2 = false;          //!< From a T2 delivery system descriptor. The UK describes T2 multiplexes as DVB-T
        int m_plpId = -1;           //!< DVB-T2 PLP, -1 if not known
        bool m_lowPriority = false; //!< DVB-T hierarchical modes: carried in the low priority stream
        bool m_actual = false;      //!< From the NIT of the network being received, rather than another's
        bool m_learned = false;     //!< The frequency was learned by tuning to it (setFrequency), rather than from a NIT
        // From a satellite delivery system descriptor (or learned receiving DVB-S or S2): the frequency's the downlink's
        bool m_satellite = false;
        bool m_s2 = false;          //!< DVB-S2, else DVB-S
        int m_symbolRate = 0;       //!< Symbols a second
        int m_polarisation = 0;     //!< 0 linear horizontal, 1 linear vertical, 2 circular left, 3 circular right
        int m_orbitalPosition = 0;  //!< Tenths of a degree east (west negative)
        int m_fecInner = 0;         //!< EN 300 468 table 35: 1 1/2, 2 2/3, 3 3/4, 4 5/6, 5 7/8, 15 none, 0 not defined
        int m_rollOff = -1;         //!< DVB-S2: 0 0.35, 1 0.25, 2 0.20 (as the NIT codes them), S2X's 4 0.15, 5 0.10, 6 0.05 (learned from the BBHEADERs), -1 not known
        int m_isi = -1;             //!< DVB-S2: the input stream it was received in (of a multiple input stream), -1 if not
    };

    virtual ~DTVGuide() {}

    //! UTC, seconds since 1970, from the broadcast's time table and the time since it was received. 0 until received
    virtual qint64 getTime() const = 0;
    virtual int getTransportStreamId() const = 0;
    //! The network id of the transport stream being received, -1 if not known
    virtual int getNetworkId() const = 0;
    //! The services, without those removed
    virtual std::vector<Service> getServices() const = 0;
    //! The same programme as a service's on now on another service, as in HD. False if none
    virtual bool getSimulcast(int networkId, int serviceId, Service& simulcast) const = 0;
    //! A service, as getServices() gives it. False if not in the guide
    virtual bool getService(int networkId, int serviceId, Service& service) const = 0;
    //! A programme of a service's schedule, by its id. False if not in the guide
    virtual bool getEvent(int networkId, int serviceId, int eventId, Event& event) const = 0;
    //! Changes whenever what the guide gives does, so views can skip rebuilding what hasn't changed
    virtual quint64 getRevision() const = 0;
    //! Of a service in the transport stream being received (or the first with the id, if not known yet)
    virtual bool getNowNext(int serviceId, Event& now, Event& next) const = 0;
    //! Of any service in the guide
    virtual bool getNowNext(int networkId, int serviceId, Event& now, Event& next) const = 0;
    //! A service's schedule, in order, from the time less an hour (or all, if no time)
    virtual std::vector<Event> getEvents(int networkId, int serviceId) const = 0;
    //! The same for many services (network and service ids) at once, in their order
    virtual std::vector<std::vector<Event>> getEvents(const std::vector<std::pair<int, int>>& services) const = 0;
    //! For each service (network and service ids), a value that changes whenever what getEvents() gives for it may
    //! (but for the time passing), so views can skip fetching schedules that haven't changed. 0 if not in the guide
    virtual std::vector<quint64> getServiceRevisions(const std::vector<std::pair<int, int>>& services) const = 0;
    virtual int getEventCount() const = 0;
    //! The name of an event's genre (Event::m_genre), as the network names it (the UK's differ), empty if none
    virtual QString genreName(int genre, int networkId) const;

    //! EN 300 468 table 29's name of a genre's level 1, empty if none
    static QString genreText(int genre);
};

//! A programme from the guide to record
struct SDRBASE_API DTVScheduledRecording
{
    int m_networkId = 0;
    int m_serviceId = 0;
    int m_eventId = 0;
    qint64 m_start = 0;         //!< UTC, seconds since 1970. Updated if the guide changes
    int m_duration = 0;         //!< Seconds
    QString m_title;
    QString m_serviceName;

    qint64 end() const { return m_start + m_duration; }
    bool operator==(const DTVScheduledRecording& other) const {
        return (m_networkId == other.m_networkId) && (m_serviceId == other.m_serviceId) && (m_eventId == other.m_eventId)
            && (m_start == other.m_start) && (m_duration == other.m_duration) && (m_title == other.m_title);
    }
};

//! A series to record, from the programme guide: each programme of a service with its series CRID, once (a
//! repeat, by its programme CRID, isn't recorded again)
struct SDRBASE_API DTVSeriesRecording
{
    int m_networkId = 0;
    int m_serviceId = 0;
    QString m_seriesCrid;       //!< Lower case, as the guide has it
    QString m_title;            //!< Of the programme it was set from
    QString m_serviceName;
    QStringList m_scheduled;    //!< Programme CRIDs (or "event:" ids, without one) scheduled already, the latest last

    bool operator==(const DTVSeriesRecording& other) const {
        return (m_networkId == other.m_networkId) && (m_serviceId == other.m_serviceId) && (m_seriesCrid == other.m_seriesCrid)
            && (m_title == other.m_title) && (m_scheduled == other.m_scheduled);
    }
};

// Recordings of programmes from the guide, as the guide's views schedule them: a demodulator implements it
class SDRBASE_API DTVRecordingSchedule
{
public:
    virtual ~DTVRecordingSchedule() {}

    //! A programme from the guide to record. False if it's already scheduled
    virtual bool scheduleRecording(int networkId, int serviceId, int eventId) = 0;
    virtual void cancelScheduledRecording(int networkId, int serviceId, int eventId) = 0;
    virtual bool isScheduled(int networkId, int serviceId, int eventId) const = 0;
    virtual QList<DTVScheduledRecording> getScheduledRecordings() const = 0;
    //! Record a programme's series: every programme of its service with its series CRID, each once (not repeats).
    //! False if it has no series CRID, it's recorded already, or series can't be recorded
    virtual bool scheduleSeries(int networkId, int serviceId, int eventId) = 0;
    //! Stop recording a programme's series (the programmes scheduled from it stay scheduled)
    virtual void cancelSeries(int networkId, int serviceId, int eventId) = 0;
    virtual void cancelSeries(int networkId, int serviceId, const QString& seriesCrid) = 0;
    //! The programme's series is being recorded
    virtual bool isSeriesScheduled(int networkId, int serviceId, int eventId) const = 0;
    virtual QList<DTVSeriesRecording> getSeriesRecordings() const = 0;
    //! Scheduled recordings may retune to their service's multiplex
    virtual bool recordRetune() const = 0;
};

#endif // INCLUDE_DTVGUIDE_H
