// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef _XCP_QUEUE_H
#define _XCP_QUEUE_H
#include "queue.h"
/*
 * A simple FIFO queue that drops randomly when it gets full
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class XcpQueue : public Queue {
 public:
    XcpQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
	     QueueLogger* logger, double utilization_target);
    void receivePacket(Packet& pkt);
 private:
    linkspeed_bps _target_bitrate;    // target link utilization in bits/s
    simtime_picosec _last_update;     // start time of current interval
    mem_b _bytes_forwarded;           // bytes forwarded since start of current interval
    uint32_t _data_packets_forwarded; // data packets forwarded since start of interval 
    linkspeed_bps _mean_bitrate;      // mean link bitrate measured over last interval
    simtime_picosec _mean_rtt;        // mean flow RTT (EWMA)
    simtime_picosec _p_sum;           // parameter to calculate epsilon_p 
    float _epsilon_p, _epsilon_n;
    simtime_picosec _control_interval; // set to mean RTT at start of control interval
    linkspeed_bps _rate_to_allocate;  // How much bitrate we have left to
                                      // allocate in the current interval
    mem_b _sigma_bytes;
};

#endif
