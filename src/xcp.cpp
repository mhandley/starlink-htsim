// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "xcp.h"
#include "ecn.h"
#include "mpxcp.h"
#include <iostream>

#define KILL_THRESHOLD 5
////////////////////////////////////////////////////////////////
//  XCP SOURCE
////////////////////////////////////////////////////////////////

simtime_picosec XcpSrc::MIN_CTL_PACKET_TIMEOUT = timeFromMs(10);
simtime_picosec XcpSrc::MAX_CTL_PACKET_TIMEOUT = timeFromMs(200);
const double XcpSrc::THROUGHPUT_TUNING_P = 0.5;

XcpSrc::XcpSrc(XcpLogger* logger, TrafficLogger* pktlogger, 
	       EventList &eventlist)
    : EventSource(eventlist,"xcp"),  _logger(logger), _flow(pktlogger)
{
    _mss = Packet::data_packet_size();
    _maxcwnd = 0xffffffff;//MAX_SENT*_mss;
    _sawtooth = 0;
    _subflow_id = -1;
    _rtt_avg = timeFromMs(0);
    _rtt_cum = timeFromMs(0);
	//_base_rtt = timeInf;
    _cap = 0;
    _flow_size = ((uint64_t)1)<<63;
    _highest_sent = 0;
    _packets_sent = 0;
    _app_limited = -1;
    _established = false;
    //_effcwnd = 0;
	_cwnd = 0;

    //_ssthresh = 30000;
    _ssthresh = 0xffffffff;


    _last_acked = 0;
	//last_ping = timeInf;
    _dupacks = 0;
    _rtt = 0;
    _rto = timeFromMs(1000);
    _mdev = 0;
    _recoverq = 0;
    _in_fast_recovery = false;
    _drops = 0;

#ifdef PACKET_SCATTER
    _crt_path = 0;
    DUPACK_TH = 3;
    _paths = NULL;
#endif


    _old_route = NULL;
    _last_packet_with_old_route = 0;

    _rtx_timeout_pending = false;
    _RFC2988_RTO_timeout = timeInf;

    _nodename = "xcpsrc";

	_route_max_throughput = INT64_MAX;

	_ctl_packet_timeout = eventlist.now();
	eventlist.sourceIsPendingRel(*this,0);

	_mpxcp_src = NULL;
	_instantaneous_queuesize = 0;
	_instantaneous_sent = 0;
	_persistent_sent = 0;
	_last_rtt_start_time = 0;
	_last_tuning_queue_size = 0;
	_last_tuning_start_time = 0;
	_can_update_last_tuning_stat = true;
}

#ifdef PACKET_SCATTER
void XcpSrc::set_paths(vector<const Route*>* rt) {
    //this should only be used with route
    _paths = new vector<const Route*>();

    for (unsigned int i=0;i<rt->size();i++){
	Route* t = new Route(*(rt->at(i)));
	t->push_back(_sink);
	_paths->push_back(t);
    }
    DUPACK_TH = 3 + rt->size();
    cout << "Setting DUPACK TH to " << DUPACK_TH << endl;
}
#endif

void XcpSrc::set_app_limit(int pktps) {
	// pktps in packets/second
    if (_app_limited==0 && pktps){
		_cwnd = START_PACKET_NUMBER * _mss;
    }
    _ssthresh = 0xffffffff;
	linkspeed_bps old_limit = _app_limited;
    _app_limited = static_cast<linkspeed_bps>(pktps) * static_cast<linkspeed_bps>(Packet::data_packet_size()) * static_cast<linkspeed_bps>(8);
	if (_established && old_limit < _app_limited) {
    	send_packets();
	}
}

void XcpSrc::set_app_limit(double bitsps) {
	static const double zero = 0.99;
	if (bitsps <= 0) {
		bitsps = 0;
		_cwnd = 0;
	}
	if (_app_limited == 0 && bitsps > zero) {
		_cwnd = START_PACKET_NUMBER * _mss;
	}
	_ssthresh = 0xffffffff;
	linkspeed_bps old_limit = _app_limited;
    _app_limited = bitsps;
	if (_established && old_limit < _app_limited) {
    	send_packets();
	}
}

