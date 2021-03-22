// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
#include "constellation.h"
#include "ping.h"
//#include "isl.h"
//#include "satellite.h"
//#include "city.h"

int main() {
    EventList eventlist;
    Constellation constellation(eventlist,
				speedFromMbps((uint64_t)10000), memFromPkt(20), // Uplinks
				speedFromMbps((uint64_t)10000), memFromPkt(20), // Downlinks
				speedFromMbps((uint64_t)10000), memFromPkt(20)); // ISLs

    //cout << "create London\n";
    City london(51.5, 0.0, constellation);
    //cout << "create NY\n";
    City newyork(40.76, 73.98, constellation);
    //cout << "London: add_uplinks\n";
    //london.add_uplinks(sats, num_sats, 0);
    //cout << "NY: add_uplinks\n";
    //newyork.add_uplinks(sats, num_sats, 0);
    //cout << "London: update_uplinks\n";
    london.update_uplinks(0);
    //cout << "NY: update_uplinks\n";
    newyork.update_uplinks(0);
    //cout << "NY: update_uplinks done\n";
    
    /*
    cout << "London: running\n";
    for (int time = 0; time < 4000; time++) {
	newyork.update_uplinks(timeFromSec(time));
    }
    */
    // CbrSrc src(eventlist, 12000, 100, 0);
    // CbrSink sink;
    //cout << "Find route, Lon-NY\n";
    Route* rt_out = london.find_route(newyork, 0);
    //cout << "Find route, NY-Lon\n";
    Route* rt_back = newyork.find_route(london, 0);
    //cout << "Find route, NY-Lon done\n";

    PingClient pingclient(eventlist, timeFromSec(1), 1500 /*bytes*/);
    PingServer pingserver;
    rt_out->push_back(static_cast<PacketSink*>(&pingserver));
    rt_back->push_back(static_cast<PacketSink*>(&pingclient));
    /*
    cout << "Out:" << endl;
    print_route(*rt_out);
    cout << "\nBack:" << endl;
    print_route(*rt_back);
    */
    pingclient.connect(*rt_out, *rt_back, pingserver, eventlist.now(), 3600);

    /*
    for (int i = 0; i < rt->size(); i++) {
	cout << "hop: " << i << endl;
        PacketSink* sink = rt->at(i);
        cout << sink << endl;
        cout << sink->nodename() << endl;
    }
    */
    //print_route(*rt);

    simtime_picosec last_route_update = eventlist.now();
    while (eventlist.doNextEvent()) {
	//cout << "Time now: " << timeAsMs(eventlist.now()) << endl;
	simtime_picosec now = eventlist.now();
	if (timeAsMs(now - last_route_update) > 100) {
	    london.update_uplinks(eventlist.now());
        newyork.update_uplinks(eventlist.now());

        rt_out->decr_refcount();
        rt_back->decr_refcount();

	    rt_out = london.find_route(newyork, eventlist.now());
	    rt_back = newyork.find_route(london, eventlist.now());
	    rt_out->push_back(static_cast<PacketSink*>(&pingserver));
	    rt_back->push_back(static_cast<PacketSink*>(&pingclient));
	    pingclient.update_route(*rt_out, *rt_back);
	    last_route_update =	now;
	}
    }
    //cout << "Sim done, time now: " << timeAsMs(eventlist.now()) << endl;
}
