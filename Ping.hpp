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
//#include "ip_icmp.h"
#include <netinet/ip_icmp.h>
#include <unistd.h>

#ifndef MAX_PACKET_LENGTH
#define MAX_PACKET_LENGTH   64
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

struct Options {
    int deadline = 0;           // number of milliseconds to run ping for, 0 for unlimited
    double timeout = 1000.;     // time to wait for a response in milliseconds
    int timeToLive = 64;        // maximum number of nodes packet is alloed to travel
    int interval = 1000000;     // time between packets in microseconds
    int maxCount = 0;           // maximum number of packets to send before ending loop, 0 for unlimited
    int packet_size = 64;       // number of bytes to allocate to the packet
    bool ipv6 = false;          // flag for ipv6 ips
};

struct icmpPacket {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
    char msg[MAX_PACKET_LENGTH - 8];
};

struct ipv6 {
    BYTE vtf[4];
    unsigned short p_len;
    BYTE nxt_hdr;
    BYTE ttl;
    BYTE src[16];
    BYTE dst[16];
};

struct icmpv4 {
    struct ip iphdr;
    struct icmpPacket packet;
};

struct icmpv6 {
    struct ipv6 iphdr;
    struct icmpPacket packet;
};

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType);
void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr *pingTarget, int targetSize);
unsigned short checksum(void *b, int len);
void setSessionOption(char opt, const char *value, struct Options &settings);
void loadIPHeader(struct ip *hdr, struct Options *pingSettings, const char *ip);
void loadIPHeader(struct ipv6 *hdr, struct Options *pingSettings);
void getAddrFromIP(const char *ip, int ipType, BYTE *resAddr);

#endif //PINGCPP_PING_HPP
