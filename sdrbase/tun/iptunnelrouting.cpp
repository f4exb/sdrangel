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

#include <algorithm>
#include <cstring>

#include "iptunnelrouting.h"

using namespace IPTunnelTypes;

namespace {

constexpr int MaxPlannedRoutes = 4096;

Error makeError(ErrorCode code, const QString& operation, const QString& message)
{
    Error error;
    error.code = code;
    error.operation = operation;
    error.message = message;
    return error;
}

int width(const QHostAddress& address)
{
    return (address.protocol() == QAbstractSocket::IPv4Protocol) ? 32
        : ((address.protocol() == QAbstractSocket::IPv6Protocol) ? 128 : 0);
}

QByteArray bytes(const QHostAddress& address)
{
    QByteArray result(16, 0);

    if (address.protocol() == QAbstractSocket::IPv4Protocol)
    {
        quint32 value = address.toIPv4Address();
        result[0] = char(value >> 24);
        result[1] = char(value >> 16);
        result[2] = char(value >> 8);
        result[3] = char(value);
    }
    else if (address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        Q_IPV6ADDR value = address.toIPv6Address();
        std::memcpy(result.data(), value.c, 16);
    }

    return result;
}

QHostAddress addressFromBytes(const QByteArray& value, bool ipv4)
{
    if (ipv4)
    {
        const auto *p = reinterpret_cast<const unsigned char *>(value.constData());
        return QHostAddress((quint32(p[0]) << 24) | (quint32(p[1]) << 16) |
                            (quint32(p[2]) << 8) | quint32(p[3]));
    }

    Q_IPV6ADDR result {};
    std::memcpy(result.c, value.constData(), 16);
    return QHostAddress(result);
}

Prefix canonical(Prefix prefix)
{
    QByteArray value = bytes(prefix.address);
    const int bits = width(prefix.address);

    for (int bit = prefix.length; bit < bits; ++bit) {
        value[bit / 8] = char(quint8(value[bit / 8]) & ~(0x80u >> (bit % 8)));
    }

    prefix.address = addressFromBytes(value, bits == 32);
    return prefix;
}

bool contains(const Prefix& outer, const Prefix& inner)
{
    if ((outer.address.protocol() != inner.address.protocol()) || (outer.length > inner.length)) {
        return false;
    }

    QByteArray a = bytes(outer.address);
    QByteArray b = bytes(inner.address);
    const int full = outer.length / 8;

    if (full && (std::memcmp(a.constData(), b.constData(), size_t(full)) != 0)) {
        return false;
    }

    const int remainder = outer.length % 8;

    if (!remainder) {
        return true;
    }

    const quint8 mask = quint8(0xffu << (8 - remainder));
    return (quint8(a[full]) & mask) == (quint8(b[full]) & mask);
}

QPair<Prefix, Prefix> split(const Prefix& prefix)
{
    Prefix first = prefix;
    Prefix second = prefix;
    ++first.length;
    ++second.length;
    QByteArray value = bytes(second.address);
    const int bit = prefix.length;
    value[bit / 8] = char(quint8(value[bit / 8]) | (0x80u >> (bit % 8)));
    second.address = addressFromBytes(value, width(prefix.address) == 32);
    return {first, second};
}

bool subtractOne(const Prefix& route, const Prefix& exclusion, QVector<Prefix>& output)
{
    if ((route.address.protocol() != exclusion.address.protocol()) || !contains(route, exclusion))
    {
        output.append(route);
        return true;
    }

    if (contains(exclusion, route)) {
        return true;
    }

    const auto children = split(route);

    if (!subtractOne(children.first, exclusion, output) || (output.size() > MaxPlannedRoutes)) {
        return false;
    }

    return subtractOne(children.second, exclusion, output) && (output.size() <= MaxPlannedRoutes);
}

void removeRedundant(QVector<Prefix>& prefixes)
{
    std::sort(prefixes.begin(), prefixes.end(), [](const Prefix& a, const Prefix& b) {
        if (a.address.protocol() != b.address.protocol()) {
            return a.address.protocol() < b.address.protocol();
        }

        if (a.length != b.length) {
            return a.length < b.length;
        }

        return a.address.toString() < b.address.toString();
    });

    QVector<Prefix> result;

    for (const Prefix& prefix : prefixes)
    {
        bool covered = false;

        for (const Prefix& kept : result)
        {
            if (contains(kept, prefix))
            {
                covered = true;
                break;
            }
        }

        if (!covered) {
            result.append(prefix);
        }
    }

    prefixes = result;
}

}

