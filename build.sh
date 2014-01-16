#!/bin/sh
mkdir binary
g++ -o binary/nb4 core.cpp dns.cpp -lgnutls -pthread

