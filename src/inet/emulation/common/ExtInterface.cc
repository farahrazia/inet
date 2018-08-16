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

#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <omnetpp/platdep/sockets.h>

#include "inet/common/Endian.h"
#include "inet/common/ModuleAccess.h"
#include "inet/emulation/common/ExtInterface.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"

namespace inet {

Define_Module(ExtInterface);

void ExtInterface::initialize(int stage)
{
    InterfaceEntry::initialize(stage);
    if (stage == INITSTAGE_LINK_LAYER) {
        registerInterface();
        if (!strcmp("copyFromExt", par("copyConfiguration")))
            copyInterfaceConfigurationFromExt();
        else
            configureInterface();
    }
    else if (stage == INITSTAGE_LINK_LAYER_2) {
        if (!strcmp("copyToExt", par("copyConfiguration")))
            copyInterfaceConfigurationToExt();
    }
}

void ExtInterface::configureInterface()
{
    const char *addressString = par("address");
    MacAddress address = strcmp(addressString, "auto") ? MacAddress(addressString) : MacAddress::generateAutoAddress();
    setMacAddress(address);
    setInterfaceToken(address.formInterfaceIdentifier());
    setMtu(par("mtu"));
    setBroadcast(true);      //TODO
    setMulticast(true);      //TODO
    setPointToPoint(true);   //TODO
}

void ExtInterface::registerInterface()
{
    IInterfaceTable *interfaceTable = findModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
    if (interfaceTable)
        interfaceTable->addInterface(this);
    inet::registerInterface(*this, gate("upperLayerIn"), gate("upperLayerOut"));
}

void ExtInterface::copyInterfaceConfigurationFromExt()
{
    std::string device = par("device").stdstringValue();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , device.c_str() , IFNAMSIZ-1);

    //get the IPv4 address
    ioctl(fd, SIOCGIFADDR, &ifr);
    Ipv4Address ipv4Address = Ipv4Address(ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr));

    //get the IPv4 netmask
    ioctl(fd, SIOCGIFNETMASK, &ifr);
    Ipv4Address ipv4Netmask = Ipv4Address(ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr));

    ioctl(fd, SIOCGIFHWADDR, &ifr);
    MacAddress macAddress;
    macAddress.setAddressBytes((unsigned char *)ifr.ifr_hwaddr.sa_data);

    ioctl(fd, SIOCGIFMTU, &ifr);
    int mtu = ifr.ifr_mtu;

    //TODO get IPv4 multicast addresses

    //TODO get IPv6 addresses

    close(fd);

    Ipv4InterfaceData *interfaceData = ipv4Data();
    if (interfaceData == nullptr)
        setIpv4Data(interfaceData = new Ipv4InterfaceData());
    setMacAddress(macAddress);
    setMtu(mtu);
    interfaceData->setIPAddress(Ipv4Address(ipv4Address));
    interfaceData->setNetmask(Ipv4Address(ipv4Netmask));
}

void ExtInterface::copyInterfaceConfigurationToExt()
{
    // TODO:
}

} // namespace inet