void 
XcpSrc::startflow() {
    _cwnd = START_PACKET_NUMBER * _mss;
    //_unacked = _cwnd;
    _established = false;

    send_packets();
}

// uint32_t XcpSrc::effective_window() {
//     return _in_fast_recovery?_ssthresh:_cwnd;
// }

void XcpSrc::replace_route(const Route* newroute) {
    _old_route = _route;
    _route = newroute;
    _last_packet_with_old_route = _highest_sent;
	//_last_ping = timeInf;

    //  Printf("Wiating for ack %d to delete\n",_last_packet_with_old_route);
}

void XcpSrc::set_mpxcp_xrc(MultipathXcpSrc* src) {
	_mpxcp_src = src;
}

linkspeed_bps XcpSrc::throughput() const {
	if (_rtt == 0) {
		return 0;
	}

	uint32_t c = _cwnd;
	if (_app_limited * timeAsSec(_rtt) / 8 < _cwnd) {
		c = _app_limited * timeAsSec(_rtt) / 8;
	}

	linkspeed_bps ans = c / Packet::data_packet_size() * Packet::data_packet_size() * 8;
	ans /= timeAsSec(_rtt);

	cout << "ADDR: " << this << " CWND: " << _cwnd << " REAL C: " << c << " NOW: " << timeAsMs(eventlist().now()) << endl;
	cout << "COMPUTING THROUGHPUT: " << ans << endl;

	return ans;
}

linkspeed_bps XcpSrc::route_max_throughput() const {
	return _route_max_throughput;
}

void 
XcpSrc::connect(const Route& routeout, const Route& routeback, XcpSink& sink, 
		simtime_picosec starttime) {
    _route = &routeout;

    assert(_route);
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the XCP source that generated it
    _sink->connect(*this, routeback);

    eventlist().sourceIsPending(*this,starttime);
}

#define ABS(X) ((X)>0?(X):-(X))

