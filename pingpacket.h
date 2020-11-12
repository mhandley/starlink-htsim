#ifndef PING_PACKET
#define PING_PACKET

#include <list>
#include "network.h"

class PingPacket : public Packet {
public:
  static PacketDB<PingPacket> _packetdb;
  inline static PingPacket* newpkt(PacketFlow &flow, route_t &route, packetid_t seqno,
				   int size, simtime_picosec timestamp) {
    PingPacket* p = _packetdb.allocPacket();
    p->_timestamp = timestamp;
    p->_route = NULL;
    p->set_route(flow,route,size,seqno);
    return p;
  }

  packetid_t seqno() const {return id();}
  simtime_picosec timestamp() const {return _timestamp;}
  void free() {_packetdb.freePacket(this);}
  virtual ~PingPacket(){};
private:
  simtime_picosec _timestamp;
};
#endif