bool Prefix::isValid() const
{
    const int bits = width(address);
    return bits && (length >= 0) && (length <= bits);
}

QString Prefix::toString() const
{
    return QStringLiteral("%1/%2").arg(address.toString()).arg(length);
}

bool Prefix::operator==(const Prefix& other) const
{
    if (!isValid() || !other.isValid()) {
        return (address == other.address) && (length == other.length);
    }

    Prefix a = canonical(*this);
    Prefix b = canonical(other);
    return (a.address == b.address) && (a.length == b.length);
}

OperationResult iptunnelPlanRouting(const RoutingConfiguration& routing)
{
    OperationResult result;

    if (routing.mode == ConfigurationMode::External) {
        return result;
    }

    if (routing.metric < 0)
    {
        result.success = false;
        result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("routing"), QStringLiteral("Route metric cannot be negative"));
        return result;
    }

    QVector<Prefix> includes;
    QVector<Prefix> excludes;

    for (Prefix prefix : routing.include)
    {
        if (!prefix.isValid())
        {
            result.success = false;
            result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("routing"), QStringLiteral("Invalid included prefix"));
            return result;
        }

        prefix = canonical(prefix);

        if (prefix.isDefault() && !routing.allowDefaultRoute)
        {
            result.success = false;
            result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("routing"), QStringLiteral("Default routes require allowDefaultRoute"));
            return result;
        }

        includes.append(prefix);
    }

    for (Prefix prefix : routing.exclude)
    {
        if (!prefix.isValid())
        {
            result.success = false;
            result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("routing"), QStringLiteral("Invalid excluded prefix"));
            return result;
        }

        excludes.append(canonical(prefix));
    }

    removeRedundant(includes);
    removeRedundant(excludes);

    QVector<Prefix> planned = includes;

    for (const Prefix& exclusion : excludes)
    {
        bool intersects = false;

        for (const Prefix& include : includes)
        {
            if (contains(include, exclusion) || contains(exclusion, include))
            {
                intersects = true;
                break;
            }
        }

        if (!intersects) {
            result.routePlan.warnings.append(QStringLiteral("Exclusion %1 is outside every included prefix").arg(exclusion.toString()));
        }

        QVector<Prefix> next;

        for (const Prefix& route : planned)
        {
            if (contains(exclusion, route)) {
                continue;
            }

            if (!subtractOne(route, exclusion, next))
            {
                result.success = false;
                result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("routing"), QStringLiteral("Route exclusions expand beyond the 4096-route limit"));
                return result;
            }
        }

        planned = next;
    }

    removeRedundant(planned);
    result.routePlan.routes = planned;
    return result;
}

