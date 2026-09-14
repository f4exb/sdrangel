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

#include "httpclient.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{

std::string lowered(const std::string& text)
{
    std::string result(text);

    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string trimmed(const std::string& text)
{
    size_t begin = text.find_first_not_of(" \t\r\n");

    if (begin == std::string::npos) {
        return std::string();
    }

    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string socketError()
{
#ifdef _WIN32
    int code = WSAGetLastError();

    switch (code)
    {
    case WSAECONNREFUSED: return "connection refused";
    case WSAETIMEDOUT:    return "timed out";
    case WSAECONNRESET:   return "connection reset";
    case WSAEHOSTUNREACH: return "host unreachable";
    default:              return "socket error " + std::to_string(code);
    }
#else
    // A receive timeout set with SO_RCVTIMEO comes back as EAGAIN, whose text says nothing
    // about time
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        return "timed out";
    }

    return std::strerror(errno);
#endif
}

} // namespace

HttpClient::Socket HttpClient::invalidSocket()
{
#ifdef _WIN32
    return INVALID_SOCKET;
#else
    return -1;
#endif
}

std::string HttpClient::Head::header(const std::string& name) const
{
    std::string wanted = lowered(name);

    for (const auto& header : m_headers)
    {
        if (lowered(header.first) == wanted) {
            return header.second;
        }
    }

    return std::string();
}

bool HttpClient::startup(std::string& error)
{
#ifdef _WIN32
    WSADATA data;
    int result = WSAStartup(MAKEWORD(2, 2), &data);

    if (result != 0)
    {
        error = "WSAStartup failed with " + std::to_string(result);
        return false;
    }
#else
    (void) error;
#endif
    return true;
}

void HttpClient::cleanup()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

HttpClient::~HttpClient()
{
    close();
}

bool HttpClient::open(const std::string& host, int port, int timeoutMs, std::string& error)
{
    close();

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *addresses = nullptr;
    std::string service = std::to_string(port);

    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses) != 0)
    {
        error = "cannot resolve " + host;
        return false;
    }

    std::string reason;

    for (addrinfo *address = addresses; address != nullptr; address = address->ai_next)
    {
        Socket handle = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

        if (handle == invalidSocket()) {
            continue;
        }

        if (connect(handle, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
        {
            m_socket = handle;
            break;
        }

        // Read before closing the socket and freeing the addresses, either of which
        // replaces the last error with one of its own
        reason = socketError();

#ifdef _WIN32
        closesocket(handle);
#else
        ::close(handle);
#endif
    }

    freeaddrinfo(addresses);

    if (m_socket == invalidSocket())
    {
        error = reason.empty() ? "no usable address" : reason;
        return false;
    }

    // Nagle would hold small JSON-RPC messages back waiting for more to send
    int flag = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flag), sizeof(flag));

#ifdef SO_NOSIGPIPE
    // A send on a connection the server has closed must fail, not raise SIGPIPE and end the
    // process. Linux has no such option and uses MSG_NOSIGNAL on the send instead
    setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag));
#endif

    if (timeoutMs > 0)
    {
#ifdef _WIN32
        DWORD timeout = static_cast<DWORD>(timeoutMs);
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
    }

    m_buffer.clear();
    m_eof = false;
    return true;
}

void HttpClient::interrupt()
{
    if (m_socket != invalidSocket())
    {
#ifdef _WIN32
        shutdown(m_socket, SD_BOTH);
#else
        shutdown(m_socket, SHUT_RDWR);
#endif
    }
}

void HttpClient::close()
{
    if (m_socket != invalidSocket())
    {
#ifdef _WIN32
        closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = invalidSocket();
    }

    m_buffer.clear();
    m_eof = false;
}

bool HttpClient::sendAll(const std::string& data, std::string& error)
{
    size_t sent = 0;

    while (sent < data.size())
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        int count = ::send(m_socket, data.data() + sent, static_cast<int>(data.size() - sent), flags);

        if (count <= 0)
        {
            error = "send failed (" + socketError() + ")";
            return false;
        }

        sent += static_cast<size_t>(count);
    }

    return true;
}

bool HttpClient::send(const std::string& method, const std::string& path, const std::vector<Header>& headers,
    const std::string& body, std::string& error)
{
    std::string request = method + " " + path + " HTTP/1.1\r\n";

    for (const auto& header : headers) {
        request += header.first + ": " + header.second + "\r\n";
    }

    // One request per connection keeps the response framing simple: no pipelining to unpick,
    // and a body with neither Content-Length nor chunking ends at the close
    request += "Connection: close\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    request += "\r\n";
    request += body;

    return sendAll(request, error);
}

