///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_MESSAGECENTERSETTINGS_H
#define INCLUDE_MESSAGECENTERSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <vector>

#include "messagecenterengine.h"

class Serializable;

// A sink the routes send records to, or a source records are read from. The type tells which:
//   sinks:   "file" (default: path, format 0 JSON lines, 1 CSV or 2 Pager CSV), "udp" (address, port, format 0 Pager
//            demodulator or 1 JSON), "command" (text: the command, the text of the message goes into its
//            arguments), "speech" (text: what is said), "mark" (icon, colour of the text and of the row of the message
//            in the log, each optional), "map" (the position in the text of the message on the Map)
//   sources: "file" (default: a JSON lines file), "csv" (path: a log of this message center or of the Pager
//            demodulator), "udp" (address and port to listen on, JSON lines or the Pager demodulator format)
struct MessageCenterFileSetting
{
    QString m_name;
    QString m_type;             //!< Empty for a file
    QString m_path;
    int m_format = 0;
    QString m_address;          //!< udp
    int m_port = 0;             //!< udp
    QString m_text;             //!< command, speech
    QString m_icon;             //!< mark: a built-in icon (:name) or the path of an image, empty for none
    quint32 m_color = 0;        //!< mark: colour of the text, ARGB, 0 for none
    quint32 m_background = 0;   //!< mark: colour of the row, ARGB, 0 for none

    static const QStringList& sinkTypes();      //!< file, udp, command, speech, mark, map
    static const QStringList& sourceTypes();    //!< file, csv, udp
    QString typeOrDefault() const { return m_type.isEmpty() ? QString("file") : m_type; }
    //!< What is wrong with the setting as a sink or source, empty if nothing
    QString check(bool sink) const;
};

struct MessageCenterSettings
{
    QString m_title;
    quint32 m_rgbColor;
    int m_logSize;                          //!< Messages kept in the log
    bool m_historyEnabled;                  //!< The log kept in a file, there again at the next start
    int m_historyMaxMB;                     //!< Size kept of the history, MB
    int m_historyDays;                      //!< Days kept of the history
    int m_claimTimeoutSec;                  //!< A page given to a transmitter that does not claim it in this time goes back to the queue
    int m_silentSec;                        //!< A channel that does not announce itself for this time is offline
    std::vector<MessageCenterEngine::Route> m_routes;
    QList<MessageCenterContact> m_contacts;         //!< The address book
    QList<MessageCenterFileSetting> m_sinks;
    QList<MessageCenterFileSetting> m_sources;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    QByteArray m_logColumns;                //!< The columns of the table of the log: order, sizes, hidden
    QString m_voicePlaybackDeviceName;      //!< Audio output of the voice files played from the log

    MessageCenterSettings();
    void resetToDefaults();
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const MessageCenterSettings& settings);

    static QByteArray routesToJson(const std::vector<MessageCenterEngine::Route>& routes);
    static std::vector<MessageCenterEngine::Route> routesFromJson(const QByteArray& json);
    static QByteArray filesToJson(const QList<MessageCenterFileSetting>& files);
    static QList<MessageCenterFileSetting> filesFromJson(const QByteArray& json);
    //! The presets of the routes and their sinks: Empty (every message to the log, as a new message center) and
    //! Emergency (an example: the log, and a mark on the messages of a kind, by a few words of their text)
    static QStringList presetNames();
    //! The routes and the sinks of a preset; false if there is none of that name
    static bool preset(const QString& name, std::vector<MessageCenterEngine::Route>& routes, QList<MessageCenterFileSetting>& sinks);
};

#endif // INCLUDE_MESSAGECENTERSETTINGS_H
