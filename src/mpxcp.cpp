// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "mpxcp.h"
#include "ecn.h"
#include "loggers.h"
#include <iostream>

////////////////////////////////////////////////////////////////
//  MPXCP SOURCE
////////////////////////////////////////////////////////////////

static map<XcpRouteInfo,XcpSrc*>::const_iterator search(const map<XcpRouteInfo,XcpSrc*> &old_subflows, const XcpRouteInfo &info) {
    for (auto it = old_subflows.begin() ; it != old_subflows.end() ; ++it) {
        if (it->first == info) {
            return it;
        }
    }
    return old_subflows.end();
} 

const int32_t MultipathXcpSrc::MAX_THROUGHPUT = INT32_MAX;
const simtime_picosec MultipathXcpSrc::MIN_ROUTE_UPDATE_INTERVAL = 10000000000;
const size_t MultipathXcpSrc::MAX_QUEUE_SIZE = -1;
// const double MultipathXcpSrc::TUNING_ALPHA = 0.1;
// const double MultipathXcpSrc::TUNING_BETA = 0.9;
// const double MultipathXcpSrc::TUNING_GAMMA = 0.1;
// const double MultipathXcpSrc::TUNING_DELTA = 0.1;
const double MultipathXcpSrc::TUNING_ALPHA = 1.0;
const double MultipathXcpSrc::TUNING_BETA = 0.0;
const double MultipathXcpSrc::TUNING_GAMMA = 1.0;
const double MultipathXcpSrc::TUNING_DELTA = 0.9;


XcpNetworkTopology* MultipathXcpSrc::_network_topology = NULL;
Logfile* MultipathXcpSrc::_logfile = NULL;
Logfile* MultipathXcpSink::_logfile = NULL;
EventList* MultipathXcpSink::_eventlist = NULL;

MultipathXcpSrc::MultipathXcpSrc(MultipathXcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, simtime_picosec rtx_time, size_t desired_spare_routes) : EventSource(eventlist,"mpxcpsrc"),  _logger(logger), _xcp_rtx_scanner(rtx_time, eventlist)
{
    _total_throughput = 0;
    _flow_size = static_cast<uint64_t>(-1);
    _app_limited = INT64_MAX;
    _flow_started = false;
    _route_update_interval = MIN_ROUTE_UPDATE_INTERVAL;
    _active_routes = 0;
    _spare_routes = desired_spare_routes;
    _size_in_queue = 0;
    _queue_size_last_update_time = 0;
    _last_min_size_in_queue = 0;
    _min_size_in_queue = 0;
    _last_tuning = 0;
    _tuning = 0;
    _declined_requests = 0;
    _last_queue_buildup_speed = 0;
    _queue_buildup_speed = 0;
    _tolerate_queue_size = 10000;
    _sent_packet = 0;
    _virtual_queue_size = 0;
    _accumulate_size_in_queue = 0;
    _garbage_collection_time = INT64_MAX;
}

void MultipathXcpSrc::set_flowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void MultipathXcpSrc::set_app_limit(int pktps) {
    // pktps in packets/second
    update_queue_size();
    _app_limited = static_cast<linkspeed_bps>(pktps) * static_cast<linkspeed_bps>(Packet::data_packet_size()) * static_cast<linkspeed_bps>(8);
}

void MultipathXcpSrc::startflow() {
    static const size_t start_subflow_size = 5;
    if (_flow_started) {
        return;
    }

    update_subflow_list(start_subflow_size);
    adjust_subflow_throughput();
/* Set app_limit in adjust_subflow_throughput handles this
    for (auto src : _subflows) {
        src.second->startflow();
    }
*/
    _flow_started = true;
}

void MultipathXcpSrc::set_network_topology(XcpNetworkTopology &networktopology) {
    _network_topology = &networktopology;
}

// MODIFY
void MultipathXcpSrc::receivePacket(Packet& pkt) {

}

void MultipathXcpSrc::set_start_time(simtime_picosec time) {
    _size_in_queue = 0;
    _virtual_queue_size = 0;
    _queue_size_last_update_time = time;
    eventlist().sourceIsPending(*this,time);
}

void MultipathXcpSrc::set_sink(MultipathXcpSink* sink) {
    _sink = sink;
}

