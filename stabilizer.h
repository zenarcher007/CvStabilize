#ifndef Stabilizer_h
#define Stabilizer_h

#include <opencv2/opencv.hpp>
#include <thread>
#include "AtomicPriorityQueue.h"

using namespace std; // TODO: Header / cpp separation...

class Stabilizer {
  private:
    unsigned long frameCount; // Count of the frames used
    cv::VideoCapture* cap;
    std::mutex popMutex;
    std::mutex frameMtx;
    std::condition_variable popNotify; // Notified each time an item is inserted into the list, for the popper to check if the right item is available next
    std::atomic<int> dispatchCount; // Number of threads currently running
    std::thread* threads;
    int processorCount;
    std::atomic<bool> emergencyStop; // Tells running threads to stop
    unsigned long retiredCount; // Count of the frames retired via the ">>" operator
    cv::Rect viewRect; // The view portion you want to keep within the stabilizing frame
    cv::Mat refImg; // The "match" image to lock onto throughout the video
    cv::Point refPos; // The upper-left corner of the reference image within the larger frame


    class Frame : Comparable<Frame> {
      private:
      public:
      long number;
      cv::Mat frame;
      // Constructor: uses mtx synchronization to initialize this object by grabbing the next frame from cap and incrementing frameCount
      Frame(unsigned long* frameCount, mutex* mtx, cv::VideoCapture* cap) {
        std::scoped_lock l(*mtx);
        *frameCount = *frameCount + 1; // I don't know why the ++ operator doesn't work with this...
        number = *frameCount;
        *cap >> frame;
      }
      int compareTo(Frame* other) { // compareTo method for AtomicPriorityQueue. Reverse its order to prefer sooner frames first.
        return other->number - number;
        //return number - other->number;
      }
      friend std::ostream& operator<<(std::ostream& os, Frame const& f) { // Make printable via cout
        return os << "<Frame #" << f.number << ">";
      }
    };
    
    AtomicPriorityQueue<Frame> outputQueue;

  public:
  Stabilizer(cv::VideoCapture* cap, cv::Rect& viewRect, cv::Point& refPos, cv::Mat& refImg): cap(cap), outputQueue(), popMutex(), frameMtx(), popNotify() {
    frameCount = 0;
    dispatchCount.store(false, std::memory_order_relaxed);
    threads = nullptr;
    processorCount = 0;
    emergencyStop.store(false, std::memory_order_relaxed);
    retiredCount = 0;
    this->viewRect = viewRect;
    this->refImg = refImg;
    this->refPos = refPos;
  }

  ~Stabilizer() {
    emergencyStop.store(true, std::memory_order_release);
    for(int i = 0; i < processorCount; ++i) {
      threads[i].join();
    }
    if(threads != nullptr)
      delete[] threads;
  }

  bool run(int processorCount) {
    if(dispatchCount.load(std::memory_order_relaxed) != 0) return false; // Already running
    this->processorCount = processorCount;
    emergencyStop.store(false, std::memory_order_release);
    frameCount = 0;
    retiredCount = 0;
    dispatchCount.store(processorCount, std::memory_order_release);
    if(threads != nullptr) {
      delete[] threads;
      threads = nullptr;
    }
    threads = new std::thread[processorCount];
    for(int i = 0; i < processorCount; ++i) {
      threads[i] = std::thread(stabilize, &frameCount, &frameMtx, cap, &outputQueue, &popNotify, &dispatchCount, &emergencyStop, viewRect, refImg, refPos);
    }
    return true;
  }

  // Override >> operator to write frame data to given Mat object
  // If the list is empty and the algorithm is not completed, waits until a new item is inserted in-order.
  //Stabilizer& operator >> (CV_OUT cv::Mat& image)
  void operator >> (CV_OUT cv::Mat& image) {
    std::unique_lock<std::mutex> lk(popMutex);
    while(true) {
      Frame* f = outputQueue.peek(std::memory_order_relaxed);
      if(f == nullptr) { // End of stream, or is the output starved, waiting for more data?
        if(dispatchCount.load(std::memory_order_relaxed) == 0) { // Is there something that's going to put more in, or is it done?
          image = cv::Mat(); 
          return; // Write an empty frame if none available
        }
      } else if(f->number == retiredCount + 1) { // If the next frame you want is assembled next in the queue
        break;
      }
      popNotify.wait(lk);
    }
    ++retiredCount;
    Frame r = outputQueue.pop();
    image = r.frame;
  }

  static void stabilize(unsigned long* frameCount, mutex* frameMtx, cv::VideoCapture* cap, AtomicPriorityQueue<Frame>* outputQueue, std::condition_variable* popNotify, std::atomic<int>* dispatchCount, std::atomic<bool>* emergencyStop, cv::Rect viewRect, cv::Mat refImg, cv::Point refPos) {
    // Find original offset from the matchRect to the viewRect
    int offsetX = viewRect.x - refPos.x;
    int offsetY = viewRect.y - refPos.y;
    while(emergencyStop->load(std::memory_order_relaxed) == false) {
      Frame frame(frameCount, frameMtx, cap); // Synchronously grab next frame
      if(frame.frame.empty()) break;
      // Process / stabilize frames...
      cv::Mat diffImg;
      cv::matchTemplate(frame.frame, refImg, diffImg, cv::TM_CCOEFF_NORMED);
      double minVal, maxVal;
      cv::Point minLoc, maxLoc;
      cv::minMaxLoc(diffImg, &minVal, &maxVal, &minLoc, &maxLoc);
      cv::Point viewStart(std::clamp(maxLoc.x + offsetX, 0, frame.frame.cols - viewRect.width), std::clamp(maxLoc.y + offsetY, 0, frame.frame.rows - viewRect.height));
      cv::Rect stabilizedRect(viewStart, cv::Point(viewStart.x + viewRect.width, viewStart.y + viewRect.height));
      frame.frame = frame.frame(stabilizedRect); // Snip portion of frame to that of stabilizedRect
      // Reorganize frame in atomic priority queue
      outputQueue->push(frame);
      popNotify->notify_all(); // Notify popper
    }
    dispatchCount->fetch_sub(1, std::memory_order_relaxed); // Decrement number of threads running as it exits
  }
  
};

#endif