/******************************************************************************
 * Created by Joshua Jones on 4/19/2020.
 * Copyright (c) 2020 
 * All rights reserved.
 ******************************************************************************/

#ifndef PINGCPP_PING_HPP
#define PINGCPP_PING_HPP

#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sstream>
#include <netdb.h>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#ifndef MAX_PACKET_LENGTH
#define MAX_PACKET_LENGTH   1024
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

struct Options {
    int deadline = 0;           // number of milliseconds to run ping for, 0 for unlimited
    struct timeval timeout;     // time to wait for a response in milliseconds
    int interval = 1000000;     // time between packets in microseconds
    int maxCount = 0;           // maximum number of packets to send before ending loop, 0 for unlimited
    int packet_size = 64;       // number of bytes to allocate to the packet
};

struct icmpPacket {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
    char msg[MAX_PACKET_LENGTH - 8];
};

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType);
void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr *pingTarget, int targetSize);
unsigned short checksum(void *b, int len);
void setSessionOption(char opt, const char *value, struct Options &settings);

#endif //PINGCPP_PING_HPP
