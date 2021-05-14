// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#ifndef CONSTELLATION_H
#define CONSTELLATION_H

class Constellation;
#include "isl.h"
#include "satellite.h"
#include "city.h"
#include "binary_heap.h"

#define MAXNODES 1600

enum LinkType {UPLINK=0, DOWNLINK=1, ISL=2};

class Constellation {
public:
    Constellation(EventList& eventlist,
		  linkspeed_bps uplinkbitrate, mem_b uplinkqueuesize,
		  linkspeed_bps dowlinkbitrate, mem_b downlinkqueuesize,
		  linkspeed_bps islbitrate, mem_b islqueuesize);
    Satellite** sats() {return _sats;}
    int num_sats() const {return _num_sats;}
    inline Link& activate_link(Node& src, Node& dst, LinkType linktype) {
	return _link_factory.activate_link(src, dst,
					   _linkbitrate[linktype],
					   _linkqueuesize[linktype]);
    }
    inline void drop_link(Link& link) {
	_link_factory.drop_link(link);
    }
    void dijkstra_up_all_links();
    void dijkstra_down_links_in_route(Route* route);
    simtime_picosec get_rtt(Route* route);
    void dijkstra(City& src, City& dst, simtime_picosec time);
    Route* find_route(City& dst);
    inline mem_b uplinkqueuesize() const {return _linkqueuesize[::UPLINK];}
    inline mem_b downlinkqueuesize() const {return _linkqueuesize[::DOWNLINK];}
    inline mem_b isllinkqueuesize() const {return _linkqueuesize[::ISL];}
    inline linkspeed_bps uplinkbitrate() const {return _linkbitrate[::UPLINK];}
    inline linkspeed_bps downlinkbitrate() const {return _linkbitrate[::DOWNLINK];}
    inline linkspeed_bps isllinkbitrate() const {return _linkbitrate[::ISL];}
    
private:
    Link_factory _link_factory;
    Satellite *_sats[MAXNODES];
    linkspeed_bps _linkbitrate[3];
    mem_b _linkqueuesize[3];
    int _num_sats;
    BinaryHeap heap;
    Node* _route_src;
};

#endif
