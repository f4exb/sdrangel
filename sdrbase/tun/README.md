# IPTunnel

`IPTunnel` gives SDRangel plugins one asynchronous layer-3 interface on Windows,
Linux and macOS. The public API exchanges one complete IPv4 or IPv6 datagram per
`QByteArray`; Wintun, Linux TUN and macOS utun details remain private.

```cpp
IPTunnel tunnel;
IPTunnel::Configuration configuration;
configuration.logicalId = "angel61";
configuration.interfaceName = "sdrangel0";
configuration.addresses.append({QHostAddress("10.255.0.1"), 32, {}});
configuration.routing.include.append({QHostAddress("192.168.20.0"), 24});

connect(&tunnel, &IPTunnel::outboundPacketsAvailable, [&] {
    QByteArray packet;
    while (tunnel.takeOutboundPacket(packet)) {
        transport.send(packet);
    }
});
connect(&transport, &Transport::packetReceived, [&] (const QByteArray& packet) {
    tunnel.injectInboundPacket(packet);
});
tunnel.open(configuration);
```

Opening and route changes finish through `operationFinished`. Callers should
wait for `State::Open` before injecting packets and handle `QueueFull` by waiting
for `inboundCapacityAvailable`. `Accepted` means queued for injection into the
OS, not delivered end-to-end.

Managed routing accepts several destination prefixes and exclusions. A default
route requires an explicit opt-in. Use `planRouting()` or `previewRouting()` to
see the exact disjoint routes produced by exclusions. External mode changes no
addresses or routes and is intended for policy routing, pre-provisioned devices,
and privileged-helper deployments.

To connect two Ethernet LANs, use different non-overlapping subnets, enable IP
forwarding on both tunnel hosts, permit forwarding in their firewalls, and add a
route on each LAN's normal router through its local tunnel host. For example,
route `192.168.20.0/24` through the tunnel host on `192.168.10.0/24`, and route
`192.168.10.0/24` through the tunnel host on `192.168.20.0/24`. NAT, firewall
policy, DNS and forwarding controls are deliberately outside this class.

Creating or configuring interfaces usually needs administrator/root privileges.
Linux can attach to a persistent TUN pre-created for the application user.
macOS can adopt a utun descriptor supplied by a future privileged helper.
Windows loads the signed `wintun.dll` shipped beside the executable. When
SDRangel is not already elevated, opening a tunnel starts the bundled
`sdrangel-tunnel-helper.exe` through a Windows UAC prompt. The helper remains
alive only while the tunnel is open, owns Wintun and relays packets over a
private authenticated local pipe; the main GUI remains unelevated.
