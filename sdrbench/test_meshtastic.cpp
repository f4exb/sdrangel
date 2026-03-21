///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QMap>
#include <QDebug>
#include <QStringList>

#include "mainbench.h"
#include "modemmeshtastic/meshtasticpacket.h"

namespace
{

using modemmeshtastic::DecodeResult;
using modemmeshtastic::Packet;

static QMap<QString, QString> toFieldMap(const DecodeResult& result)
{
    QMap<QString, QString> map;

    for (const DecodeResult::Field& field : result.fields)
    {
        if (!map.contains(field.path)) {
            map.insert(field.path, field.value);
        }
    }

    return map;
}

static bool compareField(
    const QMap<QString, QString>& fields,
    const QString& path,
    const QString& expected,
    QStringList& failures)
{
    if (!fields.contains(path))
    {
        failures.append(QString("missing field '%1' (expected '%2')").arg(path, expected));
        return false;
    }

    const QString value = fields.value(path);

    if (value != expected)
    {
        failures.append(QString("field '%1' mismatch: got '%2' expected '%3'").arg(path, value, expected));
        return false;
    }

    return true;
}

struct RegressionCase
{
    QString name;
    QString command;
    QString keySpecList;
    bool expectDataDecoded = true;
    bool expectDecrypted = false;
    QMap<QString, QString> expectedFields;
};

} // namespace

