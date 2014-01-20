#!/bin/sh
mkdir -p binary
g++ -o binary/nb4 -std=c++11 main.cpp socketcommon.cpp client.cpp mobileclient.cpp server.cpp netcore.cpp dns.cpp -lgnutls -pthread -g

