// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#ifndef ISL_H
#define ISL_H

#include "pipe.h"
#include "node.h"
#include "queue.h"

class Link_factory {
 public:
    Link_factory(EventList& eventlist);
    Link& activate_link(Node& src, Node& dst, linkspeed_bps bitrate,
			mem_b maxqueuesize);
    void drop_link(Link& link);
 private:
    EventList& _eventlist;
    int _pool_size;
    int _active_link_count;
    Link** _active_links;
    int _free_link_count;
    Link** _free_links;
};

// A link is a pipe, but it also contains an associated queue so we
// can dynamically build routes on demand from a topology of nodes and
// links.  The general idea is that the packet is received on the
// queue first, then on the link.  When it is received on the link,
// the link length can be recalculated, so the packet arrives at the
// next sink at the right time.

class Link: public Pipe {
    friend class Link_factory;
 public:
    Link(Node& src, Node& dst, linkspeed_bps bitrate, mem_b maxsize,
	 int link_index, EventList& eventlist);
    void reassign(Node& src, Node& dst, linkspeed_bps bitrate, mem_b maxqueuesize,
		  int link_index);
    void going_down();
    void receivePacket(Packet& pkt); // inherited from PacketSink
    void doNextEvent(); // inherited from EventSource
    virtual simtime_picosec delay() const;
    const Node& src() {return *_src;}
    const Node& dst() {return *_dst;}
    Queue& queue() {return _queue;}
    Node& get_neighbour(Node& n);
 private:
    Node* _src;
    Node* _dst;
    Queue _queue;  
    bool _up;
    int _link_index; // used only by Link factory
};

#endif
