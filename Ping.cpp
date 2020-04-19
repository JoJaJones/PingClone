/******************************************************************************
 * Created by Joshua Jones on 4/18/2020.
 * Copyright (c) 2020
 * All rights reserved.
 ******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#ifndef MAX_PACKET_LENGTH
#define MAX_PACKET_LENGTH   1024
#endif

#define FLOOD 1
#define QUIET 2
#define ADAPTIVE 4

using namespace std;

struct Options {
    int deadline = 0;
    int timeout = 1;
    int timeToLive = 64;
    int interval = 1000000;  //time between packets in milliseconds
    int maxCount = 0;
    int packet_size = 64;
};

struct Packet {
    struct icmphdr hdr;

    char msg[MAX_PACKET_LENGTH];
};

bool executePing = true;

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType);
void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr_in *pingAddr);
unsigned short checksum(void *b, int len);

int main() {
    int echoSocket;
    char ip[INET6_ADDRSTRLEN];
    int ipType, icmpType;
    char host[] = "216.58.195.78";
    int pingSettingsOn = 0;
    struct Options pingSettings;
    struct sockaddr *pingTarget;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;


    setIP(host, ip, ipType, icmpType);

    struct timespec pingStart, pingEnd;

    cout<<(ipType == AF_INET)<<" "<<(icmpType == IPPROTO_ICMP)<<endl;
    if(ipType == AF_INET){
        inet_pton(ipType, ip, &(sa.sin_addr));
        sa.sin_family = ipType;
    } else {
        inet_pton(ipType, ip, &(sa6.sin6_addr));
        sa6.sin6_family = ipType;
    }

    echoSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    cout<<"echoSocket: "<<echoSocket<<" ip: "<<ip<<endl;
    signal(SIGINT, finish);
    clock_gettime(CLOCK_MONOTONIC, &pingStart);
    pingAddr(echoSocket, host, ip, pingSettingsOn, pingSettings, &sa);
    clock_gettime(CLOCK_MONOTONIC, &pingEnd);
    double total_time = 0;
    double elapsed = ((double)(pingEnd.tv_nsec-pingStart.tv_nsec))/1000000.;

    total_time = (pingEnd.tv_sec - pingStart.tv_sec) * 1000. + elapsed;
    cout<<total_time<<endl;
}

void finish(int code){
    executePing = false;
}

void setIP(char *host, char *ip, int &ipType, int &icmpType) {
    int status;
    struct addrinfo ipSettings;
    char res[INET6_ADDRSTRLEN];
    struct addrinfo *target;
    struct hostent *host_entity;

    host_entity = gethostbyname(host);

    memset(&ipSettings, 0, sizeof ipSettings); // make sure the struct is empty
    ipSettings.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    ipSettings.ai_socktype = SOCK_STREAM; // TCP stream sockets
    if ((status = getaddrinfo(host, 0, &ipSettings, &target)) != 0) {
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

    ipType = target->ai_family;
    if(ipType == AF_INET){
        icmpType = IPPROTO_ICMP;
    } else {
        icmpType = IPPROTO_ICMPV6;
    }

    strcpy(ip, res);
}

void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr_in *pingAddr) {
    struct timespec curStart, curEnd;
    struct Packet packet;
    struct sockaddr_in returnAddr;
    long double rtt_msec = 0., rtt_avg_msec = 0., total_msec = 0.;

    int msgCount = 0, msgRecvCount = 0, addressLength,
                 msgLength = pingSettings.packet_size - sizeof(icmphdr);
    struct timeval timeOut;

    timeOut.tv_sec = pingSettings.timeout;
    timeOut.tv_usec = 0;

    if(setsockopt(sckt, SOL_IP, IP_TTL, &pingSettings.timeToLive, sizeof(pingSettings.timeToLive)) != 0){
        cout<<"sockoptfail"<<endl;
        return;
    } else {
        cout << "\nSocket set to TTL..." << endl;
    }
    setsockopt(sckt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOut, sizeof(timeOut));


    struct sockaddr *pingConverted = (struct sockaddr *)pingAddr;
    while(executePing){
        bool packetSent = true;

        bzero(&packet, sizeof(packet));
        packet.hdr.type = ICMP_ECHO;
        packet.hdr.un.echo.id = getpid();

        for (int i = 0; i < msgLength - 1; ++i) {
            packet.msg[i] = i + '0';
        }

        packet.msg[msgLength] = 0;
        packet.hdr.un.echo.sequence = msgCount++;
        packet.hdr.checksum = checksum(&packet, sizeof(packet));

        usleep(pingSettings.interval);
        clock_gettime(CLOCK_MONOTONIC, &curStart);
        if(sendto(sckt, &packet, sizeof(packet), 0, (struct sockaddr*)pingAddr, sizeof(*pingAddr)) <= 0){
            cout<<"\nPacket Sending Failure"<<endl;
            fprintf(stderr, "send error: %d\n", errno);
            packetSent = false;
        }

        addressLength = sizeof(returnAddr);

        if(recvfrom(sckt, &packet, sizeof(packet), 0, (struct sockaddr *)&returnAddr, &addressLength) <= 0
            && msgCount > 1){
            cout<<"\nPacket receive failed"<<endl;
            fprintf(stderr, "receive error: %d\n", errno);
        } else {
            msgRecvCount++;
            clock_gettime(CLOCK_MONOTONIC, &curEnd);

            double elapsed = ((double)(curEnd.tv_nsec-curStart.tv_nsec))/1000000.;

            rtt_msec = (curEnd.tv_sec - curStart.tv_sec) * 1000. + elapsed;
            total_msec += rtt_msec;
            rtt_avg_msec = total_msec / msgRecvCount;

            if(packetSent){
                if(!(packet.hdr.type == 69 && packet.hdr.code == 0)){
                    cout<<"Error, TCMP type "<<packet.hdr.type<<" code "<<packet.hdr.code<<endl;
                } else {
                    cout<<pingSettings.packet_size<<" bytes from "<<host<<" ("<<ip<<") message seq = "<<msgCount
                        <<" ttl = "<<pingSettings.timeToLive<<" rtt = "<<rtt_msec<<" ms."<<endl;
                    msgRecvCount++;
                }
            }
        }

    }



}

unsigned short checksum(void *b, int len){
    unsigned short *ref = (unsigned short *)b;
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
    //reduce sum to 16 bits
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}