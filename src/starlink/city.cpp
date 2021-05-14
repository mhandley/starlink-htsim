// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include <math.h>
#include <algorithm>
#include "constellation.h"
#include "isl.h"
#include "city.h"

// assume 57 degrees from vertical
//#define MAXDIST 1010
#ifdef XCP_STATIC_NETWORK
#define MAXDIST 10
#else
#define MAXDIST 1123
#endif


bool active_uplink_cmp(ActiveUplink* l1, ActiveUplink* l2) {
    return l1->_dist < l2->_dist;
}

bool inactive_sat_cmp(InactiveSat* s1, InactiveSat* s2) {
    return s1->_dist > s2->_dist;
}


// latitude and longitude are given in degrees
City::City(double latitude, double longitude, Constellation& constellation)
    : _constellation(constellation)
{
#ifdef XCP_STATIC_NETWORK
	_latitude = latitude;
	_longitude = longitude;
	_last_coords_update = 1; // force pos update
    _last_full_update = 0;
    update_coordinates(0);
    add_uplinks(_constellation.sats(), _constellation.num_sats(), 0);
#else
    // store lat, long in radians
    _latitude = radFromDeg(latitude);
    _longitude = radFromDeg(longitude);
    _last_coords_update = 1; // force pos update
    _last_full_update = 0;
    update_coordinates(0);
    add_uplinks(_constellation.sats(), _constellation.num_sats(), 0);
#endif
}

Logged* City::logged() const {
	return _logged;
}

void City::set_logged(Logged* logged) {
	_logged = logged;
}

void City::update_coordinates(simtime_picosec time) {
#ifdef XCP_STATIC_NETWORK
	Eigen::Vector3d v(_latitude,_longitude,0);
	_coordinates = v;
#else
    // To calculate city coordinates, place city on x-axis, rotate by
    // latitude around y-axis, then rotate by longitude + time*speed
    // around z-axis

    if (_last_coords_update == time)
	return;
    _last_coords_update = time;
    
    Eigen::Vector3d v(EARTH_RADIUS, 0.0, 0.0);
    simtime_picosec length_of_day = timeFromSec(86400);
    simtime_picosec time_of_day = time % length_of_day;
    double long_angle = _longitude - 2*M_PI*(double)time_of_day/(double)length_of_day;
    double lat_angle = _latitude;
    Eigen::AngleAxis<double> r_lat(lat_angle, Eigen::Vector3d(0.0, -1.0, 0.0));
    Eigen::AngleAxis<double> r_long(long_angle, Eigen::Vector3d(0.0, 0.0, -1.0));
    _coordinates = r_long*r_lat*v;
    cout << timeAsSec(time) << " City coords(" << this << ") " << position() << endl;
#endif
}

void City::add_uplinks(Satellite* sats[], size_t num_sats,simtime_picosec time) {
    double mindist = 200000;
    for (size_t i = 0; i < num_sats; i++) {
	double dist = distance((Node&)(*sats[i]), time);
	if (dist < mindist) mindist = dist;
	cout << "DIST: " << dist << " MAXDIST: " << MAXDIST << endl;
	//cout << "Sat " << i << " dist " << dist << " mindist " << mindist << endl;
	if (dist < MAXDIST) {
	    Link& uplink =
		_constellation.activate_link(*this, *(sats[i]), ::UPLINK);
	    Link& downlink =
		_constellation.activate_link(*(sats[i]), *this, ::DOWNLINK);
	    Node::add_link(uplink);
	    sats[i]->add_link(downlink);
	    _active_uplinks.push_back(new ActiveUplink(dist, *(sats[i]), uplink, downlink));
	} else {
	    InactiveSat* ias = new InactiveSat(dist, *(sats[i]));
	    //cout << "new ias2: " << ias << endl;
	    _inactive_sats.push_back(ias);
	}
    }
    // longest is at the end
    std::sort(_active_uplinks.begin(), _active_uplinks.end(), active_uplink_cmp);
    // shortest is at the end
    std::sort(_inactive_sats.begin(), _inactive_sats.end(), inactive_sat_cmp);

    _last_full_update = time;
}

