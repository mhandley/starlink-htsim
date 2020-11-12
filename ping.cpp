// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include <sstream>
#include "ping.h"
#include "pingpacket.h"

PingClient::PingClient(EventList &eventlist, int pps, int mss) :
    EventSource(eventlist, "pingclient"), _flow(NULL)
{
    _period = timeFromSec(1.0)/pps;
    _server = NULL;
    _route = NULL;
    _mss = mss;
    _maxseqno = 0;
    _remaining_pkts = -1;
}


PingClient::PingClient(EventList &eventlist, simtime_picosec period, int mss) :
    EventSource(eventlist, "pingclient"), _flow(NULL)
{
    _period = period;
    _server = NULL;
    _route = NULL;
    _mss = mss;
    _maxseqno = 0;
    _remaining_pkts = -1;
}


void
PingClient::connect(route_t& routeout, route_t& routeback,
		    PingServer& server, simtime_picosec startTime, int count) {
    _route = &routeout;
    _route->incr_refcount();
    _server = &server;
    if (count > 0) {
	_remaining_pkts = count;
    } else {
	_remaining_pkts = -1;
    }
    _flow.id = id;
    stringstream ss;
    ss << "pingclient(" << id << ")";
    _nodename = ss.str();

    server.connect(routeback, *this);
    eventlist().sourceIsPending(*this, startTime);
}

void
PingClient::update_route(route_t& routeout, route_t& routeback) {
    //cout << "client:update_route: " << _route->refcount() << endl;
    if ((_route->refcount()) > 0) {
	_route->decr_refcount();
    }
    _route = &routeout;
    _route->incr_refcount();
    _server->update_route(routeback);
    //cout << "client:update_route done: " << _route->refcount() << endl;
}

void PingClient::doNextEvent() {
    send_packet();
    if (_remaining_pkts == -1 || _remaining_pkts > 0) {
	eventlist().sourceIsPendingRel(*this,_period);
    }
}

void PingClient::send_packet() {
    //cout << "sending, time now " << timeAsMs(eventlist().now()) << "ms" << endl;
    // echo request and responses are the same
    _maxseqno++;
    Packet* p = PingPacket::newpkt(_flow, *_route, _maxseqno, _mss, eventlist().now());
    p->sendOn();
    if (_remaining_pkts > 0) {
	_remaining_pkts--;
    }
}

void
PingClient::receivePacket(Packet& pkt) {
    //cout << "received echo" << endl;
    PingPacket* pp = static_cast<PingPacket*>(&pkt);
    cout << "time: " << timeAsSec(eventlist().now()) << " seqno: " << pp->seqno() << " " << timeAsMs(eventlist().now() - pp->timestamp())
	 << "ms" << endl;
    pkt.free();
    //cout << "client: receive done\n";
}
		       
PingServer::PingServer() : _flow(NULL)
{
    _client = NULL;
    _route  = NULL;
    stringstream ss;
    ss << "pingserver()";
    _nodename = ss.str();
}

void
PingServer::connect(route_t& route, PingClient &client) {
    _route = &route;
    _route->incr_refcount();
    _client = &client;
    _flow.id = client.id;
}

void
PingServer::update_route(route_t& route) {
    //cout << "server:update_route: " << _route->refcount() << endl;
    if (_route->refcount() > 0)
	_route->decr_refcount();
    _route = &route;
    _route->incr_refcount();
    //cout << "server:update_route done: " << _route->refcount() << endl;
}

void
PingServer::receivePacket(Packet& pkt) {
    //cout << "received echo request" << endl;
    PingPacket* request = static_cast<PingPacket*>(&pkt);
    Packet* response = PingPacket::newpkt(_flow, *_route, request->seqno(),
    					  pkt.size(), request->timestamp());
    pkt.free();
    //cout << "client: free done\n";
    response->sendOn();
    //cout << "client: send done\n";
}







