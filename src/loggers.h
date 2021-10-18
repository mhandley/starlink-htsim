// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef LOGGERS_H
#define LOGGERS_H

#include "logfile.h"
#include "network.h"
#include "eventlist.h"
#include "loggertypes.h"
#include "queue.h"
#include "xcp.h"
#include "tcp.h"
#include "ndp.h"
#include "mtcp.h"
#include "qcn.h"
#include "mpxcp.h"
#include <list>
#include <map>

class TrafficLoggerSimple : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class TcpTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class NdpTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class TcpLoggerSimple : public TcpLogger {
 public:
    virtual void logTcp(TcpSrc &tcp, TcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class MultipathTcpLoggerSimple: public MultipathTcpLogger {
 public:
    void logMultipathTcp(MultipathTcpSrc& mtcp, MultipathTcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class XcpLoggerSimple : public XcpLogger {
 public:
    virtual void logXcp(XcpSrc &xcp, XcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class MultipathXcpLoggerSimple: public MultipathXcpLogger {
 public:
    void logMultipathXcp(MultipathXcpSrc& mpxcp, MultipathXcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class QueueLoggerSimple : public QueueLogger {
 public:
    void logQueue(Queue& queue, QueueEvent ev, Packet& pkt);
    static string event_to_str(RawLogEvent& event);
};

class QueueLoggerSampling : public QueueLogger, public EventSource {
 public:
    QueueLoggerSampling(simtime_picosec period, EventList& eventlist);
    void logQueue(Queue& queue, QueueEvent ev, Packet& pkt);
    void doNextEvent();
    static string event_to_str(RawLogEvent& event);
 private:
    Queue* _queue;
    simtime_picosec _lastlook;
    simtime_picosec _period;
    mem_b _lastq;
    bool _seenQueueInD;
    mem_b _minQueueInD;
    mem_b _maxQueueInD;
    mem_b _lastDroppedInD;
    mem_b _lastIdledInD;
    int _numIdledInD;
    int _numDropsInD;
    double _cumidle;
    double _cumarr;
    double _cumdrop;
};

class SinkLoggerSampling : public Logger, public EventSource {
 public:
    SinkLoggerSampling(simtime_picosec period, EventList& eventlist,
		       Logger::EventType sink_type, int _event_type);
    virtual void doNextEvent();
    void monitorSink(DataReceiver* sink);
    void monitorMultipathSink(DataReceiver* sink);
    void monitorXcpSink(DataReceiver* sink);
    void monitorMultipathXcpSink(DataReceiver* sink);
 protected:
    vector<DataReceiver*> _sinks;
    vector<DataReceiver*> _sinks_xcp;
    vector<uint32_t> _multipath;
    vector<uint32_t> _multipath_xcp;

    vector<uint64_t> _last_seq;
    vector<uint64_t> _last_seq_xcp;
    vector<uint32_t> _last_sndbuf;
    vector<double> _last_rate;
    vector<double> _last_rate_xcp;

    struct lttime
    {
	bool operator()(const Logged* i1, const Logged* i2) const
	{
	    return i1->id<i2->id;
	}
    };
    typedef map<MultipathTcpSrc*,double,lttime> multipath_map;
    typedef map<MultipathXcpSrc*,double,lttime> multipath_map_xcp;
    multipath_map _multipath_src;
    multipath_map _multipath_seq;
    //multipath_map_xcp _multipath_src_xcp;
    //multipath_map_xcp _multipath_seq_xcp;
	
    simtime_picosec _last_time;
    simtime_picosec _period;
    Logger::EventType _sink_type;
    int _event_type;
};

class TcpSinkLoggerSampling : public SinkLoggerSampling {
 public:
    TcpSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class NdpSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    NdpSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class XcpSinkLoggerSampling : public SinkLoggerSampling {
 public:
    XcpSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class MemoryLoggerSampling : public Logger, public EventSource {
 public:
    MemoryLoggerSampling(simtime_picosec period, EventList& eventlist);
    void doNextEvent();
    void monitorTcpSink(TcpSink* sink);
    void monitorTcpSource(TcpSrc* sink);
    void monitorMultipathTcpSink(MultipathTcpSink* sink);
    void monitorMultipathTcpSource(MultipathTcpSrc* src);
    void monitorMultipathXcpSink(MultipathXcpSink* sink);
    void monitorMultipathXcpSource(MultipathXcpSrc* src);
    static string event_to_str(RawLogEvent& event);
 private:
    vector<TcpSink*> _tcp_sinks;
    vector<MultipathTcpSink*> _mtcp_sinks;
    vector<TcpSrc*> _tcp_sources;
    vector<MultipathTcpSrc*> _mtcp_sources;

    simtime_picosec _period;
};


class AggregateTcpLogger : public Logger, public EventSource {
 public:
    AggregateTcpLogger(simtime_picosec period, EventList& eventlist);
    void doNextEvent();
    void monitorTcp(TcpSrc& tcp);
    static string event_to_str(RawLogEvent& event);
 private:
    simtime_picosec _period;
    typedef vector<TcpSrc*> tcplist_t;
    tcplist_t _monitoredTcps;
};

class QcnLoggerSimple : public QcnLogger {
 public:
    void logQcn(QcnReactor &src, QcnEvent ev, double var3);

    void logQcnQueue(QcnQueue &src, QcnQueueEvent ev, double var1, 
		     double var2, double var3);
    static string event_to_str(RawLogEvent& event);
};



#endif
