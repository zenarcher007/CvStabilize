//
//  AtomicList.h
//  CalcInstruments
//
//  Created by Justin Douty on 4/11/22.
//

#ifndef AtomicList_h
#define AtomicList_h

#include <atomic>

/* An interface that allows objects to be compared with one another. */
template <typename generic>
class Comparable {
  virtual int compareTo(generic* other) = 0;
};

template <typename generic>
class AtomicNode {
private:
public:
  std::atomic<AtomicNode*> next;
  generic data;
  //std::unique_ptr<atomic<AtomicNode>> next;
  AtomicNode(generic item): data(item) {
    std::atomic_thread_fence(std::memory_order_release);
  }
  
  ~AtomicNode() {
    delete next.load(); // Free all nodes down the chain that would otherwise have no way to get to them anyways.
  }
  
  // TODO: Better synchronization methods might be needed...
  generic* getData(std::memory_order sync = std::memory_order_acquire) {
    std::atomic_thread_fence(sync);
    return &data;
  }

  AtomicNode* getNext(std::memory_order sync = std::memory_order_acquire) {
    return next.load(sync);
  }
  
  // TODO: Emplace??
  //void setData(generic item) {
  //  data = item;
  //  std::atomic_thread_fence(std::memory_order_release);
  //}
  
};


/* A basic atomic single linked list. This is very abstract. The nodes are manipulatable outside the class instance, although
 this contains a few helpful functions... */
template <typename generic>
class AtomicSingleLinkedList {
public:
  std::atomic<AtomicNode<generic>*> head;

  AtomicSingleLinkedList() {
    head.store(NULL);
  }
  ~AtomicSingleLinkedList() {
    delete head; // Should free others down the line.
  }
  
  /* Returns the head of the list. */
  inline AtomicNode<generic>* getHead(std::memory_order sync = std::memory_order_acquire) {
    return head.load(sync);
  }
  
  /* Returns the next node given the current node, or null if non-existent... */
  inline AtomicNode<generic>* getNext(AtomicNode<generic>* curr, std::memory_order preSync = std::memory_order_acquire) {
    return curr->next.load(preSync);
  }
  
  /* Inserts an item after a given node. Inserts an item after the head node if the given node is NULL. */
  void insertAfter(AtomicNode<generic>* prevNode, generic& item, std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    if(prevNode == NULL) {
      addFront(item); // Delegation
      return;
    }
    AtomicNode<generic>* newNode = new AtomicNode<generic>(item);
    //newNode->setData(item);
    AtomicNode<generic>* nodeAfter = prevNode->next.load(preSync); // The previous node's next node.
    newNode->next.store(nodeAfter, std::memory_order_relaxed); // Set this node's next
    prevNode->next.store(newNode, postSync); // Connect newNode in with the previous node.
  }
  
  /* Add a node to the front. */
  void addFront(generic& item, std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    AtomicNode<generic>* first = head.load(preSync); // Make sure nothing weird happens to the ordering of something before calling the function
    AtomicNode<generic>* newNode = new AtomicNode<generic>(item);
    //newNode->setData(item);
    newNode->next.store(first, std::memory_order_relaxed);
    head.store(newNode, postSync);
  }
  
  /* Alias for addFront */
  inline void push(generic& item, std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    addFront(item, preSync, postSync);
  }
  
  /* It can delete the next node, given only the PREVIOUS node. If the previous node is a null pointer, it will delete the head. */
  void delNode(AtomicNode<generic>* prevNode, std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    //std::atomic_thread_fence(preSync); // TODO: Can I just do this??
    /* TODO: Can I just use relaxed in between one acquire operation and another release operation? - I think so? */
    if(prevNode == NULL) { // You are trying to delete the head
      AtomicNode<generic>* oldHead = head.load(preSync);
      head.store(oldHead->next.load(preSync), postSync);
      head->next.store(NULL, preSync); // Set this so it doesn't delete the rest of the chain when its destructor is called.
      delete oldHead;
    } else { // You are trying to delete the next node
      AtomicNode<generic>* nextNode = prevNode->next.load(preSync);
      if(nextNode != NULL) { // If the node after the next node is not past the end of the list
        prevNode->next.store(nextNode->next.load(preSync)); // Reroute around the next node, to the node after it...
        nextNode->next.store(NULL, preSync); // Set this so it doesn't delete the rest of the chain when its destructor is called.
        delete nextNode;
      } else {
        // This doesn't make sense. If you wanted to delete the last node, you should have
        // given it the node before it; not the very last node. Give up and do nothing.
      }
    }
    //std::atomic_thread_fence(postSync);
  }
  
