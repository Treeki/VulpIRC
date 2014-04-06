#!/bin/sh
mkdir -p binary

NETCODE="socketcommon.cpp client.cpp mobileclient.cpp server.cpp ircserver.cpp ircserver_numerics.cpp netcore.cpp"
SOURCES="$NETCODE main.cpp window.cpp dns.cpp ini.cpp richtext.cpp zlibwrapper.cpp"
FLAGS="-Wall -static -static-libgcc -static-libstdc++ -std=c++11 -pthread -DUSE_ZLIB"

g++ -o binary/vulpircd_static $FLAGS $SOURCES -lz

