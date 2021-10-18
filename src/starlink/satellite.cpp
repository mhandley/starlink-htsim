// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include <math.h>
#include "isl.h"
#include "satellite.h"
#define UNUSED(expr) (void)(expr)

Satellite::Satellite(int planenum, int sat_in_plane, int sat_id,
		     double inclination, double raan, double mean_anomaly,
		     double period_in_secs, double altitude) :
    _planenum(planenum), _sat_in_plane(sat_in_plane), _sat_id(sat_id)
{
#ifdef XCP_STATIC_NETWORK
    _raan = raan;
    _mean_anomaly = mean_anomaly;
    _last_coords_update = 1; // force pos update
    update_coordinates(0);
    _altitude = 0;
    _period = 0;
    _inclination = 0;
    UNUSED(_sat_id);
    UNUSED(_inclination);
#else
    _inclination = radFromDeg(inclination);
    _raan = radFromDeg(raan);
    _mean_anomaly = radFromDeg(mean_anomaly);
    _period = timeFromSec(period_in_secs);
    _altitude = altitude + EARTH_RADIUS;
    _last_coords_update = 1; // force pos update
    update_coordinates(0);
    cout << "satinit: " << position() << endl;
#endif
    UNUSED(_planenum);
    UNUSED(_sat_in_plane);
}

void Satellite::update_coordinates(simtime_picosec time) {
#ifdef XCP_STATIC_NETWORK
    Eigen::Vector3d v(_raan,_mean_anomaly,0);
    _coordinates = v;
#else
    if (_last_coords_update == time)
	return;
    _last_coords_update = time;
    Eigen::Vector3d v(_altitude, 0.0, 0.0);
    simtime_picosec orbit_time = time % _period;
    double angle = _mean_anomaly + 2*M_PI*(double)orbit_time/(double)_period;
    Eigen::AngleAxis<double> r1(angle, Eigen::Vector3d(0.0, 0.0, 1.0));
    Eigen::AngleAxis<double> r2(_inclination, Eigen::Vector3d(1.0, 0.0, 0.0));
    Eigen::AngleAxis<double> r3(_raan, Eigen::Vector3d(0.0, 0.0, 1.0));
    _coordinates = r3*r2*r1*v;
    if (_sat_id == 0) {
	cout << "Sat 0: " << position() << endl;
    }
#endif
}

