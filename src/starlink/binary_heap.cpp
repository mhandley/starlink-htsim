// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
// C++ implementation of a binary heap

#include "binary_heap.h"


BinaryHeap::BinaryHeap(int maxsize) {
    _maxsize = maxsize;
    heap = new Node*[_maxsize];
    _size = 0;
}

BinaryHeap::~BinaryHeap() {
    delete heap;
}

// returns the index of the parent node
int BinaryHeap::parent_ix(int ix) {
    return (ix - 1) / 2;
}

// return the index of the left child 
int BinaryHeap::left_child_ix(int ix) {
    return 2*ix + 1;
}

// return the index of the right child 
int BinaryHeap::right_child_ix(int ix) {
    return 2*ix + 2;
}

void BinaryHeap::swap_by_ix(int x, int y) {
    Node *tempnode = heap[x];
    set_node(x, heap[y]);
    set_node(y, tempnode);
}

void BinaryHeap::insert_at_infinity(Node& node) {
    if (_size >= _maxsize) {
        cout<<"The heap is full. Cannot insert"<< _maxsize << " " << _size << endl;
	return;
    }

    node.set_dist(DIST_INFINITY);
    node.clear_routing();
    set_node(_size, &node);
    _size++;
}

void BinaryHeap::move_up(int ix) {
    while (ix != 0 && heap[BinaryHeap::parent_ix(ix)]->dist() > heap[ix]->dist()) {
        BinaryHeap::swap_by_ix(BinaryHeap::parent_ix(ix), ix);
        ix = BinaryHeap::parent_ix(ix);
    }
}

void BinaryHeap::decrease_distance(Node& node, double distance) {
    node.set_dist(distance);
    move_up(node.heap_ix);
}

// insert the item at the appropriate position
void BinaryHeap::insert(Node& node) {
    if (_size >= _maxsize) {
	cout<<"The heap is full. Cannot insert"<<endl;
	return;
    }

    // first insert the time at the last position of the array 
    // and move it up
    set_node(_size, &node);
    node.clear_routing();

    // move up until the heap property satisfies
    move_up(_size);
    _size++;
}

// moves the item at position i of array a
// into its appropriate position
void BinaryHeap::min_heapify(int ix){
    // find left child node
    int left_ix = BinaryHeap::left_child_ix(ix);

    // find right child node
    int right_ix = BinaryHeap::right_child_ix(ix);

        // find the largest among 3 nodes
    int smallest = ix;

    // check if the left node is larger than the current node
    if (left_ix <= _size && heap[left_ix]->dist() < heap[smallest]->dist()) {
	smallest = left_ix;
    }

    // check if the right node is larger than the current node 
    // and left node
    if (right_ix <= _size && heap[right_ix]->dist() < heap[smallest]->dist()) {
	smallest = right_ix;
    }

    // swap the largest node with the current node 
    // and repeat this process until the current node is larger than 
    // the right and the left node
    if (smallest != ix) {
	//int temp = heap[i];
	//heap[i] = heap[largest];
	//heap[largest] = temp;
	swap_by_ix(smallest, ix);
	min_heapify(smallest);
    }
}

// returns the  maximum item of the heap
Node& BinaryHeap::get_min() {
    return *heap[0];
}

// deletes the max item and return
Node& BinaryHeap::extract_min() {
    Node* min_item = heap[0];

    // replace the first item with the last item
    set_node(0, heap[_size - 1]);
    _size--;

    // maintain the heap property by heapifying the 
    // first item
    min_heapify(0);
    return *min_item;
}

// prints the heap
void BinaryHeap::printHeap() {
    for (int i = 0; i < _size; i++) {
	cout<< heap[i]->dist() << endl;
    }
    cout<<endl;
}
