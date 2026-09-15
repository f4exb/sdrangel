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

#ifndef INCLUDE_MCPBRIDGE_HTTPCLIENT_H_
#define INCLUDE_MCPBRIDGE_HTTPCLIENT_H_

#include <string>
#include <utility>
#include <vector>

//!< Just enough HTTP/1.1 to talk to SDRangel's MCP server: one request per connection,
//!< no TLS, no redirects, no keep alive. Bodies arrive either with a Content-Length or
//!< chunked, and a chunked body can be read incrementally so that the event stream can
//!< be followed as it arrives.
class HttpClient
{
public:
    using Header = std::pair<std::string, std::string>;

    struct Head
    {
        int m_status = 0;
        std::string m_reason;
        std::vector<Header> m_headers;
        long long m_contentLength = -1; //!< -1 when there is no Content-Length
        bool m_chunked = false;

        //!< Case insensitive lookup, empty when the header is absent
        std::string header(const std::string& name) const;
    };

    HttpClient() = default;
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    static bool startup(std::string& error);   //!< WSAStartup on Windows, nothing elsewhere
    static void cleanup();

    bool open(const std::string& host, int port, int timeoutMs, std::string& error);
    void close();
    //!< Wakes a read blocked on this socket from another thread. Only the socket is touched:
    //!< the buffer and the rest belong to the thread reading, which closes when its read
    //!< returns. Closing from the other thread would not wake a read on Linux, and frees a
    //!< descriptor number the reader still holds
    void interrupt();
    bool isOpen() const { return m_socket != invalidSocket(); }

    //!< Sends the request and its body. An empty method body is fine.
    bool send(const std::string& method, const std::string& path, const std::vector<Header>& headers,
        const std::string& body, std::string& error);

    bool readHead(Head& head, std::string& error);

    //!< Reads the whole body, however it is framed. Not for the event stream, which never ends.
    bool readBody(const Head& head, std::string& body, std::string& error);

    //!< One piece of a chunked body, for following a stream. Returns false at the end of the
    //!< body or on error, with error empty when the end was reached cleanly.
    bool readChunk(std::string& chunk, std::string& error);

    //!< Whatever has arrived of a body that is delimited by the connection closing rather
    //!< than chunked, which is how the event stream comes back. Same false at the end.
    bool readSome(std::string& data, std::string& error);

private:
#ifdef _WIN32
    using Socket = unsigned long long; //!< SOCKET, kept out of the header so windows.h is not needed here
#else
    using Socket = int;
#endif

    static Socket invalidSocket();

    bool fill(std::string& error);                       //!< One read into m_buffer
    bool readLine(std::string& line, std::string& error);
    bool readExactly(size_t count, std::string& out, std::string& error);
    bool sendAll(const std::string& data, std::string& error);

    Socket m_socket = invalidSocket();
    std::string m_buffer;   //!< Bytes read from the socket but not yet consumed
    bool m_eof = false;
};

#endif // INCLUDE_MCPBRIDGE_HTTPCLIENT_H_
