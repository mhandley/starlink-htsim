#include "xcpnetworktopology.h"

XcpNetworkTopology::XcpNetworkTopology(EventList& eventlist,
		  linkspeed_bps uplinkbitrate, mem_b uplinkqueuesize,
		  linkspeed_bps downlinkbitrate, mem_b downlinkqueuesize,
		  linkspeed_bps islbitrate, mem_b islqueuesize) : _constellation(eventlist,uplinkbitrate,uplinkqueuesize,downlinkbitrate,downlinkqueuesize,islbitrate,islqueuesize){}

bool XcpNetworkTopology::changed() {
    return true;
}

map<XcpRouteInfo,XcpSrc*> XcpNetworkTopology::get_paths(MultipathXcpSrc* src, MultipathXcpSink* sink, simtime_picosec time, size_t number) {
    map<XcpRouteInfo,XcpSrc*> ans;

    _constellation.dijkstra_up_all_links();

    for (size_t i = 0 ; i < number ; ++i) {
        //_constellation.dijkstra(*(_nodes[src]),*(_nodes[sink]));
        //Route* route = _constellation.find_route(*(_nodes[sink]));

        Route* route = (_nodes[src])->find_route(*(_nodes[sink]),time);

        if (route == NULL) {
            break;
        }

        simtime_picosec rtt = _constellation.get_rtt(route);
        XcpRouteInfo temp(rtt,route);

        ans[temp] = NULL;
        _constellation.dijkstra_down_links_in_route(route);
    }

    return ans;
}

const Route* XcpNetworkTopology::get_route_from_info(const XcpRouteInfo &info) const {
    return info.route();
}

const Route* XcpNetworkTopology::get_return_route_from_info(const XcpRouteInfo &info) const {
    Route* ans = new Route();
    const Route* route = info.route();
    auto it = route->end();
    for (--it ; ; --it) {
        Link* link = dynamic_cast<Link*>(*it);
        if (link != NULL) {
            Link* reverse_link = get_reverse_link(link);
            if (reverse_link == NULL) {
                delete ans;
                return NULL;
            } else {
                ans->push_back(static_cast<PacketSink*>(reverse_link->queue()));
                ans->push_back(static_cast<PacketSink*>(reverse_link));
            }
        }
        if (it == route->begin()) {
            break;
        }
    }

    ans->use_refcount();

    return ans;
}

MultipathXcpSrc* XcpNetworkTopology::build_city_src(double latitude, double longtitude, EventList &eventlist) {
    static const simtime_picosec rtx_time = timeFromMs(10);
    City* city = new City (latitude, longtitude, _constellation);
    MultipathXcpSrc* src = new MultipathXcpSrc(NULL,NULL,eventlist,rtx_time);
    city->set_logged(src);

    _nodes[src] = city;
    return src;
}

MultipathXcpSink* XcpNetworkTopology::build_city_sink(double latitude, double longtitude, EventList& eventlist) {
    City* city = new City (latitude, longtitude, _constellation);
    MultipathXcpSink* sink = new MultipathXcpSink();
    city->set_logged(sink);

    _nodes[sink] = city;
    return sink;
}

Link* XcpNetworkTopology::get_reverse_link(Link* link) const {
    if (link->reverse_link()) {
        return link->reverse_link();
    }
    const Node& u = link->dst();
/*
    cout << "Getting Reverse Link: Src " << &(link->src()) << " Dst " << &(link->dst()) << endl;

    for (auto it = link->src()._links.begin() ; it != link->src()._links.end() ; ++it) {
        cout << *it << endl;
    }
    cout << endl;
    for (auto it = link->dst()._links.begin() ; it != link->dst()._links.end() ; ++it) {
        cout << *it << endl;
    }
    cout << endl;
*/
    for (auto it = u._links.begin() ; it != u._links.end() ; ++it) {
        cout << *it << endl;
        if (&((*it)->dst()) == &(link->src())) {
            link->set_reverse_link(*it);
            return *it;
        }
    }
    // shouldn't get here
    abort();
}
