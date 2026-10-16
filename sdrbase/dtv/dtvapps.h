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

#ifndef INCLUDE_DTVAPPS_H
#define INCLUDE_DTVAPPS_H

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <QMutex>
#include <QString>
#include <QStringList>

#include "export.h"
#include "dtv/dtvsections.h"
#include "dtv/dtvcarousel.h"

// The interactive applications of a DVB transport stream's services (the red button): from each service's PMT, its
// AIT (application information table, TS 102 809 5.3) and object carousels; from the AITs, the applications
// (HbbTV's, TS 102 796, and others), with where they're loaded from: a URL, or a carousel. The carousels (MHEG-5's,
// as the UK's, and HbbTV's) of the services chosen are received, as DTVCarousels. Fed with the transport stream's
// packets in the demodulator's thread; read from others
class SDRBASE_API DTVApps
{
public:
    static constexpr int m_hbbtvApplicationType = 0x0010;
    static constexpr int m_mhegDataBroadcastId = 0x0106;    //!< UK MHEG-5 (D-Book)
    static constexpr int m_hbbtvDataBroadcastId = 0x0123;

    struct SDRBASE_API App {
        int m_serviceId = 0;
        uint32_t m_organisationId = 0;
        int m_applicationId = 0;
        int m_applicationType = 0;      //!< 0x10 HbbTV
        int m_controlCode = 0;          //!< 1 AUTOSTART, 2 PRESENT, ...
        int m_visibility = 3;           //!< 0 not visible, 1 to applications only, 3 to users too
        int m_priority = 0;
        bool m_serviceBound = false;
        QString m_name;
        QString m_language;
        std::vector<int> m_profiles;    //!< HbbTV's application_profile, each as profile << 24 | major << 16 | minor << 8 | micro
        QStringList m_urlBases;         //!< Of HTTP transports
        int m_componentTag = -1;        //!< Of an object carousel transport: the carousel's PMT component tag
        int m_carouselPid = -1;         //!< That stream's PID, -1 if it's not in the PMT
        //! A carousel of another service (not received): that service's original network, transport stream and id
        int m_remoteNetworkId = -1;
        int m_remoteTransportStreamId = -1;
        int m_remoteServiceId = -1;
        QString m_location;             //!< Initial path, from the simple_application_location_descriptor
        QStringList m_boundaries;       //!< Of the application, from the simple_application_boundary_descriptor

        //! Where the application starts: a URL base and the location, or for a carousel a dvb: URL (TS 102 851)
        QString url(int originalNetworkId, int transportStreamId) const;
        QString controlCodeName() const;
        QString typeName() const;
        //! The application id as HbbTV shows it: organisation.application, in hex
        QString idText() const;
    };

    struct SDRBASE_API Carousel {
        int m_serviceId = 0;
        int m_pid = 0;
        int m_componentTag = -1;
        int64_t m_carouselId = -1;      //!< From the PMT's carousel_identifier_descriptor, or the DSI
        int m_dataBroadcastId = -1;     //!< 0x0106 MHEG-5, 0x0123 HbbTV, -1 if not signalled
        bool m_hbbtvApp = false;        //!< An HbbTV application's in it (MHEG-5 and HbbTV may share a carousel)
        bool m_received = false;        //!< Its DSI has been received
        DTVCarousel::Progress m_progress;
        bool m_complete = false;
        int m_files = 0;                //!< Received

        QString kindName() const;       //!< MHEG-5, HbbTV, both, or DSM-CC
    };

    DTVApps();
    ~DTVApps();
    void reset();           //!< For a new transport stream: forgets everything

    //! One 188-byte packet
    void feed(const uint8_t *packet);

    //! The service whose carousels are received: 0 none, -1 all of them
    void setCarouselService(int serviceId);
    int getCarouselService() const { return m_carouselService; }

    int getTransportStreamId() const;
    int getOriginalNetworkId() const;   //!< From the SDT, -1 until it's received
    //! The applications of a service, or all of them (0)
    std::vector<App> getApps(int serviceId = 0) const;
    //! The object carousels of a service, or all of them (0), signalled in the PMT or received
    std::vector<Carousel> getCarousels(int serviceId = 0) const;
    std::vector<int> getServices() const;  //!< With applications or carousels
    //! The PID of a service's stream with a component tag (as an application's carousel), -1 if it hasn't one
    int getComponentPid(int serviceId, int componentTag) const;
    //! The carousel on a PID: its tree, and files
    std::vector<DTVCarousel::Entry> getCarouselEntries(int pid) const;
    bool getCarouselFile(int pid, const QString& path, QByteArray& data) const;
    //! The carousel on a PID is being received, and has all its modules
    bool isCarouselComplete(int pid) const;
    std::map<QString, QByteArray> getCarouselFiles(int pid) const;
    //! Changes when anything that's read does
    int getRevision() const { return m_revision; }

private:
    static constexpr int m_patPid = 0x0000;
    static constexpr int m_sdtPid = 0x0011;
    static constexpr int m_patTable = 0x00;
    static constexpr int m_pmtTable = 0x02;
    static constexpr int m_sdtActual = 0x42;
    static constexpr int m_aitTable = 0x74;
    static constexpr int m_dsmccInfoTable = 0x3B;   //!< DSI and DII
    static constexpr int m_dsmccDataTable = 0x3C;   //!< DDB

    struct Stream {
        int m_pid = 0;
        int m_type = 0;
        int m_componentTag = -1;
        std::vector<int> m_associationTags;
        int64_t m_carouselId = -1;
        int m_dataBroadcastId = -1;
        bool m_ait = false;
    };

    struct Service {
        int m_pmtPid = -1;
        int m_pmtVersion = -1;
        std::vector<Stream> m_streams;
    };

    struct AIT {
        int m_version = -1;
        std::map<int, std::vector<App>> m_sections;     //!< By section number
    };

    void packet(int pid, const uint8_t *p);
    void section(int pid, const uint8_t *s, int length);
    void pat(const uint8_t *s, int length);
    void pmt(int pid, const uint8_t *s, int length);
    void ait(int pid, const uint8_t *s, int length);
    void updatePids();
    void updateCarousels();
    int resolveTag(int serviceId, int tag) const;
    bool carouselWanted(int serviceId) const;
    static QString dvbText(const uint8_t *text, int length);

    mutable QMutex m_mutex;
    std::map<int, std::unique_ptr<DTVSectionAssembler>> m_assemblers;    //!< By PID: of the PIDs read
    std::vector<bool> m_wanted;                         //!< By PID: those read
    int m_transportStreamId;
    int m_originalNetworkId;
    std::map<int, int> m_programs;                      //!< From the PAT: program number to PMT PID
    std::map<int, Service> m_services;                  //!< By program number (service id)
    std::map<std::pair<int, int>, AIT> m_aits;          //!< By PID and application type
    std::map<int, std::unique_ptr<DTVCarousel>> m_carousels;    //!< By PID of its DSI
    std::map<int, int> m_carouselServices;              //!< Carousel PID to its service
    std::map<int, std::set<int>> m_carouselPids;        //!< PID to the carousels (by DSI PID) fed from it
    std::atomic<int> m_carouselService;
    int m_carouselServiceApplied;
    std::atomic<int> m_revision;
};

#endif // INCLUDE_DTVAPPS_H
