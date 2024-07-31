#ifndef calibrator_h
#define calibrator_h

#include "stabilizer.h"
#include "pointcloudtracker.h"

/* Requirements:
 * Doesn't affect the passed-in VideoCapture
 * Has public instance variables for the calibration parameters
 * Autonomously minimizes an error metric on the point cloud to find the calibration parameters
 */
class Calibrator {
private:
  cv::VideoCapture cap;
  PointCloudTracker pct;

  // Reads the passed-in vector, and returns a unique_ptr to a vector of the same size, with each point in the new vector being positionally-independent from the original
  std::unique_ptr<std::vector<cv::Point2f>> getPositionalIndependent(const std::vector<cv::Point2f>& points) {
    // Find the average point
    cv::Point2f average;
    for(int i = 0; i < points.size(); ++i) {
      average += points[i];
    }
    average /= (float) points.size();

    std::unique_ptr<std::vector<cv::Point2f>> relativePoints = std::make_unique<std::vector<cv::Point2f>>();
    relativePoints->reserve(points.size());

    for (int i = 0; i < points.size(); ++i) {
      relativePoints->push_back(points[i] - average);
    }
    return relativePoints;
  }

protected:
  cv::Mat getNextFrame() {
    cv::Mat frame;
    cap >> frame;
    if(frame.empty()) {
      cap.set(cv::CAP_PROP_POS_FRAMES, 0); // rewind to beginning
      cap >> frame;
    }
    return frame;
  }

public:
  Calibrator(cv::VideoCapture cap) {
    this->cap = cap;
  }
    

  void calibrate() {
    int initialPos = cap.get(cv::CAP_PROP_POS_FRAMES); // Satisfy requirements: doesn't affect the passed-in VideoCapture
    cv::Mat frame = getNextFrame();

    std::unique_ptr<std::vector<cv::Point2f>> relativePrevPoints = getPositionalIndependent(*pct.getPoints());

    




    // Restore the original position of the video capture
    cap.set(cv::CAP_PROP_POS_FRAMES, initialPos);
  }
};

#endif