bool HttpClient::fill(std::string& error)
{
    char block[8192];
    int count = recv(m_socket, block, sizeof(block), 0);

    if (count == 0)
    {
        m_eof = true;
        return false;
    }

    if (count < 0)
    {
        error = "read failed (" + socketError() + ")";
        m_eof = true;
        return false;
    }

    m_buffer.append(block, static_cast<size_t>(count));
    return true;
}

bool HttpClient::readLine(std::string& line, std::string& error)
{
    for (;;)
    {
        size_t end = m_buffer.find("\r\n");

        if (end != std::string::npos)
        {
            line = m_buffer.substr(0, end);
            m_buffer.erase(0, end + 2);
            return true;
        }

        if (!fill(error))
        {
            if (error.empty()) {
                error = "connection closed while reading a line";
            }

            return false;
        }
    }
}

bool HttpClient::readExactly(size_t count, std::string& out, std::string& error)
{
    while (m_buffer.size() < count)
    {
        if (!fill(error))
        {
            if (error.empty()) {
                error = "connection closed with " + std::to_string(count - m_buffer.size()) + " bytes still to read";
            }

            return false;
        }
    }

    out = m_buffer.substr(0, count);
    m_buffer.erase(0, count);
    return true;
}

bool HttpClient::readHead(Head& head, std::string& error)
{
    std::string line;

    if (!readLine(line, error)) {
        return false;
    }

    // HTTP/1.1 200 OK
    size_t firstSpace = line.find(' ');

    if ((firstSpace == std::string::npos) || (line.compare(0, 5, "HTTP/") != 0))
    {
        error = "not an HTTP response: " + line;
        return false;
    }

    size_t secondSpace = line.find(' ', firstSpace + 1);
    head.m_status = std::atoi(line.substr(firstSpace + 1).c_str());
    head.m_reason = (secondSpace == std::string::npos) ? std::string() : trimmed(line.substr(secondSpace + 1));

    for (;;)
    {
        if (!readLine(line, error)) {
            return false;
        }

        if (line.empty()) {
            break;
        }

        size_t colon = line.find(':');

        if (colon == std::string::npos) {
            continue;
        }

        head.m_headers.emplace_back(trimmed(line.substr(0, colon)), trimmed(line.substr(colon + 1)));
    }

    std::string length = head.header("Content-Length");

    if (!length.empty()) {
        head.m_contentLength = std::atoll(length.c_str());
    }

    head.m_chunked = (lowered(head.header("Transfer-Encoding")).find("chunked") != std::string::npos);
    return true;
}

bool HttpClient::readChunk(std::string& chunk, std::string& error)
{
    std::string line;

    if (!readLine(line, error)) {
        return false;
    }

    // The size line may carry chunk extensions after a semicolon
    size_t semicolon = line.find(';');
    std::string sizeText = trimmed((semicolon == std::string::npos) ? line : line.substr(0, semicolon));
    size_t size = static_cast<size_t>(std::strtoul(sizeText.c_str(), nullptr, 16));

    if (size == 0)
    {
        // Trailers, then the final blank line. The body is complete, which is not an error
        for (;;)
        {
            std::string trailer;

            if (!readLine(trailer, error) || trailer.empty()) {
                break;
            }
        }

        error.clear();
        return false;
    }

    if (!readExactly(size, chunk, error)) {
        return false;
    }

    std::string terminator;
    return readLine(terminator, error);
}

bool HttpClient::readSome(std::string& data, std::string& error)
{
    if (m_buffer.empty() && !fill(error)) {
        return false;
    }

    data.clear();
    data.swap(m_buffer);
    return true;
}

bool HttpClient::readBody(const Head& head, std::string& body, std::string& error)
{
    body.clear();

    if (head.m_chunked)
    {
        std::string chunk;

        while (readChunk(chunk, error)) {
            body += chunk;
        }

        return error.empty();
    }

    if (head.m_contentLength >= 0)
    {
        if (head.m_contentLength == 0) {
            return true;
        }

        return readExactly(static_cast<size_t>(head.m_contentLength), body, error);
    }

    // Neither framing: the body runs to the close, which Connection: close guarantees
    while (fill(error)) {
        // keep reading
    }

    body.swap(m_buffer);
    m_buffer.clear();
    return error.empty();
}
