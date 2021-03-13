#include "xcprouteinfo.h"

////////////////////////////////////////////////////////////////
//  XCPROUTEINFO SOURCE
////////////////////////////////////////////////////////////////

XcpRouteInfo::XcpRouteInfo(simtime_picosec rtt, Route* route) : _rtt(rtt), _route(route) {}

const simtime_picosec XcpRouteInfo::rtt() const {
    return _rtt;
}

const Route* XcpRouteInfo::route() const {
    return _route;
}

void XcpRouteInfo::set_route(const Route* route) const {
    if (_route != route) {
        delete _route;
    }
    _route = const_cast<Route*>(route);
}


const bool XcpRouteInfo::operator < (const XcpRouteInfo &rhs) const {
    if (_rtt < rhs.rtt()) {
        return true;
    }
    if (_rtt == rhs.rtt()) {
        auto it1 = _route->begin();
        auto it2 = rhs.route()->begin();
        for ( ; it1 != _route->end() && it2 != rhs.route()->end() ; ++it1,++it2) {
            if (*it1 > *it2) {
                return false;
            }
            if (*it1 < *it2) {
                return true;
            }
        }
        if (it2 == rhs.route()->end()) {
            return false;
        }
        return true;
    }
    return false;
}

const bool XcpRouteInfo::operator == (const XcpRouteInfo &rhs) const {
    if (_rtt == rhs.rtt()) {
        auto it1 = _route->begin();
        auto it2 = rhs.route()->begin();
        for ( ; it1 != _route->end() && it2 != rhs.route()->end() ; ++it1,++it2) {
            if (*it1 != *it2) {
                return false;
            }
        }
        if (it1 != _route->end() || it2 != rhs.route()->end()) {
            return false;
        }
        return true;
    }
    return false;
}