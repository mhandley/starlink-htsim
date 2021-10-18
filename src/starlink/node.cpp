// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include <math.h>
#include "isl.h"
#include "node.h"

Link_factory* Node::link_factory = 0;

Node::Node()
    : _last_coords_update(0), _link_count(0), _dist(DIST_INFINITY),
      _parent(0), _parentlink(0)
{
    if (!link_factory) {
	cerr << "You must initialize the Link factory before constructing nodes\n";
	abort();
    }
}

void Node::add_link_to_dst(Node& dst, linkspeed_bps bitrate, mem_b maxqueuesize) {
    Link& link = link_factory->activate_link(*this, dst, bitrate, maxqueuesize);
    add_link(link);
}

void Node::add_link(Link& link) {
    cout << "Adding link " << _link_count << endl;
    //cout << "add_link, this: " << this << " src: " << &(link.src()) << endl;
    assert(&(link.src()) == this);
    assert(_link_count < MAXLINKS);
    _links.insert(&link);
    _link_count++;
}

void Node::remove_link(Link& link) {
    // TBD
    _links.erase(&link);
    _link_count--;
    //abort();
}

double Node::distance(Node& dst, simtime_picosec time) {
    update_coordinates(time);
    dst.update_coordinates(time);
    cout << "This Coordinates: " << _coordinates[0] << " " << _coordinates[1] << " " << _coordinates[2] << endl;
    cout << "Dst Coordinates: " << dst.coordinates()[0] << " " << dst.coordinates()[1] << " " << dst.coordinates()[2] << endl;
    double dist = (_coordinates - dst.coordinates()).norm();
    // cout << "time: " << timeAsMs(time) << " dist: " << dist << endl;
    return dist;
}

string Node::position() const {
    static string s;
    s = to_string(_coordinates[0]) + " " + to_string(_coordinates[1]) + " " + to_string(_coordinates[2]);
    return s;
}

void
Node::clear_routing() {
    _parent = NULL;
    _parentlink = NULL;
}

void Node::print_links() const {
    string mypos = position();
    string theirpos;
    unordered_set<Link*>::const_iterator i;;
    for (i = _links.begin(); i != _links.end(); i++) {
	theirpos = (*i)->dst().position();
	cout << mypos << endl;
	cout << theirpos << endl << endl << endl;
    }
}

