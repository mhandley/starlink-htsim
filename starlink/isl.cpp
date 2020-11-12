// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#include "isl.h"


// Links go up and down pretty regularly, but the number stays pretty
// constant.  Rather than create and free them, we use a factory and
// reuse old links.
Link_factory::Link_factory(EventList& eventlist) : _eventlist(eventlist){
    _active_link_count = 0;
    _free_link_count = 0;
    _pool_size = 128;
    _active_links = (Link**)malloc(sizeof(Link*) * _pool_size);
    _free_links = (Link**)malloc(sizeof(Link*) * _pool_size);
}

Link& Link_factory::activate_link(Node& src, Node& dst,
				  linkspeed_bps bitrate, mem_b maxqueuesize) {
    //cout << "LF: activate_link: " << &src << endl;
    if (_active_link_count == _pool_size) {
	// our arrays are too small
	_pool_size *= 2;
	_active_links = (Link**)realloc(_active_links, sizeof(Link*) * _pool_size);
	_free_links = (Link**)realloc(_active_links, sizeof(Link*) * _pool_size);
    }
    Link* link;
    if (_free_link_count == 0) {
	//cout << "here1\n";
	link = new Link(src, dst, bitrate, maxqueuesize, _active_link_count, _eventlist);
	_active_links[_active_link_count] = link;
	_active_link_count++;
    } else {
	//cout << "here2\n";
	link = _free_links[_free_link_count - 1];
	link->reassign(src, dst, bitrate, maxqueuesize, _active_link_count);
	_active_links[_active_link_count] = link;
	_active_link_count++;
	_free_link_count--;
    }
    //cout << "LF src: " << &(link->src()) << endl;
    return *link;
}

void Link_factory::drop_link(Link& link) {
    int ix = link._link_index;
    _free_links[_free_link_count] = &link;
    link._link_index = _free_link_count;
    _free_link_count++;

    if (ix < _active_link_count-1) {
	// swap to fill the hole
	assert(_active_links[ix] == &link);
	_active_links[ix] = _active_links[_active_link_count-1];
	_active_links[ix]->_link_index = ix;
    }
    _active_link_count--;
    link.going_down();
}


Link::Link(Node& src, Node& dst, linkspeed_bps bitrate, mem_b maxqueuesize,
	   int link_index, EventList& eventlist) :
    Pipe(0, eventlist), _src(&src), _dst(&dst),
    _queue(bitrate, maxqueuesize, eventlist, NULL),  _link_index(link_index)
{
    stringstream ss;
    ss << "link(" << delay()/1000000 << "us)";
    _nodename = ss.str();
    _up = true;
}

void Link::reassign(Node& src, Node& dst, linkspeed_bps bitrate,
		    mem_b maxqueuesize, int link_index) {
    //cout << "reassign: src: " << &src << endl;
    _src = &src;
    //cout << "reassign: _src: " << _src << endl;
    //assert(_src > (void*)0x0000700eefbfc1d0);
    _dst = &dst;
    _link_index = link_index;
    _up = true;
    _queue.set_bitrate(bitrate);
    _queue.set_bufsize(maxqueuesize);
}

void Link::going_down() {
    _up = false;
    _queue.drop_all(); // drops packets from queue
    // XXX need to drop the packets in flight...
}

void Link::receivePacket(Packet& pkt) {
    //cout << "link at " << timeAsMs(eventlist().now()) << "ms" << endl;
    set_delay(delay());
    simtime_picosec now = eventlist().now();
    double dist = _src->distance(*_dst, now);
    simtime_picosec delay = Pipe::delay();
    cout << "time: " << timeAsSec(now) << " hop: " << pkt.nexthop() << " delay: " << timeAsMs(delay) << " dist: " << dist << endl;
    Pipe::receivePacket(pkt);
}

void Link::doNextEvent() {
    Pipe::doNextEvent();
}

simtime_picosec Link::delay() const {
    double dist = _src->distance(*_dst, eventlist().now());
    simtime_picosec delay = timeFromDistVacuum(dist);
    return delay;
}

Node& Link::get_neighbour(Node& n) {
    if (&n == _src) {
	return *_dst;
    }
    if (&n == _dst) {
	return *_src;
    }
    abort();
}

