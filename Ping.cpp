/******************************************************************************
 * Created by Joshua Jones on 4/18/2020.
 * Copyright (c) 2020 
 * All rights reserved.
 ******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>


#define FLOOD 1
#define QUIET 2
#define ADAPTIVE 4

using namespace std;

struct Options {
    int deadline = 0;
    int timeout = 1;
    int timeToLive = 64;
    int interval = 1000;  //time between packets in milliseconds
    int maxCount = 0;
    int packet_size = 64;
};

struct Packet {
    struct icmphdr hdr;
    char *msg;
};

bool executePing = true;

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType);
void pingAddr(char* ip, const int &settings, struct Options &pingSettings);
unsigned short checksum(void *b, int len);

int main() {
    int echoSocket;
    char ip[INET6_ADDRSTRLEN];
    int ipType, icmpType;
    char host[] = "google.com";
    int pingSettingsOn = 0;
    struct Options pingSettings;

    setIP(host, ip, ipType, icmpType);

    struct timespec pingStart, pingEnd;

    echoSocket = socket(ipType, SOCK_RAW, icmpType);
    cout<<ip<<endl;

    int num = 0;
    for(unsigned int i = 65535; i < 100000; i++){
        checksum(&i, sizeof(i));
    }

    signal(SIGINT, finish);
    clock_gettime(CLOCK_MONOTONIC, &pingStart);
//    pingAddr(ip, pingSettingsOn, pingSettings);
    clock_gettime(CLOCK_MONOTONIC, &pingEnd);

}

void finish(int code){
    executePing = false;
}

void setIP(char *host, char *ip, int &ipType, int &icmpType) {
    int status;
    struct addrinfo ipSettings;
    struct addrinfo *target;
    char res[INET6_ADDRSTRLEN];

    memset(&ipSettings, 0, sizeof ipSettings); // make sure the struct is empty
    ipSettings.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    ipSettings.ai_socktype = SOCK_STREAM; // TCP stream sockets
    if ((status = getaddrinfo(host, "3490", &ipSettings, &target)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    if (target->ai_family == AF_INET){
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)target->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), res, sizeof res);
    } else{
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)target->ai_addr;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), res, sizeof res);
    }

    if(ipType == AF_INET){
        icmpType = IPPROTO_ICMP;
    } else {
        icmpType = IPPROTO_ICMPV6;
    }

    strcpy(ip, res);
}

void pingAddr(char* ip, const int &settings, struct Options &pingSettings){
    struct timespec curStart, curEnd;

}

unsigned short checksum(void *b, int len){
    auto *ref = (unsigned short *)b;
    unsigned short result;
    unsigned int sum = 0;

    // sequential words of data in the packet
    while(len > 1){
        sum += *ref;
        ref++;
        len-=2;
    }

    if(len == 1){ //handle remaining byte of data if present
        sum += *(char *)ref;
    }
    cout<<sum<<" ";
    //reduce sum to 16 bits
    sum = (sum >> 16) + (sum & 0xffff);
    cout<<sum<<" ";
    sum += (sum >> 16);
    cout<<sum<<endl;
    result = ~sum;
    return result;
}