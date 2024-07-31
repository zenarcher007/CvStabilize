#ifndef _pointcloudtracker_h
#define _pointcloudtracker_h

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>
#include <vector>
#include <cassert>

class PointCloudTracker {
private:
  // == For feature point tracking ==
  cv::Mat old_gray; // The last image; used during pointcloud analysis
  std::vector<cv::Point2f> oldPoints, newPoints; // Store point cloud points
  std::vector<cv::KeyPoint> keypoints;
  int lastRefreshPointCount;
public:

  PointCloudTracker(): oldPoints(), newPoints(), keypoints(), old_gray() {
    lastRefreshPointCount = 0;
  }

  // This will never be a nullptr
  const std::vector<cv::Point2f>* getPoints() {
    return reinterpret_cast<const std::vector<cv::Point2f>*>(&newPoints);
  }

  void update(cv::Mat& frame) {
    //// Preprocess for feature point tracking
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);


    // If first time, init variables and do inital setup
    if(old_gray.empty()) {
      old_gray = gray;
      cv::cvtColor(frame, old_gray, cv::COLOR_BGR2GRAY);
      cv::goodFeaturesToTrack(gray, newPoints, 100, 0.3, 7, cv::Mat(), 7, false, 0.04);
      oldPoints = newPoints;
    }

    // Update points on next frame
    // https://docs.opencv.org/3.4/d4/dee/tutorial_optical_flow.html
    std::vector<uchar> status;
    std::vector<float> err;
    cv::TermCriteria criteria = cv::TermCriteria((cv::TermCriteria::COUNT) + (cv::TermCriteria::EPS), 10, 0.03);
    
    calcOpticalFlowPyrLK(old_gray, gray, oldPoints, newPoints, status, err, cv::Size(30,30), 2, criteria);
    
    std::vector<cv::Point2f> good_new;
    for(uint i = 0; i < newPoints.size(); i++) {
      // Select good points
      if(status[i] == 1) {
        good_new.push_back(newPoints[i]);
      }
    }
    newPoints = good_new;
    // Check if we lost enough points and we need more
    std::vector<cv::KeyPoint> keypoints;
    if(newPoints.size() < lastRefreshPointCount - (lastRefreshPointCount / 4)) { // If we lost more than 25% of the points, add some more
      lastRefreshPointCount = newPoints.size();
      auto fastDetector = cv::FastFeatureDetector::create();
       
      fastDetector->detect(gray, keypoints); // detect points from "gray" image to keypoints vector

      for(cv::KeyPoint p : keypoints) {
        newPoints.push_back(p.pt);
      }

    }

    // Roll-over data to next frame
    cv::cvtColor(frame, old_gray, cv::COLOR_BGR2GRAY); // Update Grayscale image onto old_gray
    
    // deep-copy newPoints to oldPoints for next iteration
    for (uint i = 0; i < newPoints.size(); ++i)  {
      oldPoints[i] = cv::Point2f(newPoints[i].x, newPoints[i].y);
    }


  }


};
#endif