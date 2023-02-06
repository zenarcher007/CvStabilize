#
# A simple makefile for compiling a c++ project
#

# Compile paths:
SOURCES = stabilize.cpp
DEBUGDIR = debug
RELEASEDIR = release
EXECUTABLE = stabilize

# Header search paths
SYSTEM_HEADERS = /usr/local/include
HEADERS = -I/usr/local/include/opencv4 -I /opt/homebrew/Cellar/ffmpeg/5.1.2/include

# Options      # pkg-config --libs opencv4
# -lopencv_calib3d -lopencv_imgproc -lopencv_contrib -lopencv_legacy -lopencv_core -lopencv_ml -lopencv_features2d -lopencv_objdetect -lopencv_flann -lopencv_video -lopencv_highgui
# -lopenc_calib3d -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_mI -lopencv_objdetect -lopencv_photo -lopencv_shape -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videoio -lopencv_videostab
CV_FLAGS = -lpthread -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_objdetect -lopencv_photo -lopencv_shape -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videoio -lopencv_videostab
COMPILER = clang++
CFLAGS = --system-header-prefix=$(SYSTEM_HEADERS) $(HEADERS) -std=c++20 $(CV_FLAGS)
LAUNCHARGS = 
RM = rm -rf



# Debug options (in addition to global CFLAGS):
DFLAGS = $(CFLAGS) -gdwarf-4 -g3

# Release options (in addition to global CFLAGS):
RFLAGS = $(CFLAGS) -O3 -flto

#all: clean default test
all: clean release
default: all

debug:
	mkdir -p debug
	$(COMPILER) $(DFLAGS) $(SOURCES) -o $(DEBUGDIR)/$(EXECUTABLE)

release:
	mkdir -p release
	$(COMPILER) $(RFLAGS) $(SOURCES) -o $(RELEASEDIR)/$(EXECUTABLE)

.PHONY: all clean debug release

#stabilize.cpp:
#	$(COMPILER) $(CFLAGS) $(SRC)/main/stabilize.cpp

test: debug
	$(DEBUGDIR)/$(EXECUTABLE) $(LAUNCHARGS)

#test: release
#	$(RELEASEDIR)/$(EXECUTABLE) $(LAUNCHARGS)

clean:
	$(RM) $(DEBUGDIR)/* $(RELEASEDIR)/*
