#!/bin/sh
mkdir -p binary

NETCODE="socketcommon.cpp client.cpp mobileclient.cpp server.cpp ircserver.cpp netcore.cpp"
SOURCES="$NETCODE main.cpp window.cpp dns.cpp ini.cpp richtext.cpp"
FLAGS="-std=c++11 -DUSE_GNUTLS -lgnutls -pthread -g"

g++ -o binary/vulpircd $FLAGS $SOURCES

