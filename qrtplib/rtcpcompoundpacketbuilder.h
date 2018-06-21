/*

 This file is a part of JRTPLIB
 Copyright (c) 1999-2017 Jori Liesenborgs

 Contact: jori.liesenborgs@gmail.com

 This library was developed at the Expertise Centre for Digital Media
 (http://www.edm.uhasselt.be), a research center of the Hasselt University
 (http://www.uhasselt.be). The library is based upon work done for
 my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 IN THE SOFTWARE.

 */

/**
 * \file rtcpcompoundpacketbuilder.h
 */

#ifndef RTCPCOMPOUNDPACKETBUILDER_H

#define RTCPCOMPOUNDPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtcpcompoundpacket.h"
#include "rtptimeutilities.h"
#include "rtcpsdespacket.h"
#include "rtperrors.h"
#include "rtpendian.h"
#include <list>

#include "export.h"

namespace qrtplib
{

/** This class can be used to construct an RTCP compound packet.
 *  The RTCPCompoundPacketBuilder class can be used to construct an RTCP compound packet. It inherits the member
 *  functions of RTCPCompoundPacket which can be used to access the information in the compound packet once it has
 *  been built successfully. The member functions described below return \c ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT
 *  if the action would cause the maximum allowed size to be exceeded.
 */
class QRTPLIB_API RTCPCompoundPacketBuilder: public RTCPCompoundPacket
{
public:
    /** Constructs an RTCPCompoundPacketBuilder instance, optionally installing a memory manager. */
    RTCPCompoundPacketBuilder();
    ~RTCPCompoundPacketBuilder();

    /** Starts building an RTCP compound packet with maximum size \c maxpacketsize.
     *  Starts building an RTCP compound packet with maximum size \c maxpacketsize. New memory will be allocated
     *  to store the packet.
     */
    int InitBuild(std::size_t maxpacketsize);

    /** Starts building a RTCP compound packet.
     *  Starts building a RTCP compound packet. Data will be stored in \c externalbuffer which
     *  can contain \c buffersize bytes.
     */
    int InitBuild(void *externalbuffer, std::size_t buffersize);

    /** Adds a sender report to the compound packet.
     *  Tells the packet builder that the packet should start with a sender report which will contain
     *  the sender information specified by this function's arguments. Once the sender report is started,
     *  report blocks can be added using the AddReportBlock function.
     */
    int StartSenderReport(uint32_t senderssrc, const RTPNTPTime &ntptimestamp, uint32_t rtptimestamp, uint32_t packetcount, uint32_t octetcount);

    /** Adds a receiver report to the compound packet.
     *  Tells the packet builder that the packet should start with a receiver report which will contain
     *  he sender SSRC \c senderssrc. Once the sender report is started, report blocks can be added using the
     *  AddReportBlock function.
     */
    int StartReceiverReport(uint32_t senderssrc);

    /** Adds the report block information specified by the function's arguments.
     *  Adds the report block information specified by the function's arguments. If more than 31 report blocks
     *  are added, the builder will automatically use a new RTCP receiver report packet.
     */
    int AddReportBlock(uint32_t ssrc, uint8_t fractionlost, int32_t packetslost, uint32_t exthighestseq, uint32_t jitter, uint32_t lsr, uint32_t dlsr);

    /** Starts an SDES chunk for participant \c ssrc. */
    int AddSDESSource(uint32_t ssrc);

    /** Adds a normal (non-private) SDES item of type \c t to the current SDES chunk.
     *  Adds a normal (non-private) SDES item of type \c t to the current SDES chunk. The item's value
     *  will have length \c itemlength and will contain the data \c itemdata.
     */
    int AddSDESNormalItem(RTCPSDESPacket::ItemType t, const void *itemdata, uint8_t itemlength);
#ifdef RTP_SUPPORT_SDESPRIV
    /** Adds an SDES PRIV item described by the function's arguments to the current SDES chunk. */
    int AddSDESPrivateItem(const void *prefixdata, uint8_t prefixlength, const void *valuedata, uint8_t valuelength);
#endif // RTP_SUPPORT_SDESPRIV

