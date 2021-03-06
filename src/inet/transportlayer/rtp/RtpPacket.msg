//
// Copyright (C) 2008 Andras Varga
// Copyright (C) 2001 Matthias Oppitz
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet::rtp;

cplusplus {{
    const B RTPPACKET_FIX_HEADERLENGTH = B(12);
}}

//
// This class represents an RTP data packet. Real data can either
// be encapsulated, or simulated by adding length.
//
// The following RTP header fields exist but aren't used:
// padding, extension, csrcCount. The csrcList can't be used
// because csrcCount is always 0.
//
class RtpHeader extends FieldsChunk
{
    chunkLength = RTPPACKET_FIX_HEADERLENGTH;  // 12-byte fixed header

    // The rtp version of this ~RtpPacket.
    uint8 version = 2;      // 2 bits

    // Set to 1 if padding is used in this ~RtpPacket, 0 otherwise.
    // This implementation doesn't use padding bytes, so it is always 0.
    bool padding = false;      // 1 bit

    // Set to 1, if this ~RtpPacket contains an rtp header extension, 0 otherwise.
    // This implementation doesn't support rtp header extensions, so it is always 0.
    bool extension = false;      // 1 bit

    // uint8_t cc;      // 4 bits, csrc count

    // The marker.
    bool marker = false;      // 1 bit

    // The type of payload carried in this ~RtpPacket.
    int8 payloadType;      // 7 bits

    // The sequence number of this ~RtpPacket.
    uint16 sequenceNumber;      // 16 bits

    // The rtp time stamp of this ~RtpPacket.
    uint32 timeStamp;      // 32 bits

    // The ssrc identifier of the creator of this ~RtpPacket.
    uint32 ssrc;      // 32 bits

    // no mixers, no contributing sources
    uint32 csrc[];      // cc * 32 bits
};

cplusplus(RtpHeader) {{
  public:
    /**
     * Writes a one line info about this RtpHeader into the given string.
     */
    virtual std::string str() const override;

    /**
     * Writes a longer description about this RtpHeader into the given stream.
     */
    virtual void dump() const;
}}