void
XcpSrc::receivePacket(Packet& pkt) 
{
	cout << "SRC: " << get_id() << " time: " << timeAsMs(eventlist().now()) << " CWND: " << _cwnd << endl;

	cout << "SRC receiving packet: " << pkt.type() << endl;
	if (pkt.type() == XCPCTLACK) {
		XcpCtlAck* p = dynamic_cast<XcpCtlAck*>(&pkt);
		_route_max_throughput = p->allowed_throughput();

		// Maybe we can update rtt also using control packet
		//compute rtt
    	uint64_t m = eventlist().now() - p->ts_echo();

    	if (m!=0){
        	if (_rtt>0){
            	uint64_t abs;
            	if (m>_rtt)
                    abs = m - _rtt;
            	else
                	abs = _rtt - m;

            	_mdev = 3 * _mdev / 4 + abs/4;
            	_rtt = 7*_rtt/8 + m/8;
            	_rto = _rtt + 4*_mdev;
        	} else {
            	_rtt = m;
            	_mdev = m/2;
            	_rto = _rtt + 4*_mdev;
        	}
    	}

		return;
	}

	XcpAck *p = dynamic_cast<XcpAck*>(&pkt);
	if (p == NULL) {
		return;
	}

    simtime_picosec ts_echo;
    XcpAck::seq_t seqno = p->ackno();

    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
  
    ts_echo = p->ts_echo();
    p->free();

	// Old ACK but not dup ACK
    if (seqno < _last_acked) {
        //cout << "O seqno" << seqno << " last acked "<< _last_acked;
        return;
    }

    if (seqno==1){
        //assert(!_established);
        _established = true;
    }
    else if (seqno>1 && !_established) {
        cout << "Should be _established " << seqno << endl;
    }

    //assert(seqno >= _last_acked);  // no dups or reordering allowed in this simple simulator

    //compute rtt
    uint64_t m = eventlist().now()-ts_echo;

    if (m!=0){
        if (_rtt>0){
            uint64_t abs;
            if (m>_rtt)
                abs = m - _rtt;
            else
                abs = _rtt - m;

            _mdev = 3 * _mdev / 4 + abs/4;
            _rtt = 7*_rtt/8 + m/8;
            _rto = _rtt + 4*_mdev;
        } else {
            _rtt = m;
            _mdev = m/2;
            _rto = _rtt + 4*_mdev;
        }
		// if (_base_rtt == timeInf || _base_rtt > m)
		//     _base_rtt = m;
    }
    // cout << "Base "<<timeAsMs(_base_rtt)<< " RTT " << timeAsMs(_rtt)<< " Queued " << queued_packets << endl;

    if (_rto<timeFromMs(1000))
        _rto = timeFromMs(1000);

    if (seqno >= _flow_size){
        cout << "Flow " << nodename() << " finished at " << timeAsMs(eventlist().now()) << endl;	
    }

	// Adjust the window (MAY NEED MODIFICATION)
	int64_t signed_cwnd = _cwnd;
	signed_cwnd += static_cast<int64_t>(p->allowed_demand());// * static_cast<int64_t>(_rtt) / 1000000000;

	cout << "SOURCE RECEIVE ALLOWED DEMAND: " << p->allowed_demand() << endl;
	if (signed_cwnd > static_cast<int64_t>(_maxcwnd)) {
		signed_cwnd = _maxcwnd;
	}

	if (signed_cwnd < static_cast<int64_t>(_mss)) {
		signed_cwnd = _mss;
	}

	_cwnd = signed_cwnd;

    if (seqno > _last_acked) { // a brand new ack
        if (_old_route){
            if (seqno >= _last_packet_with_old_route) {
            //delete _old_route;
                _old_route = NULL;
            //printf("Deleted old route\n");
            }
        }
	    _RFC2988_RTO_timeout = eventlist().now() + _rto;// RFC 2988 5.3
		//_last_ping = eventlist().now();
    
        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
            _RFC2988_RTO_timeout = timeInf;// RFC 2988 5.2
			//last_ping = timeInf;
        }

#ifdef MODEL_RECEIVE_WINDOW
	int cnt;

	_sent_packets.ack_packet(seqno);

	//if ((cnt = _sent_packets.ack_packet(seqno)) > 2)
	//  cout << "ACK "<<cnt<<" packets on " << _flow.id << " " << _highest_sent+1 << " packets in flight " << _sent_packets.crt_count << " diff " << (_highest_sent+_mss-_last_acked)/1000 << " last_acked " << _last_acked << " at " << timeAsMs(eventlist().now()) << endl;
#endif

	    if (!_in_fast_recovery) { // best behaviour: proper ack of a new packet, when we were expecting it
	        //clear timers
      
	        _last_acked = seqno;
	        _dupacks = 0;

	        //_unacked = _cwnd;
	        //_effcwnd = _cwnd;
	        if (_logger) 
		        _logger->logXcp(*this, XcpLogger::XCP_RCV);
	        send_packets();
	        return;
	    }
		// We're in fast recovery, i.e. one packet has been
		// dropped but we're pretending it's not serious
		if (seqno >= _recoverq) {
			// got ACKs for all the "recovery window": resume
			// normal service
			//uint32_t flightsize = _highest_sent - seqno;
			//_cwnd = min(_ssthresh, flightsize + _mss);
			//_unacked = _cwnd;
			//_effcwnd = _cwnd;
			_last_acked = seqno;
			_dupacks = 0;
			_in_fast_recovery = false;

			if (_logger) 
				_logger->logXcp(*this, XcpLogger::XCP_RCV_FR_END);
			send_packets();
			return;
		}
		// In fast recovery, and still getting ACKs for the
		// "recovery window"
		// This is dangerous. It means that several packets
		// got lost, not just the one that triggered FR.
		//uint32_t new_data = seqno - _last_acked;
		_last_acked = seqno;
		//if (new_data < _cwnd) 
		//    _cwnd -= new_data; 
		//else 
		//    _cwnd = 0;
		//_cwnd += _mss;
		if (_logger) 
			_logger->logXcp(*this, XcpLogger::XCP_RCV_FR);

		retransmit_packet();
		send_packets();
		return;
    }
    // It's a dup ack
    if (_in_fast_recovery) { // still in fast recovery; hopefully the prodigal ACK is on it's way 
		//_cwnd += _mss;
		//if (_cwnd>_maxcwnd) {
	    	//_cwnd = _maxcwnd;
		//}
		// When we restart, the window will be set to
		// min(_ssthresh, flightsize+_mss), so keep track of
		// this
		//_unacked = min(_ssthresh, (uint32_t)(_highest_sent-_recoverq+_mss)); 
		//if (_last_acked+_cwnd >= _highest_sent+_mss) 
		//    _effcwnd=_unacked; // starting to send packets again
		if (_logger) 
			_logger->logXcp(*this, XcpLogger::XCP_RCV_DUP_FR);
		send_packets();
		return;
    }
    // Not yet in fast recovery. What should we do instead?
    _dupacks++;

