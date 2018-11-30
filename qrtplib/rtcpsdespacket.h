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
 * \file rtcpsdespacket.h
 */

#ifndef RTCPSDESPACKET_H

#define RTCPSDESPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtpendian.h"

#include "export.h"

namespace qrtplib
{

class RTCPCompoundPacket;

/** Describes an RTCP source description packet. */
class QRTPLIB_API RTCPSDESPacket: public RTCPPacket
{
public:
    /** Identifies the type of an SDES item. */
    enum ItemType
    {
        None, /**< Used when the iteration over the items has finished. */
        CNAME, /**< Used for a CNAME (canonical name) item. */
        NAME, /**< Used for a NAME item. */
        EMAIL, /**< Used for an EMAIL item. */
        PHONE, /**< Used for a PHONE item. */
        LOC, /**< Used for a LOC (location) item. */
        TOOL, /**< Used for a TOOL item. */
        NOTE, /**< Used for a NOTE item. */
        PRIV, /**< Used for a PRIV item. */
        Unknown /**< Used when there is an item present, but the type is not recognized. */
    };

    /** Creates an instance based on the data in \c data with length \c datalen.
     *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
     *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it
     *  points to is valid as long as the class instance exists.
     */
    RTCPSDESPacket(uint8_t *data, std::size_t datalen);
    ~RTCPSDESPacket()
    {
    }

    /** Returns the number of SDES chunks in the SDES packet.
     *  Returns the number of SDES chunks in the SDES packet. Each chunk has its own SSRC identifier.
     */
    int GetChunkCount() const;

    /** Starts the iteration over the chunks.
     *  Starts the iteration. If no SDES chunks are present, the function returns \c false. Otherwise,
     *  it returns \c true and sets the current chunk to be the first chunk.
     */
    bool GotoFirstChunk();

    /** Sets the current chunk to the next available chunk.
     *  Sets the current chunk to the next available chunk. If no next chunk is present, this function returns
     *  \c false, otherwise it returns \c true.
     */
    bool GotoNextChunk();

    /** Returns the SSRC identifier of the current chunk. */
    uint32_t GetChunkSSRC() const;

    /** Starts the iteration over the SDES items in the current chunk.
     *  Starts the iteration over the SDES items in the current chunk. If no SDES items are
     *  present, the function returns \c false. Otherwise, the function sets the current item
     *  to be the first one and returns \c true.
     */
    bool GotoFirstItem();

    /** Advances the iteration to the next item in the current chunk.
     *  If there's another item in the chunk, the current item is set to be the next one and the function
     *  returns \c true. Otherwise, the function returns \c false.
     */
    bool GotoNextItem();

    /** Returns the SDES item type of the current item in the current chunk. */
    ItemType GetItemType() const;

    /** Returns the item length of the current item in the current chunk. */
    std::size_t GetItemLength() const;

    /** Returns the item data of the current item in the current chunk. */
    uint8_t *GetItemData();

#ifdef RTP_SUPPORT_SDESPRIV
    /** If the current item is an SDES PRIV item, this function returns the length of the
     *  prefix string of the private item.
     */
    std::size_t GetPRIVPrefixLength() const;

    /** If the current item is an SDES PRIV item, this function returns actual data of the
     *  prefix string.
     */
    uint8_t *GetPRIVPrefixData();

    /** If the current item is an SDES PRIV item, this function returns the length of the
     *  value string of the private item.
     */
    std::size_t GetPRIVValueLength() const;

    /** If the current item is an SDES PRIV item, this function returns actual value data of the
     *  private item.
     */
    uint8_t *GetPRIVValueData();
#endif // RTP_SUPPORT_SDESPRIV

private:
    RTPEndian m_endian;
    uint8_t *currentchunk;
    int curchunknum;
    std::size_t itemoffset;
};

inline int RTCPSDESPacket::GetChunkCount() const
{
    if (!knownformat)
        return 0;
    RTCPCommonHeader *hdr = (RTCPCommonHeader *) data;
    return ((int) hdr->count);
}

inline bool RTCPSDESPacket::GotoFirstChunk()
{
    if (GetChunkCount() == 0)
    {
        currentchunk = 0;
        return false;
    }
    currentchunk = data + sizeof(RTCPCommonHeader);
    curchunknum = 1;
    itemoffset = sizeof(uint32_t);
    return true;
}

inline bool RTCPSDESPacket::GotoNextChunk()
{
    if (!knownformat)
        return false;
    if (currentchunk == 0)
        return false;
    if (curchunknum == GetChunkCount())
        return false;

    std::size_t offset = sizeof(uint32_t);
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + sizeof(uint32_t));

    while (sdeshdr->sdesid != 0)
    {
        offset += sizeof(RTCPSDESHeader);
        offset += (std::size_t)(sdeshdr->length);
        sdeshdr = (RTCPSDESHeader *) (currentchunk + offset);
    }
    offset++; // for the zero byte
    if ((offset & 0x03) != 0)
        offset += (4 - (offset & 0x03));
    currentchunk += offset;
    curchunknum++;
    itemoffset = sizeof(uint32_t);
    return true;
}

