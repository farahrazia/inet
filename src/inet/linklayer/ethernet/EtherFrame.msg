//
// Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.linklayer.common.EtherType;
import inet.linklayer.common.FcsMode;
import inet.linklayer.common.Ieee802Ctrl;
import inet.linklayer.common.MacAddress;

cplusplus {{
#include "inet/linklayer/ethernet/Ethernet.h"
}}

namespace inet;

enum EthernetControlOpCode
{
    ETHERNET_CONTROL_PAUSE = 1;
}

enum SapCode;

//
// Ethernet MAC header
//
class EthernetMacHeader extends FieldsChunk
{
    chunkLength = B(14);
    MacAddress dest;
    MacAddress src;
    int typeOrLength;
}

cplusplus {{
    inline bool isEth2Header(const EthernetMacHeader& hdr)  { return isEth2Type(hdr.getTypeOrLength()); }  // typeOrLength is type
    inline bool isIeee8023Header(const EthernetMacHeader& hdr)  { return isIeee8023Length(hdr.getTypeOrLength()); }  // typeOrLength is length
}}

//
//
// Ethernet control frames used by the PAUSE protocol
//
class EthernetControlFrame extends FieldsChunk
{
//    EthernetHeader.etherType = ETHERTYPE_FLOW_CONTROL;
    int opCode = -1;
}

//
// Ethernet frame used by the PAUSE protocol
//
class EthernetPauseFrame extends EthernetControlFrame
{
    chunkLength = B(4);
    opCode = ETHERNET_CONTROL_PAUSE;
    int pauseTime; // in 512 bit-time units
}

//
// Ethernet 802.1Q Tag.
//
class Ieee802_1QHeader extends FieldsChunk
{
//    previous etherType = 0x8100 (C-TAG), or 0x88a8 (S-TAG);
    chunkLength = B(4);
    uint8_t pcp;    // Priority Code Point (PCP): a 3-bit field which refers to the IEEE 802.1p priority. It indicates the frame priority level.
                    // Values are from 0 (best effort) to 7 (highest); 1 represents the lowest priority.
    bool de;        // Drop Eligible (DE): a 1-bit field. May be used separately or in conjunction with PCP to indicate frames eligible to be dropped in the presence of congestion.
    short VID=0;   // VLAN Identifier (VID): a 12-bit field specifying the VLAN to which the frame belongs.
                    // The hexadecimal values of 0x000 and 0xFFF are reserved. All other values may be used as VLAN identifiers, allowing up to 4,094 VLANs.
                    // The reserved value 0x000 indicates that the frame does not belong to any VLAN; in this case,
                    // the 802.1Q tag specifies only a priority and is referred to as a priority tag. On bridges,
                    // VLAN 1 (the default VLAN ID) is often reserved for a management VLAN; this is vendor-specific.
    int nextTypeOrLength;
}

class EthernetPadding extends FieldsChunk
{
}

class EthernetFcs extends FieldsChunk
{
    chunkLength = ETHER_FCS_BYTES;
    uint32_t fcs;
    FcsMode fcsMode = FCS_MODE_UNDEFINED;
}

