// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef MPXCP_H
#define MPXCP_H

/*
 * An MPXCP source and sink
 */

#include <list>
#include <map>
#include "config.h"
#include "network.h"
#include "xcppacket.h"
#include "eventlist.h"
#include "sent_packets.h"
#include "xcp.h"
#include "xcpnetworktopology.h"
#include "xcprouteinfo.h"
#include "logfile.h"
#include "loggers.h"

//#define MODEL_RECEIVE_WINDOW 1

#define timeInf 0

//#define MAX_SENT 10000
class XcpNetworkTopology;
class XcpSinkLoggerSampling;
class MultipathXcpSink;

// Must set network_topology and _logfile before use
class MultipathXcpSrc : public PacketSink, public EventSource {
    friend class MultipathXcpSink;
public:
    static Logfile* _logfile;

    MultipathXcpSrc(MultipathXcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, simtime_picosec rtx_time, size_t desired_spare_route = 5);//

    uint32_t get_id() const {return id;}//
    void startflow();//
    void doNextEvent();//
    void set_start_time(simtime_picosec starttime);//
    void set_sink(MultipathXcpSink* sink);
    void set_flowsize(uint64_t flow_size_in_bytes);//
    void set_app_limit(int pktps);//
    void update_subflow_list(size_t number_of_paths);//
    void adjust_subflow_throughput();//
    XcpSrc* create_new_src();//
    void remove_src(XcpSrc* src);//
    linkspeed_bps update_total_throughput();

    bool require_one_packet(bool request_update_inst_queue = false);

    virtual void receivePacket(Packet& pkt);
    virtual const string& nodename() { return _nodename; }

    static void set_network_topology(XcpNetworkTopology &networktopology);//

    // should really be private, but loggers want to see:
    uint64_t _flow_size;   // The amount of data to be sent 
    map<XcpRouteInfo,XcpSrc*> _subflows;
    MultipathXcpSink* _sink;
    linkspeed_bps _app_limited;  // Unit: bits/second
    linkspeed_bps _total_throughput;

private:
    void collect_garbage();//
    simtime_picosec update_route_update_interval();//
    void update_queue_size();
    void update_tuning();
    void assign_instantaneous_queue();
    simtime_picosec get_max_rtt_of_subflows() const;

    static const int32_t MAX_THROUGHPUT;
    static const simtime_picosec MIN_ROUTE_UPDATE_INTERVAL;
    static XcpNetworkTopology* _network_topology;
    static const size_t MAX_QUEUE_SIZE;

    static const double TUNING_ALPHA;
    static const double TUNING_BETA;
    static const double TUNING_GAMMA;
    static const double TUNING_DELTA;

    // Housekeeping
    MultipathXcpLogger* _logger;

    string _nodename;
    bool _flow_started;
    XcpRtxTimerScanner _xcp_rtx_scanner;
    simtime_picosec _route_update_interval;
    simtime_picosec _garbage_collection_time;
    vector<XcpSrc*> _garbage;
    size_t _active_routes;
    size_t _spare_routes;
    size_t _size_in_queue;
    simtime_picosec _queue_size_last_update_time;
    linkspeed_bps _last_tuning;
    linkspeed_bps _tuning;
    size_t _declined_requests;
    double _last_queue_buildup_speed;  // bits per second
    double _queue_buildup_speed;  // bits per second
    size_t _last_min_size_in_queue;
    size_t _min_size_in_queue;
    size_t _tolerate_queue_size;
    size_t _sent_packet;

    list<pair<size_t,simtime_picosec> > _min_queue_stat;
    mem_b _virtual_queue_size;

    size_t _accumulate_size_in_queue;
};

// Must set _logfile and _eventlist before use
class MultipathXcpSink : public PacketSink, public DataReceiver, public Logged {
    friend class MultipathXcpSrc;
public:
    static Logfile* _logfile;
    static const simtime_picosec _sink_logger_interval;
    static EventList* _eventlist;

    MultipathXcpSink();

    XcpSink* create_new_sink();//
    void remove_sink(XcpSink* sink);//
    void set_src(MultipathXcpSrc* src);//

    virtual void receivePacket(Packet& pkt);//
    virtual uint64_t cumulative_ack();//
    virtual uint32_t drops();//

    virtual uint32_t get_id() {return id;}

    virtual const string& nodename() { return _nodename; }

    MultipathXcpSrc* _src;

private:
    string _nodename;
    map<XcpSink*,XcpSinkLoggerSampling*> _sinks;
};

#endif