#ifdef PACKET_SCATTER
    if (_dupacks!=DUPACK_TH) 
#else
	if (_dupacks < 3) 
#endif
	    { // not yet serious worry
			if (_logger) 
		    	_logger->logXcp(*this, XcpLogger::XCP_RCV_DUP);
			send_packets();
			return;
	    }
    // _dupacks==3
    if (_last_acked < _recoverq) {  
        /* See RFC 3782: if we haven't recovered from timeouts
	   etc. don't do fast recovery */
		if (_logger) 
	    	_logger->logXcp(*this, XcpLogger::XCP_RCV_3DUPNOFR);
		send_packets();
		return;
    }
  
    // begin fast recovery
  
    //only count drops in CA state
    _drops++;
  
    //deflate_window();
  
    //if (_sawtooth>0)
	//_rtt_avg = _rtt_cum/_sawtooth;
    //else
	//_rtt_avg = timeFromMs(0);
  
    //_sawtooth = 0;
    //_rtt_cum = timeFromMs(0);
  
    //_cwnd = _ssthresh + 3 * _mss;
    //_unacked = _ssthresh;
    //_effcwnd = 0;
    _in_fast_recovery = true;
    _recoverq = _highest_sent; // _recoverq is the value of the
    // first ACK that tells us things
    // are back on track
    if (_logger) 
		_logger->logXcp(*this, XcpLogger::XCP_RCV_DUP_FASTXMIT);
	retransmit_packet();
	send_packets();
}
/*
// MAY NEED MODIFICATION: RFC 5681 Page 8 TOP
void XcpSrc::deflate_window(){
    //_ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));
}

// NEED MODIFICATION
void
XcpSrc::inflate_window() {
This is TCP implementation
    int newly_acked = (_last_acked + _cwnd) - _highest_sent;
    // be very conservative - possibly not the best we can do, but
    // the alternative has bad side effects.
    if (newly_acked > _mss) newly_acked = _mss; 
    if (newly_acked < 0)
	return;
    if (_cwnd < _ssthresh) { //slow start
	int increase = min(_ssthresh - _cwnd, (uint32_t)newly_acked);
	_cwnd += increase;
	newly_acked -= increase;
    } else {
	// additive increase
	uint32_t pkts = _cwnd/_mss;

	int tcp_inc = (newly_acked * _mss)/_cwnd;
	_cwnd += tcp_inc;

	if (pkts!=_cwnd/_mss) {
	    _rtt_cum += _rtt;
	    _sawtooth ++;
	}
    }

}
*/
// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
void 
XcpSrc::send_packets() {
	cout << "SRC sending packet" << endl;

	if (_instantaneous_queuesize > _last_tuning_queue_size || ((_last_tuning_start_time + _rtt < eventlist().now()) && _can_update_last_tuning_stat)) {
		cout << this << " UPDATING last tuning queue size: " << _last_tuning_queue_size << " now tuning: " << _instantaneous_queuesize << " NOW: " << timeAsMs(eventlist().now()) << endl;
		_last_tuning_queue_size = _instantaneous_queuesize;
		//_last_tuning_start_time = eventlist().now();
		_instantaneous_sent = 0;
		_persistent_sent = 0;
		//_last_tuning_start_time = 0;
	}

	double demand = MAX_THROUGHPUT;

	if (_rtt != 0) {
		demand = _app_limited * timeAsSec(_rtt) / 8 + _last_tuning_queue_size - _cwnd;
		//demand = _app_limited * timeAsSec(_rtt) / 8 + _instantaneous_queuesize - _cwnd;
	}

	cout << "DEMAND 2: " << demand << " RTT: " << timeAsSec(_rtt) << endl;;

	if (demand <= 0) {
		cout << "BEFORE CHANGING CWND: " << _cwnd << endl;
		demand = 0;
		//if (_last_rtt_start_time + _rtt < eventlist().now()) {
			//_cwnd = _cwnd * (1.0 - THROUGHPUT_TUNING_P) + (_app_limited * timeAsSec(_rtt) / 8 + _last_tuning_queue_size) * THROUGHPUT_TUNING_P;
			//_cwnd = _cwnd * (1.0 - THROUGHPUT_TUNING_P) + (_app_limited * timeAsSec(_rtt) / 8 + _instantaneous_queuesize) * THROUGHPUT_TUNING_P;
			_cwnd = (_app_limited * timeAsSec(_rtt) / 8 + _last_tuning_queue_size);
			_last_rtt_start_time = eventlist().now();
			cout << "AFTER CHANGING CWND: " << _cwnd << endl;
		//}
	}

	int c = _cwnd;

	if (_app_limited * timeAsSec(_rtt) / 8 + _last_tuning_queue_size < _cwnd) {
		c = _app_limited * timeAsSec(_rtt) / 8 + _last_tuning_queue_size;
	}

	demand /= (c / Packet::data_packet_size());

	if (demand >= MAX_THROUGHPUT) {
		demand = MAX_THROUGHPUT;
	}

    if (!_established){
		//send SYN packet and wait for SYN/ACK
		Packet * p  = XcpPacket::new_syn_pkt(_flow, *_route, 1, 1, demand);
		_highest_sent = 1;

		cout << "SENDON1" << endl;
		p->sendOn();

		if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
			_RFC2988_RTO_timeout = eventlist().now() + _rto;
		}	
		//cout << "Sending SYN, waiting for SYN/ACK" << endl;
		return;
    }
