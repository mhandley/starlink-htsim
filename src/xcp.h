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

    void set_ssthresh(uint64_t s){_ssthresh = s;}

    //uint32_t effective_window();
    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    virtual const string& nodename() { return _nodename; }

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _flow_size;   // The amount of data to be sent 
    uint32_t _cwnd;
    uint32_t _maxcwnd;
    uint64_t _last_acked;
    uint32_t _ssthresh;
    uint16_t _dupacks;
    int32_t _app_limited;  // Unit: bytes/second

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

    void set_app_limit(int pktps);

    const Route* _route;
    //simtime_picosec _last_ping;
    void send_packets();
	
    int _subflow_id;

    virtual void inflate_window();
    virtual void deflate_window();

 private:

    static const uint32_t MAX_THROUGHPUT = UINT32_MAX;

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
};

class XcpSink : public PacketSink, public DataReceiver, public Logged {
    friend class XcpSrc;
 public:
    XcpSink();

    void receivePacket(Packet& pkt);
    XcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint64_t _packets;
    uint32_t _drops;
    uint64_t cumulative_ack(){ return _cumulative_ack + _received.size()*1000;}
    uint32_t drops(){ return _src->_drops;}
    uint32_t get_id(){ return id;}
    virtual const string& nodename() { return _nodename; }

    list<XcpAck::seq_t> _received; /* list of packets above a hole, that 
				      we've received */
    XcpSrc* _src;
 private:
    // Connectivity
    uint16_t _crt_path;

    void connect(XcpSrc& src, const Route& route);
    const Route* _route;

    // Mechanism
    void send_ack(simtime_picosec ts,bool marked, uint32_t cwnd, uint32_t demand,
		  simtime_picosec rtt);

    string _nodename;
};

class XcpRtxTimerScanner : public EventSource {
 public:
    XcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerXcp(XcpSrc &xcpsrc);
 private:
    simtime_picosec _scanPeriod;
    typedef list<XcpSrc*> xcps_t;
    xcps_t _xcps;
};

#endif
