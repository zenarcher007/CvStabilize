#
# A simple makefile for compiling a c++ project
#

# Compile paths:
SRC_DIR = .
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
BUILDDIR = build
DEBUGDIR = build
CPPSTANDARD=c++20

# Header search paths
HEADERS = -I/usr/local/include/opencv4 -I /opt/homebrew/Cellar/ffmpeg/*/include

CV_FLAGS = -Wno-unused-value -Wno-c++11-extensions -lpthread -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_objdetect -lopencv_photo -lopencv_shape -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videoio -lopencv_videostab
COMPILER := $(shell if command -v clang++ >/dev/null 2>&1; then echo clang++; else echo g++; fi)
#COMPILER = clang++
CFLAGS = $(HEADERS) -std=$(CPPSTANDARD) $(CV_FLAGS)
DFLAGS = $(CFLAGS) -gdwarf-4 -g3
LAUNCHARGS = 
RM = rm -rf

# Release options (in addition to global CFLAGS):
RFLAGS = $(CFLAGS) -O0
#-flto

#all: clean default test
all: build run
default: all

.PHONY: all build run

debug: $(SOURCES)
	mkdir -p "$(DEBUGDIR)"
	cd "$(DEBUGDIR)"
	for source in $^; do \
		outfile="$(DEBUGDIR)/$$(basename "$$source" | cut -d '.' -f1)" ; \
		cp -f "$$source" "$(DEBUGDIR)/" && \
		"$(COMPILER)" $(DFLAGS) "$$source" --save-temps=obj -o "$$outfile" ; \
	done

build: $(SOURCES)
	mkdir -p "$(BUILDDIR)"
	for source in $^; do \
		outfile="$(BUILDDIR)/$$(basename "$$source" | cut -d '.' -f1)" ; \
		"$(COMPILER)" "$$source" -o "$$outfile" $(RFLAGS); \
	done
#for source in $(SOURCES); do \
#  outfile="$(BUILDDIR)/$$(basename "$$source" | cut -d '.' -f1)" ; \
#	[ -f "$$outfile" ] && [ "$$source" -nt "$$outfile" ] && "$(COMPILER)" "$(RFLAGS)" "$$source" -o "$$outfile" ; \
#done | parallel


# Set shell for "run" task
run: SHELL:=/bin/bash
run:
	$(info ===== Running Tests On Last (or current) Compilation ===== )
	@echo "\n\n===== Running Tests On Last (or current) Compilation at $$(date) =====" >> tests.log
	@for sfile in $(BUILDDIR)/*; do \
    grep '\.' <<< "$$sfile" &>/dev/null && continue ; \
	  /bin/echo "Queueing $$sfile ..." 1>&2 ; \
	  /bin/echo "/bin/echo && /bin/echo '>Testing $$sfile ... ' && ./$$sfile 2>&1 && echo '>Passed' $$sfile " ; \
	done | bash -c "parallel --keep-order --group 2>&1 --halt-on-error soon,fail,10 2>&1 | tee >(cat 1>&2) >> tests.log" 2>&1 | bash -c "tee >(cat 1>&2)" | grep 'parallel: This job failed' >/dev/null && exit 1 || exit 0




clean:
	$(RM) $(BUILDDIR)/* ; \
	rm -f tests.log