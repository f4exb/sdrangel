///////////////////////////////////////////////////////////////////////////////////
// SDRdaemon - send I/Q samples read from a SDR device over the network via UDP. //
//                                                                               //
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

// Original code is posted at: https://cppcodetips.wordpress.com/2014/01/29/udp-socket-class-in-c/

#ifndef INCLUDE_UDPSOCKET_H_
#define INCLUDE_UDPSOCKET_H_

#include <cstring>            // For string
#include <exception>         // For exception class
#include <string>
#include <sys/types.h>       // For data types
#include <sys/socket.h>      // For socket(), connect(), send(), and recv()
#include <netdb.h>           // For gethostbyname()
#include <arpa/inet.h>       // For inet_addr()
#include <unistd.h>          // For close()
#include <netinet/in.h>      // For sockaddr_in
#include <errno.h>
#include <climits>

using namespace std;

/**
 *   Signals a problem with the execution of a socket call.
 */

class CSocketException: public std::exception
{
public:
    /**
   *   Construct a SocketException with a explanatory message.
   *   @param message explanatory message
   *   @param bSysMsg true if system message (from strerror(errno))
   *   should be postfixed to the user provided message
   */
    CSocketException(const string &message, bool bSysMsg = false) throw();


    /** Destructor.
     * Virtual to allow for subclassing.
     */
    virtual ~CSocketException() throw ();

    /** Returns a pointer to the (constant) error description.
     *  @return A pointer to a \c const \c char*. The underlying memory
     *          is in posession of the \c Exception object. Callers \a must
     *          not attempt to free the memory.
     */
    virtual const char* what() const throw (){  return m_sMsg.c_str(); }

protected:
    /** Error message.
     */
    std::string m_sMsg;
};

/**
 *   Base class representing basic communication endpoint.
 */

class CSocket
{
public:
    virtual ~CSocket();

    /**
     * Enum to represent type of socket(UDP or TCP)
     */
    enum SocketType
    {
        TcpSocket = SOCK_STREAM,
        UdpSocket = SOCK_DGRAM,
        UnknownSocketType =-1
    };

    /**
     * Enum to represent type network layer protocol used for socket
     */
    enum NetworkLayerProtocol
    {
        IPv4Protocol = AF_INET,
        IPv6Protocol = AF_INET6,
        UnknownNetworkLayerProtocol = -1
    };

    /**
     * Enum to represent Wait Result when reading data from a socket
     */
    enum ReadResult
    {
       DATA_ARRIVED   = 0,
       DATA_TIMED_OUT = ETIMEDOUT,
       DATA_EXCEPTION = 255
    };

    /**
   *   Get the local address
   *   @return local address of socket
   *   @exception CSocketException thrown if fetch fails
   */

    string GetLocalAddress();

    /**
   *   Get the local port
   *   @return local port of socket
   *   @exception CSocketException thrown if fetch fails
   */

    unsigned short GetLocalPort();


    /**
   *   Set the local port to the specified port and the local address
   *   to any interface
   *   @param localPort local port
   *   @exception CSocketException thrown if setting local port fails
   */

    void BindLocalPort(unsigned short localPort);

    /**
   *   Set the local port to the specified port and the local address
   *   to the specified address.  If you omit the port, a random port
   *   will be selected.
   *   @param localAddress local address
   *   @param localPort local port
   *   @exception CSocketException thrown if setting local port or address fails
   */

    void BindLocalAddressAndPort(const string &localAddress, unsigned short localPort = 0);

    /**
    *   Returns the size of the internal read buffer. This limits the amount of data that the client
    *   can receive before you call
   */
    unsigned long int GetReadBufferSize ();

    /**
    *   Sets the read buffer size of the socket.
    *   @param Size of the buffer.
    */
    void SetReadBufferSize(unsigned int nSize);

    /**
    *   Sets the socket to Blocking/Non blocking state.
    *   @param Bool flag for Non blocking status.
    */
    void SetNonBlocking(bool bBlocking);

    /**
   *   Establish a socket connection with the given foreign
   *   address and port
   *   @param foreignAddress foreign address (IP address or name)
   *   @param foreignPort foreign port
   *   @exception SocketException thrown if unable to establish connection
   */
    void ConnectToHost(const string &foreignAddress, unsigned short foreignPort);

    /**
     *   Write the given buffer to this socket.  Call connect() before
     *   calling send()
     *   @param buffer buffer to be written
     *   @param bufferLen number of bytes from buffer to be written
     *   @exception SocketException thrown if unable to send data
     */
    void Send(const void *buffer, int bufferLen);