/*
    if (_app_limited >= 0 && _rtt > 0) {
		uint64_t d = (uint64_t)_app_limited * _rtt / timeFromSec(1) / 8;
		if (c > d) {
			c = d;
		}
		//if (c<1000)
		//c = 1000;

		if (c==0){
			//      _RFC2988_RTO_timeout = timeInf;
		}

		//rtt in ms
		//printf("%d\n",c);
    }
*/
	cout << this->nodename() << " " << this << " EFF WINDOW: " << c << " ACTUAL WINDOW: " << _cwnd << " DEMAND: " << demand << endl;

    while ((_last_acked + c >= _highest_sent + _mss) 
	   && (_highest_sent + _mss <= _flow_size + 1)) {

		    bool sent_inst = false;
		//if (_instantaneous_queuesize > 0) {
			double target_ratio = _app_limited * timeAsSec(_rtt) / 8.0 / c;
			target_ratio = target_ratio / (1.0 - target_ratio);

			cout << this << " TARGET RATIO: " << target_ratio << endl;

			if (target_ratio > 0) {
				double real_ratio = static_cast<double>(_persistent_sent) / static_cast<double>(_instantaneous_sent);

				cout << this << " REAL RATIO: " << real_ratio << " PS: " << _persistent_sent << " IS: " << _instantaneous_sent << endl;

				if (_instantaneous_sent == 0 || real_ratio > target_ratio) {
					_instantaneous_sent += _mss;
					_instantaneous_queuesize -= _mss;

					_can_update_last_tuning_stat = false;
					sent_inst = true;
					cout << this << " REQUESTING INSTANTANEOUS PACKET REMAINING: " << _instantaneous_queuesize << " NOW: " << timeAsMs(eventlist().now()) << endl;
				} else {
					_persistent_sent += _mss;
					
					real_ratio = static_cast<double>(_persistent_sent) / static_cast<double>(_instantaneous_sent);
					if (real_ratio > target_ratio) {
						_can_update_last_tuning_stat = true;
					}
				}
			} else {
				_can_update_last_tuning_stat = true;
			}
		//}

		_can_update_last_tuning_stat = true;

		if (_instantaneous_queuesize <= 0) {
			cout << "INSTANTANEOUS QSIZE IS ZERO" << endl;
			_instantaneous_queuesize = 0;
			if (_can_update_last_tuning_stat) {
				//_instantaneous_sent = 0;
				//_persistent_sent = 0;
				//_last_tuning_queue_size = 0;
			}
		}

		if (_mpxcp_src && !_mpxcp_src->require_one_packet(_instantaneous_queuesize <= 0 && _last_tuning_queue_size > 0)) {
			if (sent_inst || _can_update_last_tuning_stat) {
				_instantaneous_queuesize = 0;
				_instantaneous_sent = 0;
				_persistent_sent = 0;
				_last_tuning_queue_size = 0;
			}
			cout << "GOT DECLINED ADDR: " << this << endl;
			break;
		}

		XcpPacket* p = XcpPacket::newpkt(_flow, *_route, _highest_sent+1, _mss,
						_cwnd, demand, _rtt);
		p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
		p->set_ts(eventlist().now());
		
		_highest_sent += _mss;  //XX beware wrapping
		_packets_sent += _mss;
		
		cout << "SENDON2" << " ADDR: " << this << " INST QSIZE: " << _instantaneous_queuesize << endl;
		p->sendOn();


		if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
			_RFC2988_RTO_timeout = eventlist().now() + _rto;
		}

		if (c > _app_limited * timeAsSec(_rtt) / 8.0 + _last_tuning_queue_size) {
			c = _app_limited * timeAsSec(_rtt) / 8.0 + _last_tuning_queue_size;
		}
    }
}