    /** Adds a BYE packet to the compound packet.
     *  Adds a BYE packet to the compound packet. It will contain \c numssrcs source identifiers specified in
     *  \c ssrcs and will indicate as reason for leaving the string of length \c reasonlength
     *  containing data \c reasondata.
     */
    int AddBYEPacket(uint32_t *ssrcs, uint8_t numssrcs, const void *reasondata, uint8_t reasonlength);

    /** Adds the APP packet specified by the arguments to the compound packet.
     *  Adds the APP packet specified by the arguments to the compound packet. Note that \c appdatalen has to be
     *  a multiple of four.
     */
    int AddAPPPacket(uint8_t subtype, uint32_t ssrc, const uint8_t name[4], const void *appdata, std::size_t appdatalen);

    /** Finishes building the compound packet.
     *  Finishes building the compound packet. If successful, the RTCPCompoundPacket member functions
     *  can be used to access the RTCP packet data.
     */
    int EndBuild();

private:
    class Buffer
    {
    public:
        Buffer() :
                packetdata(0), packetlength(0)
        {
        }
        Buffer(uint8_t *data, std::size_t len) :
                packetdata(data), packetlength(len)
        {
        }

        uint8_t *packetdata;
        std::size_t packetlength;
    };

    class Report
    {
    public:
        Report()
        {
            headerdata = (uint8_t *) headerdata32;
            isSR = false;
            headerlength = 0;
        }
        ~Report()
        {
            Clear();
        }

        void Clear()
        {
            std::list<Buffer>::const_iterator it;
            for (it = reportblocks.begin(); it != reportblocks.end(); it++)
            {
                if ((*it).packetdata)
                    delete[] (*it).packetdata;
            }
            reportblocks.clear();
            isSR = false;
            headerlength = 0;
        }

        std::size_t NeededBytes()
        {
            std::size_t x, n, d, r;
            n = reportblocks.size();
            if (n == 0)
            {
                if (headerlength == 0)
                    return 0;
                x = sizeof(RTCPCommonHeader) + headerlength;
            }
            else
            {
                x = n * sizeof(RTCPReceiverReport);
                d = n / 31; // max 31 reportblocks per report
                r = n % 31;
                if (r != 0)
                    d++;
                x += d * (sizeof(RTCPCommonHeader) + sizeof(uint32_t)); /* header and SSRC */
                if (isSR)
                    x += sizeof(RTCPSenderReport);
            }
            return x;
        }

        std::size_t NeededBytesWithExtraReportBlock()
        {
            std::size_t x, n, d, r;
            n = reportblocks.size() + 1; // +1 for the extra block
            x = n * sizeof(RTCPReceiverReport);
            d = n / 31; // max 31 reportblocks per report
            r = n % 31;
            if (r != 0)
                d++;
            x += d * (sizeof(RTCPCommonHeader) + sizeof(uint32_t)); /* header and SSRC */
            if (isSR)
                x += sizeof(RTCPSenderReport);
            return x;
        }

        bool isSR;

        uint8_t *headerdata;
        uint32_t headerdata32[(sizeof(uint32_t) + sizeof(RTCPSenderReport)) / sizeof(uint32_t)]; // either for ssrc and sender info or just ssrc
        std::size_t headerlength;
        std::list<Buffer> reportblocks;
    };

    class SDESSource
    {
    public:
        SDESSource(uint32_t s) :
                ssrc(s), totalitemsize(0)
        {
        }
        ~SDESSource()
        {
            std::list<Buffer>::const_iterator it;
            for (it = items.begin(); it != items.end(); it++)
            {
                if ((*it).packetdata)
                    delete[] (*it).packetdata;
            }
            items.clear();
        }

        std::size_t NeededBytes()
        {
            std::size_t x, r;
            x = totalitemsize + 1; // +1 for the 0 byte which terminates the item list
            r = x % sizeof(uint32_t);
            if (r != 0)
                x += (sizeof(uint32_t) - r); // make sure it ends on a 32 bit boundary
            x += sizeof(uint32_t); // for ssrc
            return x;
        }

