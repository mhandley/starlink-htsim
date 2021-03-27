// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef _XCP_QUEUE_H
#define _XCP_QUEUE_H
#include "queue.h"
/*
 * A simple FIFO queue that drops randomly when it gets full
 */

#include <map>
#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class XcpQueue : public Queue {
 public:
    XcpQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
	     QueueLogger* logger, double utilization_target = 0.9);
    virtual void receivePacket(Packet& pkt);
    virtual void completeService();

 private:
    linkspeed_bps _target_bitrate;    // target link utilization in bits/s
    simtime_picosec _last_update;     // start time of current interval
    mem_b _bytes_forwarded;           // bytes forwarded since start of current interval
    uint32_t _data_packets_forwarded; // data packets forwarded since start of interval 
    linkspeed_bps _mean_bitrate;      // mean link bitrate measured over last interval
    simtime_picosec _mean_rtt;        // mean flow RTT (EWMA)
    double _p_sum;           // parameter to calculate epsilon_p 
    float _epsilon_p, _epsilon_n;
    simtime_picosec _control_interval; // set to mean RTT at start of control interval
    int64_t _rate_to_allocate;  // How much bitrate we have left to
                                      // allocate in the current interval
    mem_b _sigma_bytes;

    simtime_picosec _queue_update_time;
    mem_b _persistent_queue_size;
    mem_b _min_queue_size;
    float _residue_pos_feedback;
    float _residue_neg_feedback;

    static const double XCP_ALPHA;
    static const double XCP_BETA;
    static const double XCP_GAMMA;
    static const simtime_picosec XCP_TOLERATE_QUEUE_TIME;
    static const double P_RTT;
    static const simtime_picosec MIN_IDLE_INTERVAL;
    static const simtime_picosec MAX_PACKET_RTT;

#ifdef MPXCP_VERSION_1
    static const size_t MAX_THROUGHPUT_SIZE;
    map<linkspeed_bps,simtime_picosec,std::greater<linkspeed_bps> > _max_throughputs;
#endif

    void update_persistent_queue_size();
};

#endif