void 
XcpSrc::retransmit_packet() {
	double demand = MAX_THROUGHPUT;
	if (_rtt != 0) {
		demand = _app_limited * timeAsSec(_rtt) / 8 - _cwnd;
	}

	if (demand <= 0) {
		demand = 0;
		_cwnd = _app_limited * timeAsSec(_rtt) / 8;
	}

	if (demand >= MAX_THROUGHPUT) {
		demand = MAX_THROUGHPUT;
	}

    if (!_established){
		assert(_highest_sent == 1);

		//uint32_t demand = _mss * 2; // request 2 pkt initial window
		Packet* p  = XcpPacket::new_syn_pkt(_flow, *_route, 1, 1, demand);

		cout << "SENDON3" << endl;
		p->sendOn();

		cout << "Resending SYN, waiting for SYN/ACK" << endl;
		return;	
    }

    //uint32_t demand = _cwnd*10;
    // XcpPacket* p = XcpPacket::newpkt(_flow, *_route, _last_acked+1, _mss,
	// 			     _cwnd, demand, _rtt_avg);
	XcpPacket* p = XcpPacket::newpkt(_flow, *_route, _last_acked+1, _mss, _cwnd, demand, _rtt);

    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());

	cout << "SENDON4" << endl;
    p->sendOn();

    _packets_sent += _mss;

    if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
		_RFC2988_RTO_timeout = eventlist().now() + _rto;
    }
}

void XcpSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    if (now <= _RFC2988_RTO_timeout || _RFC2988_RTO_timeout==timeInf) 
	return;

    if (_highest_sent == 0) 
	return;

    cout <<"At " << now/(double)1000000000<< " RTO " << _rto/1000000000 << " MDEV " 
	 << _mdev/1000000000 << " RTT "<< _rtt/1000000000 << " SEQ " << _last_acked / _mss << " HSENT "  << _highest_sent 
	 << " CWND "<< _cwnd/_mss << " FAST RECOVERY? " << 	_in_fast_recovery << " Flow ID " 
	 << str()  << endl;

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if(!_rtx_timeout_pending) {
		_rtx_timeout_pending = true;

		// check the timer difference between the event and the real value
		simtime_picosec too_late = now - (_RFC2988_RTO_timeout);
 
		// careful: we might calculate a negative value if _rto suddenly drops very much
		// to prevent overflow but keep randomness we just divide until we are within the limit
		while(too_late > period) too_late >>= 1;

		// carry over the difference for restarting
		simtime_picosec rtx_off = (period - too_late)/200;
	
		eventlist().sourceIsPendingRel(*this, rtx_off);

		//reset our rtx timerRFC 2988 5.5 & 5.6

		_rto *= 2;
		//if (_rto > timeFromMs(1000))
		//  _rto = timeFromMs(1000);
		_RFC2988_RTO_timeout = now + _rto;
    }
}

void XcpSrc::doNextEvent() {
	if (_ctl_packet_timeout <= eventlist().now()) {
		cout << "ROUTE ADDRESS: " << _route << endl;
		XcpCtlPacket* p = XcpCtlPacket::newpkt(_flow, *_route);
		p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());
		cout << "SENDON5" << endl;
		p->sendOn();
		cout << "SENDON5 SUCCEED" << endl;

		simtime_picosec timeout = _rtt;
		if (timeout == 0 || timeout > MAX_CTL_PACKET_TIMEOUT || _cwnd == 0) {
			timeout = MAX_CTL_PACKET_TIMEOUT;
		}
		if (timeout < MIN_CTL_PACKET_TIMEOUT) {
			timeout = MIN_CTL_PACKET_TIMEOUT;
		}
		_ctl_packet_timeout = eventlist().now() + timeout;
		eventlist().sourceIsPendingRel(*this,timeout);
		return;
	}
    if(_rtx_timeout_pending) {
		_rtx_timeout_pending = false;

		if (_logger) 
			_logger->logXcp(*this, XcpLogger::XCP_TIMEOUT);

		// if (_in_fast_recovery) {
		// 	uint32_t flightsize = _highest_sent - _last_acked;
		// 	_cwnd = min(_ssthresh, flightsize + _mss);
		// }

		//deflate_window();

		_cwnd = _mss;

		//_unacked = _cwnd;
		//_effcwnd = _cwnd;
		_in_fast_recovery = false;
		_recoverq = _highest_sent;

		if (_established)
	    	_highest_sent = _last_acked + _mss;

		_dupacks = 0;

		retransmit_packet();

		// if (_sawtooth>0)
		// 	_rtt_avg = _rtt_cum/_sawtooth;
		// else
		// 	_rtt_avg = timeFromMs(0);

		// _sawtooth = 0;
		// _rtt_cum = timeFromMs(0);

    } else {
		//cout << "Starting flow" << endl;
		startflow();
    }
}

mem_b XcpSrc::get_instantaneous_queuesize() const {
	return _instantaneous_queuesize;
}

void XcpSrc::set_instantaneous_queue(mem_b size) {
	if (size == 0) {
		//_instantaneous_queuesize = 0;
		_instantaneous_sent = 0;
		_persistent_sent = 0;
		_last_tuning_queue_size = 0;
	}
	_instantaneous_queuesize = size;
	//_last_tuning_start_time = 0;
	_last_tuning_start_time = (1ul << 60);
}

////////////////////////////////////////////////////////////////
//  XCP SINK
////////////////////////////////////////////////////////////////

XcpSink::XcpSink() 
    : Logged("sink"), _cumulative_ack(0) , _packets(0), _crt_path(0)
{
    _nodename = "xcpsink";
}

void 
XcpSink::connect(XcpSrc& src, const Route& route) {
    _src = &src;
    _route = &route;
    _cumulative_ack = 0;
    _drops = 0;
}

// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void
XcpSink::receivePacket(Packet& pkt) {
	if (pkt.type() == XCPCTL) {
		XcpCtlPacket* p = dynamic_cast<XcpCtlPacket*>(&pkt);
		cout << this->nodename() << " Receiving Control Packet with size: " << p->throughput() << " addr: " << p << endl;
		XcpCtlAck* ack = XcpCtlAck::newpkt(_src->_flow, *_route, p->throughput());
		ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
		ack->set_ts_echo(p->ts());
		cout << "SENDON6" << endl;
		ack->sendOn();
		p->free();
		return;
	}

    XcpPacket *p = dynamic_cast<XcpPacket*>(&pkt);
	if (p == NULL) {
		return;
	}

    XcpPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();

    //bool marked = p->flags()&ECN_CE;

    int size = p->size(); // TODO: the following code assumes all packets are the same size
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    //_packets += p->size();

    //cout << "Sink recv seqno " << seqno << " size " << size << endl;

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
		_cumulative_ack = seqno + size - 1;
	//cout << "New cumulative ack is " << _cumulative_ack << endl;
	// are there any additional received packets we can now ack?
		while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
			_received.pop_front();
			_cumulative_ack+= size;
		}
    } else if (seqno < _cumulative_ack+1) {
    } else { // it's not the next expected sequence number
		if (_received.empty()) {
			_received.push_front(seqno);
			//it's a drop in this simulator there are no reorderings.
			//_drops += (1000 + seqno-_cumulative_ack-1)/1000;
			++_drops;
		} else if (seqno > _received.back()) { // likely case
			_received.push_back(seqno);
		} else { // uncommon case - it fills a hole
			list<uint64_t>::iterator i;
			for (i = _received.begin(); i != _received.end(); i++) {
				if (seqno == *i) break; // it's a bad retransmit
				if (seqno < (*i)) {
					_received.insert(i, seqno);
					break;
				}
			}
		}
    }
    //send_ack(ts, marked, p->cwnd(), p->demand(), p->rtt());
	send_ack(ts, false, p->cwnd(), p->demand(), p->rtt());

	cout << "SENDING ACK: DEMAND: " << p->demand() << endl;

	p->free();
}

void 
XcpSink::send_ack(simtime_picosec ts, bool marked, uint32_t cwnd, int32_t demand,
		  simtime_picosec rtt) {
    const Route* rt = _route;
    
#ifdef PACKET_SCATTER
    if (_paths){
#ifdef RANDOM_PATH
	_crt_path = random()%_paths->size();
#endif
	
	rt = _paths->at(_crt_path);
	_crt_path = (_crt_path+1)%_paths->size();
    }
#endif
    
    XcpAck *ack = XcpAck::newpkt(_src->_flow, *rt, 0, _cumulative_ack, cwnd, demand, rtt);

    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
    ack->set_ts_echo(ts);
    // if (marked) 
	// ack->set_flags(ECN_ECHO);
    // else
	// ack->set_flags(0);

	cout << "SENDON7" << endl;
    ack->sendOn();
}

////////////////////////////////////////////////////////////////
//  XCP RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

XcpRtxTimerScanner::XcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
    : EventSource(eventlist,"RtxScanner"), _scanPeriod(scanPeriod) {
    eventlist.sourceIsPendingRel(*this, _scanPeriod);
}

void 
XcpRtxTimerScanner::registerXcp(XcpSrc &xcpsrc) {
    _xcps.push_back(&xcpsrc);
}

void
XcpRtxTimerScanner::unregisterXcp(XcpSrc &xcpsrc) {
	for (auto it = _xcps.begin() ; it != _xcps.end() ; ++it) {
		if (*it == &xcpsrc) {
			auto it_temp = it;
			--it;
			_xcps.erase(it_temp);
		}
	}
}

void XcpRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    xcps_t::iterator i;
    for (i = _xcps.begin(); i!=_xcps.end(); i++) {
		(*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
