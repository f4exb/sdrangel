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

#include <QMutexLocker>

#include "iptunnelbackend.h"

IPTunnelBackend::IPTunnelBackend(const IPTunnelTypes::Configuration& configuration, const Callbacks& callbacks) :
    m_configuration(configuration),
    m_callbacks(callbacks)
{
}

void IPTunnelBackend::stop()
{
    m_stopping.store(true);
    wake();
}

void IPTunnelBackend::packetsAvailable()
{
    wake();
}

void IPTunnelBackend::applyRouting(IPTunnelTypes::RequestId id, const IPTunnelTypes::RoutingConfiguration& routing)
{
    {
        QMutexLocker locker(&m_commandMutex);
        m_routingCommands.enqueue({id, routing});
    }

    wake();
}

bool IPTunnelBackend::takeRoutingCommand(RoutingCommand& command)
{
    QMutexLocker locker(&m_commandMutex);

    if (m_routingCommands.isEmpty()) {
        return false;
    }

    command = m_routingCommands.dequeue();
    return true;
}
