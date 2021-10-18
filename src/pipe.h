// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef PIPE_H
#define PIPE_H

/*
 * A pipe is a dumb device which simply delays all incoming packets
 */

#include <list>
#include <utility>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"


class Pipe : public EventSource, public PacketSink {
 public:
    Pipe(simtime_picosec delay, EventList& eventlist);
    void receivePacket(Packet& pkt); // inherited from PacketSink
    void doNextEvent(); // inherited from EventSource
    virtual simtime_picosec delay(simtime_picosec time = 0) { return _delay; }
    const string& nodename() { return _nodename; }
    void set_delay(simtime_picosec delay) {_delay = delay;}  // used when link delay changes
 protected:
    string _nodename;
 private:
    mutable simtime_picosec _delay;
    typedef pair<simtime_picosec,Packet*> pktrecord_t;
    list<pktrecord_t> _inflight; // the packets in flight (or being serialized)
};


#endif
