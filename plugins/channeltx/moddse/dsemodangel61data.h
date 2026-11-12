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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61DATA_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61DATA_H_

#include <cstdint>
#include <string>
#include <vector>

#include "dsp/dv/dvangel61file.h"
#include "dsp/dv/dvangel61ip.h"

/**
 * What the processor asks of the Angel61 protocol (DSEModAngel61) besides its frames: APRS and
 * API data messages, file transfer (3.4) and IP tunnelling (3.5, 3.6).
 */
class DSEModAngel61Data
{
public:
    virtual ~DSEModAngel61Data() {}

    //! An APRS information field (3.3) sent as a data transmission of content type 2
    virtual void setAPRS(const std::string& info) = 0;
    //! The next transmission is a data transmission whatever the speech input: the channel keyed up
    //! for a message (forMessage: it carries the configured message, as one with no speech input
    //! does) or for the tunnel's datagrams alone, and it must end by itself when they are out (a voice
    //! transmission ends only at the key down, which then never came). Consumed by the next start()
    virtual void setDataOnlyNext(bool forMessage) = 0;
    //! One explicit API data message, consumed by the next start()
    virtual void setDataMessage(const std::vector<uint8_t>& data, int contentType) = 0;

    //! Offer a file: the next transmission carries it, and the rounds that follow the answers
    virtual bool startFile(const std::string& name, const std::vector<uint8_t>& data) = 0;
    //! What the receiving station answered, from a DSD Demodulator of this instance
    virtual void handleFileControl(const DVAngel61FileControl& control) = 0;
    //! The answer this station owes a station sending it a file (3.4): queued as a content type 5
    //! message; a transmission keyed up for the queued messages alone goes to `destination` (the
    //! file's sender, empty when the receiver could not say who it was)
    virtual bool queueFileControl(const DVAngel61FileControl& control, const std::string& destination) = 0;
    //! The receiving station's answer did not come: the offer again, alone (DVAngel61FileSender::handleNoAnswer)
    virtual bool fileNoAnswer() = 0;
    virtual const DVAngel61FileSender& getFileSender() const = 0;
    //! The file has blocks to send: the transmission is kept up, or keyed up again for a round
    virtual bool fileWantsTransmission() const = 0;

    //! Queue one datagram as a content type 6 message. False when it does not fit or the queue is full
    virtual bool queueDatagram(const std::vector<uint8_t>& datagram) = 0;
    //! Queue a content type 7 tunnel control message
    virtual bool queueTunnelControl(const DVAngel61IP::Control& control) = 0;
    //! The advertise this station sends: what it will route, and how much of it at a time
    virtual void setTunnel(bool enabled, const DVAngel61IP::Control& advertise) = 0;
    //! Tunnel control heard from the peer (its compression terms, its lost contexts)
    virtual void handlePeerTunnelControl(const DVAngel61IP::Control& control) = 0;

    /**
     * DTMF (3.8): keys for the DTMF slot, with their tone length and gap; newSequence: they begin a
     * sequence. hold (a Send): the voice is replaced by idle frames while they play, and the
     * transmission lasts until they have played. only: the transmission is for them (the preamble,
     * idle frames, the last frame). False when no key is valid.
     */
    virtual bool queueDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence, bool hold, bool only) = 0;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61DATA_H_
