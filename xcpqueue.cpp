// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include <math.h>
#include <iostream>
#include "xcpqueue.h"
#include "xcppacket.h"

XcpQueue::XcpQueue(linkspeed_bps bitrate, mem_b maxsize, 
		   EventList& eventlist, QueueLogger* logger,
		   double utilization_target)
    : Queue(bitrate,maxsize,eventlist,logger), 
      _target_bitrate((linkspeed_bps)(bitrate * utilization_target)),
      _bytes_forwarded(0),
      _mean_bitrate(0),
      _mean_rtt(10),
      _p_sum(10),
      _epsilon_p(0),
      _epsilon_n(0)
{
    _control_interval = timeFromMs(100);
    cout << "bitrate: " << bitrate << " target: " << _target_bitrate << endl;
}

#define XCP_ALPHA 0.4
#define XCP_GAMMA 0.1
#define P_RTT 0.05
void
XcpQueue::receivePacket(Packet& pkt) 
{
    int crt = _queuesize + pkt.size();
    simtime_picosec now = eventlist().now();
    
    if (now - _last_update > _control_interval) {
	cout << "recalculating stats, now " << timeAsMs(now) << "\n";
	cout << "td: " << timeAsMs(now - _last_update) << " ci: " << timeAsMs(_control_interval) << endl;
	// time to calculate stats
	if (_mean_rtt == 0) {
	    _mean_bitrate = 0;
	    cout << timeAsMs(now) << " bitrate2: " << _mean_bitrate << endl;
	} else {
	    _mean_bitrate = _bytes_forwarded * 8 * timeFromSec(1) / (now - _last_update);
	}
	cout << timeAsMs(now) << " bitrate: " << _mean_bitrate << " bytes " << _bytes_forwarded << endl;
	_control_interval = _mean_rtt;
	cout << "target: " << _target_bitrate;
	_rate_to_allocate = _target_bitrate - _mean_bitrate;

	cout << "rta: " << _rate_to_allocate << endl;
	_sigma_bytes = XCP_ALPHA * (_rate_to_allocate >> 3) * timeAsSec(_control_interval);
	cout << "sigma: " << _sigma_bytes << endl;

	mem_b h_bytes = XCP_GAMMA * _bytes_forwarded - _sigma_bytes;
	if (h_bytes < 0) {
	    h_bytes= 0;
	}

	cout << "h: " << h_bytes << endl;
	
	mem_b sigma_pos = _sigma_bytes;
	if (sigma_pos < 0) {
	    sigma_pos = 0;
	}

	if (_mean_rtt * _p_sum == 0) {
	    _epsilon_p = 0;
	} else {
	    _epsilon_p = (h_bytes + sigma_pos) / (timeAsSec(_mean_rtt) * _p_sum);
	}
	_p_sum = 0;

	mem_b sigma_neg = -_sigma_bytes;
	if (sigma_neg < 0) {
	    sigma_neg = 0;
	}

	if (_mean_rtt * _bytes_forwarded == 0) {
	    _epsilon_n = 0;
	    cout << "en: here\n";
	} else {
	    _epsilon_n = (h_bytes + sigma_neg) / (timeAsSec(_mean_rtt) * _bytes_forwarded);
	    cout << "en: sigma_neg: " << sigma_neg << endl;
	}

	_bytes_forwarded = 0;
	_data_packets_forwarded = 0;
	_last_update = now;
	cout << timeAsMs(now) << " ep: " << _epsilon_p << " en: " << _epsilon_n << endl;
    }

    _bytes_forwarded += pkt.size();
    _data_packets_forwarded++;


    if (crt > _maxsize) {
	/* drop the packet */
	if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
	pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
	_num_drops++;
	pkt.free();
	return;
    }

    XcpPacket* xcp_pkt = static_cast<XcpPacket*>(&pkt);
    simtime_picosec rtt = xcp_pkt->rtt();
    uint16_t psize = pkt.size();
    uint32_t cwnd = xcp_pkt->cwnd();
    uint32_t demand = xcp_pkt->demand();
    cout << timeAsMs(now) << " rtt: " << timeAsMs(rtt) << " cwnd: " << cwnd << " demand: " << demand << endl;

    // update mean RTT using EWMA
    _mean_rtt = P_RTT * rtt + (1-P_RTT) * _mean_rtt;

    _p_sum += (psize * rtt)/ cwnd;  // units:  picoseconds (per packet)
    // p_i is positive per packet feedback
    float p_i = _epsilon_p * timeAsSec(rtt) * timeAsSec(rtt) * psize / cwnd;
    // n_i is negative per packet feedback
    float n_i = _epsilon_n * timeAsSec(rtt) * psize;
    uint32_t h_feedback = p_i + n_i;
    cout << timeAsMs(now) << " p_i " << p_i << " n_i " << n_i << endl;
    cout << timeAsMs(now) << " h_fb " << h_feedback << endl;
    if (demand < h_feedback) {
	h_feedback = demand;
    }
    xcp_pkt->set_demand(h_feedback);

    /* enqueue the packet */
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();

    if (_logger) 
	_logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty) {
	/* schedule the dequeue event */
	assert(_enqueued.size()==1);
	beginService();
    }
}
