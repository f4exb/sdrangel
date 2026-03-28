# Meshtastic port payload reference

This note summarizes Meshtastic port names used by SDRangel packet decoding and what payload kind to expect.

Scope:
- Port names and IDs come from the local map in `meshtasticpacket.cpp` (`kPortMap`).
- Payload types are practical expectations based on Meshtastic app semantics.
- SDRangel currently does **generic** data decode only (header + protobuf envelope fields + raw payload), not per-port protobuf decoding.

## How to interpret payload display in SDRangel

- Text is shown only when payload bytes look like valid UTF-8.
- Otherwise payload is shown as hex.
- For most ports (POSITION, NODEINFO, TELEMETRY, ROUTING, etc.), payload is protobuf/binary, so hex is the normal representation.
- Seeing a mix of readable text and binary garbage (for example NODEINFO_APP showing the node name) is normal: protobuf stores `string` fields as raw UTF-8 bytes inline in the binary stream. The readable parts are string field values; the surrounding bytes are protobuf field tags (varints), numeric IDs, and other binary-encoded fields.

## Port list (current SDRangel map)

| Port name | ID | Expected payload kind | Typical content | SDRangel decode level |
|---|---:|---|---|---|
| UNKNOWN_APP | 0 | binary/unknown | unspecified | envelope + raw payload |
| TEXT / TEXT_MESSAGE_APP | 1 | UTF-8 text | plain chat text | envelope + text/hex payload |
| REMOTE_HARDWARE_APP | 2 | protobuf/binary | remote hardware control messages | envelope + raw payload |
| POSITION_APP | 3 | protobuf/binary | GPS position fields | envelope + raw payload |
| NODEINFO_APP | 4 | protobuf/binary | node metadata / identity (long name, short name, hardware model, node ID) | envelope + raw payload. Node name strings appear in clear because protobuf stores `string` fields as raw UTF-8 bytes; surrounding bytes are binary (field tags, varint IDs, enums). |
| ROUTING_APP | 5 | protobuf/binary | routing/control data | envelope + raw payload |
| ADMIN_APP | 6 | protobuf/binary | admin/config operations | envelope + raw payload |
| TEXT_MESSAGE_COMPRESSED_APP | 7 | compressed/binary | compressed text payload | envelope + raw payload |
| WAYPOINT_APP | 8 | protobuf/binary | waypoint structures | envelope + raw payload |
| AUDIO_APP | 9 | binary | encoded audio frames | envelope + raw payload |
| DETECTION_SENSOR_APP | 10 | protobuf/binary | sensor detection events | envelope + raw payload |
| ALERT_APP | 11 | protobuf/binary | alert/notification message | envelope + raw payload |
| KEY_VERIFICATION_APP | 12 | protobuf/binary | key verification/signaling | envelope + raw payload |
| REPLY_APP | 32 | protobuf/binary | reply wrapper/ack data | envelope + raw payload |
| IP_TUNNEL_APP | 33 | binary | tunneled IP packet bytes | envelope + raw payload |
| PAXCOUNTER_APP | 34 | protobuf/binary | pax counter telemetry | envelope + raw payload |
| STORE_FORWARD_PLUSPLUS_APP | 35 | protobuf/binary | store-and-forward control/data | envelope + raw payload |
| NODE_STATUS_APP | 36 | protobuf/binary | node status information | envelope + raw payload |
| SERIAL_APP | 64 | binary | serial tunnel bytes | envelope + raw payload |
| STORE_FORWARD_APP | 65 | protobuf/binary | store-and-forward messages | envelope + raw payload |
| RANGE_TEST_APP | 66 | protobuf/binary | range test metadata/results | envelope + raw payload |
| TELEMETRY_APP | 67 | protobuf/binary | telemetry records | envelope + raw payload |
| ZPS_APP | 68 | protobuf/binary | ZPS-specific messages | envelope + raw payload |
| SIMULATOR_APP | 69 | protobuf/binary | simulator-generated messages | envelope + raw payload |
| TRACEROUTE_APP | 70 | protobuf/binary | traceroute path/hops | envelope + raw payload |
| NEIGHBORINFO_APP | 71 | protobuf/binary | neighbor table entries | envelope + raw payload |
| ATAK_PLUGIN | 72 | protobuf/binary | ATAK integration payloads | envelope + raw payload |

## Notes

- This is a practical operator reference, not a protocol authority.
- For authoritative and exhaustive app payload schemas, use Meshtastic upstream protobuf definitions (for example `portnums.proto` and related message protos).
- If SDRangel later adds per-port protobuf decoding, this table should be updated with "decoded fields available" per port.

## Protocol availability and per-port decoding

The Meshtastic protocol is fully public and open-source (GPL v3, same license as SDRangel).

- Full `.proto` definitions: https://github.com/meshtastic/protobufs
- Browsable API reference with all message types and fields: https://buf.build/meshtastic/protobufs
- Protocol architecture doc: https://meshtastic.org/docs/overview/mesh-algo/

Key message types relevant to SDRangel decoding:

| Proto message | Port | Fields of interest |
|---|---|---|
| `Position` | POSITION_APP (3) | latitude, longitude, altitude, time, speed, heading |
| `User` | NODEINFO_APP (4) | id (node ID string), long_name, short_name, hw_model, is_licensed |
| `Routing` | ROUTING_APP (5) | route success/failure error codes |
| `Data` | (envelope, already decoded) | portnum, payload, want_response, dest, source |
| `Telemetry` | TELEMETRY_APP (67) | device metrics (battery, voltage, utilization), environment (temp, humidity), air quality |
| `RouteDiscovery` | TRACEROUTE_APP (70) | route (node ID list), snr_towards, snr_back |
| `NeighborInfo` | NEIGHBORINFO_APP (71) | node_id, node_broadcast_interval_secs, neighbors list |
| `Waypoint` | WAYPOINT_APP (8) | id, latitude, longitude, expire, locked_to, name, description |

### Adding per-port decoding to SDRangel

Three practical approaches (in order of fit for this codebase):

1. **Extend the existing hand-parser** — `meshtasticpacket.cpp` already contains a minimal protobuf varint/length-delimited parser for the `Data` envelope. The same approach can be extended per-port for common types (POSITION, NODEINFO, TELEMETRY) with no extra dependency. Best choice for portability and minimal build impact.

2. **nanopb** — the C library used by Meshtastic firmware itself; tiny footprint, no external runtime, generates `.pb.c`/`.pb.h` from the upstream `.proto` files. Would require adding it to `CMakeLists.txt` as a dependency.

3. **Qt Protobuf** (`QProtobufSerializer`) — available in Qt 6.5+. Adds a Qt version constraint that may conflict with Qt5 builds.
