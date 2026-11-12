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

#include "iptunnelbackend.h"

using namespace IPTunnelTypes;

namespace {

/**
 * The backend of a platform with no tunnel support (not Windows, Linux or macOS): its thread
 * reports at once that the tunnel could not be opened, with an Unsupported error.
 */
class UnsupportedBackend final : public IPTunnelBackend
{
public:
    UnsupportedBackend(const Configuration& configuration, const Callbacks& callbacks) :
        IPTunnelBackend(configuration, callbacks)
    {}

protected:
    void wake() override {}

    void run() override
    {
        Error error {ErrorCode::Unsupported, QStringLiteral("open"), QStringLiteral("IP tunnels are not supported on this platform"), 0};
        m_callbacks.opened({}, error);
    }
};

}

IPTunnelBackend *iptunnelCreateBackend(const Configuration& configuration, const IPTunnelBackend::Callbacks& callbacks)
{
    return new UnsupportedBackend(configuration, callbacks);
}

Capabilities iptunnelBackendCapabilities()
{
    Capabilities c;
    c.backend = QStringLiteral("unsupported");
    return c;
}
