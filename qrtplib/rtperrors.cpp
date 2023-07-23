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

#include "rtperrors.h"
#include "rtpdefines.h"
#include "rtpinternalutils.h"
#include <stdio.h>

namespace qrtplib
{

struct RTPErrorInfo
{
    int code;
    const char *description;
};

static RTPErrorInfo ErrorDescriptions[] =
{
{ ERR_RTP_OUTOFMEM, "Out of memory" },
{ ERR_RTP_NOTHREADSUPPORT, "No JThread support was compiled in" },
{ ERR_RTP_COLLISIONLIST_BADADDRESS, "Passed invalid address (null) to collision list" },
{ ERR_RTP_HASHTABLE_ELEMENTALREADYEXISTS, "Element already exists in hash table" },
{ ERR_RTP_HASHTABLE_ELEMENTNOTFOUND, "Element not found in hash table" },
{ ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX, "Function returned an illegal hash index" },
{ ERR_RTP_HASHTABLE_NOCURRENTELEMENT, "No current element selected in hash table" },
{ ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX, "Function returned an illegal hash index" },
{ ERR_RTP_KEYHASHTABLE_KEYALREADYEXISTS, "Key value already exists in key hash table" },
{ ERR_RTP_KEYHASHTABLE_KEYNOTFOUND, "Key value not found in key hash table" },
{ ERR_RTP_KEYHASHTABLE_NOCURRENTELEMENT, "No current element selected in key hash table" },
{ ERR_RTP_PACKBUILD_ALREADYINIT, "RTP packet builder is already initialized" },
{ ERR_RTP_PACKBUILD_CSRCALREADYINLIST, "The specified CSRC is already in the RTP packet builder's CSRC list" },
{ ERR_RTP_PACKBUILD_CSRCLISTFULL, "The RTP packet builder's CSRC list already contains 15 entries" },
{ ERR_RTP_PACKBUILD_CSRCNOTINLIST, "The specified CSRC was not found in the RTP packet builder's CSRC list" },
{ ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET, "The RTP packet builder's default mark flag is not set" },
{ ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET, "The RTP packet builder's default payload type is not set" },
{ ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET, "The RTP packet builder's default timestamp increment is not set" },
{ ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE, "The specified maximum packet size for the RTP packet builder is invalid" },
{ ERR_RTP_PACKBUILD_NOTINIT, "The RTP packet builder is not initialized" },
{ ERR_RTP_PACKET_BADPAYLOADTYPE, "Invalid payload type" },
{ ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE, "Tried to create an RTP packet which would exceed the specified maximum packet size" },
{ ERR_RTP_PACKET_EXTERNALBUFFERNULL, "Illegal value (null) passed as external buffer for the RTP packet" },
{ ERR_RTP_PACKET_ILLEGALBUFFERSIZE, "Illegal buffer size specified for the RTP packet" },
{ ERR_RTP_PACKET_INVALIDPACKET, "Invalid RTP packet format" },
{ ERR_RTP_PACKET_TOOMANYCSRCS, "More than 15 CSRCs specified for the RTP packet" },
{ ERR_RTP_POLLTHREAD_ALREADYRUNNING, "Poll thread is already running" },
{ ERR_RTP_POLLTHREAD_CANTINITMUTEX, "Can't initialize a mutex for the poll thread" },
{ ERR_RTP_POLLTHREAD_CANTSTARTTHREAD, "Can't start the poll thread" },
{ ERR_RTP_RTCPCOMPOUND_INVALIDPACKET, "Invalid RTCP compound packet format" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING, "Already building this RTCP compound packet" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT, "This RTCP compound packet is already built" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT, "There's already a SR or RR in this RTCP compound packet" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_APPDATALENTOOBIG, "The specified APP data length for the RTCP compound packet is too big" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_BUFFERSIZETOOSMALL, "The specified buffer size for the RTCP compound packet is too small" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALAPPDATALENGTH, "The APP data length must be a multiple of four" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALSUBTYPE, "The APP packet subtype must be smaller than 32" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_INVALIDITEMTYPE, "Invalid SDES item type specified for the RTCP compound packet" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_MAXPACKETSIZETOOSMALL, "The specified maximum packet size for the RTCP compound packet is too small" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE, "Tried to add an SDES item to the RTCP compound packet when no SSRC was present" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_NOREPORTPRESENT, "An RTCP compound packet must contain a SR or RR" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING, "The RTCP compound packet builder is not initialized" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT, "Adding this data would exceed the specified maximum RTCP compound packet size" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_REPORTNOTSTARTED, "Tried to add a report block to the RTCP compound packet when no SR or RR was started" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_TOOMANYSSRCS, "Only 31 SSRCs will fit into a BYE packet for the RTCP compound packet" },
{ ERR_RTP_RTCPCOMPPACKBUILDER_TOTALITEMLENGTHTOOBIG, "The total data for the SDES PRIV item exceeds the maximum size (255 bytes) of an SDES item" },
{ ERR_RTP_RTCPPACKETBUILDER_ALREADYINIT, "The RTCP packet builder is already initialized" },
{ ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE, "The specified maximum packet size for the RTCP packet builder is too small" },
{ ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT, "Specified an illegal timestamp unit for the the RTCP packet builder" },
{ ERR_RTP_RTCPPACKETBUILDER_NOTINIT, "The RTCP packet builder was not initialized" },
{ ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON, "The RTCP compound packet filled sooner than expected" },
{ ERR_RTP_SCHEDPARAMS_BADFRACTION, "Illegal sender bandwidth fraction specified" },
{ ERR_RTP_SCHEDPARAMS_BADMINIMUMINTERVAL, "The minimum RTCP interval specified for the scheduler is too small" },
{ ERR_RTP_SCHEDPARAMS_INVALIDBANDWIDTH, "Invalid RTCP bandwidth specified for the RTCP scheduler" },
{ ERR_RTP_SDES_LENGTHTOOBIG, "Specified size for the SDES item exceeds 255 bytes" },
{ ERR_RTP_SDES_PREFIXNOTFOUND, "The specified SDES PRIV prefix was not found" },
{ ERR_RTP_SESSION_ALREADYCREATED, "The session is already created" },
{ ERR_RTP_SESSION_CANTGETLOGINNAME, "Can't retrieve login name" },
{ ERR_RTP_SESSION_CANTINITMUTEX, "A mutex for the RTP session couldn't be initialized" },
{ ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL, "The maximum packet size specified for the RTP session is too small" },
{ ERR_RTP_SESSION_NOTCREATED, "The RTP session was not created" },
{ ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL, "The requested transmission protocol for the RTP session is not supported" },
{ ERR_RTP_SESSION_USINGPOLLTHREAD, "This function is not available when using the RTP poll thread feature" },
{ ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL, "A user-defined transmitter was requested but the supplied transmitter component is NULL" },
{ ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC, "Only one source can be marked as own SSRC in the source table" },
{ ERR_RTP_SOURCES_DONTHAVEOWNSSRC, "No source was marked as own SSRC in the source table" },
{ ERR_RTP_SOURCES_ILLEGALSDESTYPE, "Illegal SDES type specified for processing into the source table" },
{ ERR_RTP_SOURCES_SSRCEXISTS, "Can't create own SSRC because this SSRC identifier is already in the source table" },
{ ERR_RTP_UDPV4TRANS_ALREADYCREATED, "The transmitter was already created" },
{ ERR_RTP_UDPV4TRANS_ALREADYINIT, "The transmitter was already initialize" },
{ ERR_RTP_UDPV4TRANS_ALREADYWAITING, "The transmitter is already waiting for incoming data" },
{ ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET, "The 'bind' call for the RTCP socket failed" },
{ ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET, "The 'bind' call for the RTP socket failed" },
{ ERR_RTP_UDPV4TRANS_CANTCREATESOCKET, "Couldn't create the RTP or RTCP socket" },
{ ERR_RTP_UDPV4TRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter" },
{ ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF, "Couldn't set the receive buffer size for the RTCP socket" },
{ ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTCP socket" },
{ ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF, "Couldn't set the receive buffer size for the RTP socket" },
{ ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTP socket" },
{ ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group" },
{ ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE, "The function called doesn't match the current receive mode" },
{ ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter" },
{ ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter" },
{ ERR_RTP_UDPV4TRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty" },
{ ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT, "Multicast support is not available" },
{ ERR_RTP_UDPV4TRANS_NOSUCHENTRY, "Specified entry could not be found" },
{ ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address" },
{ ERR_RTP_UDPV4TRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called" },
{ ERR_RTP_UDPV4TRANS_NOTINIT, "The 'Init' call for this transmitter has not been called" },
{ ERR_RTP_UDPV4TRANS_NOTWAITING, "The transmitter is not waiting for incoming data" },
{ ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN, "The specified port base is not an even number" },
{ ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter" },
{ ERR_RTP_UDPV6TRANS_ALREADYCREATED, "The transmitter was already created" },
{ ERR_RTP_UDPV6TRANS_ALREADYINIT, "The transmitter was already initialize" },
{ ERR_RTP_UDPV6TRANS_ALREADYWAITING, "The transmitter is already waiting for incoming data" },
{ ERR_RTP_UDPV6TRANS_CANTBINDRTCPSOCKET, "The 'bind' call for the RTCP socket failed" },
{ ERR_RTP_UDPV6TRANS_CANTBINDRTPSOCKET, "The 'bind' call for the RTP socket failed" },
{ ERR_RTP_UDPV6TRANS_CANTCREATESOCKET, "Couldn't create the RTP or RTCP socket" },
{ ERR_RTP_UDPV6TRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter" },
{ ERR_RTP_UDPV6TRANS_CANTSETRTCPRECEIVEBUF, "Couldn't set the receive buffer size for the RTCP socket" },
{ ERR_RTP_UDPV6TRANS_CANTSETRTCPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTCP socket" },
{ ERR_RTP_UDPV6TRANS_CANTSETRTPRECEIVEBUF, "Couldn't set the receive buffer size for the RTP socket" },
{ ERR_RTP_UDPV6TRANS_CANTSETRTPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTP socket" },
{ ERR_RTP_UDPV6TRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group" },
{ ERR_RTP_UDPV6TRANS_DIFFERENTRECEIVEMODE, "The function called doesn't match the current receive mode" },
{ ERR_RTP_UDPV6TRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter" },
{ ERR_RTP_UDPV6TRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter" },
{ ERR_RTP_UDPV6TRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty" },
{ ERR_RTP_UDPV6TRANS_NOMULTICASTSUPPORT, "Multicast support is not available" },
{ ERR_RTP_UDPV6TRANS_NOSUCHENTRY, "Specified entry could not be found" },
{ ERR_RTP_UDPV6TRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address" },
{ ERR_RTP_UDPV6TRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called" },
{ ERR_RTP_UDPV6TRANS_NOTINIT, "The 'Init' call for this transmitter has not been called" },
{ ERR_RTP_UDPV6TRANS_NOTWAITING, "The transmitter is not waiting for incoming data" },
{ ERR_RTP_UDPV6TRANS_PORTBASENOTEVEN, "The specified port base is not an even number" },
{ ERR_RTP_UDPV6TRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter" },
{ ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL, "The hostname is larger than the specified buffer size" },
{ ERR_RTP_SDES_MAXPRIVITEMS, "The maximum number of SDES private item prefixes was reached" },
{ ERR_RTP_INTERNALSOURCEDATA_INVALIDPROBATIONTYPE, "An invalid probation type was specified" },
{ ERR_RTP_FAKETRANS_ALREADYCREATED, "The transmitter was already created" },
{ ERR_RTP_FAKETRANS_ALREADYINIT, "The transmitter was already initialize" },
{ ERR_RTP_FAKETRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter" },
{ ERR_RTP_FAKETRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group" },
{ ERR_RTP_FAKETRANS_DIFFERENTRECEIVEMODE, "The function called doesn't match the current receive mode" },
{ ERR_RTP_FAKETRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter" },
{ ERR_RTP_FAKETRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter" },
{ ERR_RTP_FAKETRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty" },
{ ERR_RTP_FAKETRANS_NOMULTICASTSUPPORT, "Multicast support is not available" },
{ ERR_RTP_FAKETRANS_NOSUCHENTRY, "Specified entry could not be found" },
{ ERR_RTP_FAKETRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address" },
{ ERR_RTP_FAKETRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called" },
{ ERR_RTP_FAKETRANS_NOTINIT, "The 'Init' call for this transmitter has not been called" },
{ ERR_RTP_FAKETRANS_PORTBASENOTEVEN, "The specified port base is not an even number" },
{ ERR_RTP_FAKETRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter" },
{ ERR_RTP_FAKETRANS_WAITNOTIMPLEMENTED, "The WaitForIncomingData is not implemented in the Gst transmitter" },
{ ERR_RTP_RTPRANDOMURANDOM_CANTOPEN, "Unable to open /dev/urandom for reading" },
{ ERR_RTP_RTPRANDOMURANDOM_ALREADYOPEN, "The device /dev/urandom was already opened" },
{ ERR_RTP_RTPRANDOMRANDS_NOTSUPPORTED, "The rand_s call is not supported on this platform" },
{ ERR_RTP_EXTERNALTRANS_ALREADYCREATED, "The external transmission component was already created" },
{ ERR_RTP_EXTERNALTRANS_ALREADYINIT, "The external transmission component was already initialized" },
{ ERR_RTP_EXTERNALTRANS_ALREADYWAITING, "The external transmission component is already waiting for incoming data" },
{ ERR_RTP_EXTERNALTRANS_BADRECEIVEMODE, "The external transmission component only supports accepting all incoming packets" },
{ ERR_RTP_EXTERNALTRANS_CANTINITMUTEX, "The external transmitter was unable to initialize a required mutex" },
{ ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS, "Only parameters of type RTPExternalTransmissionParams can be passed to the external transmission component" },
{ ERR_RTP_EXTERNALTRANS_NOACCEPTLIST, "The external transmitter does not have an accept list" },
{ ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED, "The external transmitter does not have a destination list" },
{ ERR_RTP_EXTERNALTRANS_NOIGNORELIST, "The external transmitter does not have an ignore list" },
{ ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT, "The external transmitter does not support the multicast functions" },
{ ERR_RTP_EXTERNALTRANS_NOSENDER, "No sender has been set for this external transmitter" },
{ ERR_RTP_EXTERNALTRANS_NOTCREATED, "The external transmitter has not been created yet" },
{ ERR_RTP_EXTERNALTRANS_NOTINIT, "The external transmitter has not been initialized yet" },
{ ERR_RTP_EXTERNALTRANS_NOTWAITING, "The external transmitter is not currently waiting for incoming data" },
{ ERR_RTP_EXTERNALTRANS_SENDERROR, "The external transmitter was unable to actually send the data" },
{ ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG, "The specified data size exceeds the maximum amount that has been set" },
{ ERR_RTP_UDPV4TRANS_CANTGETSOCKETPORT, "Unable to obtain the existing socket info using 'getsockname'" },
{ ERR_RTP_UDPV4TRANS_NOTANIPV4SOCKET, "The existing socket specified does not appear to be an IPv4 socket" },
{ ERR_RTP_UDPV4TRANS_SOCKETPORTNOTSET, "The existing socket that was specified does not have its port set yet" },
{ ERR_RTP_UDPV4TRANS_CANTGETSOCKETTYPE, "Can't get the socket type of the specified existing socket" },
{ ERR_RTP_UDPV4TRANS_INVALIDSOCKETTYPE, "The specified existing socket is not an UDP socket" },
{ ERR_RTP_UDPV4TRANS_CANTGETVALIDSOCKET, "Can't get a valid socket when trying to choose a port automatically" },
{ ERR_RTP_UDPV4TRANS_TOOMANYATTEMPTSCHOOSINGSOCKET, "Can't seem to get RTP/RTCP ports automatically, too many attempts" },
{ ERR_RTP_RTPSESSION_CHANGEREQUESTEDBUTNOTIMPLEMENTED, "Flag to change data was requested, but OnChangeRTPOrRTCPData was not reimplemented" },
{ ERR_RTP_SECURESESSION_CONTEXTALREADYINITIALIZED, "The initialization function was already called" },
{ ERR_RTP_SECURESESSION_CANTINITIALIZE_SRTPCONTEXT, "Unable to initialize libsrtp context" },
{ ERR_RTP_SECURESESSION_CANTINITMUTEX, "Unable to initialize a mutex" },
{ ERR_RTP_SECURESESSION_CONTEXTNOTINITIALIZED, "The libsrtp context initialization function must be called before it can be used" },
{ ERR_RTP_SECURESESSION_NOTENOUGHDATATOENCRYPT, "There's not enough RTP or RTCP data to encrypt" },
{ ERR_RTP_SECURESESSION_CANTENCRYPTRTPDATA, "Unable to encrypt RTP data" },
{ ERR_RTP_SECURESESSION_CANTENCRYPTRTCPDATA, "Unable to encrypt RTCP data" },
{ ERR_RTP_SECURESESSION_NOTENOUGHDATATODECRYPT, "There's not enough RTP or RTCP data to decrypt" },
{ ERR_RTP_SECURESESSION_CANTDECRYPTRTPDATA, "Unable to decrypt RTP data" },
{ ERR_RTP_SECURESESSION_CANTDECRYPTRTCPDATA, "Unable to decrypt RTCP data" },
{ ERR_RTP_ABORTDESC_ALREADYINIT, "The RTPAbortDescriptors instance is already initialized" },
{ ERR_RTP_ABORTDESC_NOTINIT, "The RTPAbortDescriptors instance is not yet initialized" },
{ ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS, "Unable to create two connected TCP sockets for the abort descriptors" },
{ ERR_RTP_ABORTDESC_CANTCREATEPIPE, "Unable to create a pipe for the abort descriptors" },
{ ERR_RTP_SESSION_THREADSAFETYCONFLICT, "For the background poll thread to be used, thread safety must also be set" },
{ ERR_RTP_SELECT_ERRORINSELECT, "Error in the call to 'select'" },
{ ERR_RTP_SELECT_SOCKETDESCRIPTORTOOLARGE, "A socket descriptor value is too large for a call to 'select' (exceeds FD_SETSIZE)" },
{ ERR_RTP_SELECT_ERRORINPOLL, "Error in the call to 'poll' or 'WSAPoll'" },
{ ERR_RTP_TCPTRANS_NOTINIT, "The TCP transmitter is not yet initialized" },
{ ERR_RTP_TCPTRANS_ALREADYINIT, "The TCP transmitter is already initialized" },
{ ERR_RTP_TCPTRANS_NOTCREATED, "The TCP transmitter is not yet created" },
{ ERR_RTP_TCPTRANS_ALREADYCREATED, "The TCP transmitter is already created" },
{ ERR_RTP_TCPTRANS_ILLEGALPARAMETERS, "The parameters for the TCP transmitter are invalid" },
{ ERR_RTP_TCPTRANS_CANTINITMUTEX, "Unable to initialize a mutex during the initialization of the TCP transmitter" },
{ ERR_RTP_TCPTRANS_ALREADYWAITING, "The TCP transmitter is already waiting for data" },
{ ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE, "The address specified is not a valid address for the TCP transmitter" },
{ ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED, "No socket was specified in the address used for the TCP transmitter" },
{ ERR_RTP_TCPTRANS_NOMULTICASTSUPPORT, "The TCP transmitter does not support multicasting" },
{ ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED, "The TCP transmitter does not support receive modes other than 'accept all'" },
{ ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size for the TCP transmitter is limited to 64KB" },
{ ERR_RTP_TCPTRANS_NOTWAITING, "The TCP transmitter is not waiting for data" },
{ ERR_RTP_TCPTRANS_SOCKETALREADYINDESTINATIONS, "The specified destination address (socket) was already added to the destination list of the TCP transmitter" },
{ ERR_RTP_TCPTRANS_SOCKETNOTFOUNDINDESTINATIONS, "The specified destination address (socket) was not found in the list of destinations of the TCP transmitter" },
{ ERR_RTP_TCPTRANS_ERRORINSEND, "An error occurred in the TCP transmitter while sending a packet" },
{ ERR_RTP_TCPTRANS_ERRORINRECV, "An error occurred in the TCP transmitter while receiving a packet" },
{ 0, 0 } };

std::string RTPGetErrorString(int errcode)
{
    int i;

    if (errcode >= 0)
        return std::string("No error");

    i = 0;
    while (ErrorDescriptions[i].code != 0)
    {
        if (ErrorDescriptions[i].code == errcode)
            return std::string(ErrorDescriptions[i].description);
        i++;
    }

    char str[16];

    RTP_SNPRINTF(str, 16, "(%d)", errcode);

    return std::string("Unknown error code") + std::string(str);
}

} // end namespace

