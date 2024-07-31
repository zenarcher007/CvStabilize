
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated" // Temporarily ignore distracting "deprecated" warnings, which nothing can be done about
#include <opencv2/opencv.hpp>
#pragma clang diagnostic pop
#include <math.h>
#include <thread>
#include "stabilizer.h"
#include <time.h>

#define RECTPOINTSIZE 15 // The size of the "knobs" for dragging the view and reference rectangles

using namespace std;
using namespace cv;


void stabilize(VideoCapture& cap) {
  while(true) {
    Mat frame;
    cap >> frame; // Read frame-by-frame
    if(frame.empty()) break; // Exit when at the end

    //setMouseCallback("Video Output", click_callback, nullptr);
    imshow("Video Output", frame);
    waitKey(1);
  }
}

// Takes the given variable, and updates it to the slider's position.
//template <typename Integral>
static void slider_callback(int pos, void* data) {
  int* sliderPos = (int*) data;
  *sliderPos = pos;
}

// Uses the distance formula to calculate the distance between two points.
int distance(cv::Point& p1, cv::Point& p2) {
  int xDiff = p2.x - p1.x;
  int yDiff = p2.y - p1.y;
  return sqrt(xDiff*xDiff + yDiff*yDiff);
}

// Data to be passed to rectPairDrag_callback during the initial positioning of the frames.
struct RectFrameData {
  cv::Rect viewRect;
  cv::Rect matchRect;
  int selectedCorner; // For storing which point is being dragged during the mouse dragging callback
  cv::Rect* selectedRect; // For storing which rectangle is currently selected
};

// Updates the corner of point ptId (numbered clockwise from 0, starting from upper left corner) of the Rect to the absolute position of Point pt, as if it were dragged.
void updateRectCorner(cv::Rect& rect, int ptId, cv::Point& pt) {
  switch(ptId) {
    case 0: // Upper left corner; used for changing the position of the entire rectangle
      rect.x = pt.x;
      rect.y = pt.y;
      break;
    case 1: // Upper right corner
      rect.width = pt.x - rect.x;
      rect.height -= pt.y - rect.y; // Keep the bottom portion of the rectangle in-place
      rect.y = pt.y;
      break;
    case 2: // Bottom right corner 
      rect.height = pt.y - rect.y;
      rect.width = pt.x - rect.x;
      break;
    case 3: // Bottom left corner
      rect.width -= pt.x - rect.x; // Keep the right portion of the rectangle in-place
      rect.x = pt.x;
      rect.height = pt.y - rect.y;
      break;
  }
}

// Returns the absolute position of the corner of the given rectangle (using the same numbering system as above)
cv::Point findRectCorner(cv::Rect& rect, int ptId) {
  switch(ptId) {
    case 0: // Upper left corner
      return cv::Point(rect.x, rect.y);
    case 1: // Upper right corner
      return cv::Point(rect.x + rect.width, rect.y);
    case 2: // Bottom right corner
      return cv::Point(rect.x + rect.width, rect.y + rect.height);
    case 3: // Bottom left corner
      return cv::Point(rect.x, rect.y + rect.height);
  }
  return cv::Point(0,0);
}




