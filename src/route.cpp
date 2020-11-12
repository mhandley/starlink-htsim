// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-  
#include <climits>
#include "route.h"
#include "network.h"
#include "queue.h"


#define MAXQUEUES 10
#define REFCOUNT_UNUSED -1

Route::Route() : _reverse(NULL), _refcount(REFCOUNT_UNUSED) {}

void
Route::add_endpoints(PacketSink *src, PacketSink* dst) {
    _sinklist.push_back(dst);
    if (_reverse) {
	_reverse->push_back(src);
    }
}

// refcount functions are const, but actually modify refcount.  This
// is ugly, but avoids the need to use external reference-counted
// pointers.
void
Route::use_refcount() const {
    //cout << this << " use_refcount" << endl;
    assert(_refcount == REFCOUNT_UNUSED);
    _refcount = 0;
}

void
Route::incr_refcount() const {
    if (_refcount != REFCOUNT_UNUSED) {
	_refcount++;
    }
    //cout << this << "incr_refcount, now " << _refcount << endl;
}

void
Route::decr_refcount() const {
    //cout << this << "decr_refcount" << endl;
    assert(_refcount < 1000);
    if (_refcount != REFCOUNT_UNUSED) {
	assert(_refcount > 0);
	_refcount--;
	//cout << this << "decr_refcount, now " << _refcount << endl;
	if (_refcount == 0) {
	    //cout << "FREEING ROUTE" << endl;
	    delete(this);
	    return;
	}
    }
}

string Route::str() const {
    string s = "Route: ";
    for (int i = 0; i < _sinklist.size(); i++) {
	cout << "i: " << i << "\n";
	cout << _sinklist[i] << endl;
	s += _sinklist[i]->nodename() + " ";
    }
    return s;
}
