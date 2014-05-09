#!/bin/sh

clang++ -I /usr/local/include/ test_cam_render.cpp `pkg-config --libs mediastreamer` -o test -D__APPLE__=1
