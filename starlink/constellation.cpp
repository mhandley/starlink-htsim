// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                             

#include "binary_heap.h"
#include "constellation.h"

Constellation::Constellation(EventList& eventlist,
                  linkspeed_bps uplinkbitrate, mem_b uplinkqueuesize,
                  linkspeed_bps downlinkbitrate, mem_b downlinkqueuesize,
                  linkspeed_bps islbitrate, mem_b islqueuesize) :
    _link_factory(eventlist), heap(MAXNODES), _route_src(0)
{
    Logfile* lg;

    _linkbitrate[::UPLINK] = uplinkbitrate;
    _linkbitrate[::DOWNLINK] = downlinkbitrate;
    _linkbitrate[::ISL] = islbitrate;
    
    _linkqueuesize[::UPLINK] = uplinkqueuesize;
    _linkqueuesize[::DOWNLINK] = downlinkqueuesize;
    _linkqueuesize[::ISL] = islqueuesize;
    
    Node::link_factory = &_link_factory;

    double raan = 0;
    int satnum;
    for (int plane = 0; plane < 24; plane++) {
	double mean_anomaly = 0;
	for (int sat = 0; sat < 66; sat++) {
	    mean_anomaly = (plane * 13.0/24.0 + sat) * 360.0 / 66;
	    raan = plane * 360.0 / 24;
	    satnum = sat + plane*66;
	    _sats[satnum] = new Satellite(plane, sat, satnum, 53.0, raan,
					 mean_anomaly, 5739.0, 550.0 /* alt in km */);
	}
    }
    _num_sats = satnum + 1;
    int isl_plane_shift = -1;
    int isl_plane_step = 1;
    int sats_per_orbit = 66;
    for (int plane = 0; plane < 24; plane++) {
	for (int sat = 0; sat < sats_per_orbit; sat++) {
	    int satnum = sat + plane*sats_per_orbit;
	    Satellite *s = _sats[satnum];
	    
	    Satellite *nextsat = _sats[satnum+1];
	    if (sat == sats_per_orbit-1) {
		// next sat is the first in the plane 
		nextsat = _sats[plane*sats_per_orbit];
	    }
	    s->add_link_to_dst(*nextsat, _linkbitrate[ISL], _linkqueuesize[ISL]);

	    Satellite *prevsat = _sats[satnum-1];
	    if (sat == 0) {
		// prev sat is the last in the plane
		prevsat = _sats[plane*sats_per_orbit + sats_per_orbit - 1];
	    }
	    s->add_link_to_dst(*prevsat, _linkbitrate[ISL], _linkqueuesize[ISL]);


	    int leftoffset = isl_plane_step*sats_per_orbit + isl_plane_shift;  // default offset, ignoring wrapping
	    // ensure we connect to the correct plane
	    while ((sat + leftoffset) / sats_per_orbit < isl_plane_step) {
		leftoffset += sats_per_orbit;
	    }
	    while ((sat + leftoffset) / sats_per_orbit > isl_plane_step) {
		leftoffset -= sats_per_orbit;
	    }

	    int leftsatnum = satnum + leftoffset;
	    if (leftsatnum >= _num_sats) {
		leftsatnum = leftsatnum - _num_sats + 13/* + isl_plane_shift*/;
		if (leftsatnum >= sats_per_orbit) {
		    leftsatnum -= sats_per_orbit;
		}
	    }
	    Satellite *leftsat = _sats[leftsatnum];
	    s->add_link_to_dst(*leftsat, _linkbitrate[ISL], _linkqueuesize[ISL]);

	    int rightoffset = -isl_plane_step*sats_per_orbit - isl_plane_shift;  // default offset, ignoring wrapping

	    // ensure we connect to the correct plane
	    while (floor((sat + rightoffset) / (double)sats_per_orbit) < -isl_plane_step) { // division rounds towards zero
		rightoffset += sats_per_orbit;
	    }
	    while (floor((sat + rightoffset) / (double) sats_per_orbit) > -isl_plane_step) {
		rightoffset -= sats_per_orbit;
	    }

	    int rightsatnum = satnum + rightoffset;
	    if (rightsatnum < 0) {
		rightsatnum = rightsatnum + _num_sats - 13;
		if (rightsatnum < _num_sats - sats_per_orbit) {
		    rightsatnum += sats_per_orbit;
		}
	    }

	    Satellite *rightsat = _sats[rightsatnum];
	    s->add_link_to_dst(*rightsat, _linkbitrate[ISL], _linkqueuesize[ISL]);
	}
    }
    /*
    for (int sat=0; sat < _num_sats; sat++) {
	_sats[sat]->print_links();
    }
    
    for (int sat=_num_sats-66; sat < _num_sats; sat++) {
	_sats[sat]->print_links();
    }
    */
}

void
Constellation::dijkstra(City& src, City& dst) {
    assert(heap.size() == 0);
    _route_src = &src;
    heap.insert(src);
    for (int sat=0; sat < _num_sats; sat++) {
	heap.insert_at_infinity(*_sats[sat]);
    }
    heap.insert_at_infinity(dst);
    src.set_dist(0);
    while (heap.size() > 0) {
	Node& u = heap.extract_min();
	//for (int i = 0; i < u.link_count(); i++) {
	unordered_set<Link*>::iterator i;;
	for (i = u._links.begin(); i != u._links.end(); i++) {
	    Link& l = *(*i);
	    Node& n = l.get_neighbour(u);
	    simtime_picosec dist = n.dist();
	    simtime_picosec linkdelay = l.delay();
	    //cout << "ps2:" << linkdelay << endl;
	    simtime_picosec newdist = u.dist() + linkdelay;
	    //cout << "newdist:" << newdist << endl;
	    if (newdist < dist) {
		//cout << "newdist2:" << newdist << " was " << dist << endl;
		n.set_dist(newdist);
		n.set_parent(u, l);
		heap.decrease_distance(n, newdist);
	    }
	}
    }
}


Route*
Constellation::find_route(City& dst) {
    //cout << "find_route" << endl;
    Route* route = new Route();
    Node* n = &dst;
    int hop = 0;
    while (n && n != _route_src) {
	Link* l = n->parentlink();
	if (l) {
	    //cout << l->nodename() << " " << this << endl;
	    route->push_front(static_cast<PacketSink*>(l));
	    //cout << l->queue().nodename() << " " << this << endl;
	    route->push_front(static_cast<PacketSink*>(&(l->queue())));
	    hop++;
	}
	n = n->parent();
	if (n == NULL) {
	    cout << "no next hop" << endl;
	    break;
	}
    }
    //cout << _route_src->position() << endl;
    //cout << "Latency: " << timeAsMs(dst.dist()) << endl;

    /*
    cout << "test" << endl;
    for (int sat=0; sat < _num_sats; sat++) {
        cout << "sat: " << sat << " dist: " << _sats[sat]->dist() << endl;
    }
    */

    return route;
}
