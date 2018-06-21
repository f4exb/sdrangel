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
 * \file rtcpsdesinfo.h
 */

#ifndef RTCPSDESINFO_H

#define RTCPSDESINFO_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpdefines.h"
#include "rtptypes.h"
#include <string.h>
#include <list>

#include "export.h"

namespace qrtplib
{

/** The class RTCPSDESInfo is a container for RTCP SDES information. */
class QRTPLIB_API RTCPSDESInfo
{
public:
    /** Constructs an instance, optionally installing a memory manager. */
    RTCPSDESInfo()
    {
    }
    virtual ~RTCPSDESInfo()
    {
        Clear();
    }

    /** Clears all SDES information. */
    void Clear();

    /** Sets the SDES CNAME item to \c s with length \c l. */
    int SetCNAME(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_CNAME - 1, s, l);
    }

    /** Sets the SDES name item to \c s with length \c l. */
    int SetName(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_NAME - 1, s, l);
    }

    /** Sets the SDES e-mail item to \c s with length \c l. */
    int SetEMail(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_EMAIL - 1, s, l);
    }

    /** Sets the SDES phone item to \c s with length \c l. */
    int SetPhone(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_PHONE - 1, s, l);
    }

    /** Sets the SDES location item to \c s with length \c l. */
    int SetLocation(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_LOCATION - 1, s, l);
    }

    /** Sets the SDES tool item to \c s with length \c l. */
    int SetTool(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_TOOL - 1, s, l);
    }

    /** Sets the SDES note item to \c s with length \c l. */
    int SetNote(const uint8_t *s, std::size_t l)
    {
        return SetNonPrivateItem(RTCP_SDES_ID_NOTE - 1, s, l);
    }

#ifdef RTP_SUPPORT_SDESPRIV
    /** Sets the entry for the prefix string specified by \c prefix with length \c prefixlen to contain
     *  the value string specified by \c value with length \c valuelen (if the maximum allowed
     *  number of prefixes was reached, the error code \c ERR_RTP_SDES_MAXPRIVITEMS is returned.
     */
    int SetPrivateValue(const uint8_t *prefix, std::size_t prefixlen, const uint8_t *value, std::size_t valuelen);

    /** Deletes the entry for the prefix specified by \c s with length \c len. */
    int DeletePrivatePrefix(const uint8_t *s, std::size_t len);
#endif // RTP_SUPPORT_SDESPRIV

    /** Returns the SDES CNAME item and stores its length in \c len. */
    uint8_t *GetCNAME(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_CNAME - 1, len);
    }

    /** Returns the SDES name item and stores its length in \c len. */
    uint8_t *GetName(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_NAME - 1, len);
    }

    /** Returns the SDES e-mail item and stores its length in \c len. */
    uint8_t *GetEMail(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_EMAIL - 1, len);
    }

    /** Returns the SDES phone item and stores its length in \c len. */
    uint8_t *GetPhone(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_PHONE - 1, len);
    }

    /** Returns the SDES location item and stores its length in \c len. */
    uint8_t *GetLocation(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_LOCATION - 1, len);
    }

    /** Returns the SDES tool item and stores its length in \c len. */
    uint8_t *GetTool(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_TOOL - 1, len);
    }

    /** Returns the SDES note item and stores its length in \c len. */
    uint8_t *GetNote(std::size_t *len) const
    {
        return GetNonPrivateItem(RTCP_SDES_ID_NOTE - 1, len);
    }
#ifdef RTP_SUPPORT_SDESPRIV
    /** Starts the iteration over the stored SDES private item prefixes and their associated values. */
    void GotoFirstPrivateValue();

    /** Returns SDES priv item information.
     *  If available, returns \c true and stores the next SDES
     *  private item prefix in \c prefix and its length in
     *  \c prefixlen. The associated value and its length are
     *  then stored in \c value and \c valuelen. Otherwise,
     *  it returns \c false.
     */
    bool GetNextPrivateValue(uint8_t **prefix, std::size_t *prefixlen, uint8_t **value, std::size_t *valuelen);

    /** Returns SDES priv item information.
     *  Looks for the entry which corresponds to the SDES private
     *  item prefix \c prefix with length \c prefixlen. If found,
     *  the function returns \c true and stores the associated
     *  value and its length in \c value and \c valuelen
     *  respectively.
     */
    bool GetPrivateValue(const uint8_t *prefix, std::size_t prefixlen, uint8_t **value, std::size_t *valuelen) const;
#endif // RTP_SUPPORT_SDESPRIV
private:
    int SetNonPrivateItem(int itemno, const uint8_t *s, std::size_t l)
    {
        if (l > RTCP_SDES_MAXITEMLENGTH)
            return ERR_RTP_SDES_LENGTHTOOBIG;
        return nonprivateitems[itemno].SetInfo(s, l);
    }
    uint8_t *GetNonPrivateItem(int itemno, std::size_t *len) const
    {
        return nonprivateitems[itemno].GetInfo(len);
    }

    class SDESItem
    {
    public:
        SDESItem()
        {
            str = 0;
            length = 0;
        }
        ~SDESItem()
        {
            if (str)
                delete[] str;
        }
        uint8_t *GetInfo(std::size_t *len) const
        {
            *len = length;
            return str;
        }
        int SetInfo(const uint8_t *s, std::size_t len)
        {
            return SetString(&str, &length, s, len);
        }
    protected:
        int SetString(uint8_t **dest, std::size_t *destlen, const uint8_t *s, std::size_t len)
        {
            if (len <= 0)
            {
                if (*dest)
                    delete[] (*dest);
                *dest = 0;
                *destlen = 0;
            }
            else
            {
                len = (len > RTCP_SDES_MAXITEMLENGTH) ? RTCP_SDES_MAXITEMLENGTH : len;
                uint8_t *str2 = new uint8_t[len];
                memcpy(str2, s, len);
                *destlen = len;
                if (*dest)
                    delete[] (*dest);
                *dest = str2;
            }
            return 0;
        }
    private:
        uint8_t *str;
        std::size_t length;
    };

    SDESItem nonprivateitems[RTCP_SDES_NUMITEMS_NONPRIVATE];

#ifdef RTP_SUPPORT_SDESPRIV
    class SDESPrivateItem: public SDESItem
    {
    public:
        SDESPrivateItem()
        {
            prefixlen = 0;
            prefix = 0;
        }
        ~SDESPrivateItem()
        {
            if (prefix)
                delete[] prefix;
        }
        uint8_t *GetPrefix(std::size_t *len) const
        {
            *len = prefixlen;
            return prefix;
        }
        int SetPrefix(const uint8_t *s, std::size_t len)
        {
            return SetString(&prefix, &prefixlen, s, len);
        }
    private:
        uint8_t *prefix;
        std::size_t prefixlen;
    };

    std::list<SDESPrivateItem *> privitems;
    std::list<SDESPrivateItem *>::const_iterator curitem;
#endif // RTP_SUPPORT_SDESPRIV
};

} // end namespace

#endif // RTCPSDESINFO_H

