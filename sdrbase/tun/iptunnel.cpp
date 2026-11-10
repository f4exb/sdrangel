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

#include <memory>

#include <QDateTime>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QTimer>

#include "iptunnel.h"
#include "iptunnelbackend.h"
#include "iptunnelrouting.h"

using namespace IPTunnelTypes;

namespace {

struct QueuedPacket
{
    QByteArray data;
    qint64 queuedAt;
};

}

class IPTunnelPrivate
{
public:
    explicit IPTunnelPrivate(IPTunnel *owner) :
        q(owner)
    {}

    IPTunnel *q;
    mutable QMutex mutex;
    State currentState = State::Closed;
    InterfaceInfo info;
    Statistics stats;
    Configuration configuration;
    QQueue<QueuedPacket> outbound;
    QQueue<QueuedPacket> inbound;
    qint64 outboundBytes = 0;
    qint64 inboundBytes = 0;
    bool outboundSignalArmed = false;
    bool inboundWasFull = false;
    RequestId nextRequest = 1;
    RequestId openRequest = 0;
    quint64 generation = 0;
    std::unique_ptr<IPTunnelBackend> backend;

    RequestId requestId()
    {
        QMutexLocker locker(&mutex);
        return nextRequest++;
    }

    static bool expired(const QueuedPacket& packet, const QueueLimits& limits)
    {
        return (limits.maximumAgeMs > 0) && (QDateTime::currentMSecsSinceEpoch() - packet.queuedAt > limits.maximumAgeMs);
    }

    bool queueFull(const QueueLimits& limits, int packets, qint64 bytes, qint64 next) const
    {
        return (packets >= limits.packets) || (bytes + next > limits.bytes);
    }

    bool familyEnabled(const QByteArray& packet) const
    {
        const int version = quint8(packet[0]) >> 4;
        return ((version == 4) && configuration.ipv4) || ((version == 6) && configuration.ipv6);
    }

    void expireOutboundLocked()
    {
        while (!outbound.isEmpty() && expired(outbound.head(), configuration.outboundQueue))
        {
            outboundBytes -= outbound.dequeue().data.size();
            ++stats.outboundExpired;
        }

        stats.outboundQueuedPackets = outbound.size();
        stats.outboundQueuedBytes = outboundBytes;
    }

    void expireInboundLocked()
    {
        while (!inbound.isEmpty() && expired(inbound.head(), configuration.inboundQueue))
        {
            inboundBytes -= inbound.dequeue().data.size();
            ++stats.inboundExpired;
        }

        stats.inboundQueuedPackets = inbound.size();
        stats.inboundQueuedBytes = inboundBytes;
    }

    void postState(State state)
    {
        QMetaObject::invokeMethod(q, [this, state]() {
            {
                QMutexLocker locker(&mutex);
                currentState = state;
            }

            emit q->stateChanged(state);
        }, Qt::QueuedConnection);
    }

    void backendOpened(quint64 expected, const InterfaceInfo& interfaceInfo, const Error& error)
    {
        QMetaObject::invokeMethod(q, [this, expected, interfaceInfo, error]() {
            RequestId id;

            {
                QMutexLocker locker(&mutex);

                if (expected != generation) {
                    return;
                }

                id = openRequest;
                info = interfaceInfo;
                currentState = error.isError() ? State::Error : State::Open;
            }

            emit q->stateChanged(error.isError() ? State::Error : State::Open);
            OperationResult result;
            result.success = !error.isError();
            result.error = error;
            emit q->operationFinished(id, result);

            if (error.isError()) {
                emit q->errorOccurred(error);
            }
        }, Qt::QueuedConnection);
    }