        std::size_t NeededBytesWithExtraItem(uint8_t itemdatalength)
        {
            std::size_t x, r;
            x = totalitemsize + sizeof(RTCPSDESHeader) + (std::size_t) itemdatalength + 1;
            r = x % sizeof(uint32_t);
            if (r != 0)
                x += (sizeof(uint32_t) - r); // make sure it ends on a 32 bit boundary
            x += sizeof(uint32_t); // for ssrc
            return x;
        }

        void AddItem(uint8_t *buf, std::size_t len)
        {
            Buffer b(buf, len);
            totalitemsize += len;
            items.push_back(b);
        }

        uint32_t ssrc;
        std::list<Buffer> items;
    private:
        std::size_t totalitemsize;
    };

    class SDES
    {
    public:
        SDES()
        {
            sdesit = sdessources.end();
        }
        ~SDES()
        {
            Clear();
        }

        void Clear()
        {
            std::list<SDESSource *>::const_iterator it;

            for (it = sdessources.begin(); it != sdessources.end(); it++)
                delete *it;
            sdessources.clear();
        }

        int AddSSRC(uint32_t ssrc)
        {
            SDESSource *s = new SDESSource(ssrc);
            sdessources.push_back(s);
            sdesit = sdessources.end();
            sdesit--;
            return 0;
        }

        int AddItem(uint8_t *buf, std::size_t len)
        {
            if (sdessources.empty())
                return ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE;
            (*sdesit)->AddItem(buf, len);
            return 0;
        }

        std::size_t NeededBytes()
        {
            std::list<SDESSource *>::const_iterator it;
            std::size_t x = 0;
            std::size_t n, d, r;

            if (sdessources.empty())
                return 0;

            for (it = sdessources.begin(); it != sdessources.end(); it++)
                x += (*it)->NeededBytes();
            n = sdessources.size();
            d = n / 31;
            r = n % 31;
            if (r != 0)
                d++;
            x += d * sizeof(RTCPCommonHeader);
            return x;
        }

        std::size_t NeededBytesWithExtraItem(uint8_t itemdatalength)
        {
            std::list<SDESSource *>::const_iterator it;
            std::size_t x = 0;
            std::size_t n, d, r;

            if (sdessources.empty())
                return 0;

            for (it = sdessources.begin(); it != sdesit; it++)
                x += (*it)->NeededBytes();
            x += (*sdesit)->NeededBytesWithExtraItem(itemdatalength);
            n = sdessources.size();
            d = n / 31;
            r = n % 31;
            if (r != 0)
                d++;
            x += d * sizeof(RTCPCommonHeader);
            return x;
        }

        std::size_t NeededBytesWithExtraSource()
        {
            std::list<SDESSource *>::const_iterator it;
            std::size_t x = 0;
            std::size_t n, d, r;

            if (sdessources.empty())
                return 0;

            for (it = sdessources.begin(); it != sdessources.end(); it++)
                x += (*it)->NeededBytes();

            // for the extra source we'll need at least 8 bytes (ssrc and four 0 bytes)
            x += sizeof(uint32_t) * 2;

            n = sdessources.size() + 1; // also, the number of sources will increase
            d = n / 31;
            r = n % 31;
            if (r != 0)
                d++;
            x += d * sizeof(RTCPCommonHeader);
            return x;
        }

        std::list<SDESSource *> sdessources;
    private:
        std::list<SDESSource *>::const_iterator sdesit;
    };

    RTPEndian m_endian;
    std::size_t maximumpacketsize;
    uint8_t *buffer;
    bool external;
    bool arebuilding;

    Report report;
    SDES sdes;

    std::list<Buffer> byepackets;
    std::size_t byesize;

    std::list<Buffer> apppackets;
    std::size_t appsize;

    void ClearBuildBuffers();
};

} // end namespace

#endif // RTCPCOMPOUNDPACKETBUILDER_H