  /**
   *   Read into the given buffer up to bufferLen bytes data from this
   *   socket.  Call connect() before calling recv()
   *   @param buffer buffer to receive the data
   *   @param bufferLen maximum number of bytes to read into buffer
   *   @return number of bytes read, 0 for EOF, and -1 for error
   *   @exception SocketException thrown if unable to receive data
   */
    int Recv(void *buffer, int bufferLen);

    /**
     *   Get the foreign address.  Call connect() before calling recv()
     *   @return foreign address
     *   @exception SocketException thrown if unable to fetch foreign address
     */
    string GetPeerAddress();

    /**
     *   Get the foreign port.  Call connect() before calling recv()
     *   @return foreign port
     *   @exception SocketException thrown if unable to fetch foreign port
     */
    unsigned short GetPeerPort();

    /**
     *   Writing sStr to socket
     */
    CSocket& operator<<(const string& sStr );

    /**
     *   Reading data to sStr from socket
     */
    CSocket& operator>>(string& sStr);

    /**
     *   Blocking function to check whether data arrived in socket for reading.
     *   @param timeToWait waits for 'timeToWait' seconds.
     */
    virtual int OnDataRead(unsigned long timeToWait = ULONG_MAX);

    /**
     *   To Bind socket to a symbolic device name like eth0
     *   @param sInterface NIC device name
     */
    void SetBindToDevice(const string& sInterface);

protected:
    /**
     *   Internal Socket descriptor
     **/
    int m_sockDesc;

    CSocket(SocketType type, NetworkLayerProtocol protocol);
    CSocket(int sockDesc);
    static void FillAddr( const string & localAddress, unsigned short localPort, sockaddr_in& localAddr );

private:
    // Prevent the user from trying to use Exact copy of this object
    CSocket(const CSocket &sock);
    void operator=(const CSocket &sock);
};

/**
 *   UDP Socket class.
 */

class UDPSocket : public CSocket
{
public:
/**
   *   Construct a UDP socket
   *   @exception SocketException thrown if unable to create UDP socket
   */
    UDPSocket();
  /**
   *   Construct a UDP socket with the given local port
   *   @param localPort local port
   *   @exception SocketException thrown if unable to create UDP socket
   */
    UDPSocket(unsigned short localPort);

  /**
   *   Construct a UDP socket with the given local port and address
   *   @param localAddress local address
   *   @param localPort local port
   *   @exception SocketException thrown if unable to create UDP socket
   */
    UDPSocket(const string &localAddress, unsigned short localPort);

  /**
   *   Unset foreign address and port
   *   @return true if disassociation is successful
   *   @exception SocketException thrown if unable to disconnect UDP socket
   */

  /**
   *   Unset foreign address and port
   *   @return true if disassociation is successful
   *   @exception SocketException thrown if unable to disconnect UDP socket
   */
    void DisconnectFromHost();

  /**
   *   Send the given buffer as a UDP datagram to the
   *   specified address/port
   *   @param buffer buffer to be written
   *   @param bufferLen number of bytes to write
   *   @param foreignAddress address (IP address or name) to send to
   *   @param foreignPort port number to send to
   *   @return true if send is successful
   *   @exception SocketException thrown if unable to send datagram
   */
    void SendDataGram(const void *buffer, int bufferLen, const string &foreignAddress,
        unsigned short foreignPort);

    /**
     *   Read read up to bufferLen bytes data from this socket.  The given buffer
     *   is where the data will be placed
     *   @param buffer buffer to receive data
     *   @param bufferLen maximum number of bytes to receive
     *   @param sourceAddress address of datagram source
     *   @param sourcePort port of data source
     *   @return number of bytes received and -1 for error
     *   @exception SocketException thrown if unable to receive datagram
     */
    int RecvDataGram(void *buffer, int bufferLen, string &sourceAddress,
               unsigned short &sourcePort);

    /**
    *   Set the multicast TTL
    *   @param multicastTTL multicast TTL
    *   @exception SocketException thrown if unable to set TTL
    */
    void SetMulticastTTL(unsigned char multicastTTL);

    /**
     *   Join the specified multicast group
     *   @param multicastGroup multicast group address to join
     *   @exception SocketException thrown if unable to join group
     */
    void JoinGroup(const string &multicastGroup);

    /**
     *   Leave the specified multicast group
     *   @param multicastGroup multicast group address to leave
     *   @exception SocketException thrown if unable to leave group
     */
    void LeaveGroup(const string &multicastGroup);

private:
    void SetBroadcast();

};


#endif /* INCLUDE_UDPSOCKET_H_ */