void MultipathXcpSrc::doNextEvent() {
    cout << "MPXCP DONEXTEVENT" << endl;

    if (_accumulate_size_in_queue > _flow_size) {
        cout << this << " finished at time: " << timeAsMs(eventlist().now()) << endl;
        return;
    }

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
    cout << "Updating Subflow List" << nodename() << " Number of paths: " << number_of_paths << endl;
    if (!_network_topology->changed() && number_of_paths <= _subflows.size()) {
        return;
    } else {
        map<XcpRouteInfo,XcpSrc*> old_subflows = std::move(_subflows);
        simtime_picosec min_rtt = 0;
        if (!old_subflows.empty()) {
            min_rtt = old_subflows.begin()->second->rtt();
        }
        _subflows = _network_topology->get_paths(this,_sink, eventlist().now() + min_rtt, number_of_paths);

        cout << "GET PATH SUCCESSFUL" << endl;

        cout << "OLD PATHS: " << endl;
        for (auto flow : old_subflows) {
            cout << flow.first << endl;
        }

        cout << "NEW PATHS: " << endl;
        for (auto flow : _subflows) {
            cout << flow.first << endl;
        }

        for (auto src_it = _subflows.begin() ; src_it != _subflows.end() ; ++src_it) {
            //TODO
            //auto it = old_subflows.find(src_it->first);
            auto it = search(old_subflows,src_it->first);
            if (it != old_subflows.end()) {
                cout << "FOUND IN OLD SUBFLOW" << endl;
                _subflows[src_it->first] = it->second;
                src_it->first.set_route(it->first.route());
                old_subflows.erase(it);
            } else {
                // New route.
                cout << "NEW XCPSRC" << endl;
                XcpSrc* new_src = create_new_src();
                Route* new_route_out = const_cast<Route*>(_network_topology->get_route_from_info(src_it->first));
                Route* new_route_back = const_cast<Route*>(_network_topology->get_return_route_from_info(src_it->first));
                XcpSink* new_sink = _sink->create_new_sink();

                XcpRouteInfo tr(0,new_route_out);
                XcpRouteInfo tb(0,new_route_back);

                cout << tr << endl;
                cout << tb << endl;

                new_route_out->push_back(new_sink);
                new_route_back->push_back(new_src);

                new_src->connect(*new_route_out, *new_route_back, *new_sink, eventlist().now());

                src_it->second = new_src;
            }
        }

        simtime_picosec max_rtt = 0;
        for (auto src : old_subflows) {
            simtime_picosec rtt = (src.second->rtt() ? src.second->rtt() : src.first.rtt());
            if (rtt > max_rtt) {
                max_rtt = rtt;
            }
            cout << "Garbage push back: " << src.second << " Route addr: " << src.second->_route << " NOW: " << timeAsMs(eventlist().now()) << endl;
            _garbage.push_back(src.second);

            src.second->set_flowsize(1);
        }

        if (_garbage_collection_time < eventlist().now() + 2 * max_rtt) {
            _garbage_collection_time = eventlist().now() + 2 * max_rtt;
        }
        cout << "NEW GARBAGE COLLECTION TIME: " << _garbage_collection_time << " MAX RTT: " << max_rtt << endl;
    }
    cout << "FINISH UPDATING SUBFLOW LIST" << endl;
}