void City::update_uplinks(simtime_picosec time) {
#ifdef XCP_STATIC_NETWORK
#else
    for (int i = 0; i < _active_uplinks.size(); i++) {
	Satellite& s = _active_uplinks[i]->_sat;
	double dist = distance((Node&)s, time);
	_active_uplinks[i]->_dist = dist;
	if (i > 0 && _active_uplinks[i-1]->_dist > dist) {
	    // swap the nodes.  largest will trickle to the end as the
	    // list starts sorted, and distances don't change fast,
	    // this pseudo-bubblesort is adequate.
	    ActiveUplink *a = _active_uplinks[i-1];
	    _active_uplinks[i-1] = _active_uplinks[i];
	    _active_uplinks[i] = a;
	}
    }

    // At this point, the ActiveUplink with the largest distance is at the end of _active_uplinks
    
    if (time - _last_full_update > timeFromSec(10)) {
	// it's long enough since the last full sort - we'll sort now.

	// update distances
	for (int i = 0; i < _inactive_sats.size(); i++) {
	    Satellite& s = _inactive_sats[i]->_sat;
	    double dist = distance((Node&)s, time);
	    _inactive_sats[i]->_dist = dist;
	}
	// full sort
	std::sort(_inactive_sats.begin(), _inactive_sats.end(), inactive_sat_cmp);
	_last_full_update = time;
    } else {
	// only update the nearest satellites - the rest can't matter yet
	int num_inactive = _inactive_sats.size();
	assert(num_inactive > 11);
	for (int i = num_inactive - 11; i < num_inactive; i++) {
	    Satellite& s = _inactive_sats[i]->_sat;
	    double dist = distance((Node&)s, time);
	    _inactive_sats[i]->_dist = dist;
	}
	// this is a single pass of a bubblesort, over the last 10
	// items only!  it ensures the lowest is at the end.  Nothing
	// else is guaranteed, but in practive we run this so often
	// it's fine - the lowest will always end up at the end.
	for (int i = num_inactive - 10; i < num_inactive; i++) {
	    if (_inactive_sats[i-1]->_dist < _inactive_sats[i]->_dist) {
		// shuffle lower delay sats towards end
		InactiveSat* ias = _inactive_sats[i-1];
		_inactive_sats[i-1] = _inactive_sats[i];
		_inactive_sats[i] = ias;
	    }
	}
    }


    // OK, now the sat lists are (more or less) sorted.  Time to move things around.
    // At this point, the closest inactive sat is at the end, and the furthest active sat is at the end.

    // Move any satellites that are now out of range from _active_uploinks to newly_inactive.
    InactiveSat* newly_inactive[_active_uplinks.size()];
    int newly_inactive_count = 0;
    while(1) {
	ActiveUplink* a = _active_uplinks.back();
	double dist = a->_dist;
	if (dist > MAXDIST) {
	    _active_uplinks.pop_back();
	    InactiveSat* ias = new InactiveSat(dist, a->_sat);
	    newly_inactive[newly_inactive_count] = ias;
	    newly_inactive_count++;
	    _constellation.drop_link(a->_uplink);
	    _constellation.drop_link(a->_downlink);
	    Node::remove_link(a->_uplink);
	    a->_sat.remove_link(a->_downlink);
	    delete(a);
	} else {
	    // no more to remove
	    break;
	}
    }

    // At this point, everything in _active_uplinks should be in
    // range, and everything in newly_inactive should be out of range.
    // The expectation is they stay out of range now for another
    // orbit.

    // now we move any previously inactive satellites that have come
    // into range from _inactive_sats into _active_sats.
    int newly_active_count = 0;
    while(1) {
	InactiveSat* s = _inactive_sats.back();
	double dist = distance((Node&)(s->_sat), time);
	if (dist < MAXDIST) {
	    _inactive_sats.pop_back();
	    Link& uplink = _constellation.activate_link(*this, s->_sat, ::UPLINK);
	    Link& downlink = _constellation.activate_link(s->_sat, *this, ::DOWNLINK);
	    _active_uplinks.push_back(new ActiveUplink(dist, s->_sat, uplink, downlink));
	    Node::add_link(uplink);
	    s->_sat.add_link(downlink);
	    delete(s);
	    newly_active_count++;
	} else {
	    // no more to move
	    break;
	}
    }

    // finally add the newly inactive sats to the main inactive list
    for (int i = 0; i < newly_inactive_count; i++) {
	_inactive_sats.push_back(newly_inactive[i]);
    }
#endif
}

Route* City::find_route(City& dst, simtime_picosec time) {
    Route* rt = NULL;
	cout << "FIND ROUTE DIJKSTRA" << endl;
    _constellation.dijkstra(*this, dst, time);
	cout << "FIND ROUTE FIND ROUTE" << endl;
    rt = _constellation.find_route(dst);
	cout << "FIND ROUTE FINISH" << endl;
	if (rt) {
        rt->use_refcount();
		rt->incr_refcount();
	}
    return rt;
}
