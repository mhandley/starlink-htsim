// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include <math.h>
#include <iostream>
#include "xcpqueue.h"
#include "xcppacket.h"
#include "xcp.h"

#define eps 1e-6

const double XcpQueue::XCP_ALPHA = 0.4;
const double XcpQueue::XCP_BETA = 0.226;
const double XcpQueue::XCP_GAMMA = 0.1;
const simtime_picosec XcpQueue::XCP_TOLERATE_QUEUE_TIME = 2000000000;  // 2000000000 picoseconds(2ms)
const double XcpQueue::P_RTT = 0.125;
const simtime_picosec XcpQueue::MIN_IDLE_INTERVAL = 5000000000;  // 5000000000 picoseconds(5ms)
const simtime_picosec XcpQueue::MAX_PACKET_RTT = 500000000000; // 500000000000 picoseconds(500ms)
#ifdef MPXCP_VERSION_1
const size_t XcpQueue::MAX_THROUGHPUT_SIZE = 10;
#endif

XcpQueue::XcpQueue(linkspeed_bps bitrate, mem_b maxsize, 
           EventList& eventlist, QueueLogger* logger,
           double utilization_target)
    : Queue(bitrate,maxsize,eventlist,logger), 
      _target_bitrate((linkspeed_bps)(bitrate * utilization_target)),
      _bytes_forwarded(0),
      _mean_bitrate(0),
      _mean_rtt(100000000000),
      _p_sum(0),
      _epsilon_p(0),
      _epsilon_n(0),
	  _residue_neg_feedback(0),
	  _residue_pos_feedback(0),
      _min_queue_size(0),
      _queue_update_time(0)
{
    _control_interval = timeFromMs(100);
    cout << "bitrate: " << bitrate << " target: " << _target_bitrate << endl;
    _rate_to_allocate = _target_bitrate;
}
/*
#define XCP_ALPHA 0.4
#define XCP_GAMMA 0.1
#define P_RTT 0.05
*/
void
XcpQueue::receivePacket(Packet& pkt) 
{
    cout << "Queue update time: " << _queue_update_time << endl;

    int crt = _queuesize + pkt.size();
    simtime_picosec now = eventlist().now();

    cout << _nodename << " Address: " << this << " receiving packet" << endl;

	if (_mean_rtt == 0 && pkt.type() == XCP) {
        XcpPacket* packet = dynamic_cast<XcpPacket*>(&pkt);
        if (packet) {
            _mean_rtt = packet->rtt();
            //_control_interval = _mean_rtt;
        }
    }

    if (now - _last_update > _control_interval) {
        update_persistent_queue_size();
        cout << _nodename << " Address: " << this << " recalculating stats, now " << timeAsMs(now) << "\n";
        cout << "td: " << timeAsMs(now - _last_update) << " ci: " << timeAsMs(_control_interval) << endl;
        // time to calculate stats
		// if (_mean_rtt == 0) {
		// 	_mean_bitrate = 0;
		// } else {
			_mean_bitrate = _bytes_forwarded * 8 * timeFromSec(1) / (now - _last_update);
		// }

        cout << this << " NOW: " << timeAsMs(now) << " bitrate: " << _mean_bitrate << " bytes: " << _bytes_forwarded << endl;
        _control_interval = _mean_rtt;
		if (_control_interval < MIN_IDLE_INTERVAL) {
			_control_interval = MIN_IDLE_INTERVAL;
		}
        cout << "target: " << _target_bitrate << endl;
        _rate_to_allocate = static_cast<int64_t>(_target_bitrate) - static_cast<int64_t>(_mean_bitrate);

        cout << "rta: " << _rate_to_allocate << endl;
        _sigma_bytes = XCP_ALPHA * (_rate_to_allocate / 8.0) * timeAsSec(_mean_rtt) - XCP_BETA * _persistent_queue_size;
        cout << XCP_ALPHA << " " << _rate_to_allocate << " " << _mean_rtt << " " << XCP_BETA << " " << _persistent_queue_size << endl;
        cout << "sigma: " << _sigma_bytes << endl;

        mem_b h_bytes = XCP_GAMMA * _bytes_forwarded - (_sigma_bytes > 0 ? _sigma_bytes : -_sigma_bytes);
        if (h_bytes < 0) {
            h_bytes= 0;
        }

        cout << "h: " << h_bytes << endl;
    
        mem_b sigma_pos = _sigma_bytes;
        if (sigma_pos < 0) {
            sigma_pos = 0;
        }

		_residue_pos_feedback = h_bytes + sigma_pos;

        if (_mean_rtt * _p_sum < eps) {
            cout << "ATTENTION A" << endl;
            _epsilon_p = 0;
        } else {
            _epsilon_p = _residue_pos_feedback / (timeAsSec(_mean_rtt) * _p_sum);
        }
        _p_sum = 0;

        mem_b sigma_neg = -_sigma_bytes;
        if (sigma_neg < 0) {
            sigma_neg = 0;
        }

		_residue_neg_feedback = h_bytes + sigma_neg;

        if (_mean_rtt * _bytes_forwarded < eps) {
            _epsilon_n = 0;
            cout << "en: here\n";
        } else {
            _epsilon_n = _residue_neg_feedback / (timeAsSec(_mean_rtt) * _bytes_forwarded);
            cout << "en: sigma_neg: " << sigma_neg << endl;
        }

        _bytes_forwarded = 0;
        _data_packets_forwarded = 0;
        _last_update = now;
        cout << timeAsMs(now) << " ep: " << _epsilon_p << " en: " << _epsilon_n << endl;
    }

	_bytes_forwarded += pkt.size();
    _data_packets_forwarded++;

    cout << pkt.type() << " TYPE " << XCP << " TYPE " << XCPCTL << endl;

    if (pkt.type() == XCP) {
        XcpPacket* xcp_pkt = static_cast<XcpPacket*>(&pkt);
        simtime_picosec rtt = xcp_pkt->rtt();

#ifdef MPXCP_VERSION_1
        cout << "BEFORE IF" << endl;
        if (xcp_pkt->rtt() > 0) {
            linkspeed_bps throughput = 0;
            throughput = xcp_pkt->cwnd();
            throughput *= 8UL;
            throughput /= timeAsSec(rtt);
            _max_throughputs[throughput] = now;
            auto it = _max_throughputs.upper_bound(throughput);
            _max_throughputs.erase(it,_max_throughputs.end());
            if (_max_throughputs.size() > MAX_THROUGHPUT_SIZE) {
                _max_throughputs.erase(_max_throughputs.begin());
            }
        }
        cout << "AFTER IF" << endl;
#endif

        uint16_t psize = pkt.size();
        uint32_t cwnd = xcp_pkt->cwnd();
        int32_t demand = xcp_pkt->demand();
        cout << timeAsMs(now) << " rtt: " << timeAsMs(rtt) << " cwnd: " << cwnd << " demand: " << demand << endl;

		if (rtt > MAX_PACKET_RTT) {
			rtt = MAX_PACKET_RTT;
		}

        // update mean RTT using EWMA
        if (rtt != 0) {
            _mean_rtt = P_RTT * rtt + (1-P_RTT) * _mean_rtt;
        }

        _p_sum += (static_cast<double>(psize) * timeAsSec(rtt)) / static_cast<double>(cwnd);
        // p_i is positive per packet feedback
        float p_i = _epsilon_p * timeAsSec(rtt) * timeAsSec(rtt) * psize / cwnd;
        // n_i is negative per packet feedback
        float n_i = _epsilon_n * timeAsSec(rtt) * psize;
        int32_t h_feedback = p_i - n_i + 0.5;
        cout << timeAsMs(now) << " p_i " << p_i << " n_i " << n_i << endl;
        cout << timeAsMs(now) << " h_fb " << h_feedback << endl;
        if (demand > h_feedback) {
            xcp_pkt->set_demand(h_feedback);
        } else {
			p_i = demand + n_i;
			n_i = (_residue_neg_feedback < n_i + h_feedback - demand ? _residue_neg_feedback : n_i + h_feedback - demand);
		}

		_residue_pos_feedback -= p_i;
		_residue_neg_feedback -= n_i;
/*
		if (_residue_pos_feedback < 0) {
			_residue_pos_feedback = 0;
		}
		if (_residue_neg_feedback < 0) {
			_residue_neg_feedback = 0;
		}
*/
		if (_residue_pos_feedback <= 0) {
            _residue_pos_feedback = 0;
			_epsilon_p = 0;
		}
		if (_residue_neg_feedback <= 0) {
            _residue_neg_feedback = 0;
			_epsilon_n = 0;
		}

        cout << "Queuesize: " << queuesize() << " My NAME: " << _nodename << " Address: " << this << " Destination: " << dynamic_cast<XcpSink*>(*(--(pkt.route()->end())))->str() << " Feedback: " << xcp_pkt->demand() << " Seqno: " << xcp_pkt->seqno() << " Next Hop: " << xcp_pkt->nexthop() << endl;
    }

    if (pkt.type() == XCPCTL) {
        linkspeed_bps max_throughput = INT64_MAX;
        XcpCtlPacket* p = dynamic_cast<XcpCtlPacket*>(&pkt);

#ifdef MPXCP_VERSION_1
        cout << "BEFORE XCPCTL ADJUST" << endl;
        cout << "Packet addr: " << p << endl;
        cout << "MAX THROUGHPUT SIZE: " << _max_throughputs.size() << endl;
        for (auto it = _max_throughputs.begin() ; it != _max_throughputs.end() ; ) {
            cout << "it first: " << it->first << " it second: " << timeAsMs(it->second) << " Mean rtt: " << timeAsMs(_mean_rtt) << " Now: " << timeAsMs(now) << endl;;
            if (it->second + _mean_rtt < now) {
                cout << "ERASED" << endl;
                _max_throughputs.erase(it++);
/*
                if (it != _max_throughputs.begin()) {
                    --it;
                }
*/
            } else {
                cout << "BROKEN" << endl;
                max_throughput = it->first;
                break;
            }
        }
        cout << max_throughput << endl;
        max_throughput += _rate_to_allocate;
        cout << "rate to allocate: " << _rate_to_allocate << endl;
        cout << "AFTER XCPCTL ADJUST" << endl;
        cout << "MAX THROUGHPUT SIZE: " << _max_throughputs.size() << endl;
        cout << max_throughput << endl;
#endif

        if (max_throughput < p->throughput()) {
            p->set_throughput(max_throughput);
        }
    }

	if (crt > _maxsize) {
        /* drop the packet */
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        _num_drops++;
        pkt.free();
        cout << "DROPPED" << endl;
        return;
    }

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

void
XcpQueue::completeService()
{
    update_persistent_queue_size();
    if (_enqueued.empty()) {
        // queue must have been explicitly cleared
        return;
    }

    /* dequeue the packet */
    Packet* pkt = _enqueued.back();
    _enqueued.pop_back();
    _queuesize -= pkt->size();

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

    /* tell the packet to move on to the next pipe */
    pkt->sendOn();

    if (_min_queue_size > queuesize()) {
        _min_queue_size = queuesize();
    }

    if (!_enqueued.empty()) {
    /* schedule the next dequeue event */
    beginService();
    }
}

void
XcpQueue::update_persistent_queue_size() {
    if (_queue_update_time <= eventlist().now()) {
        _persistent_queue_size = _min_queue_size;
        _min_queue_size = queuesize();

        simtime_picosec Tq = XCP_TOLERATE_QUEUE_TIME;
        //simtime_picosec Tq2 = (_mean_rtt - _queuesize * timeFromSec(1) * 8.0 / _bitrate) / 2.0;
		int64_t Tq2 = (static_cast<double>(_mean_rtt) - _queuesize * static_cast<double>(timeFromSec(1)) * 8.0 / static_cast<double>(_bitrate)) / 2.0;
		if (Tq2 < 0) {
			Tq2 = 0;
		}
        if (Tq < Tq2) {
            Tq = Tq2;
        }
        _queue_update_time = eventlist().now() + Tq;
    }
}
