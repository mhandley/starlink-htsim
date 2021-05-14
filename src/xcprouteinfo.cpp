#include "xcprouteinfo.h"

////////////////////////////////////////////////////////////////
//  XCPROUTEINFO SOURCE
////////////////////////////////////////////////////////////////

std::ostream& operator<< (std::ostream& os, const XcpRouteInfo& obj) {
    os << obj._rtt << " " << &obj << " ";
    for (auto it = obj._route->begin() ; it != obj._route->end() ; ++it) {
        os << *it << " ";
    }
    return os;
}

XcpRouteInfo::XcpRouteInfo(simtime_picosec rtt, Route* route) : _rtt(rtt), _route(route) {}

const simtime_picosec XcpRouteInfo::rtt() const {
    return _rtt;
}

const Route* XcpRouteInfo::route() const {
    return _route;
}

void XcpRouteInfo::set_route(const Route* route) const {
    if (_route != route) {
        if (_route) {
            cout << "DECR in XCPROUTEINFO SET ROUTE" << endl;
            _route->decr_refcount();
        }
        _route = const_cast<Route*>(route);
        _route->incr_refcount();
    }
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
        /*
        if (it2 == rhs.route()->end()) {
            return false;
        }
        return true;
        */
    }
    return false;
}

const bool XcpRouteInfo::operator == (const XcpRouteInfo &rhs) const {
    if (_route->size() < 2 + rhs.route()->size() && _route->size() + 2 > rhs.route()->size()) {
        //_rtt == rhs.rtt()
        auto it1 = _route->begin();
        auto it2 = rhs.route()->begin();
        for ( ; it1 != _route->end() && it2 != rhs.route()->end() ; ++it1,++it2) {
            if (*it1 != *it2) {
                return false;
            }
        }
        // if (it1 != _route->end() || it2 != rhs.route()->end()) {
        //     return false;
        // }
        return true;
    }
    return false;
}