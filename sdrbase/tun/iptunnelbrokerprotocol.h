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

#ifndef INCLUDE_IPTUNNELBROKERPROTOCOL_H
#define INCLUDE_IPTUNNELBROKERPROTOCOL_H

#include <cstring>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QString>

#include "iptunneltypes.h"

/**
 * The wire protocol between an unelevated SDRangel and sdrangel-tunnel-helper.exe, the elevated
 * helper that owns the Wintun interface on Windows (iptunnel_win.cpp, iptunnelhelper_main.cpp).
 *
 * SDRangel listens on a private local pipe (a QLocalServer for the current user only) and starts
 * the helper through a UAC prompt, passing it the pipe's name and a random 32 byte token. The helper
 * connects and sends Authenticate with the token; SDRangel then sends the Configuration, the helper
 * opens the tunnel and answers Opened. After that, IP packets flow as PacketFromOS (helper to
 * SDRangel) and PacketToOS (SDRangel to helper, acknowledged by PacketAccepted), routing changes as
 * ApplyRouting and RoutingFinished, and failures as Error. Close, or the pipe disconnecting, ends
 * the session and the helper exits.
 *
 * Each message is a frame: a HeaderSize byte header (Magic, Version, the Message type and the
 * payload's length, in QDataStream's big-endian order) and then the payload. The encode and decode
 * functions build and check the payloads; the decoders bound every count and string they read, as
 * the payload comes from another process.
 */
namespace IPTunnelBrokerProtocol
{
    constexpr quint32 Magic = 0x5344544e; // "SDTN"
    constexpr quint8 Version = 1;
    constexpr int HeaderSize = 10;
    constexpr quint32 MaximumPayload = 2 * 1024 * 1024;

    /** A frame's type. The values are on the wire: new types go at the end */
    enum class Message : quint8 {
        Authenticate = 1,   //!< helper -> SDRangel: the token it was started with
        Configuration,      //!< SDRangel -> helper: the tunnel's Configuration (encodeConfiguration)
        Opened,             //!< helper -> SDRangel: the interface opened, or the error (encodeOpened)
        PacketFromOS,       //!< helper -> SDRangel: an IP packet the OS sent into the tunnel
        PacketToOS,         //!< SDRangel -> helper: an IP packet to inject into the OS
        PacketAccepted,     //!< helper -> SDRangel: a PacketToOS was queued (its size, qint64)
        ApplyRouting,       //!< SDRangel -> helper: a routing change (encodeRouting)
        RoutingFinished,    //!< helper -> SDRangel: its result (encodeRoutingResult)
        Error,              //!< helper -> SDRangel: an error, and whether it is fatal (encodeError)
        Close               //!< SDRangel -> helper: close the tunnel and exit
    };

    /** takeFrame's result: no whole frame buffered yet, a frame taken, or a stream not to be trusted */
    enum class ParseResult { NeedMore, Ready, Invalid };

    inline QDataStream& writePrefix(QDataStream& stream, const IPTunnelTypes::Prefix& prefix)
    {
        stream << prefix.address << qint32(prefix.length);
        return stream;
    }

    inline QDataStream& readPrefix(QDataStream& stream, IPTunnelTypes::Prefix& prefix)
    {
        qint32 length = -1;
        stream >> prefix.address >> length;
        prefix.length = length;
        return stream;
    }

    inline QDataStream& writeRouting(QDataStream& stream, const IPTunnelTypes::RoutingConfiguration& routing)
    {
        stream << quint8(routing.mode) << quint32(routing.include.size());

        for (const auto& prefix : routing.include) {
            writePrefix(stream, prefix);
        }

        stream << quint32(routing.exclude.size());

        for (const auto& prefix : routing.exclude) {
            writePrefix(stream, prefix);
        }

        stream << routing.allowDefaultRoute << qint32(routing.metric);
        return stream;
    }

    inline bool readRouting(QDataStream& stream, IPTunnelTypes::RoutingConfiguration& routing)
    {
        quint8 mode = 0;
        quint32 count = 0;
        stream >> mode >> count;

        if ((mode > quint8(IPTunnelTypes::ConfigurationMode::External)) || (count > 4096)) {
            return false;
        }

        routing.mode = IPTunnelTypes::ConfigurationMode(mode);
        routing.include.clear();
        routing.include.reserve(int(count));

        for (quint32 i = 0; i < count; ++i)
        {
            IPTunnelTypes::Prefix prefix;
            readPrefix(stream, prefix);
            routing.include.append(prefix);
        }

        stream >> count;

        if (count > 4096) {
            return false;
        }

        routing.exclude.clear();
        routing.exclude.reserve(int(count));

        for (quint32 i = 0; i < count; ++i)
        {
            IPTunnelTypes::Prefix prefix;
            readPrefix(stream, prefix);
            routing.exclude.append(prefix);
        }

        qint32 metric = 0;
        stream >> routing.allowDefaultRoute >> metric;
        routing.metric = metric;
        return stream.status() == QDataStream::Ok;
    }

