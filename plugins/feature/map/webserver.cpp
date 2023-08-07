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

#include <QResource>
#include <QFile>
#include <QRegularExpression>
#include <QDebug>

#include "webserver.h"

// port - port to listen on / is listening on. Use 0 for any free port.
WebServer::WebServer(quint16 &port, QObject* parent) :
    QTcpServer(parent),
    m_defaultMimeType("application/octet-stream")
{
    listen(QHostAddress::Any, port);
    port = serverPort();
    qDebug() << "WebServer on port " << port;

    m_mimeTypes.insert(".html", new MimeType("text/html; charset=\"utf-8\"", false));
    m_mimeTypes.insert(".png", new MimeType("image/png"));
    m_mimeTypes.insert(".glb", new MimeType("model/gltf-binary"));
    m_mimeTypes.insert(".glbe", new MimeType("model/gltf-binary"));
    m_mimeTypes.insert(".js", new MimeType("text/javascript"));
    m_mimeTypes.insert(".css", new MimeType("text/css"));
    m_mimeTypes.insert(".json", new MimeType("application/json"));
    m_mimeTypes.insert(".geojson", new MimeType("application/geo+json"));
}

void WebServer::incomingConnection(qintptr socket)
{
    QTcpSocket* s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
    //addPendingConnection(socket);
}

// Don't include leading or trailing / in from
void WebServer::addPathSubstitution(const QString &from, const QString &to)
{
    qDebug() << "Mapping " << from << " to " << to;
    m_pathSubstitutions.insert(from, to);
}

void WebServer::addSubstitution(QString path, QString from, QString to)
{
    Substitution *s = new Substitution(from, to);
    if (m_substitutions.contains(path))
    {
        QList<Substitution *> *list = m_substitutions.value(path);
        QMutableListIterator<Substitution *> i(*list);
        while (i.hasNext()) {
            Substitution *sub = i.next();
            if (sub->m_from == from) {
                i.remove();
                delete sub;
            }
        }
        list->append(s);
    }
    else
    {
        QList<Substitution *> *list = new QList<Substitution *>();
        list->append(s);
        m_substitutions.insert(path, list);
    }
}

QString WebServer::substitute(QString path, QString html)
{
    QList<Substitution *> *list = m_substitutions.value(path);
    for (const auto s : *list) {
        html = html.replace(s->m_from, s->m_to);
    }
    return html;
}

void WebServer::addFile(const QString &path, const QByteArray &data)
{
    m_files.insert(path, data);
}

void WebServer::sendFile(QTcpSocket* socket, const QByteArray &data, MimeType *mimeType, const QString &path)
{
    QString header = QString("HTTP/1.0 200 Ok\r\nContent-Type: %1\r\n\r\n").arg(mimeType->m_type);
    if (mimeType->m_binary)
    {
        // Send file as binary
        QByteArray headerUtf8 = header.toUtf8();
        socket->write(headerUtf8);
        socket->write(data);
    }
    else
    {
        // Send file as text
        QString html = QString(data);
        // Make any substitutions in the content of the file
        if (m_substitutions.contains(path)) {
            html = substitute(path, html);
        }
        QTextStream os(socket);
        os.setAutoDetectUnicode(true);
        os << header << html;
    }
}

void WebServer::readClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    if (socket->canReadLine())
    {
        QString line = socket->readLine();
        //qDebug() << "WebServer HTTP Request: " << line;

        QStringList tokens = QString(line).split(QRegularExpression("[ \r\n][ \r\n]*"));
        if (tokens[0] == "GET")
        {
            // Get file type from extension
            QString path = tokens[1];
            MimeType *mimeType = &m_defaultMimeType;
            int extensionIdx = path.lastIndexOf(".");
            if (extensionIdx != -1) {
                QString extension = path.mid(extensionIdx);
                if (m_mimeTypes.contains(extension)) {
                     mimeType = m_mimeTypes[extension];
                }
            }

            // Try mapping path
            QStringList dirs = path.split('/');
            if ((dirs.length() >= 2) && m_pathSubstitutions.contains(dirs[1]))
            {
                dirs[1] = m_pathSubstitutions.value(dirs[1]);
                dirs.removeFirst();
                QString newPath = dirs.join('/');
                //qDebug() << "Mapping " << path << " to " << newPath;
                path = newPath;
            }

            // See if we can find the file in our resources
            QResource res(path);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            if (res.isValid() && (res.uncompressedSize() > 0))
            {
                QByteArray data = res.uncompressedData();
                sendFile(socket, data, mimeType, path);
            }
#else
            if (res.isValid() && (res.size() > 0))
            {
                QByteArray data = QByteArray::fromRawData((const char *)res.data(), res.size());
                if (res.isCompressed()) {
                    data = qUncompress(data);
                }
                sendFile(socket, data, mimeType, path);
            }
#endif
            else if (m_files.contains(path))
            {
                // Path is a file held in memory
                sendFile(socket, m_files.value(path).data(), mimeType, path);
            }
            else
            {
                // See if we can find a file on disk
                QFile file(path);
                if (file.open(QIODevice::ReadOnly))
                {
                    QByteArray data = file.readAll();
                    if (path.endsWith(".glbe")) {
                        for (int i = 0; i < data.size(); i++) {
                            data[i] = data[i] ^ 0x55;
                        }
                    }
                    sendFile(socket, data, mimeType, path);
                }
                else
                {
                    qDebug() << "WebServer " << path << " not found";
                    // File not found
                    QTextStream os(socket);
                    os.setAutoDetectUnicode(true);
                    os << "HTTP/1.0 404 Not Found\r\n"
                        "Content-Type: text/html; charset=\"utf-8\"\r\n"
                        "\r\n"
                        "<html>\n"
                        "<body>\n"
                        "<h1>404 Not Found</h1>\n"
                        "</body>\n"
                        "</html>\n";
                }
            }

            socket->close();

            if (socket->state() == QTcpSocket::UnconnectedState) {
                delete socket;
            }
        }
    }
}

void WebServer::discardClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    socket->deleteLater();
}
