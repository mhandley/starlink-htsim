#ifndef XCPROUTEINFO_H
#define XCPROUTEINFO_H

#include <iostream>
#include "config.h"
#include "route.h"

class XcpRouteInfo {
public:
    XcpRouteInfo(simtime_picosec rtt, Route* info);
    const simtime_picosec rtt() const;
    const Route* route() const;
    void set_route(const Route* route) const;// This function should be called with extra care
    const bool operator < (const XcpRouteInfo & rhs) const;
    const bool operator == (const XcpRouteInfo & rhs) const;

    simtime_picosec _rtt;
    mutable Route* _route;
};

std::ostream& operator<< (std::ostream& os, const XcpRouteInfo& obj);

#endif