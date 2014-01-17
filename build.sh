#!/bin/sh
mkdir -p binary
g++ -o binary/nb4 -std=c++11 core.cpp dns.cpp -lgnutls -pthread

