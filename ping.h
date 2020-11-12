// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef PING_H
#define PING_H

/*
 * A ping implementation.  Client and Server are both Sink and Src
 */

#include "config.h"
#include "network.h"
#include "eventlist.h"

class PingServer;
class PingClient;

class PingClient: public EventSource, public PacketSink {
public:
    PingClient(EventList &eventlist, int pps, int mss);
    PingClient(EventList &eventlist, simtime_picosec  pps, int mss);

    void connect(route_t& routeout, route_t& routeback,
		 PingServer& sink, simtime_picosec startTime, int count);
    void update_route(route_t& routeout, route_t& routeback);
    void send_packet();
    void receivePacket(Packet& pkt);
    void doNextEvent();
    virtual const string& nodename() { return _nodename; }

 private:
    // Connectivity
    PacketFlow _flow;
    PingServer* _server;
    route_t* _route;
    int _mss;
 
    // Mechanism
    simtime_picosec _period;
    int _remaining_pkts;
    packetid_t _maxseqno;
    string _nodename;
    void send_packets();
};

class PingServer : public PacketSink {
public:
    PingServer();
    void connect(route_t& route, PingClient &client);
    void update_route(route_t& route);
    void receivePacket(Packet& pkt);
    //uint32_t get_id(){ return id;}
    //uint32_t drops(){return 1;}
    virtual const string& nodename() { return _nodename; }
private:
    PacketFlow _flow;
    PingClient* _client;
    route_t* _route;
    string _nodename;
};

#endif
