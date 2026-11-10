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

#ifndef INCLUDE_IPTUNNELROUTING_H
#define INCLUDE_IPTUNNELROUTING_H

#include "iptunneltypes.h"

// The platform independent checks behind IPTunnel's static functions (validate, validatePacket,
// planRouting), which IPTunnel also applies on open and to every packet it queues.

//! The disjoint routes of a managed routing configuration: the included prefixes less the excluded ones (at most 4096)
IPTunnelTypes::OperationResult iptunnelPlanRouting(const IPTunnelTypes::RoutingConfiguration& routing);
//! Whether a configuration can be opened: its families, MTU, queue limits, open mode, addresses and routing
IPTunnelTypes::ValidationResult iptunnelValidateConfiguration(const IPTunnelTypes::Configuration& configuration);
//! Whether a packet is one whole IPv4 or IPv6 datagram that fits the MTU
IPTunnelTypes::ValidationResult iptunnelValidatePacket(const QByteArray& packet, int mtu);

#endif // INCLUDE_IPTUNNELROUTING_H
