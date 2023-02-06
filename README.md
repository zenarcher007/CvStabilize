# CvStabilize
A simple, multithreaded video stabilizer using the OpenCV library in C++.  

  This program uses a multithreaded approach to stabilize a postprocessed video. It works by creating a number of threads, each of which use mutual exclusion to grab video frames as needed. Each thread processes its frame independently, and then reassembles it in order via an atomic implementation of a priority queue. The main thread continually waits until the next few frames are assembled in order, and writes them out to a new video file on disk. This parallelization style achieves fast and efficient CPU-based video stabilization.
  
 ### Usage:
  Video is stabilized using OpenCV's template matching feature. In the HighGui window, the resulting video will be cropped to the "View Window" rectangle, offset by the matching portion of video frames specified by the "Reference" image rectangle. Moving the slider will seek through the video, and clicking will toggle dragging the corners of the rectangles. Press enter to begin stabilizing. Experiment with different "reference" images to lock onto static portions of a video (smaller and higher contrast reference points typically work the best).
    
 ### Motivation:
  CvStabilize was a personal project inspired by the need for clear video from a shaky camera in a reasonable time, in which I came up with this parallelization algorithm for it. It has applications to parallel algorithm design and subject tracking.
  
### Notes:
  • This program works best from a nonmoving camera location, fixed on a nonmoving subject you want to keep in view, given a static point in the background that the program is able to lock onto.  
  • OpenCV does not preserve audio. However, specifying --copy-audio will invoke ffmpeg in the shell to copy the original audio to the stabilized video afterwards.