    void backendPacket(quint64 expected, const QByteArray& packet)
    {
        bool notify = false;

        {
            QMutexLocker locker(&mutex);

            if ((expected != generation) || ((currentState != State::Opening) && (currentState != State::Open))) {
                return;
            }

            ++stats.packetsReadFromOS;
            stats.bytesReadFromOS += packet.size();
            ValidationResult validation = iptunnelValidatePacket(packet, configuration.mtu);

            if (!validation.valid || !familyEnabled(packet))
            {
                if ((packet.size() > configuration.mtu) || (packet.size() > 65535)) {
                    ++stats.oversizedPackets;
                } else {
                    ++stats.invalidPackets;
                }

                return;
            }

            expireOutboundLocked();

            if (queueFull(configuration.outboundQueue, outbound.size(), outboundBytes, packet.size()))
            {
                ++stats.outboundOverflow;
                return;
            }

            outbound.enqueue({packet, QDateTime::currentMSecsSinceEpoch()});
            outboundBytes += packet.size();
            stats.outboundQueuedPackets = outbound.size();
            stats.outboundQueuedBytes = outboundBytes;

            if (!outboundSignalArmed)
            {
                outboundSignalArmed = true;
                notify = true;
            }
        }

        if (notify)
        {
            QMetaObject::invokeMethod(q, [this]() {
                emit q->outboundPacketsAvailable();
            }, Qt::QueuedConnection);
        }
    }

    bool takeInbound(quint64 expected, QByteArray& packet)
    {
        bool capacity = false;

        {
            QMutexLocker locker(&mutex);

            if ((expected != generation) || (currentState == State::Closed)) {
                return false;
            }

            expireInboundLocked();

            if (inbound.isEmpty()) {
                return false;
            }

            packet = inbound.dequeue().data;
            inboundBytes -= packet.size();
            stats.inboundQueuedPackets = inbound.size();
            stats.inboundQueuedBytes = inboundBytes;

            if (inboundWasFull)
            {
                inboundWasFull = false;
                capacity = true;
            }
        }

        if (capacity)
        {
            QMetaObject::invokeMethod(q, [this]() {
                emit q->inboundCapacityAvailable();
            }, Qt::QueuedConnection);
        }

        return true;
    }

    void packetWritten(quint64 expected, qint64 bytes)
    {
        QMutexLocker locker(&mutex);

        if (expected == generation)
        {
            ++stats.packetsWrittenToOS;
            stats.bytesWrittenToOS += bytes;
        }
    }

    void routeFinished(quint64 expected, RequestId id, const OperationResult& result)
    {
        QMetaObject::invokeMethod(q, [this, expected, id, result]() {
            {
                QMutexLocker locker(&mutex);

                if (expected != generation) {
                    return;
                }
            }

            emit q->operationFinished(id, result);

            if (!result.success) {
                emit q->errorOccurred(result.error);
            }
        }, Qt::QueuedConnection);
    }

    void backendError(quint64 expected, const Error& error, bool fatal)
    {
        QMetaObject::invokeMethod(q, [this, expected, error, fatal]() {
            {
                QMutexLocker locker(&mutex);

                if (expected != generation) {
                    return;
                }

                ++stats.nativeFailures;

                if (fatal) {
                    currentState = State::Error;
                }
            }

            emit q->errorOccurred(error);

            if (fatal) {
                emit q->stateChanged(State::Error);
            }
        }, Qt::QueuedConnection);
    }
};

IPTunnel::IPTunnel(QObject *parent) :
    QObject(parent),
    m_private(new IPTunnelPrivate(this))
{
    qRegisterMetaType<IPTunnelTypes::State>();
    qRegisterMetaType<IPTunnelTypes::Error>();
    qRegisterMetaType<IPTunnelTypes::OperationResult>();
    qRegisterMetaType<IPTunnelTypes::InterfaceInfo>();
}

IPTunnel::~IPTunnel()
{
    close();
    delete m_private;
}

IPTunnel::Capabilities IPTunnel::capabilities()
{
    return iptunnelBackendCapabilities();
}

IPTunnel::ValidationResult IPTunnel::validate(const Configuration& c)
{
    return iptunnelValidateConfiguration(c);
}