/* Controls the resizing of two rectangles by dragging their corners. Stores the last point dragged as the last element of the tuple;
make sure the same integer pointer is passed each time. */
static void rectPairDrag_callback(int event, int x, int y, int flags, void* data) {
  //std::tuple<cv::Rect*, cv::Rect*>* rect = (std::tuple<cv::Rect*, cv::Rect*, int*>*) data;
  std::pair<RectFrameData*, int*>* allData = (std::pair<RectFrameData*, int*>*) data; // Hold frame data, and an integer to set the poll time
  RectFrameData* rdata = allData->first; 

  if(event == EVENT_MOUSEMOVE && rdata->selectedCorner != -1) { // Track the selected point //  EVENT_FLAG_LBUTTON
    *allData->second = 1; // Set to faster poll delay
    cv::Point mousePos(x,y);
    updateRectCorner(*rdata->selectedRect, rdata->selectedCorner, mousePos);
  } else if(event == EVENT_LBUTTONUP) { // Find, and toggle dragging of the selected point.
    // Rather than clicking, holding, and dragging, clicking toggles hold of a point.
    // This change was made due to, strangely, incredibly poor performance when holding the mouse and dragging.
    if(rdata->selectedCorner == -1) {
      *allData->second = 1; // Set to faster poll delay
      int closestDist = INT_MAX;
      int closestCorner = 0;
      for(cv::Rect* tryRect : {&rdata->matchRect, &rdata->viewRect}) {
        for(int ptId = 0; ptId < 4; ++ptId) {
          cv::Point absCorner = findRectCorner(*tryRect, ptId);
          cv::Point mousePt = cv::Point(x,y);
          int dist = distance(mousePt, absCorner);
          if(dist < closestDist) {
            rdata->selectedRect = tryRect;
            closestCorner = ptId;
            closestDist = dist;
          }
        }
      }
      rdata->selectedCorner = closestCorner;
    } else {
      rdata->selectedCorner = -1;
      rdata->selectedRect = nullptr;
    }
  }
}

// Draws a rectangle with large points on its upper left, and bottom right corners
void drawRect(Mat* frame, cv::Rect& rect, cv::Scalar* color, int highlightPt = -1, string text = "") {
  cv::rectangle(*frame, cv::Point(rect.x, rect.y), cv::Point(rect.x+rect.width, rect.y+rect.height), *color, 5);
  for(int ptId = 0; ptId < 4; ++ptId) { // Draw a circle at each corner
    cv::Point cornerPos = findRectCorner(rect, ptId);
    Scalar color(0,255,0);
    if(ptId == highlightPt)
      color = Scalar(255,0,255);
    circle(*frame, cornerPos, 10, color, -1);
  }
  Point textPoint = findRectCorner(rect, 3);
  textPoint.y += 30;
  textPoint.x += 30;
  cv::putText(*frame, text, textPoint, cv::FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(255, 225, 0), 3);
}

void boolFlipMouseCallback(int event, int x, int y, int flags, void* data) {
  bool* b = (bool*) data;
  *b = !*b;
}

// Returns the rest of a string after the last [item] char (arguments as C-strings)
string getAfterLast(char* inStr, char item) {
  int dotPos = 0;
  int i = 0;
  while(inStr[i] != (char) 0) {
    if(inStr[i] == item) // Find the last item
      dotPos = i;
    ++i;
  }
  return string(&inStr[dotPos]);
}


void show_help(string progName) {
  cerr << "Usage: " << progName << " <Video File> <Output Video File> [--copy-audio, --motion-limit <n>]\n\n";
  cerr << "When picking a view and reference image portion, click to toggle dragging each corner of the rectangles to position them accordingly. The \"View Window\" rectangle corresponds to the cropped portion of the frame you want to see in the final output, offset from the \"Reference\" rectangle, which the algorithm searches for in each video frame. Try picking differernt reference images to obtain better results.\n";
  cerr << "After picking a view and reference image portion, press Enter to begin stabilizing. While processing, you may click the screen to toggle faster updating of the video output (decreased performance).\n";
  cerr << "--copy-audio: run the ffmpeg copy audio command when complete, rather than only displaying it. This must be specified after all positional parameters.\n";
  cerr << "--motion-limit: prevent updating the frame if the euclidian distnace between the same point and the last frame is greater than n pixels. Helps to reduce frame blur and mispredicted frames.\n";
}

// Returns whether the given flag is specified after the input and output file arguments
bool containsFlagArg(const char arg[], int argc, char** argv) {
  for(int argi = 3; argi < argc; ++argi) {
    if(strcmp(argv[argi], arg) == 0)
      return true;
  }
  return false;
}

// Gets the value after a given flag, if specified. Else, returns NULL
char* getFlagValue(const char arg[], int argc, char** argv) {
  for(int argi = 3; argi < argc; ++argi)
    if(strcmp(argv[argi], arg) == 0)
      if(argi+1 < argc)
        return argv[argi+1];
  return NULL;
}


