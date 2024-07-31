#ifndef testcalibrator_h
#define testcalibrator_h

#include "../calibrator.h"
#include <cassert>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>

/*
Mat getNextFrame() {
    Mat frame;
    cap >> frame;
    if frame.empty() {
      cap.set(CAP_PROP_POS_FRAMES, 0); // rewind to beginning
      cap >> frame;
    }
    return frame;
  }*/

class TestCalibrator {
  public:
  TestCalibrator() {
  }
  
  void testCalibratorDoesntAffectVideoCapture() {
    cv::VideoCapture cap("yellowbreastedchat.mov");
    Calibrator calibrator(cap);
    assert(cap.get(cv::CAP_PROP_POS_FRAMES) == 0);
    cv::Mat frame = calibrator.calibrate();
    assert(cap.get(cv::CAP_PROP_POS_FRAMES) == 0);

    
  }

  void runtests() {
    testCalibratorDataGenDoesntAffectVideoCapture();
  }

};

int main() {
  TestCalibrator tc;
  tc.runtests();
}

#endif