#!/bin/sh
mkdir -p binary

NETCODE="socketcommon.cpp client.cpp mobileclient.cpp server.cpp ircserver.cpp netcore.cpp"
SOURCES="$NETCODE main.cpp dns.cpp"
FLAGS="-std=c++11 -lgnutls -pthread -g"

g++ -o binary/nb4 $FLAGS $SOURCES

