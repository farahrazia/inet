//
// Copyright (C) OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#define WANT_WINSOCK2

#include <omnetpp/platdep/sockets.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>

#include "inet/common/checksum/EthernetCRC.h"
#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/emulation/common/RawSocket.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

namespace inet {

Define_Module(RawSocket);

RawSocket::~RawSocket()
{
    if (fd != INVALID_SOCKET) {
        close(fd);
        fd = INVALID_SOCKET;
    }
}

void RawSocket::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        device = par("device");
        const char *protocolName = par("protocol");
        if (!strcmp(protocolName, "ipv4")) {
            protocol = &Protocol::ipv4;
            fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        }
        else if (!strcmp(protocolName, "ethernetMac")) {
            protocol = &Protocol::ethernetMac;
            struct ifreq if_mac;
            struct ifreq if_idx;
            fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
            /* Get the index of the interface to send on */
            memset(&if_idx, 0, sizeof(struct ifreq));
            strncpy(if_idx.ifr_name, device, IFNAMSIZ-1);
            if (ioctl(fd, SIOCGIFINDEX, &if_idx) < 0)
                perror("SIOCGIFINDEX");
            /* Get the MAC address of the interface to send on */
            memset(&if_mac, 0, sizeof(struct ifreq));
            strncpy(if_mac.ifr_name, device, IFNAMSIZ-1);
            if (ioctl(fd, SIOCGIFHWADDR, &if_mac) < 0)
                perror("SIOCGIFHWADDR");
            ifindex = if_idx.ifr_ifindex;
            macAddress.setAddressBytes(if_mac.ifr_hwaddr.sa_data);
        }
        else
            throw cRuntimeError("Unknown protocol");
        if (fd == INVALID_SOCKET)
            throw cRuntimeError("RawSocket interface: Root privileges needed");
        // bind to interface:
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", device);
        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0)
            throw cRuntimeError("RawSocket: couldn't bind raw socket to '%s' interface", device);

        struct sockaddr_ll socket_address;
        memset(&socket_address, 0, sizeof (socket_address));
        socket_address.sll_family = PF_PACKET;
        socket_address.sll_ifindex = ifindex;
        socket_address.sll_protocol = htons(ETH_P_ALL);
        int n = bind(fd, (struct sockaddr *)&socket_address, sizeof(socket_address));
        if (n < 0)
            throw cRuntimeError("Cannot bind socket: %d", n);

        if (gate("upperLayerOut")->isConnected()) {
            auto scheduler = check_and_cast<RealTimeScheduler *>(getSimulation()->getScheduler());
            scheduler->addCallback(fd, this);
        }

        numSent = numReceived;
        WATCH(numSent);
        WATCH(numReceived);
    }
}

void RawSocket::handleMessage(cMessage *msg)
{
    Packet *packet = check_and_cast<Packet *>(msg);
    if (protocol != packet->getTag<PacketProtocolTag>()->getProtocol())
        throw cRuntimeError("Invalid protocol");

    struct sockaddr_in ip_addr;
    struct sockaddr_ll socket_address;
    struct sockaddr * addr = nullptr;
    size_t addrsize = 0;

    if (protocol == &Protocol::ipv4) {
        ip_addr.sin_family = AF_INET;
#if !defined(linux) && !defined(__linux) && !defined(_WIN32)
        ip_addr.sin_len = sizeof(struct sockaddr_in);
#endif // if !defined(linux) && !defined(__linux) && !defined(_WIN32)
        ip_addr.sin_port = htons(0);
        addr = (struct sockaddr *)&ip_addr;
        addrsize = sizeof(ip_addr);
    }
    else if (protocol == &Protocol::ethernetMac) {
        /* Index of the network device */
        socket_address.sll_ifindex = ifindex;
        /* Address length*/
        socket_address.sll_halen = ETH_ALEN;
        /* Destination MAC */
        socket_address.sll_addr[0] = macAddress.getAddressByte(0);
        socket_address.sll_addr[1] = macAddress.getAddressByte(1);
        socket_address.sll_addr[2] = macAddress.getAddressByte(2);
        socket_address.sll_addr[3] = macAddress.getAddressByte(3);
        socket_address.sll_addr[4] = macAddress.getAddressByte(4);
        socket_address.sll_addr[5] = macAddress.getAddressByte(5);
        addr = (struct sockaddr *)&socket_address;
        addrsize = sizeof(socket_address);
    }

    auto bytesChunk = packet->peekAllAsBytes();
    uint8 buffer[1 << 16];
    size_t packetLength = bytesChunk->copyToBuffer(buffer, sizeof(buffer));
    ASSERT(packetLength == (size_t)packet->getByteLength());

    sendBytes(buffer, packetLength, addr, addrsize);
    numSent++;
    delete packet;
}

void RawSocket::sendBytes(uint8 *buf, size_t numBytes, struct sockaddr *to, socklen_t addrlen)
{
    //TODO check: is this an IPv4 packet --OR-- is this packet acceptable by fd socket?
    if (fd == INVALID_SOCKET)
        throw cRuntimeError("RawSocket::sendBytes(): no raw socket.");

    int sent = sendto(fd, buf, numBytes, 0, to, addrlen);    //note: no ssize_t on MSVC

    if ((size_t)sent == numBytes)
        EV << "Sent " << sent << " bytes packet.\n";
    else
        EV << "Sending packet FAILED! (sendto returned " << sent << " (" << strerror(errno) << ") instead of " << numBytes << ").\n";
    return;
}

void RawSocket::refreshDisplay() const
{
    char buf[80];
    sprintf(buf, "device: %s\nsnt:%d rcv:%d", device, numSent, numReceived);
    getDisplayString().setTagArg("t", 0, buf);
}

void RawSocket::finish()
{
    std::cout << getFullPath() << ": " << numSent << " packets sent, " << numReceived << " packets received\n";
    close(fd);
    fd = INVALID_SOCKET;
}

bool RawSocket::notify(int fd)
{
    Enter_Method_Silent();
    ASSERT(this->fd == fd);
    uint8_t buffer[1 << 16];
    memset(&buffer, 0, sizeof(buffer));
    // type of buffer in recvfrom(): win: char *, linux: void *
    int n = ::recv(fd, (char *)buffer, sizeof(buffer), 0);
    if (n < 0)
        throw cRuntimeError("Calling recvfrom failed: %d", n);
    n = std::max(n, ETHER_MIN_LEN - 4);
    uint32_t checksum = htonl(ethernetCRC(buffer, n));
    memcpy(&buffer[n], &checksum, sizeof(checksum));
    auto data = makeShared<BytesChunk>(static_cast<const uint8_t *>(buffer), n + 4);
    auto packet = new Packet("RawSocketPacket", data);
    auto interfaceEntry = check_and_cast<InterfaceEntry *>(getContainingNicModule(this));
    packet->addTag<InterfaceInd>()->setInterfaceId(interfaceEntry->getInterfaceId());
    packet->addTag<PacketProtocolTag>()->setProtocol(protocol);
    packet->addTag<DispatchProtocolReq>()->setProtocol(protocol);
    emit(packetReceivedSignal, packet);
    send(packet, "upperLayerOut");
    emit(packetSentToUpperSignal, packet);
    return true;
}

} // namespace inet