void MainBench::testMeshtastic(const QString& argsStr)
{
    qInfo() << "MainBench::testMeshtastic: start args=" << argsStr;

    const QStringList selectedCases = argsStr.trimmed().isEmpty()
        ? QStringList()
        : argsStr.split(',', Qt::SkipEmptyParts);

    const QList<RegressionCase> cases = {
        {
            "plain_text",
            "MESH:from=0x11223344;id=0x00000001;port=TEXT;encrypt=0;text=hello-mesh",
            "",
            true,
            false,
            {
                {"decode.path", "plain"},
                {"data.port_name", "TEXT_MESSAGE_APP"},
                {"data.text", "hello-mesh"}
            }
        },
        {
            "encrypted_text",
            "MESH:from=0x11223344;id=0x00000002;port=TEXT;encrypt=1;key=default;text=secret-mesh",
            "LongFast=default",
            true,
            true,
            {
                {"decode.path", "aes_ctr_be"},
                {"data.port_name", "TEXT_MESSAGE_APP"},
                {"data.text", "secret-mesh"}
            }
        },
        {
            "position_fixed32",
            "MESH:from=0x11223344;id=0x00000003;port=POSITION_APP;encrypt=0;payload_hex=0df0ec1e1d15d0ea6601187b",
            "",
            true,
            false,
            {
                {"data.port_name", "POSITION_APP"},
                {"position.latitude", "48.8566000"},
                {"position.longitude", "2.3522000"},
                {"position.altitude_m", "123"}
            }
        },
        {
            "nodeinfo_user",
            "MESH:from=0x11223344;id=0x00000004;port=NODEINFO_APP;encrypt=0;payload_hex=0a0921313233346162636412064e6f646520411a024e41",
            "",
            true,
            false,
            {
                {"data.port_name", "NODEINFO_APP"},
                {"nodeinfo.id", "!1234abcd"},
                {"nodeinfo.long_name", "Node A"},
                {"nodeinfo.short_name", "NA"}
            }
        },
        {
            "telemetry_wrapped",
            "MESH:from=0x11223344;id=0x00000005;port=TELEMETRY_APP;encrypt=0;payload_hex=1208083710f41c28901c",
            "",
            true,
            false,
            {
                {"data.port_name", "TELEMETRY_APP"},
                {"telemetry.device.battery_level_pct", "55.0"},
                {"telemetry.device.voltage_v", "3.700"},
                {"telemetry.device.uptime_s", "3600"}
            }
        },
        {
            "telemetry_direct_metrics",
            "MESH:from=0x11223344;id=0x00000006;port=TELEMETRY_APP;encrypt=0;payload_hex=083710f41c28901c",
            "",
            true,
            false,
            {
                {"data.port_name", "TELEMETRY_APP"},
                {"telemetry.device.battery_level_pct", "55.0"},
                {"telemetry.device.voltage_v", "3.700"},
                {"telemetry.device.uptime_s", "3600"},
                {"telemetry.decode_mode", "direct_metrics_payload"}
            }
        },
        {
            "traceroute_simple",
            "MESH:from=0x11223344;id=0x00000007;port=TRACEROUTE_APP;encrypt=0;payload_hex=0d040302011018",
            "",
            true,
            false,
            {
                {"data.port_name", "TRACEROUTE_APP"},
                {"traceroute.forward_hops", "1"},
                {"traceroute.route[0].node_id", "!01020304"},
                {"traceroute.route[0].snr_towards_db", "3.00"}
            }
        },
        {
            "telemetry_malformed_fallback",
            "MESH:from=0x11223344;id=0x00000008;port=TELEMETRY_APP;encrypt=0;payload_hex=ff",
            "",
            true,
            false,
            {
                {"data.port_name", "TELEMETRY_APP"},
                {"data.payload_hex", "ff"}
            }
        },
        {
            "wrong_key_undecoded",
            "MESH:from=0x11223344;id=0x00000009;port=TEXT;encrypt=1;key=default;text=wrong-key",
            "LongFast=simple2",
            false,
            false,
            {
                {"decode.path", "undecoded"},
                {"decode.decrypted", "false"}
            }
        }
    };

    int selectedCount = 0;
    int failedCount = 0;

    for (const RegressionCase& regressionCase : cases)
    {
        if (!selectedCases.isEmpty() && !selectedCases.contains(regressionCase.name)) {
            continue;
        }

        selectedCount++;
        QStringList failures;

        QByteArray frame;
        QString txSummary;
        QString txError;

        if (!Packet::buildFrameFromCommand(regressionCase.command, frame, txSummary, txError))
        {
            failures.append(QString("buildFrameFromCommand failed: %1").arg(txError));
        }
        else
        {
            DecodeResult result;
            const bool decodeOk = regressionCase.keySpecList.isEmpty()
                ? Packet::decodeFrame(frame, result)
                : Packet::decodeFrame(frame, result, regressionCase.keySpecList);

            if (!decodeOk)
            {
                failures.append("decodeFrame returned false");
            }
            else
            {
                if (result.dataDecoded != regressionCase.expectDataDecoded)
                {
                    failures.append(QString("dataDecoded mismatch: got %1 expected %2")
                        .arg(result.dataDecoded ? "true" : "false",
                             regressionCase.expectDataDecoded ? "true" : "false"));
                }

                if (result.decrypted != regressionCase.expectDecrypted)
                {
                    failures.append(QString("decrypted mismatch: got %1 expected %2")
                        .arg(result.decrypted ? "true" : "false",
                             regressionCase.expectDecrypted ? "true" : "false"));
                }

                const QMap<QString, QString> fields = toFieldMap(result);

                for (auto it = regressionCase.expectedFields.constBegin(); it != regressionCase.expectedFields.constEnd(); ++it) {
                    compareField(fields, it.key(), it.value(), failures);
                }
            }
        }

        if (!failures.isEmpty())
        {
            failedCount++;
            qWarning().noquote() << QString("MainBench::testMeshtastic: case '%1' FAILED").arg(regressionCase.name);
            for (const QString& failure : failures) {
                qWarning().noquote() << QString("  - %1").arg(failure);
            }
        }
        else
        {
            qInfo().noquote() << QString("MainBench::testMeshtastic: case '%1' PASSED").arg(regressionCase.name);
        }
    }

    if (selectedCount == 0)
    {
        qWarning() << "MainBench::testMeshtastic: no matching cases selected";
        return;
    }

    qInfo() << "MainBench::testMeshtastic: completed"
        << "selected=" << selectedCount
        << "failed=" << failedCount
        << "passed=" << (selectedCount - failedCount);
}
