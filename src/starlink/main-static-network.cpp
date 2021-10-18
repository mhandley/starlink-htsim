// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// The following are defined on the command line in the Makefile
//#define XCP_STATIC_NETWORK
//#define MPXCP_VERSION_1

#include "xcpnetworktopology.h"
#include "ping.h"
#include "mpxcp.h"
//#include "isl.h"
//#include "satellite.h"
//#include "city.h"

int main() {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(50));
    Logfile logfile("mpxcp1.log",eventlist);
    logfile.setStartTime(0);
    Link::_logfile = &logfile;
    Link::_queue_logger_sampling_interval = timeFromMs(10);
    MultipathXcpSink::_logfile = &logfile;
    MultipathXcpSink::_eventlist = &eventlist;
    MultipathXcpSrc::_logfile = &logfile;

    Packet::set_packet_size(1000);
/*
    XcpNetworkTopology topo(eventlist,
				speedFromMbps((uint64_t)10000), memFromPkt(100), // Uplinks
				speedFromMbps((uint64_t)10000), memFromPkt(100), // Downlinks
				speedFromMbps((uint64_t)10000), memFromPkt(100)); // ISLs
*/
    XcpNetworkTopology topo(eventlist,
				speedFromMbps((uint64_t)10), memFromPkt(1000), // Uplinks
				speedFromMbps((uint64_t)10), memFromPkt(1000), // Downlinks
				speedFromMbps((uint64_t)10), memFromPkt(1000)); // ISLs
    MultipathXcpSrc::set_network_topology(topo);

    MultipathXcpSrc* s1 = topo.build_city_src(-10,5,eventlist);
    MultipathXcpSink* e1 = topo.build_city_sink(10,5,eventlist);
    MultipathXcpSrc* s2 = topo.build_city_src(-10,-5,eventlist);
    MultipathXcpSink* e2 = topo.build_city_sink(10,-5,eventlist);

    s1->set_sink(e1);
    e1->set_src(s1);
    s2->set_sink(e2);
    e2->set_src(s2);

    //s1->set_app_limit(100);
    //s2->set_app_limit(100);

    s1->set_start_time(0);
    s2->set_start_time(timeFromSec(10));

    while (eventlist.doNextEvent()) {
    }

    //cout << "Sim done, time now: " << timeAsMs(eventlist.now()) << endl;
}

#undef XCP_STATIC_NETWORK
#undef MPXCP_VERSION_1