inline uint32_t RTCPSDESPacket::GetChunkSSRC() const
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    uint32_t *ssrc = (uint32_t *) currentchunk;
    return m_endian.qToHost(*ssrc);
}

inline bool RTCPSDESPacket::GotoFirstItem()
{
    if (!knownformat)
        return false;
    if (currentchunk == 0)
        return false;
    itemoffset = sizeof(uint32_t);
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid == 0)
        return false;
    return true;
}

inline bool RTCPSDESPacket::GotoNextItem()
{
    if (!knownformat)
        return false;
    if (currentchunk == 0)
        return false;

    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid == 0)
        return false;

    std::size_t offset = itemoffset;
    offset += sizeof(RTCPSDESHeader);
    offset += (std::size_t)(sdeshdr->length);
    sdeshdr = (RTCPSDESHeader *) (currentchunk + offset);
    if (sdeshdr->sdesid == 0)
        return false;
    itemoffset = offset;
    return true;
}

inline RTCPSDESPacket::ItemType RTCPSDESPacket::GetItemType() const
{
    if (!knownformat)
        return None;
    if (currentchunk == 0)
        return None;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    switch (sdeshdr->sdesid)
    {
    case 0:
        return None;
    case RTCP_SDES_ID_CNAME:
        return CNAME;
    case RTCP_SDES_ID_NAME:
        return NAME;
    case RTCP_SDES_ID_EMAIL:
        return EMAIL;
    case RTCP_SDES_ID_PHONE:
        return PHONE;
    case RTCP_SDES_ID_LOCATION:
        return LOC;
    case RTCP_SDES_ID_TOOL:
        return TOOL;
    case RTCP_SDES_ID_NOTE:
        return NOTE;
    case RTCP_SDES_ID_PRIVATE:
        return PRIV;
    default:
        return Unknown;
    }
    return Unknown;
}

inline std::size_t RTCPSDESPacket::GetItemLength() const
{
    if (!knownformat)
        return None;
    if (currentchunk == 0)
        return None;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid == 0)
        return 0;
    return (std::size_t)(sdeshdr->length);
}

inline uint8_t *RTCPSDESPacket::GetItemData()
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid == 0)
        return 0;
    return (currentchunk + itemoffset + sizeof(RTCPSDESHeader));
}

#ifdef RTP_SUPPORT_SDESPRIV
inline std::size_t RTCPSDESPacket::GetPRIVPrefixLength() const
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
        return 0;
    if (sdeshdr->length == 0)
        return 0;
    uint8_t *preflen = currentchunk + itemoffset + sizeof(RTCPSDESHeader);
    std::size_t prefixlength = (std::size_t)(*preflen);
    if (prefixlength > (std::size_t)((sdeshdr->length) - 1))
        return 0;
    return prefixlength;
}

inline uint8_t *RTCPSDESPacket::GetPRIVPrefixData()
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
        return 0;
    if (sdeshdr->length == 0)
        return 0;
    uint8_t *preflen = currentchunk + itemoffset + sizeof(RTCPSDESHeader);
    std::size_t prefixlength = (std::size_t)(*preflen);
    if (prefixlength > (std::size_t)((sdeshdr->length) - 1))
        return 0;
    if (prefixlength == 0)
        return 0;
    return (currentchunk + itemoffset + sizeof(RTCPSDESHeader) + 1);
}

inline std::size_t RTCPSDESPacket::GetPRIVValueLength() const
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
        return 0;
    if (sdeshdr->length == 0)
        return 0;
    uint8_t *preflen = currentchunk + itemoffset + sizeof(RTCPSDESHeader);
    std::size_t prefixlength = (std::size_t)(*preflen);
    if (prefixlength > (std::size_t)((sdeshdr->length) - 1))
        return 0;
    return ((std::size_t)(sdeshdr->length)) - prefixlength - 1;
}

inline uint8_t *RTCPSDESPacket::GetPRIVValueData()
{
    if (!knownformat)
        return 0;
    if (currentchunk == 0)
        return 0;
    RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *) (currentchunk + itemoffset);
    if (sdeshdr->sdesid != RTCP_SDES_ID_PRIVATE)
        return 0;
    if (sdeshdr->length == 0)
        return 0;
    uint8_t *preflen = currentchunk + itemoffset + sizeof(RTCPSDESHeader);
    std::size_t prefixlength = (std::size_t)(*preflen);
    if (prefixlength > (std::size_t)((sdeshdr->length) - 1))
        return 0;
    std::size_t valuelen = ((std::size_t)(sdeshdr->length)) - prefixlength - 1;
    if (valuelen == 0)
        return 0;
    return (currentchunk + itemoffset + sizeof(RTCPSDESHeader) + 1 + prefixlength);
}

#endif // RTP_SUPPORT_SDESPRIV

} // end namespace

#endif // RTCPSDESPACKET_H

