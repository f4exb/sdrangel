///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_WEB_SERVER_H_
#define INCLUDE_WEB_SERVER_H_

#include <QTcpServer>
#include <QTcpSocket>

// WebServer for making simple dynamic html pages and serving binaries from
// resources or local disk
class WebServer : public QTcpServer
{
    Q_OBJECT

    struct Substitution {
        QString m_from;
        QString m_to;
        Substitution(const QString& from, const QString& to) :
            m_from(from),
            m_to(to)
        {
        }
    };

    struct MimeType {
        QString m_type;
        bool m_binary;
        MimeType(const QString& type, bool binary=true) :
            m_type(type),
            m_binary(binary)
        {
        }
    };

private:

    // Hash of a list of paths to substitute
    QHash<QString, QString> m_pathSubstitutions;

    // Hash of path to a list of substitutions to make in the file
    QHash<QString, QList<Substitution *>*> m_substitutions;

    // Hash of files held in memory
    QHash<QString, QByteArray> m_files;

    // Hash of filename extension to MIME type information
    QHash<QString, MimeType *> m_mimeTypes;
    MimeType m_defaultMimeType;

public:
    WebServer(quint16 &port, QObject* parent = 0);
    void incomingConnection(qintptr socket) override;
    void addPathSubstitution(const QString &from, const QString &to);
    void addSubstitution(QString path, QString from, QString to);
    void addFile(const QString &path, const QByteArray &data);
    QString substitute(QString path, QString html);
    void sendFile(QTcpSocket* socket, const QByteArray &data, MimeType *mimeType, const QString &path);

private slots:
    void readClient();
    void discardClient();

};

#endif
