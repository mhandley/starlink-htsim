// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "mpxcp.h"
#include "ecn.h"
#include "loggers.h"
#include <iostream>

////////////////////////////////////////////////////////////////
//  MPXCP SOURCE
////////////////////////////////////////////////////////////////

const int32_t MultipathXcpSrc::MAX_THROUGHPUT = INT32_MAX;
const simtime_picosec MultipathXcpSrc::MIN_ROUTE_UPDATE_INTERVAL = 100000000000;

XcpNetworkTopology* MultipathXcpSrc::_network_topology = NULL;
Logfile* MultipathXcpSrc::_logfile = NULL;
Logfile* MultipathXcpSink::_logfile = NULL;
EventList* MultipathXcpSink::_eventlist = NULL;

MultipathXcpSrc::MultipathXcpSrc(MultipathXcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, simtime_picosec rtx_time, size_t desired_spare_routes) : EventSource(eventlist,"mpxcpsrc"),  _logger(logger), _xcp_rtx_scanner(rtx_time, eventlist)
{
    _total_throughput = 0;
    _flow_size = static_cast<uint64_t>(-1);
    _app_limited = static_cast<linkspeed_bps>(-1);
    _flow_started = false;
    _route_update_interval = MIN_ROUTE_UPDATE_INTERVAL;
    _active_routes = 0;
    _spare_routes = desired_spare_routes;
}

void MultipathXcpSrc::set_flowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void MultipathXcpSrc::set_app_limit(int pktps) {
    // pktps in packets/second
    _app_limited = static_cast<linkspeed_bps>(pktps) * static_cast<linkspeed_bps>(Packet::data_packet_size()) * static_cast<linkspeed_bps>(8);
}

void MultipathXcpSrc::startflow() {
    static const size_t start_subflow_size = 5;
    if (_flow_started) {
        return;
    }

    update_subflow_list(start_subflow_size);
    adjust_subflow_throughput();

    for (auto src : _subflows) {
        src.second->startflow();
    }

    _flow_started = true;
}

void MultipathXcpSrc::set_network_topology(XcpNetworkTopology &networktopology) {
    _network_topology = &networktopology;
}

// MODIFY
void MultipathXcpSrc::receivePacket(Packet& pkt) {

}

void MultipathXcpSrc::doNextEvent() {

    if(_flow_started) {
        collect_garbage();
        update_subflow_list(_active_routes + _spare_routes);
        adjust_subflow_throughput();
        if (_logger) {
            _logger->logMultipathXcp(*this,MultipathXcpLogger::UPDATE_TIMEOUT);
        }
    } else {
        startflow();
    }

    eventlist().sourceIsPendingRel(*this,update_route_update_interval());
}

void MultipathXcpSrc::update_subflow_list(size_t number_of_paths) {
    if (!_network_topology->changed() && number_of_paths < _subflows.size()) {
        return;
    } else {
        map<XcpRouteInfo,XcpSrc*> old_subflows = _subflows;
        _subflows = _network_topology->get_paths(this,_sink, number_of_paths);

        for (auto src_it = _subflows.begin() ; src_it != _subflows.end() ; ++src_it) {
            //TODO
            auto it = old_subflows.find(src_it->first);
            if (it != old_subflows.end()) {
                _subflows[src_it->first] = it->second;
                src_it->first.set_route(it->first.route());
                old_subflows.erase(it);
            } else {
                // New route.
                XcpSrc* new_src = create_new_src();
                Route* new_route_out = const_cast<Route*>(_network_topology->get_route_from_info(src_it->first));
                Route* new_route_back = const_cast<Route*>(_network_topology->get_return_route_from_info(src_it->first));
                XcpSink* new_sink = _sink->create_new_sink();

                new_route_out->push_back(new_sink);
                new_route_back->push_back(new_src);

                new_src->connect(*new_route_out, *new_route_back, *new_sink, eventlist().now());
            }
        }

        simtime_picosec max_rtt = 0;
        for (auto src : old_subflows) {
            if (src.first.rtt() > max_rtt) {
                max_rtt = src.first.rtt();
            }
            _garbage.push_back(src.second);
        }

        _garbage_collection_time = eventlist().now() + 2 * max_rtt;
    }
}

