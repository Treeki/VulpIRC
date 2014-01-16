#!/bin/sh
mkdir -p binary
g++ -o binary/nb4 core.cpp dns.cpp -lgnutls -pthread