IPTunnel::ValidationResult IPTunnel::validatePacket(const QByteArray& p, int mtu)
{
    return iptunnelValidatePacket(p, mtu);
}

IPTunnel::OperationResult IPTunnel::planRouting(const RoutingConfiguration& r)
{
    return iptunnelPlanRouting(r);
}

IPTunnel::RequestId IPTunnel::open(const Configuration& configuration)
{
    RequestId id = m_private->requestId();
    ValidationResult validation = validate(configuration);

    if (!validation.valid)
    {
        QTimer::singleShot(0, this, [this, id, validation]() {
            OperationResult result;
            result.success = false;
            result.error = validation.error;
            emit operationFinished(id, result);
            emit errorOccurred(validation.error);
        });

        return id;
    }

    quint64 generation;

    {
        QMutexLocker locker(&m_private->mutex);

        if (m_private->currentState != State::Closed)
        {
            Error error {ErrorCode::Busy, QStringLiteral("open"), QStringLiteral("Tunnel is not closed"), 0};

            QTimer::singleShot(0, this, [this, id, error]() {
                OperationResult r;
                r.success = false;
                r.error = error;
                emit operationFinished(id, r);
                emit errorOccurred(error);
            });

            return id;
        }

        m_private->configuration = configuration;
        m_private->currentState = State::Opening;
        m_private->openRequest = id;
        generation = ++m_private->generation;
        m_private->stats = {};
    }

    emit stateChanged(State::Opening);
    IPTunnelBackend::Callbacks callbacks;

    callbacks.opened = [p=m_private, generation](const InterfaceInfo& info, const Error& error) {
        p->backendOpened(generation, info, error);
    };
    callbacks.packetReceived = [p=m_private, generation](const QByteArray& packet) {
        p->backendPacket(generation, packet);
    };
    callbacks.takePacketToOS = [p=m_private, generation](QByteArray& packet) {
        return p->takeInbound(generation, packet);
    };
    callbacks.packetWritten = [p=m_private, generation](qint64 bytes) {
        p->packetWritten(generation, bytes);
    };
    callbacks.routingFinished = [p=m_private, generation](RequestId rid, const OperationResult& result) {
        p->routeFinished(generation, rid, result);
    };
    callbacks.error = [p=m_private, generation](const Error& error, bool fatal) {
        p->backendError(generation, error, fatal);
    };

    std::unique_ptr<IPTunnelBackend> backend(iptunnelCreateBackend(configuration, callbacks));

    {
        QMutexLocker locker(&m_private->mutex);

        if ((generation != m_private->generation) || (m_private->currentState != State::Opening)) {
            return id;
        }

        m_private->backend = std::move(backend);
        m_private->backend->start();
    }

    return id;
}

IPTunnel::RequestId IPTunnel::previewRouting(const RoutingConfiguration& routing)
{
    RequestId id = m_private->requestId();
    OperationResult result = planRouting(routing);

    QTimer::singleShot(0, this, [this, id, result]() {
        emit operationFinished(id, result);
    });

    return id;
}

IPTunnel::RequestId IPTunnel::applyRouting(const RoutingConfiguration& routing)
{
    RequestId id = m_private->requestId();
    OperationResult plan = planRouting(routing);
    bool submitted = false;

    {
        QMutexLocker locker(&m_private->mutex);

        if (plan.success && (m_private->currentState == State::Open) && m_private->backend)
        {
            m_private->backend->applyRouting(id, routing);
            submitted = true;
        }
    }

    if (!submitted)
    {
        if (plan.success)
        {
            plan.success = false;
            plan.error = {ErrorCode::Busy, QStringLiteral("routing"), QStringLiteral("Tunnel is not open"), 0};
        }

        QTimer::singleShot(0, this, [this, id, plan]() {
            emit operationFinished(id, plan);

            if (!plan.success) {
                emit errorOccurred(plan.error);
            }
        });
    }

    return id;
}