void MultipathXcpSrc::adjust_subflow_throughput() {
    update_queue_size();

    double ans = 0;
    for (auto src : _subflows) {
        linkspeed_bps target = src.second->get_app_limit();
        linkspeed_bps reality = src.second->throughput();

        if (target > reality) {
            double temp = target - reality;

            if (src.second->rtt() == 0) {
                temp = 0;
            } else if (temp > TUNING_GAMMA * Packet::data_packet_size() * 8.0 / timeAsSec(src.second->rtt())) {
                temp = TUNING_GAMMA * Packet::data_packet_size() * 8.0 / timeAsSec(src.second->rtt());
            }
            // if (temp > TUNING_GAMMA * target) {
            //     temp = TUNING_GAMMA * target;
            // }
            ans += temp;

            cout << "DIFF 1: " << target - reality << " TARGET: " << target << " REALITY: " << reality << " subflow addr: " << src.second << " NOW: " << timeAsMs(eventlist().now()) << endl;
            cout << "DIFF 2: " << temp << endl;
        }
    }
    cout << "DIFFERENCE: " << ans << endl;
    ans *= TUNING_ALPHA;

    _tuning = ans * TUNING_DELTA + _last_tuning * (1.0 - TUNING_DELTA);
    _last_tuning = _tuning;

    cout << this << " NEW TUNING: " << _tuning << endl;

    //update_tuning();
    cout << "ADJUSTING SUBFLOW THROUGHPUT" << endl;
    int64_t remaining_throughput = _app_limited + _tuning;

    cout << remaining_throughput << endl;

    for (auto src : _subflows) {
        double set_throughput = src.second->route_max_throughput();
        cout << this << " Max Throughput: " << set_throughput << endl;
        if (remaining_throughput < set_throughput) {
            set_throughput = remaining_throughput;
        }
        remaining_throughput -= set_throughput;
        cout << src.second << " Setting TP: " << set_throughput << endl;

        src.second->set_app_limit(set_throughput);
/*
        if (eventlist().now() < timeFromSec(10)) {
            if (src.first.rtt() == timeFromMs(42)) {
                src.second->set_app_limit(1250);
            } else {
                src.second->set_app_limit(250);
            }
        }
        else {
            if (src.first.rtt() == timeFromMs(42)) {
                src.second->set_app_limit(625);
            } else {
                src.second->set_app_limit(875);
            }
        }
*/
        cout << "REMAINING TP: " << remaining_throughput << endl;
    }
/*
    for (auto src : _subflows) {
        double set_throughput = src.second->route_max_throughput();
        cout << this << " Max Throughput: " << set_throughput << endl;
        if (remaining_throughput < set_throughput) {
            double diff = set_throughput - remaining_throughput;
            if (src.second->rtt()) {
                double queue_induced_throughput = queue_size / timeAsSec(src.second->rtt()) * 8.0;
                if (diff < queue_induced_throughput) {
                    queue_size -= diff * timeAsSec(src.second->rtt()) / 8.0;
                } else {
                    queue_size = 0;
                    diff = queue_induced_throughput;
                }
            } else {
                diff = 0;
            }
            set_throughput = remaining_throughput + diff;
        }
        remaining_throughput -= set_throughput;
        cout << src.second << " Setting TP: " << set_throughput << endl;

        src.second->set_app_limit(set_throughput);

        if (eventlist().now() < timeFromSec(10)) {
            if (src.first.rtt() == timeFromMs(42)) {
                src.second->set_app_limit(1250);
            } else {
                src.second->set_app_limit(250);
            }
        }
        else {
            if (src.first.rtt() == timeFromMs(42)) {
                src.second->set_app_limit(625);
            } else {
                src.second->set_app_limit(875);
            }
        }

        if (remaining_throughput < 0) {
            remaining_throughput = 0;
        }
        cout << "REMAINING TP: " << remaining_throughput << endl;
    }
*/
/*
    for (auto src : _subflows) {
        cout << "GET SUBFLOW TP" << endl;
        remaining_throughput -= static_cast<int64_t>(src.second->throughput());
        cout << src.second->throughput() << endl;
    }

    _active_routes = 0;

    auto lp = _subflows.begin();
    auto rp = _subflows.end();
    --rp;
    cout << "HERE" << endl;
    cout << remaining_throughput << endl;
    do {
        ++_active_routes;
        cout << lp->second->route_spare_throughput() << endl;
        while (lp != rp && remaining_throughput < static_cast<int64_t>(lp->second->route_spare_throughput())) {
            cout << "Moving right pointer" << endl;
            remaining_throughput += rp->second->throughput();
            rp->second->set_app_limit(0);
            --rp;
        }

        double bps = remaining_throughput;
        if (bps > static_cast<int64_t>(lp->second->route_spare_throughput())) {
            bps = static_cast<int64_t>(lp->second->route_spare_throughput());
        }
        remaining_throughput -= bps;
        bps += lp->second->throughput();
        //pktps /= (Packet::data_packet_size() * 8);
        lp->second->set_app_limit(10000);
        //lp->second->set_app_limit(bps);
        cout << "(1)Moving left pointer: throughput: " << bps << endl;

    } while (lp++ != rp);
    cout << "HERE2" << endl;

    while (remaining_throughput > 0 && lp != _subflows.end()) {
        double bps = remaining_throughput;
        if (bps > static_cast<int64_t>(lp->second->route_spare_throughput())) {
            bps = static_cast<int64_t>(lp->second->route_spare_throughput());
        }
        remaining_throughput -= bps;
        bps += lp->second->throughput();
        //pktps /= (Packet::data_packet_size() * 8);
        lp->second->set_app_limit(bps);
        ++lp;
        ++_active_routes;
        cout << "(2)Moving left pointer: throughput: " << bps << endl;
    }
*/
    cout << "FINISH ADJUSTING THROUGHPUT" << endl;
    update_tuning();
}

