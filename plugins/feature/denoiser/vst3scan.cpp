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

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <cstdio>

#include "vst3effect.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList paths = app.arguments().mid(1);
    
    const bool probe = !paths.isEmpty() && paths.first() == QStringLiteral("--probe");
    if (probe) {
        paths.removeFirst();
    }
    
    QJsonArray plugins;

    if (probe) 
    {
        for (const Vst3PluginInfo& plugin : Vst3Effect::discover(paths)) 
        {
            QJsonObject object;
            object.insert(QStringLiteral("path"), plugin.modulePath);
            object.insert(QStringLiteral("id"), QString::fromLatin1(plugin.classId.toHex()));
            object.insert(QStringLiteral("name"), plugin.name);
            plugins.append(object);
        
        }
    } 
    else 
    {
        for (const QString& path : Vst3Effect::modulePaths(paths)) 
        {
            QProcess child;
            child.setProgram(app.applicationFilePath());
            child.setArguments({QStringLiteral("--probe"), path});
            child.start();
            if (!child.waitForStarted(5000)) {
                continue;
            }
            if (!child.waitForFinished(60000)) 
            {
                child.kill();
                child.waitForFinished(5000);
                continue;
            }
            if (child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0) 
                continue;
            const QJsonDocument result = QJsonDocument::fromJson(child.readAllStandardOutput());
            if (!result.isArray()) {
                continue;
            }
            for (const QJsonValue& value : result.array()) {
                plugins.append(value);
            }
        }
    }
    const QByteArray json = QJsonDocument(plugins).toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
    return 0;
}
