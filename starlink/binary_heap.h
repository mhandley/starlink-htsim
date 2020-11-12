// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-                                                                                                               
// C++ implementation of a binary heap

#ifndef BINARY_HEAP_H
#define BINARY_HEAP_H

#include "node.h"

class BinaryHeap {
public:
    BinaryHeap(int maxsize);
    ~BinaryHeap();

    inline int size() const {return _size;}

    // insert the item at the appropriate position
    void insert(Node& node);

    // insert the item at the end of the heap
    void insert_at_infinity(Node& node);

    void decrease_distance(Node& node, double distance);

    // returns the  minimum item of the heap
    Node& get_min();

    // deletes the max item and return
    Node& extract_min();

    // prints the heap
    void printHeap();
private:
    // returns the index of the parent node
    static int parent_ix(int i);

    // return the index of the left child 
    static int left_child_ix(int i);

    // return the index of the right child 
    static int right_child_ix(int i);

    inline void set_node(int ix, Node* n) {
	heap[ix] = n;
	n->heap_ix = ix;
    }

    void swap_by_ix(int x, int y);

    // move a node up to its correct position, either after insertion
    // at the end, or after it's priority has been decreased.
    void move_up(int ix);


    // moves the item at position i of array a
    // into its appropriate position
    void min_heapify(int ix);

    int _maxsize;
    Node** heap;
    int _size;
};

#endif