XcpSrc* MultipathXcpSrc::create_new_src() {
    XcpSrc* new_src = new XcpSrc(NULL, NULL, eventlist());
    new_src->setName("xcpsrc_" + to_string(this->get_id()) + "_" + to_string(new_src->get_id()));
    _logfile->writeName(*new_src);

    new_src->set_mpxcp_xrc(this);

    _xcp_rtx_scanner.registerXcp(*new_src);

    return new_src;
}

void MultipathXcpSrc::remove_src(XcpSrc* src) {
    _xcp_rtx_scanner.unregisterXcp(*src);
    delete src;
}

void MultipathXcpSrc::collect_garbage() {
    if (_garbage_collection_time < eventlist().now()) {
        cout << "GARBAGE COLLECTION: SIZE: " << _garbage.size() << endl;
        for (auto src : _garbage) {
            if (src->_route->is_using_refcount()) {
                src->_route->decr_refcount();
            } else {
                cout << "Deleting -> route: " << src->_route << endl;
                delete src->_route;
            }
            if (src->_sink->_route->is_using_refcount()) {
                src->_sink->_route->decr_refcount();
            } else {
                cout << "Deleting <- route: " << src->_sink->_route << endl;
                delete src->_sink->_route;
            }

            _sink->remove_sink(src->_sink);
            remove_src(src);
        }

        _garbage.clear();

        _garbage_collection_time = INT64_MAX;
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

    if (_route_update_interval < MIN_ROUTE_UPDATE_INTERVAL) {
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

bool MultipathXcpSrc::require_one_packet(bool request_update_inst_queue) {
    //return true;
    if (_accumulate_size_in_queue > _flow_size) {
        for (auto src : _subflows) {
            src.second->set_flowsize(1);
        }
        return false;
    }

    if (request_update_inst_queue) {
        assign_instantaneous_queue();
    }

    update_queue_size();
    _virtual_queue_size -= Packet::data_packet_size();

    if (_size_in_queue > Packet::data_packet_size()) {
        _size_in_queue -= Packet::data_packet_size();
        ++_sent_packet;
        if (_size_in_queue < _last_min_size_in_queue) {
            cout << "MPXCP SRC ADDR: " << this << " SHRINK QUEUE TIME: " << timeAsMs(eventlist().now()) << " QUEUE SIZE: " << _size_in_queue << endl;
        }
        cout << "MPXCP XCP ADDR: " << this << " DEPATCH A PACKET TIME: " << timeAsMs(eventlist().now()) << " QUEUE SIZE: " << _size_in_queue << " VIRTUAL QUEUESIZE: " << _min_size_in_queue << endl;
        return true;
    } else {
        ++_declined_requests;
        return false;
    }
}

void MultipathXcpSrc::update_queue_size() {
    if (_virtual_queue_size < 0) {
        _virtual_queue_size = _size_in_queue;
    }

    if (_min_size_in_queue > _size_in_queue) {
        _min_size_in_queue = _size_in_queue;
    }

    simtime_picosec now = eventlist().now();
    if (now < _queue_size_last_update_time) {
        return;
    }

    simtime_picosec diff = now - _queue_size_last_update_time;

    size_t bytes = _app_limited * timeAsSec(diff) / 8.0;

    if (_size_in_queue > MAX_QUEUE_SIZE - bytes) {
        _size_in_queue = MAX_QUEUE_SIZE;
    } else {
        _size_in_queue += bytes;
        _virtual_queue_size += bytes;
        _accumulate_size_in_queue += bytes;
    }

    _queue_size_last_update_time = now;

    cout << "MPXCP SRC ADDR: " << this << " QUEUESIZE: " << _size_in_queue << " VIRTUAL QSIZE: " << _virtual_queue_size << " TIME: " << timeAsMs(now) << endl;
}

void MultipathXcpSrc::update_tuning() {
    cout << "MPXCP SRC UPDATE TUNING ADDR: " << this << " QUEUESIZE: " << _min_size_in_queue << " ROUTE UPDATE INTERVAL: " << _route_update_interval << " SENT PACKET: "<< _sent_packet << " NOW: " << timeAsMs(eventlist().now()) << endl;;
    cout << "DECLINED: " << _declined_requests << endl;

    size_t equi_size = _min_size_in_queue / Packet::data_packet_size();
    equi_size *= Packet::data_packet_size();

    while (!_min_queue_stat.empty() && _min_queue_stat.back().first > equi_size) {
        _min_queue_stat.pop_back();
    }

    for (auto s : _min_queue_stat) {
        cout << "FIRST: " << s.first << " SECOND: " << timeAsMs(s.second) << endl;
    }
    for (auto src: _subflows) {
        cout << "CHILDREN ADDR: " << src.second << " INST QSIZE BEFORE: " << src.second->get_instantaneous_queuesize() << endl;
    }

    assign_instantaneous_queue();

    if (_min_queue_stat.empty()) {
        equi_size = (_size_in_queue < _min_size_in_queue ? _size_in_queue : _min_size_in_queue) / Packet::data_packet_size();
        equi_size *= Packet::data_packet_size();

        _min_queue_stat.push_back(make_pair(equi_size,eventlist().now()));
    }

    _last_min_size_in_queue = _min_size_in_queue;
    _min_size_in_queue = _size_in_queue;
    _declined_requests = 0;
    _sent_packet = 0;
    return;


    cout << "MPXCP SRC ADDR: " << this << " LAST TUNING: " << _last_tuning << " LAST QUEUESIZE: " << _last_min_size_in_queue << " ROUTE UPDATE INTERVAL: " << _route_update_interval << " SENT PACKET: "<< _sent_packet << " NOW: " << timeAsMs(eventlist().now()) << endl;;
    if (_route_update_interval) {
/*
        _queue_buildup_speed = (static_cast<double>(_size_in_queue) - static_cast<double>(_last_size_in_queue) - static_cast<double>(_declined_requests * Packet::data_packet_size())) * 8.0 / timeAsSec(_route_update_interval);
        cout << "Last Queue build speed: " << _last_queue_buildup_speed << " New queue build speed: " << (static_cast<double>(_size_in_queue) - static_cast<double>(_last_size_in_queue) - static_cast<double>(_declined_requests * Packet::data_packet_size())) * 8.0 / timeAsSec(_route_update_interval) << endl;
        cout << "DECLINED: " << _declined_requests << endl;
    
        double ans = TUNING_ALPHA * _queue_buildup_speed;
        cout << TUNING_ALPHA * _queue_buildup_speed << endl;
        
        ans += TUNING_BETA * _last_tuning;
        cout << TUNING_BETA * _last_tuning << endl;

        ans += TUNING_GAMMA * (_queue_buildup_speed - _last_queue_buildup_speed);
        cout << TUNING_GAMMA * (_queue_buildup_speed - _last_queue_buildup_speed) << endl;

        ans += TUNING_DELTA * _size_in_queue * 8.0 / timeAsSec(_route_update_interval);
        cout << TUNING_DELTA * _size_in_queue * 8.0 / timeAsSec(_route_update_interval) << endl;

        if (ans < 0) {
            cout << "ANS < 0" << endl;
            ans = 0;
        }
        _tuning = ans;
        _last_tuning = _tuning;
        _last_size_in_queue = _size_in_queue;
        _last_queue_buildup_speed = _queue_buildup_speed;
        _declined_requests = 0;
*/
/*
        _queue_buildup_speed = (static_cast<double>(_size_in_queue) - static_cast<double>(_last_size_in_queue) - static_cast<double>(_declined_requests * Packet::data_packet_size())) * 8.0 / timeAsSec(_route_update_interval);
        cout << "Last Queue build speed: " << _last_queue_buildup_speed << " New queue build speed: " << _queue_buildup_speed << endl;
        //_queue_buildup_speed = (1 - TUNING_DELTA) * _last_queue_buildup_speed + TUNING_DELTA * _queue_buildup_speed;
        cout << "Smoothed speed: " << _queue_buildup_speed << endl;
        cout << "DECLINED: " << _declined_requests << endl;

        double ans = TUNING_ALPHA * _queue_buildup_speed;
        cout << TUNING_ALPHA * _queue_buildup_speed << endl;

        ans += TUNING_BETA * (_queue_buildup_speed - _last_queue_buildup_speed);
        cout << TUNING_BETA * (_queue_buildup_speed - _last_queue_buildup_speed) << endl;

        //double queue_size = (1 - TUNING_DELTA) * _last_size_in_queue + TUNING_DELTA * _size_in_queue;
        double queue_size = _size_in_queue;
        ans += TUNING_GAMMA * queue_size * 8.0 / timeAsSec(_route_update_interval);
        cout << TUNING_GAMMA * queue_size * 8.0 / timeAsSec(_route_update_interval) << endl;

        if (ans < 0) {
            cout << "ANS < 0" << endl;
            ans = 0;
        }

        _tuning = ans;
        _last_tuning = _tuning;
        _last_queue_buildup_speed = _queue_buildup_speed;
        _last_size_in_queue = queue_size;
        _declined_requests = 0;
*/
        cout << "DECLINED: " << _declined_requests << endl;
        double ans = 0;
        for (auto src : _subflows) {
            linkspeed_bps target = src.second->get_app_limit();
            linkspeed_bps reality = src.second->throughput();

            if (target > reality) {
                double temp = target - reality;
                if (temp > TUNING_GAMMA * target) {
                    temp = TUNING_GAMMA * target;
                }
                ans += temp;

                cout << "DIFF 1: " << target - reality << " TARGET: " << target << " REALITY: " << reality << " subflow addr: " << src.second << " NOW: " << timeAsMs(eventlist().now()) << endl;
                cout << "DIFF 2: " << temp << endl;
            }
        }
        cout << "DIFFERENCE: " << ans << endl;
        ans *= TUNING_ALPHA;
        if (_min_size_in_queue > _tolerate_queue_size) {
            ans += TUNING_BETA * (_min_size_in_queue - _tolerate_queue_size) * 8.0 / timeAsSec(_route_update_interval);
            cout << "INSTANANEOUS: " << TUNING_BETA * (_min_size_in_queue - _tolerate_queue_size) * 8.0 / timeAsSec(_route_update_interval) << endl;
        }

        cout << "QUEUESIZE: " << _min_size_in_queue << endl;

        _tuning = ans;
        _last_tuning = _tuning;

        _last_min_size_in_queue = _min_size_in_queue;
        _min_size_in_queue = _size_in_queue;

        _declined_requests = 0;
        _sent_packet = 0;

        //_tuning = 0;
    }
    cout << "NEW TUNING: " << _tuning << " NEW QUEUESIZE: " << _min_size_in_queue << endl;
}

void MultipathXcpSrc::assign_instantaneous_queue() {
    mem_b persistent_queue = 0;
    for (auto src : _subflows) {
        persistent_queue += src.second->get_instantaneous_queuesize();
    }

    for (auto it = _min_queue_stat.begin() ; it != _min_queue_stat.end() ; ++it) {
        cout << "IT++" << endl;
        if (it->second + 2ul * get_max_rtt_of_subflows() < eventlist().now()) {
            //mem_b persistent_queue = it->first;
            persistent_queue += it->first;
            cout << "MPXCP SRC ADDR: " << this << " TOTAL INST QUEUE: " << persistent_queue << " QUEUE TIME: " << timeAsMs(it->second) << " NOW: " << timeAsMs(eventlist().now()) << endl;
/*
            for (auto src : _subflows) {
                mem_b qs = (src.second->route_max_throughput() > src.second->get_app_limit() ? src.second->route_max_throughput() - src.second->get_app_limit() : 0);
                cout << "DIFF: " << qs << " ADDR: " << src.second << " NOW: " << timeAsMs(eventlist().now()) << endl;
                qs *= timeAsSec(src.second->rtt());
                qs /= 8.0;
                qs /= Packet::data_packet_size();
                qs *= Packet::data_packet_size();
                if (qs > persistent_queue) {
                    qs = persistent_queue;
                }
                src.second->set_instantaneous_queue(qs);
                persistent_queue -= qs;

                cout << "XCP SRC ADDR: " << src.second << " INST SIZE AFTER: " << qs << endl;
                if (persistent_queue <= 0) {
                    break;
                }
            }

            _size_in_queue -= (persistent_queue > Packet::data_packet_size() ? persistent_queue - Packet::data_packet_size() : 0);
            equi_size -= (persistent_queue > Packet::data_packet_size() ? persistent_queue - Packet::data_packet_size() : 0);  // DELETE: This is to deleted when deployed
            _min_size_in_queue -= (persistent_queue > Packet::data_packet_size() ? persistent_queue - Packet::data_packet_size() : 0);  // DELETE: This is to deleted when deployed
*/
            _min_queue_stat.erase(it);
            break;

        }

/*
        if (it->second + get_max_rtt_of_subflows() > eventlist().now()) {
            if (it == _min_queue_stat.begin()) {
                break;
            }
            auto it2 = it;
            --it2;
            mem_b persistent_queue = it2->first;
            persistent_queue /= Packet::data_packet_size();
            persistent_queue *= Packet::data_packet_size();

            cout << "MPXCP SRC ADDR: " << this << " TOTAL INST QUEUE: " << persistent_queue << " QUEUE TIME: " << timeAsMs(it2->second) << " NOW: " << timeAsMs(eventlist().now()) << endl;

            for (auto src : _subflows) {
                mem_b qs = (src.second->route_max_throughput() > src.second->get_app_limit() ? src.second->route_max_throughput() - src.second->get_app_limit() : 0);
                cout << "DIFF: " << qs << " ADDR: " << src.second << " NOW: " << timeAsMs(eventlist().now()) << endl;
                qs *= timeAsSec(src.second->rtt());
                qs /= Packet::data_packet_size();
                qs *= Packet::data_packet_size();
                if (qs > persistent_queue) {
                    qs = persistent_queue;
                }
                src.second->set_instantaneous_queue(qs);
                persistent_queue -= qs;

                cout << "XCP SRC ADDR: " << src.second << " INST SIZE AFTER: " << qs << endl;
                if (persistent_queue <= 0) {
                    break;
                }
            }
            _min_queue_stat.erase(_min_queue_stat.begin(),it);
            break;
        }
*/
    }

    for (auto it = _subflows.begin() ; it != _subflows.end() ; ++it) {
        mem_b qs = (it->second->route_max_throughput() > it->second->get_app_limit() ? it->second->route_max_throughput() - it->second->get_app_limit() : 0);
        cout << "DIFF: " << qs << " ADDR: " << it->second << " NOW: " << timeAsMs(eventlist().now()) << endl;
        qs *= timeAsSec(it->second->rtt());
        qs /= 8.0;
        qs /= Packet::data_packet_size();
        qs *= Packet::data_packet_size();
        if (qs > persistent_queue) {
            qs = persistent_queue;
        }
        ++it;
        if (it == _subflows.end()) {
            qs = persistent_queue;
        }
        --it;
        it->second->set_instantaneous_queue(qs);
        persistent_queue -= qs;

        cout << "XCP SRC ADDR: " << it->second << " INST SIZE AFTER: " << qs << endl;
        if (persistent_queue <= 0) {
            persistent_queue = 0;
        }
    }
}

simtime_picosec MultipathXcpSrc::get_max_rtt_of_subflows() const {
    auto it = _subflows.end();
    if (it != _subflows.begin()) {
        for (--it ; ; --it) {
            if (it->second->get_app_limit() != 0 || it->second->get_instantaneous_queuesize() != 0) {
                return (it->second->rtt() > 0 ? it->second->rtt() : it->first.rtt());
            }
            if (it == _subflows.begin()) {
                return 0;
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////
//  MPXCP SINK
////////////////////////////////////////////////////////////////

const simtime_picosec MultipathXcpSink::_sink_logger_interval = 100000000000;

MultipathXcpSink::MultipathXcpSink() : Logged("mpxcpsink") {}

XcpSink* MultipathXcpSink::create_new_sink() {
    XcpSink* new_sink = new XcpSink();
    new_sink->setName("xcpsink_" + to_string(this->get_id()) + "_" + to_string(new_sink->get_id()));
    _logfile->writeName(*new_sink);

    XcpSinkLoggerSampling* sink_logger = new XcpSinkLoggerSampling(_sink_logger_interval, *_eventlist);
    _logfile->addLogger(*sink_logger);
    sink_logger->monitorXcpSink(new_sink);

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