#ifndef XCPNETWORKTOPOLOGY_H
#define XCPNETWORKTOPOLOGY_H

#include <map>
#include "mpxcp.h"
#include "xcprouteinfo.h"
#include "route.h"
#include "loggertypes.h"
#include "constellation.h"
#include "isl.h"
#include "city.h"

class XcpNetworkTopology {
    public:
        bool changed();
        map<XcpRouteInfo,XcpSrc*> get_paths(MultipathXcpSrc* src, MultipathXcpSink* sink, simtime_picosec time, size_t number = 10);
        const Route* get_route_from_info(const XcpRouteInfo &info) const;
        const Route* get_return_route_from_info(const XcpRouteInfo &info) const;
        MultipathXcpSrc* build_city_src(double latitude, double longitude, EventList &eventlist);
        MultipathXcpSink* build_city_sink(double latitude, double longtitude, EventList &eventlist);
    private:
        Link* get_reverse_link(Link* link) const;
        //Route* get_route_from_info_util(XcpRouteInfo info, bool order);
        //map<size_t,Node*> _entity;
        //map<Node*,size_t> _entity_r;
        map<Logged*,City*> _nodes;
        //vector<Logged*> _network_component;
        Constellation _constellation;
};

#endif