#include "xcppacket.h"

PacketDB<XcpPacket> XcpPacket::_packetdb;
PacketDB<XcpAck> XcpAck::_packetdb;
PacketDB<XcpCtlPacket> XcpCtlPacket::_packetdb;
PacketDB<XcpCtlAck> XcpCtlAck::_packetdb;