void IPTunnel::close()
{
    std::unique_ptr<IPTunnelBackend> backend;
    bool notify = false;

    {
        QMutexLocker locker(&m_private->mutex);

        if (m_private->currentState == State::Closed) {
            return;
        }

        m_private->currentState = State::Closing;
        notify = true;
        backend = std::move(m_private->backend);
    }

    if (notify) {
        emit stateChanged(State::Closing);
    }

    if (backend)
    {
        backend->stop();
        backend->wait();
    }

    {
        QMutexLocker locker(&m_private->mutex);
        ++m_private->generation;
        m_private->outbound.clear();
        m_private->inbound.clear();
        m_private->outboundBytes = 0;
        m_private->inboundBytes = 0;
        m_private->outboundSignalArmed = false;
        m_private->inboundWasFull = false;
        m_private->stats.outboundQueuedPackets = 0;
        m_private->stats.inboundQueuedPackets = 0;
        m_private->stats.outboundQueuedBytes = 0;
        m_private->stats.inboundQueuedBytes = 0;
        m_private->info = {};
        m_private->currentState = State::Closed;
    }

    emit stateChanged(State::Closed);
}

IPTunnel::State IPTunnel::state() const
{
    QMutexLocker l(&m_private->mutex);
    return m_private->currentState;
}

IPTunnel::InterfaceInfo IPTunnel::interfaceInfo() const
{
    QMutexLocker l(&m_private->mutex);
    return m_private->info;
}

IPTunnel::Statistics IPTunnel::statistics() const
{
    QMutexLocker l(&m_private->mutex);
    return m_private->stats;
}

bool IPTunnel::takeOutboundPacket(QByteArray& packet)
{
    QMutexLocker locker(&m_private->mutex);
    m_private->expireOutboundLocked();

    if (m_private->outbound.isEmpty())
    {
        m_private->outboundSignalArmed = false;
        m_private->stats.outboundQueuedPackets = 0;
        m_private->stats.outboundQueuedBytes = 0;
        return false;
    }

    packet = m_private->outbound.dequeue().data;
    m_private->outboundBytes -= packet.size();
    ++m_private->stats.packetsTakenByConsumer;
    m_private->stats.bytesTakenByConsumer += packet.size();
    m_private->stats.outboundQueuedPackets = m_private->outbound.size();
    m_private->stats.outboundQueuedBytes = m_private->outboundBytes;
    return true;
}

IPTunnel::EnqueueResult IPTunnel::injectInboundPacket(const QByteArray& packet)
{
    EnqueueResult result = EnqueueResult::Accepted;

    {
        QMutexLocker locker(&m_private->mutex);

        if (m_private->currentState != State::Open) {
            return EnqueueResult::NotOpen;
        }

        ValidationResult validation = validatePacket(packet, m_private->configuration.mtu);

        if (!validation.valid || !m_private->familyEnabled(packet))
        {
            if ((packet.size() > m_private->configuration.mtu) || (packet.size() > 65535))
            {
                ++m_private->stats.oversizedPackets;
                return EnqueueResult::PacketTooLarge;
            }

            ++m_private->stats.invalidPackets;
            return EnqueueResult::InvalidPacket;
        }

        m_private->expireInboundLocked();

        if (m_private->queueFull(m_private->configuration.inboundQueue, m_private->inbound.size(), m_private->inboundBytes, packet.size()))
        {
            ++m_private->stats.inboundOverflow;
            m_private->inboundWasFull = true;
            return EnqueueResult::QueueFull;
        }

        m_private->inbound.enqueue({packet, QDateTime::currentMSecsSinceEpoch()});
        m_private->inboundBytes += packet.size();
        ++m_private->stats.packetsAcceptedForInjection;
        m_private->stats.bytesAcceptedForInjection += packet.size();
        m_private->stats.inboundQueuedPackets = m_private->inbound.size();
        m_private->stats.inboundQueuedBytes = m_private->inboundBytes;

        if (m_private->backend) {
            m_private->backend->packetsAvailable();
        }
    }

    return result;
}
