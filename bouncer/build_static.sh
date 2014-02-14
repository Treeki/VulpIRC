#!/bin/sh
mkdir -p binary

NETCODE="socketcommon.cpp client.cpp mobileclient.cpp server.cpp ircserver.cpp netcore.cpp"
SOURCES="$NETCODE main.cpp window.cpp dns.cpp ini.cpp richtext.cpp"
FLAGS="-static -static-libgcc -static-libstdc++ -std=c++11 -pthread"

g++ -o binary/vulpircd_static $FLAGS $SOURCES

