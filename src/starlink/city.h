// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#ifndef CITY_H
#define CITY_H

#include <vector>
#include "Eigen/Dense"
#include "pipe.h"
#include "node.h"

class ActiveUplink {
 public:
    ActiveUplink(double dist, Satellite& sat, Link& uplink, Link& downlink)
	: _dist(dist), _sat(sat), _uplink(uplink), _downlink(downlink) {}
    friend bool active_uplink_cmp(ActiveUplink* l1, ActiveUplink* l2);
    double _dist;
    Satellite& _sat;
    Link& _uplink;
    Link& _downlink;
};

/* keep track of all the satellites that are out of range */
class InactiveSat {
 public:
    InactiveSat(double dist, Satellite& sat)
	: _dist(dist), _sat(sat) {
	//assert(&_sat > (void*)0x0000700f6f27b20d);
    }
    friend bool inactive_sat_cmp(InactiveSat* s1, InactiveSat* s2);
    double _dist;
    Satellite& _sat;
};

class City : public Node {
 public:
    City(double latitude, double longitude, Constellation& constellation);
    Logged* logged() const;
    void set_logged(Logged* logged);
    void add_uplinks(Satellite *sats[], size_t numsats, simtime_picosec time);
    void update_coordinates(simtime_picosec time);
    void update_uplinks(simtime_picosec time);
    Route *find_route(City& dst, simtime_picosec time);
 private:
    double _latitude;
    double _longitude;
    Constellation& _constellation;
    /* why do we keep arrays of inactive sats?  We do this because
       it's expensive to figure out the distances to all the
       satellites.  So we keep an array of inactive satellites sorted
       by distance.  But we don't bother to keep this array completely
       sorted - we only care about having it sorted so any satellites
       that are likely to become active are near the end, and then we
       sort the end properly. Every 10 seconds or so of simulated time
       (which is an eternity in a packet-level simulator) we'll sort
       the whole list properly, but the rest of the time we won't
       bother. */
    
    vector<ActiveUplink*> _active_uplinks;  // sorted so longest is at the end
    vector<InactiveSat*> _inactive_sats;    // sorted so shortest is at the end
    simtime_picosec _last_full_update;
    Logged* _logged;
};

#endif