ValidationResult iptunnelValidateConfiguration(const Configuration& configuration)
{
    ValidationResult result;

    auto fail = [&](const QString& message) {
        result.valid = false;
        result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("open"), message);
    };

    if (!configuration.ipv4 && !configuration.ipv6)
    {
        fail(QStringLiteral("At least one IP family must be enabled"));
        return result;
    }

    if ((configuration.mtu < 68) || (configuration.mtu > 65535) || (configuration.ipv6 && (configuration.mtu < 1280)))
    {
        fail(QStringLiteral("MTU must be 68..65535, and at least 1280 with IPv6 enabled"));
        return result;
    }

    const auto validLimits = [](const QueueLimits& limits) {
        return (limits.packets > 0) && (limits.bytes > 0) && (limits.maximumAgeMs >= 0);
    };

    if (!validLimits(configuration.outboundQueue) || !validLimits(configuration.inboundQueue))
    {
        fail(QStringLiteral("Queue packet/byte limits must be positive and age must be non-negative"));
        return result;
    }

    if ((configuration.openMode == OpenMode::OpenExisting) && configuration.interfaceName.isEmpty())
    {
        fail(QStringLiteral("OpenExisting requires an interface name"));
        return result;
    }

    if ((configuration.openMode == OpenMode::AdoptDescriptor) && (configuration.adoptedDescriptor < 0))
    {
        fail(QStringLiteral("AdoptDescriptor requires a valid descriptor"));
        return result;
    }

    for (const Address& address : configuration.addresses)
    {
        Prefix prefix {address.address, address.prefixLength};

        if (!prefix.isValid())
        {
            fail(QStringLiteral("Invalid local address or prefix length"));
            return result;
        }

        if (((address.address.protocol() == QAbstractSocket::IPv4Protocol) && !configuration.ipv4) ||
            ((address.address.protocol() == QAbstractSocket::IPv6Protocol) && !configuration.ipv6))
        {
            fail(QStringLiteral("A local address uses a disabled IP family"));
            return result;
        }

        if (!address.peer.isNull() && (address.peer.protocol() != address.address.protocol()))
        {
            fail(QStringLiteral("A point-to-point peer must use the local address family"));
            return result;
        }
    }

    OperationResult routeResult = iptunnelPlanRouting(configuration.routing);

    if (!routeResult.success)
    {
        result.valid = false;
        result.error = routeResult.error;
    }

    if (configuration.routing.mode == ConfigurationMode::Managed)
    {
        for (const Prefix& prefix : routeResult.routePlan.routes)
        {
            if (((prefix.address.protocol() == QAbstractSocket::IPv4Protocol) && !configuration.ipv4) ||
                ((prefix.address.protocol() == QAbstractSocket::IPv6Protocol) && !configuration.ipv6))
            {
                fail(QStringLiteral("A managed route uses a disabled IP family"));
                return result;
            }
        }
    }

    return result;
}

ValidationResult iptunnelValidatePacket(const QByteArray& packet, int mtu)
{
    ValidationResult result;

    auto fail = [&](const QString& message) {
        result.valid = false;
        result.error = makeError(ErrorCode::InvalidConfiguration, QStringLiteral("packet"), message);
    };

    if (packet.isEmpty() || (packet.size() > 65535))
    {
        fail(QStringLiteral("Packet length is outside 1..65535"));
        return result;
    }

    if (packet.size() > mtu)
    {
        fail(QStringLiteral("Packet exceeds tunnel MTU"));
        return result;
    }

    const auto *p = reinterpret_cast<const quint8 *>(packet.constData());
    const int version = p[0] >> 4;

    if (version == 4)
    {
        if (packet.size() < 20)
        {
            fail(QStringLiteral("Truncated IPv4 header"));
            return result;
        }

        const int ihl = (p[0] & 0x0f) * 4;
        const int total = (int(p[2]) << 8) | p[3];

        if ((ihl < 20) || (ihl > packet.size()) || (total < ihl) || (total != packet.size())) {
            fail(QStringLiteral("Invalid IPv4 header or total length"));
        }
    }
    else if (version == 6)
    {
        if (packet.size() < 40)
        {
            fail(QStringLiteral("Truncated IPv6 header"));
            return result;
        }

        const int payload = (int(p[4]) << 8) | p[5];

        if ((payload == 0) && (packet.size() != 40))
        {
            fail(QStringLiteral("IPv6 jumbograms are not supported"));
            return result;
        }

        if (40 + payload != packet.size()) {
            fail(QStringLiteral("Invalid IPv6 payload length"));
        }
    }
    else
    {
        fail(QStringLiteral("Packet is neither IPv4 nor IPv6"));
    }

    return result;
}
