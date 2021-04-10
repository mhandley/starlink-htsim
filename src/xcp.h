// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef XCP_H
#define XCP_H

/*
 * An XCP source and sink
 */

#include <list>
#include "config.h"
#include "network.h"
#include "xcppacket.h"
#include "eventlist.h"
#include "sent_packets.h"

//#define MODEL_RECEIVE_WINDOW 1

#define timeInf 0

//#define MAX_SENT 10000

class XcpSink;
class MultipathXcpSrc;
class MultipathXcpSink;

class XcpSrc : public PacketSink, public EventSource {
    friend class XcpSink;
 public:
    XcpSrc(XcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    uint32_t get_id(){ return id;}
    virtual void connect(const Route& routeout, const Route& routeback, 
			 XcpSink& sink, simtime_picosec startTime);
    void startflow();

    void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    void replace_route(const Route* newroute);

    void set_flowsize(uint64_t flow_size_in_bytes) {
	_flow_size = flow_size_in_bytes+_mss;
	cout << "Setting flow size to " << _flow_size << endl;
    }

    void set_mpxcp_xrc(MultipathXcpSrc* src);

    void set_ssthresh(uint64_t s){_ssthresh = s;}

    linkspeed_bps throughput() const;
    linkspeed_bps route_max_throughput() const;

    //uint32_t effective_window();
    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    virtual const string& nodename() { return _nodename; }
    simtime_picosec rtt() const {return _rtt;}

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _flow_size;   // The amount of data to be sent 
    uint32_t _cwnd;
    uint32_t _maxcwnd;
    uint64_t _last_acked;
    uint32_t _ssthresh;
    uint16_t _dupacks;
    linkspeed_bps _app_limited;  // Unit: bits/second

    int64_t _route_max_throughput;

    //round trip time estimate, needed for coupled congestion control
    simtime_picosec _rtt, _rto, _mdev;
    //simtime_picosec _base_rtt;
    int _cap;
    simtime_picosec _rtt_avg, _rtt_cum;
    //simtime_picosec when[MAX_SENT];
    int _sawtooth;

    uint16_t _mss;
    //uint32_t _unacked; // an estimate of the amount of unacked data WE WANT TO HAVE in the network
    uint32_t _effcwnd; // an estimate of our current transmission rate, expressed as a cwnd
    uint64_t _recoverq;
    bool _in_fast_recovery;

    bool _established;

    uint32_t _drops;

    XcpSink* _sink;
    simtime_picosec _RFC2988_RTO_timeout;
    bool _rtx_timeout_pending;

    static simtime_picosec MIN_CTL_PACKET_TIMEOUT;
    static simtime_picosec MAX_CTL_PACKET_TIMEOUT;
    simtime_picosec _ctl_packet_timeout;

    void set_app_limit(int pktps);
    void set_app_limit(double bitsps);
    linkspeed_bps get_app_limit() const {return _app_limited;}

    const Route* _route;
    //simtime_picosec _last_ping;
    void send_packets();
	
    int _subflow_id;

    void set_instantaneous_queue(mem_b size);
    mem_b get_instantaneous_queuesize() const;

    //virtual void inflate_window();
    //virtual void deflate_window();

 private:
    static const size_t START_PACKET_NUMBER = 2;
    static const int32_t MAX_THROUGHPUT = INT32_MAX - 1;
    static const double THROUGHPUT_TUNING_P;

    const Route* _old_route;
    uint64_t _last_packet_with_old_route;

    // Housekeeping
    XcpLogger* _logger;
    //TrafficLogger* _pktlogger;

    // Connectivity
    PacketFlow _flow;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);

    void retransmit_packet();
    //simtime_picosec _last_sent_time;

    //void clearWhen(XcpAck::seq_t from, XcpAck::seq_t to);
    //void showWhen (int from, int to);
    string _nodename;

    MultipathXcpSrc* _mpxcp_src;
    mem_b _instantaneous_queuesize;
    mem_b _instantaneous_sent;
    mem_b _persistent_sent;

    simtime_picosec _last_rtt_start_time;

    simtime_picosec _last_tuning_start_time;
    mem_b _last_tuning_queue_size;
    bool _can_update_last_tuning_stat;
};

class XcpSink : public PacketSink, public DataReceiver, public Logged {
    friend class XcpSrc;
 public:
    XcpSink();

    void receivePacket(Packet& pkt);
    XcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint64_t _packets;
    uint32_t _drops;
    uint64_t cumulative_ack(){ return _cumulative_ack + _received.size() * Packet::data_packet_size();}
    uint32_t drops(){ return _src->_drops;}
    uint32_t get_id(){ return id;}
    virtual const string& nodename() { return _nodename; }

    list<XcpAck::seq_t> _received; /* list of packets above a hole, that 
				      we've received */
    XcpSrc* _src;
    const Route* _route;
 private:
    // Connectivity
    uint16_t _crt_path;

    void connect(XcpSrc& src, const Route& route);

    // Mechanism
    void send_ack(simtime_picosec ts,bool marked, uint32_t cwnd, int32_t demand,
		  simtime_picosec rtt);

    string _nodename;
};

class XcpRtxTimerScanner : public EventSource {
 public:
    XcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerXcp(XcpSrc &xcpsrc);
    void unregisterXcp(XcpSrc &xcpsrc);
 private:
    simtime_picosec _scanPeriod;
    typedef list<XcpSrc*> xcps_t;
    xcps_t _xcps;
};

#endif
