# CvStabilize
A simple, multithreaded video stabilizer using the OpenCV library in C++.  

  This program uses a multithreaded approach to stabilize a postprocessed video. It creates a number of threads, each of which uses mutual exclusion to grab video frames as needed. Each thread processes its frame independently, and reassembles it in its original order via an atomic implementation of a priority queue. The main thread continually waits until the next few frames are in order, and writes them out to a new video file on disk. This parallelization style achieves fast and efficient CPU-based video stabilization.
  
 ### Usage:
  Video is stabilized using OpenCV's template matching feature. In the HighGui window, the resulting video will be cropped to the "View Window" rectangle, offset by the location of features matching the "Reference" image rectangle in the video. Move the slider to seek through the video, and click to toggle dragging the corners of the rectangles (as holding and dragging doesn't work well on HighGui). Then press enter to begin stabilizing. Experiment with different reference images to lock onto static features in the background (smaller and higher-contrast reference points typically work best).
    
 ### Motivation:
  CvStabilize was a personal project inspired by the need for clear video from a shaky camera in a reasonable time, in which I came up with this parallelization algorithm for it. It can be applied to parallel algorithm design and subject tracking.
  
### Notes:
  • This program works best from a nonmoving camera location, fixed on a nonmoving subject you want to keep in view, given a static point in the background that the program is able to lock onto.  
  • OpenCV does not preserve audio. However, specifying --copy-audio will invoke ffmpeg in the shell to copy the original audio to the stabilized video afterwards, presuming you have it installed.