  void graph(std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    AtomicNode<generic>* curr = head.load(preSync);
    while(true) {
      std::cout << "[" << *((generic*) curr->getData()) << "]";
      curr = curr->next.load(preSync);
      if(curr == NULL)
        break;
      else
        std::cout << " -> ";
    }
    std::cout << "\n";
  }
};


/* An atomic priority queue.
 NOTE that items in "generic" must extend "Comparable<generic>," and contain an "int compareTo(generic* other)" method! */
template <typename generic>
class AtomicPriorityQueue : public AtomicSingleLinkedList<generic> {
private:
public:
  AtomicPriorityQueue() : AtomicSingleLinkedList<generic>() {
  }
  
  /* Adds an item in its respective order, utilizing generic's compareTo() method. */
  void push(generic& item, std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    AtomicNode<generic>* curr = AtomicSingleLinkedList<generic>::getHead();
    AtomicNode<generic>* prev = NULL; // Saves the previous node to insert items after it
    if(AtomicSingleLinkedList<generic>::getHead() == NULL) { // Edge case for an empty list: insert an item
      AtomicSingleLinkedList<generic>::addFront(item, preSync, postSync);
      return;
    }
      
    while(curr != NULL) {
      if(item.compareTo(&(curr->data)) >= 0) { // This is where it should go
        break;
      }
      prev = curr;
      curr = AtomicSingleLinkedList<generic>::getNext(curr, preSync);
    }
    AtomicSingleLinkedList<generic>::insertAfter(prev, item, preSync, postSync);
  }
  



  /* Dequeues the item with the highest comparison. Avoid calling this on an empty queue. */
  generic pop(std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    AtomicNode<generic>* oldHead = AtomicSingleLinkedList<generic>::getHead(preSync);
    generic item = *oldHead->getData(std::memory_order_relaxed); // Get item from current head; preSync once.
    AtomicSingleLinkedList<generic>::head.store(AtomicSingleLinkedList<generic>::head.load(std::memory_order_relaxed)->getNext(std::memory_order_relaxed), postSync); // Increment list's head to next item; already did preSync
    oldHead->next.store(nullptr, std::memory_order_relaxed); // Set this to nullptr so it doesn't delete the rest of the chain
    delete oldHead;
    return item;
  }
  
  // Returns a pointer to the next item, if it exists.
  inline generic* peek(std::memory_order preSync = std::memory_order_acquire) {
    AtomicNode<generic>* hd = AtomicSingleLinkedList<generic>::getHead(preSync);
    if(hd == nullptr)
      return nullptr;
    else
      return hd->getData(std::memory_order_relaxed);
  }
};




/* Atomic list iterators. Note that these are not to be used while the list is actively changing, but can be used
 in guaranteed periods of inactivity of the linked list. */

template <typename generic>
class AtomicListIterator {
private:
  AtomicSingleLinkedList<generic>* list;
  AtomicNode<generic>* curr;
public:
  AtomicListIterator(AtomicSingleLinkedList<generic>* list) {
    this->list = list;
    curr = list->getHead();
  }
  
  /* Returns false if at the end of the list, and true otherwise. */
  inline bool hasNext() {
    return curr != NULL; //&& curr->next != NULL;
  }
  
  /* Will return a null pointer when it reached the end of the list.
   Don't call this once it returns NULL! */
  inline generic* next() {
    AtomicNode<generic>* old = curr;
    curr = curr->next;
    return old->getData();
  }
  
  /* Inserts an item after the current item (TODO) */
  //void insert(generic item) {
  //}
};

/* A list iterator that supports functions to delete the current node, but must keep track of the previous
 node at all times. */
template <typename generic>
class AtomicDeletingListIterator : protected AtomicListIterator<generic> {
private:
  AtomicNode<generic>* prev;
public:
  AtomicDeletingListIterator(AtomicSingleLinkedList<generic>* list) : AtomicListIterator<generic>(list) {
    prev = NULL;
  }
  
  /* Deletes the current node it is on. */
  inline generic* deleteCurrent(std::memory_order preSync = std::memory_order_acquire, std::memory_order postSync = std::memory_order_release) {
    AtomicListIterator<generic>::list->delNode(prev, preSync, postSync);
  }
  
  /* Advances to the next node, saving the previous node as well. */
  inline generic* next() {
    prev = AtomicListIterator<generic>::curr; // Record previous
    return AtomicListIterator<generic>::next();
  }
  
};

#endif /* AtomicList_h */