// First, use a very crude user interface to allow the user to select a reference (match) rectangle portion, and a view rectangle portion.
int main(int argc, char** argv) {
  if(argc <= 2) {
    show_help(argv[0]);
    exit(0);
  }
  cv::VideoCapture cap(argv[1]);
  unsigned long frameCount = cap.get(CAP_PROP_FRAME_COUNT);
  
  RectFrameData rectData;
  {
    Mat frame;
    cap >> frame;
    //imshow("Video Output", frame);

    rectData.viewRect = cv::Rect(cv::Point(frame.size().width / 3, frame.size().height / 3), cv::Point(frame.size().width / 2 - 5, frame.size().height * 2 / 3));
    rectData.matchRect = cv::Rect(cv::Point(frame.size().width / 2 + 5, frame.size().height / 3), cv::Point(frame.size().width * 2 / 3, frame.size().height * 2 / 3));
    rectData.selectedRect = nullptr;
    rectData.selectedCorner = -1;
    int seekPos = 0;
    int frameIndex = 1;
    int pollTime = 250;
    cv::Mat refImg;
    while(true) { // Keep displaying the same frame; display a new one if the seek slider is changed.
      if(frameIndex != seekPos) {
        //cout << "Update to " << seekPos << "\n";
        frameIndex = seekPos;
        auto pos = min(seekPos, (int) (frameCount-1));
        cap.set(CAP_PROP_POS_FRAMES, pos);
        cap >> frame;
        cap.set(CAP_PROP_POS_FRAMES, pos); // Rewind that frame again
      }
      
      std::pair<RectFrameData*, int*> mouseCallbackData(&rectData, &pollTime);
      setMouseCallback("Video Output", rectPairDrag_callback, (void*) &mouseCallbackData);
      createTrackbar("Frame", "Video Output", nullptr, frameCount, slider_callback, &seekPos);
      Mat drawOnFrame;
      frame.copyTo(drawOnFrame);

      // Draw rectangles
      cv::Scalar mRectColor = cv::Scalar(255, 0, 0);
      cv::Scalar vRectColor = cv::Scalar(0, 0, 255);
      
      // Highlight the corner of the selected rectangle
      int matchCorner = -1;
      int viewCorner = -1;
      if(rectData.selectedRect == &rectData.matchRect)
        matchCorner = rectData.selectedCorner;
      else if(rectData.selectedRect == &rectData.viewRect)
        viewCorner = rectData.selectedCorner;
      drawRect(&drawOnFrame, rectData.matchRect, &mRectColor, matchCorner, "Reference"); // Draw match rect
      drawRect(&drawOnFrame, rectData.viewRect, &vRectColor, viewCorner, "View Window"); // Draw view rect
     
      cv::imshow("Video Output", drawOnFrame);
      pollTime = min(1000, pollTime+3);
      if(cv::waitKey(pollTime) == 13) break; // "Enter" key to continue
    }
    cv::putText(frame, "Processing... Click to toggle live video output", cv::Point(0,0), cv::FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 225, 0), 3);
    cv::imshow("Video Output", frame);
  }
  
  // --- Done picking reference and view frames via highGui ---
  Mat refImg;
  { // Snip portion of current frame to create refImg
    Mat frame;
    cap >> frame;
    refImg = frame(rectData.matchRect);
  }
  cap.set(CAP_PROP_POS_FRAMES, 0); // Rewind to beginning

  // --- Run stabilizer ---
  {
    const char* outfile = argv[2];
    cv::Point refPos = cv::Point(rectData.matchRect.x, rectData.matchRect.y);
    Stabilizer stabilizer(&cap, rectData.viewRect, refPos, refImg);
    stabilizer.run(std::thread::hardware_concurrency());
    Mat frame;
    int seekPos = 0;
    bool liveUpdate = false;
    time_t oldTime = time(NULL);
    Mat newFrame = frame;

    // Get fourcc of input video and initialize for video output
    // https://answers.opencv.org/question/77558/get-fourcc-after-openning-a-video-file/
    int fourcc = cap.get(CAP_PROP_FOURCC);
    double fps = cap.get(CAP_PROP_FPS);
    cv::Size newSize = Size(rectData.viewRect.width, rectData.viewRect.height);
    int origcc = cv::VideoWriter::fourcc(fourcc & 255, (fourcc >> 8) & 255, (fourcc >> 16) & 255, (fourcc >> 24) & 255);
    VideoWriter outputWriter(outfile, origcc, fps, newSize, true); // Enable ffmpeg; Source for obtaining fourcc data: Link above
    //int pixelFormat = cap.get(cv::CAP_PROP_CODEC_PIXEL_FORMAT);
    outputWriter.set(cv::VIDEOWRITER_PROP_QUALITY, 100); // Preserve full original quality if possible



    char* distArg = getFlagValue("--motion-limit", argc, argv);
    int maxDistance = 0;
    if(distArg != NULL)
      maxDistance = stoi(distArg);
    while(true) {
      cv::Point oldMatchPos = stabilizer.getLastMatchPos();
      stabilizer >> newFrame;
      if(newFrame.empty()) break;
      cv::Point newMatchPos = stabilizer.getLastMatchPos();
      int dist = norm(newMatchPos - oldMatchPos);
      if(distArg == NULL || frame.empty() || dist <= maxDistance)
        frame = newFrame;
      time_t seconds = time(NULL);
      if(seconds - oldTime >= 1) {
        oldTime = seconds;
        createTrackbar("Frame", "Video Output", nullptr, frameCount, nullptr, nullptr);
        setTrackbarPos("Frame", "Video Output", min(frameCount-1, (unsigned long) seekPos));
        setMouseCallback("Video Output", boolFlipMouseCallback);
        cv::Mat img = stabilizer.getDebuggingFrame();
        // Draw view box rectangle
        cv::Scalar mRectColor = cv::Scalar(255, 0, 0);
        cv::Scalar vRectColor = cv::Scalar(0, 0, 255);
        //drawRect(img, rectData.matchRect, &mRectColor, -1, "Reference"); // Draw match rect
        //drawRect(img, rectData.viewRect, &vRectColor, -1, "View Window"); // Draw view rect
        cv::rectangle(img, stabilizer.getLastViewRect(), vRectColor, 2);
        cv::rectangle(img, stabilizer.getLastRefRect(), mRectColor, 2);
        
        if(! img.empty()) {
          cv::imshow("Video Output", img);
        }
        cv::waitKey(1);
      }
      outputWriter.write(frame); // Write to output file
      ++seekPos;
    }
    outputWriter.release();
  }
  cv::destroyAllWindows();
  cap.release();
  // ffmpeg -i example_stabilized.MOV -i example.MOV -c:v:a copy -map 0:v:0 -map 1:a:0 example_stabilized.MOV\n;
  string outAsStr = string(argv[2]);
  int lastFileDelimPos = outAsStr.find_last_of("/");
  string outParDir = "";
  if(lastFileDelimPos != string::npos)
    outParDir = outAsStr.substr(0, lastFileDelimPos) + "/";
  string outName = getAfterLast(argv[2], '/'); // File name without extension
  string outLabel = outName.substr(0, outName.find_last_of('.'));
  string outFile = outParDir + outLabel + "_audio-copied" + getAfterLast(argv[2], '.'); // Add path, name, extra string, and file extension
  
  string audioCmd = "ffmpeg -y -i \"" + string(argv[2]) + "\" -i \"" + string(argv[1]) + "\" -c copy -map 0:v -map 1:a \"" + outFile + "\"";
  if(containsFlagArg("--copy-audio", argc, argv)) {
    cerr << "Copying audio to output...:\n\n";
    cerr << "Running: " << audioCmd << "\n\n";
    int ret = system(audioCmd.c_str());
    exit(ret);
  } else {
    cerr << "Completed. Video was generated without audio. To copy audio to the new stabilized video, run:\n\n";
    cerr << audioCmd << "\n";
  }
  
  exit(0);
}