void MultipathXcpSrc::adjust_subflow_throughput() {
    int64_t remaining_throughput = _app_limited;
    for (auto src : _subflows) {
        remaining_throughput -= static_cast<int64_t>(src.second->throughput());
    }

    _active_routes = 0;

    auto lp = _subflows.begin();
    auto rp = _subflows.end();
    --rp;
    do {
        ++_active_routes;
        while (lp != rp && remaining_throughput < static_cast<int64_t>(lp->second->route_spare_throughput())) {
            remaining_throughput += rp->second->throughput();
            rp->second->set_app_limit(0);
            --rp;
        }

        int64_t pktps = remaining_throughput;
        if (pktps > static_cast<int64_t>(lp->second->route_spare_throughput())) {
            pktps = static_cast<int64_t>(lp->second->route_spare_throughput());
        }
        remaining_throughput -= pktps;
        pktps += lp->second->throughput();
        pktps /= (Packet::data_packet_size() * 8);
        lp->second->set_app_limit(pktps);
    } while (lp++ != rp);

    while (remaining_throughput > 0 && lp != _subflows.end()) {
        int64_t pktps = remaining_throughput;
        if (pktps > static_cast<int64_t>(lp->second->route_spare_throughput())) {
            pktps = static_cast<int64_t>(lp->second->route_spare_throughput());
        }
        remaining_throughput -= pktps;
        pktps += lp->second->throughput();
        pktps /= (Packet::data_packet_size() * 8);
        lp->second->set_app_limit(pktps);
        ++lp;
        ++_active_routes;
    }
}

XcpSrc* MultipathXcpSrc::create_new_src() {
    XcpSrc* new_src = new XcpSrc(NULL, NULL, eventlist());
    new_src->setName("xcpsrc_" + to_string(this->get_id()) + "_" + to_string(new_src->get_id()));
    _logfile->writeName(*new_src);

    _xcp_rtx_scanner.registerXcp(*new_src);

    return new_src;
}

void MultipathXcpSrc::remove_src(XcpSrc* src) {
    _xcp_rtx_scanner.unregisterXcp(*src);
    delete src;
}

void MultipathXcpSrc::collect_garbage() {
    if (_garbage_collection_time > eventlist().now()) {
        for (auto src : _garbage) {
            delete src->_route;
            delete src->_sink->_route;

            _sink->remove_sink(src->_sink);
            remove_src(src);
        }

        _garbage_collection_time = 0;
    }
}

simtime_picosec MultipathXcpSrc::update_route_update_interval() {
    size_t total_cwnd = 0;
    size_t rtt_cwnd_product = 0;
    for (auto src : _subflows) {
        simtime_picosec rtt = src.second->_rtt;
        size_t cwnd = src.second->_cwnd;

        rtt_cwnd_product += cwnd * rtt;
        total_cwnd += cwnd;
    }

    if (total_cwnd) {
        _route_update_interval = rtt_cwnd_product / total_cwnd;
    } else {
        _route_update_interval = MIN_ROUTE_UPDATE_INTERVAL;
    }

    return _route_update_interval;
}

linkspeed_bps MultipathXcpSrc::update_total_throughput() {
    _total_throughput = 0;
    for (auto src : _subflows) {
        _total_throughput += src.second->throughput();
    }

    return _total_throughput;
}

////////////////////////////////////////////////////////////////
//  MPXCP SINK
////////////////////////////////////////////////////////////////

const simtime_picosec MultipathXcpSink::_sink_logger_interval = 1000000000000;

MultipathXcpSink::MultipathXcpSink() : Logged("mpxcpsink") {}

XcpSink* MultipathXcpSink::create_new_sink() {
    XcpSink* new_sink = new XcpSink();
    new_sink->setName("xcpsink_" + to_string(this->get_id()) + "_" + to_string(new_sink->get_id()));
    _logfile->writeName(*new_sink);

    XcpSinkLoggerSampling* sink_logger = new XcpSinkLoggerSampling(_sink_logger_interval, *_eventlist);
    _logfile->addLogger(*sink_logger);
    sink_logger->monitorSink(new_sink);

    _sinks[new_sink] = sink_logger;

    return new_sink;
}

void MultipathXcpSink::remove_sink(XcpSink* sink) {
    auto it = _sinks.find(sink);

    if (it != _sinks.end()) {
        _logfile->removeLogger(*(it->second));
        delete it->second;
        delete it->first;
        _sinks.erase(it);
    }
}

void MultipathXcpSink::set_src(MultipathXcpSrc* src) {
    _src = src;
}

void MultipathXcpSink::receivePacket(Packet& pkt) {

}

uint64_t MultipathXcpSink::cumulative_ack() {
    uint64_t total = 0;
    for (auto sink : _sinks) {
        total += sink.first->cumulative_ack();
    }
    return total;
}

uint32_t MultipathXcpSink::drops() {
    uint32_t total = 0;
    for (auto sink : _sinks) {
        total += sink.first->drops();
    }
    return total;
}