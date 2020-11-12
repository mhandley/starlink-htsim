// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#ifndef NODE_H
#define NODE_H

#include <unordered_set>
#include "Eigen/Dense"
#include "pipe.h"

//12742km diameter
#define EARTH_RADIUS 6371
#define MAXLINKS 50
#define DIST_INFINITY 1000000000000.0

class Link;
class Link_factory;


class Node {
 public:
    static Link_factory* link_factory;
    Node();
    double distance(Node& dst, simtime_picosec time);
    void add_link_to_dst(Node& dst, linkspeed_bps bitrate, mem_b maxqueuesize);
    void add_link(Link& link);
    void remove_link(Link& link);
    inline int link_count() {return _link_count;}
    //inline Link& link(int linknum) {return *_links[linknum];}
    virtual void update_coordinates(simtime_picosec time) = 0;
    const Eigen::Vector3d& coordinates() const {return _coordinates;}
    string position() const;
    inline simtime_picosec dist() const {return _dist;}
    inline void set_dist(simtime_picosec dist) {_dist = dist;}
    inline Node* parent() const {return _parent;}
    inline Link* parentlink() const {return _parentlink;}
    inline void set_parent(Node& parent, Link& parentlink) {
	_parent = &parent;
	_parentlink = &parentlink;
    }
    void clear_routing();
    void print_links() const;  // debugging

    int heap_ix; // state for running dijkstra
    unordered_set<Link*> _links;
 protected:
    Eigen::Vector3d _coordinates;
    simtime_picosec _last_coords_update;
    int _link_count;

    // state for running dijkstra
    simtime_picosec _dist;
    Node* _parent;
    Link* _parentlink;
    
 private:
};

#endif