    inline QByteArray encodeConfiguration(const IPTunnelTypes::Configuration& configuration)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << configuration.logicalId << configuration.interfaceName << configuration.displayName
               << quint8(configuration.openMode) << quint8(configuration.configurationMode)
               << quint32(configuration.addresses.size());

        for (const auto& address : configuration.addresses) {
            stream << address.address << qint32(address.prefixLength) << address.peer;
        }

        stream << configuration.ipv4 << configuration.ipv6 << qint32(configuration.mtu)
               << qint32(configuration.outboundQueue.packets) << qint64(configuration.outboundQueue.bytes)
               << qint32(configuration.outboundQueue.maximumAgeMs)
               << qint32(configuration.inboundQueue.packets) << qint64(configuration.inboundQueue.bytes)
               << qint32(configuration.inboundQueue.maximumAgeMs);
        writeRouting(stream, configuration.routing);
        stream << qint64(configuration.adoptedDescriptor) << configuration.takeDescriptorOwnership
               << qint32(configuration.wintunRingCapacity);
        return payload;
    }

    inline bool decodeConfiguration(const QByteArray& payload, IPTunnelTypes::Configuration& configuration)
    {
        QDataStream stream(payload);
        quint8 openMode = 0;
        quint8 configurationMode = 0;
        quint32 count = 0;
        qint32 value = 0;
        qint64 descriptor = -1;
        stream >> configuration.logicalId >> configuration.interfaceName >> configuration.displayName
               >> openMode >> configurationMode >> count;

        if ((openMode > quint8(IPTunnelTypes::OpenMode::AdoptDescriptor))
            || (configurationMode > quint8(IPTunnelTypes::ConfigurationMode::External))
            || (count > 64))
        {
            return false;
        }

        configuration.openMode = IPTunnelTypes::OpenMode(openMode);
        configuration.configurationMode = IPTunnelTypes::ConfigurationMode(configurationMode);
        configuration.addresses.clear();
        configuration.addresses.reserve(int(count));

        for (quint32 i = 0; i < count; ++i)
        {
            IPTunnelTypes::Address address;
            stream >> address.address >> value >> address.peer;
            address.prefixLength = value;
            configuration.addresses.append(address);
        }

        stream >> configuration.ipv4 >> configuration.ipv6 >> value;
        configuration.mtu = value;
        stream >> value;
        configuration.outboundQueue.packets = value;
        stream >> configuration.outboundQueue.bytes;
        stream >> value;
        configuration.outboundQueue.maximumAgeMs = value;
        stream >> value;
        configuration.inboundQueue.packets = value;
        stream >> configuration.inboundQueue.bytes;
        stream >> value;
        configuration.inboundQueue.maximumAgeMs = value;

        if (!readRouting(stream, configuration.routing)) {
            return false;
        }

        stream >> descriptor >> configuration.takeDescriptorOwnership >> value;
        configuration.adoptedDescriptor = qintptr(descriptor);
        configuration.wintunRingCapacity = value;

        return (stream.status() == QDataStream::Ok) && stream.atEnd()
            && (configuration.logicalId.size() <= 256) && (configuration.interfaceName.size() <= 256)
            && (configuration.displayName.size() <= 512);
    }

    inline QDataStream& writeError(QDataStream& stream, const IPTunnelTypes::Error& error)
    {
        stream << quint8(error.code) << error.operation << error.message << qint64(error.nativeCode);
        return stream;
    }

    inline bool readError(QDataStream& stream, IPTunnelTypes::Error& error)
    {
        quint8 code = 0;
        stream >> code >> error.operation >> error.message >> error.nativeCode;

        if ((code > quint8(IPTunnelTypes::ErrorCode::NativeFailure))
            || (error.operation.size() > 256) || (error.message.size() > 4096))
        {
            return false;
        }

        error.code = IPTunnelTypes::ErrorCode(code);
        return stream.status() == QDataStream::Ok;
    }

    inline QByteArray encodeOpened(const IPTunnelTypes::InterfaceInfo& info, const IPTunnelTypes::Error& error)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << info.name << quint32(info.index) << qint32(info.mtu);
        writeError(stream, error);
        return payload;
    }

    inline bool decodeOpened(const QByteArray& payload, IPTunnelTypes::InterfaceInfo& info, IPTunnelTypes::Error& error)
    {
        QDataStream stream(payload);
        qint32 mtu = 0;
        stream >> info.name >> info.index >> mtu;
        info.mtu = mtu;
        return readError(stream, error) && stream.atEnd() && (info.name.size() <= 256);
    }

    inline QByteArray encodeError(const IPTunnelTypes::Error& error, bool fatal)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        writeError(stream, error) << fatal;
        return payload;
    }

    inline bool decodeError(const QByteArray& payload, IPTunnelTypes::Error& error, bool& fatal)
    {
        QDataStream stream(payload);
        return readError(stream, error) && ((stream >> fatal).status() == QDataStream::Ok) && stream.atEnd();
    }

    inline QByteArray encodeRouting(IPTunnelTypes::RequestId id, const IPTunnelTypes::RoutingConfiguration& routing)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << quint64(id);
        writeRouting(stream, routing);
        return payload;
    }

    inline bool decodeRouting(const QByteArray& payload, IPTunnelTypes::RequestId& id, IPTunnelTypes::RoutingConfiguration& routing)
    {
        QDataStream stream(payload);
        quint64 value = 0;
        stream >> value;
        id = value;
        return readRouting(stream, routing) && stream.atEnd();
    }

    inline QByteArray encodeRoutingResult(IPTunnelTypes::RequestId id, const IPTunnelTypes::OperationResult& result)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << quint64(id) << result.success;
        writeError(stream, result.error);
        stream << quint32(result.routePlan.routes.size());

        for (const auto& prefix : result.routePlan.routes) {
            writePrefix(stream, prefix);
        }

        stream << result.routePlan.warnings;
        return payload;
    }

    inline bool decodeRoutingResult(const QByteArray& payload, IPTunnelTypes::RequestId& id, IPTunnelTypes::OperationResult& result)
    {
        QDataStream stream(payload);
        quint64 value = 0;
        quint32 count = 0;
        stream >> value >> result.success;
        id = value;

        if (!readError(stream, result.error)) {
            return false;
        }

        stream >> count;

        if (count > 4096) {
            return false;
        }

        result.routePlan.routes.clear();
        result.routePlan.routes.reserve(int(count));

        for (quint32 i = 0; i < count; ++i)
        {
            IPTunnelTypes::Prefix prefix;
            readPrefix(stream, prefix);
            result.routePlan.routes.append(prefix);
        }

        stream >> result.routePlan.warnings;
        return (stream.status() == QDataStream::Ok) && stream.atEnd() && (result.routePlan.warnings.size() <= 4096);
    }

    //! A message as it goes on the pipe: the header, then the payload
    inline QByteArray frame(Message message, const QByteArray& payload = {})
    {
        QByteArray result;
        result.reserve(HeaderSize + payload.size());
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << quint32(Magic) << quint8(Version) << quint8(message) << quint32(payload.size());
        result.append(payload);
        return result;
    }

    //! The first whole frame of what was read from the pipe, removed from the buffer
    inline ParseResult takeFrame(QByteArray& buffer, Message& message, QByteArray& payload, QString& error)
    {
        if (buffer.size() < HeaderSize) {
            return ParseResult::NeedMore;
        }

        QDataStream stream(buffer.left(HeaderSize));
        quint32 magic = 0;
        quint32 length = 0;
        quint8 version = 0;
        quint8 type = 0;
        stream >> magic >> version >> type >> length;

        if ((stream.status() != QDataStream::Ok) || (magic != Magic) || (version != Version)
            || (type < quint8(Message::Authenticate)) || (type > quint8(Message::Close)) || (length > MaximumPayload))
        {
            error = QStringLiteral("Invalid tunnel-helper protocol frame");
            return ParseResult::Invalid;
        }

        if (buffer.size() < HeaderSize + int(length)) {
            return ParseResult::NeedMore;
        }

        message = Message(type);
        payload = buffer.mid(HeaderSize, int(length));
        buffer.remove(0, HeaderSize + int(length));
        return ParseResult::Ready;
    }
}

#endif // INCLUDE_IPTUNNELBROKERPROTOCOL_H
