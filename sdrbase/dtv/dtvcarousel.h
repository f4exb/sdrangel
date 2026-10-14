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

#ifndef INCLUDE_DTVCAROUSEL_H
#define INCLUDE_DTVCAROUSEL_H

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include <QByteArray>
#include <QString>

#include "export.h"

// A DSM-CC object carousel (ISO/IEC 13818-6, as profiled by EN 301 192 and TR 101 202): the files and directories
// a broadcaster sends round and round, for MHEG-5 and HbbTV applications. Fed with the DSM-CC sections (tables 0x3B,
// the DSI and DIIs, and 0x3C, the DDBs) of the PID the carousel's DSI is on, and of any others its taps point to.
// The DSI gives the service gateway (the root directory) and the DII listing the modules; the DIIs give the modules'
// sizes and versions; the DDBs their blocks. A complete module (zlib compressed or not) holds BIOP messages: files,
// directories and the service gateway, which the file tree is built from. A module whose version changes is received
// again
class SDRBASE_API DTVCarousel
{
public:
    enum class Kind {
        File,
        Directory,
        Gateway,    //!< The root directory
        Stream,     //!< A stream, or stream events: what's in the tree but not a file
        Unknown
    };

    //! What's in the tree, received or not: the size of a file's known from its directory before it's received
    struct Entry {
        QString m_path;         //!< From the service gateway, as /dir/file.html
        Kind m_kind;
        qint64 m_size;          //!< Of a file, -1 if not known
        bool m_received;        //!< A file that's been received (directories are listed once they have been)
    };

    struct Progress {
        int m_modules = 0;              //!< Listed by the DIIs
        int m_modulesComplete = 0;
        qint64 m_bytes = 0;             //!< Of the modules (as sent: compressed, if they are)
        qint64 m_bytesComplete = 0;
    };

    explicit DTVCarousel(int pid);
    void reset();

    //! A DSM-CC section of table 0x3B or 0x3C, with its CRC checked. Returns whether it changed what's received
    bool section(const uint8_t *s, int length);

    int getPid() const { return m_pid; }        //!< The PID its DSI is on
    //! -1 until the DSI's received
    int64_t getCarouselId() const { return m_carouselId; }
    //! The association tags of PIDs the DIIs and DDBs may come on, other than the DSI's (from the DSI's and DIIs' taps)
    const std::set<int>& getAssociationTags() const { return m_associationTags; }
    Progress getProgress() const;
    //! All the modules listed have been received
    bool isComplete() const;
    //! Changes with each module completed or dropped, and with a new DSI
    int getRevision() const { return m_revision; }

    //! The tree, as far as it's been received, in path order
    std::vector<Entry> getEntries() const;
    //! A file's contents, if it's been received
    bool getFile(const QString& path, QByteArray& data) const;
    //! The files received, by path
    std::map<QString, QByteArray> getFiles() const;

    static QString kindName(Kind kind);
    //! A file's path made safe to save it by, relative to a directory: characters not allowed in file names replaced
    static QString localPath(const QString& path);

private:
    static constexpr int m_maxModuleSize = 16 * 1024 * 1024;    //!< Larger modules are ignored
    static constexpr qint64 m_maxCarouselSize = 64 * 1024 * 1024;
    static constexpr int m_maxDepth = 32;

    struct ObjectRef {
        uint32_t m_carouselId = 0;
        int m_moduleId = -1;
        QByteArray m_objectKey;
        bool operator<(const ObjectRef& other) const;
    };

    struct Binding {
        QString m_name;
        Kind m_kind;
        ObjectRef m_object;
        qint64 m_size;          //!< Of a file, from the binding's object info, -1 if not known
    };

    struct Object {
        Kind m_kind = Kind::Unknown;
        QByteArray m_content;               //!< Of a file
        std::vector<Binding> m_bindings;    //!< Of a directory
    };

    struct Module {
        int m_size = 0;
        int m_version = -1;
        int m_blockSize = 0;
        int m_originalSize = -1;            //!< Uncompressed, when it's compressed
        std::vector<uint8_t> m_data;
        std::vector<bool> m_blocks;         //!< Received
        int m_blocksReceived = 0;
        bool m_complete = false;
    };

    void dsi(const uint8_t *m, int length);
    void dii(const uint8_t *m, int length, uint32_t transactionId);
    bool ddb(const uint8_t *m, int length);
    void completeModule(int moduleId, Module& module);
    void parseModule(int moduleId, const uint8_t *data, int length);
    void dropModule(int moduleId);
    static bool parseIOR(const uint8_t *data, int length, int& used, ObjectRef& object, Kind& kind, std::vector<int> *tags);
    static Kind kindFromId(const uint8_t *id, int length);
    void walk(const ObjectRef& directory, const QString& path, int depth, std::set<ObjectRef>& visited,
              std::vector<Entry> *entries, std::map<QString, QByteArray> *files, const QString *wanted) const;

    const int m_pid;
    int64_t m_carouselId;
    ObjectRef m_gateway;
    std::set<int> m_associationTags;
    std::map<int, Module> m_modules;                    //!< By module id
    std::map<int, std::vector<int>> m_diiModules;       //!< By the DIIs' identification: the modules each lists
    std::map<ObjectRef, Object> m_objects;              //!< From the modules complete
    int m_revision;
};

#endif // INCLUDE_DTVCAROUSEL_H
