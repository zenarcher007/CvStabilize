#ifndef stabilizer_h
#define stabilizer_h

#include <opencv2/opencv.hpp>
#include <thread>
#include "AtomicPriorityQueue.h"
#include <opencv2/core/ocl.hpp>
#include "pointcloudtracker.h"

#include "calibrator.h"


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
    PointCloudTracker pct;
    cv::Rect heuristic_viewRect;
    cv::Rect heuristic_refRect;

    class Frame : Comparable<Frame> {
      private:
      public:
      cv::Point matchLoc; // The match position of the reference point on the original frame
      long number;
      cv::Mat image;

      Frame() {
      }

      // Constructor: uses mtx synchronization to initialize this object by grabbing the next frame from cap and incrementing frameCount
      Frame(unsigned long* frameCount, mutex* mtx, cv::VideoCapture* cap) {
        std::scoped_lock l(*mtx);
        *frameCount = *frameCount + 1; // I don't know why the ++ operator doesn't work with this...
        number = *frameCount;
        *cap >> image;
      }
      int compareTo(Frame* other) { // compareTo method for AtomicPriorityQueue. Reverse its order to prefer sooner frames first.
        return other->number - number;
        //return number - other->number;
      }
      friend std::ostream& operator<<(std::ostream& os, Frame const& f) { // Make printable via cout
        return os << "<Frame #" << f.number << ">";
      }
    };
    
    cv::Point lastMatchPos;
    AtomicPriorityQueue<Frame> outputQueue;

  public:
  Stabilizer(cv::VideoCapture* cap, cv::Rect& viewRect, cv::Point& refPos, cv::Mat& refImg): pct(), cap(cap), outputQueue(), popMutex(), frameMtx(), popNotify() {
    frameCount = 0;
    dispatchCount.store(false, std::memory_order_relaxed);
    threads = nullptr;
    processorCount = 0;
    emergencyStop.store(false, std::memory_order_relaxed);
    retiredCount = 0;
    this->viewRect = viewRect;
    this->refImg = refImg;
    this->refPos = refPos;
    heuristic_viewRect = cv::Rect(0,0,0,0);
    heuristic_refRect = cv::Rect(0,0,0,0);
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

  cv::Point getLastMatchPos() {
    return lastMatchPos;
  }

  cv::Rect getLastViewRect() { return heuristic_viewRect; }
  cv::Rect getLastRefRect() { return heuristic_refRect; }

  // Retrieves a single frame that displays a snapshot of the current state of the stabilization process
  // May return a null pointer. May not display the exact frame that was processed at the time, but should be close
  // Returns an empty Mat if any errors occurred
  cv::Mat getDebuggingFrame() {
    std::scoped_lock lock(popMutex);
    Frame* frame = outputQueue.peek(std::memory_order_acquire);
    if(! frame) {
      return cv::Mat();
    }
    cv::Mat image = frame->image;

    vector<cv::Point2f> const * newPoints = pct.getPoints();
    if(! newPoints) return image;
    // Add points from newPoints to the frame
    for(cv::Point2f point : *newPoints) {
      cv::circle(image, point, 10, cv::Scalar(0, 255, 0), -1);
    }
    return image;
  }

  // Sim
  void reiszeSkew(cv::Mat& image) {

  }

  // Override >> operator to write frame data to given Mat object
  // If the list is empty and the algorithm is not completed, waits until a new item is inserted in-order.
  //Stabilizer& operator >> (CV_OUT cv::Mat& image)
  void operator >> (CV_OUT cv::Mat& image) {
    Frame r;
    // Wait until a new frame is available
    {
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
      
      r = outputQueue.pop();
    }
    heuristic_refRect = cv::Rect(r.matchLoc, r.image.size());
    pct.update(r.image);

    // The rest of this function is Synchronous post-processing

    // Determine motion based on delta between last match position and current match position
    // We must do this synchronously because the last one in the queue may not necessarily be the previous frame in the video since this
    // is being processed in parallel.
    int motionX = 0;
    int motionY = 0;
    //if(lastMatchPos.x != 0 && lastMatchPos.y != 0) {
      motionX = r.matchLoc.x - lastMatchPos.x;
      motionY = r.matchLoc.y - lastMatchPos.y;
    //}

    double stretchMultiplierX = 0;
    double stretchMultiplierY = 0.25;
    motionX *= stretchMultiplierX;
    motionY *= stretchMultiplierY;
    
    int offsetX = viewRect.x - refPos.x;
    int offsetY = viewRect.y - refPos.y;
    cv::Point viewP1(std::clamp(r.matchLoc.x + offsetX , 0, r.image.cols - viewRect.width), std::clamp(r.matchLoc.y + offsetY , 0, r.image.rows - viewRect.height));
    cv::Point viewP2(clamp(viewP1.x + viewRect.width + motionX/2, 0, r.image.cols), clamp(viewP1.y + viewRect.height + motionY/2, 0, r.image.rows));
    //viewP1.x = viewP1.x - motionX/2;
    //viewP1.y = viewP1.y - motionY/2;
    cv::Rect stabilizedRect(viewP1, viewP2);
    heuristic_viewRect = stabilizedRect;
    // Make view window reference image
    //cout << r.image.cols << " x " << r.image.rows << ": " << stabilizedRect.x << ", " << stabilizedRect.y << ", " << stabilizedRect.width << ", " << stabilizedRect.height << endl;
    r.image = r.image(stabilizedRect); // Snip portion of frame to that of stabilizedRect
    if(r.image.cols != viewRect.width || r.image.rows != viewRect.height) {
      cv::resize(r.image, r.image, cv::Size(viewRect.width, viewRect.height));
    }
    //cout << motionX << ", " << motionY << " | " << r.image.cols << ", " << r.image.rows << " | " << viewRect.width << ", " << viewRect.height << "\n";


    lastMatchPos = r.matchLoc;
    image = r.image;
  }

  static void stabilize(unsigned long* frameCount, mutex* frameMtx, cv::VideoCapture* cap, AtomicPriorityQueue<Frame>* outputQueue, std::condition_variable* popNotify, std::atomic<int>* dispatchCount, std::atomic<bool>* emergencyStop, cv::Rect viewRect, cv::Mat refImg, cv::Point refPos) {
    // Find original offset from the matchRect to the viewRect
    int offsetX = viewRect.x - refPos.x;
    int offsetY = viewRect.y - refPos.y;
    while(emergencyStop->load(std::memory_order_relaxed) == false) {
      Frame frame(frameCount, frameMtx, cap); // Synchronously grab next frame
      if(frame.image.empty()) break;
      // Template match reference point to determine view window location
      cv::Mat diffImg;
      cv::matchTemplate(frame.image, refImg, diffImg, cv::TM_CCOEFF_NORMED);
      double minVal, maxVal;
      cv::Point minLoc, maxLoc;
      cv::minMaxLoc(diffImg, &minVal, &maxVal, &minLoc, &maxLoc);
      
      // Save reference match location to frame
      frame.matchLoc = maxLoc;

      // Place frame in atomic priority queue (in order)
      outputQueue->push(frame);
      popNotify->notify_all(); // Notify popper
    }
    dispatchCount->fetch_sub(1, std::memory_order_relaxed); // Decrement number of threads running as it exits
  }
  
};

#endif