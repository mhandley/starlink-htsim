// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#ifndef SAT_H
#define SAT_H

#include "Eigen/Dense"
#include "pipe.h"
#include "node.h"

class Satellite : public Node {
 public:
    Satellite(int planenum, int sat_in_plane, int sat_id, double inclination, double raan, double mean_anomaly, double period_in_secs, double altitude);
    void update_coordinates(simtime_picosec time);
 private:
    int _planenum;
    int _sat_in_plane;
    int _sat_id;
    double _inclination;
    double _raan;
    double _mean_anomaly;
    simtime_picosec _period;
    double _altitude; // altitude in km from centre of earth
};

#endif
