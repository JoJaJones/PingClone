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

    char *msg;
};

bool executePing = true;

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType, struct addrinfo *&target);
void pingAddr(const int &sckt, char *ip, const int &icmpType, const int &settings, struct Options &pingSettings,
              sockaddr *pingAddr);
unsigned short checksum(void *b, int len);
void initPacket(int packetSize, struct Packet &pkt);
void freePacket(struct Packet &pkt);

int main() {
    int echoSocket;
    char ip[INET6_ADDRSTRLEN];
    int ipType, icmpType;
    char host[] = "192.1.1.1";
    int pingSettingsOn = 0;
    struct Options pingSettings;
    struct addrinfo *pingTarget;

    setIP(host, ip, ipType, icmpType, pingTarget);

    struct timespec pingStart, pingEnd;

    echoSocket = socket(ipType, SOCK_RAW, icmpType);

    cout<<"echoSocket: "<<echoSocket<<endl;

    signal(SIGINT, finish);
    clock_gettime(CLOCK_MONOTONIC, &pingStart);
    pingAddr(echoSocket, ip, icmpType, pingSettingsOn, pingSettings, pingTarget->ai_addr);
    clock_gettime(CLOCK_MONOTONIC, &pingEnd);

}

void finish(int code){
    executePing = false;
}

void setIP(char *host, char *ip, int &ipType, int &icmpType, struct addrinfo *&target) {
    int status;
    struct addrinfo ipSettings;
    char res[INET6_ADDRSTRLEN];

    memset(&ipSettings, 0, sizeof ipSettings); // make sure the struct is empty
    ipSettings.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    ipSettings.ai_socktype = SOCK_STREAM; // TCP stream sockets
    if ((status = getaddrinfo(host, "0000", &ipSettings, &target)) != 0) {
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

void pingAddr(const int &sckt, char *ip, const int &icmpType, const int &settings, struct Options &pingSettings,
              sockaddr *pingAddr) {
    struct timespec curStart, curEnd;
    struct Packet packet;
    struct sockaddr_in returnAddr;
    long double rtt_msec = 0., rtt_avg_msec = 0., total_msec = 0.;

    int ttl_val = 64, status;

    unsigned int msgCount = 0, msgRecvCount = 0, addressLength,
                 msgLength = pingSettings.packet_size - sizeof(icmphdr);
    struct timeval timeOut;

    timeOut.tv_sec = pingSettings.timeout;
    timeOut.tv_usec = 0;

    initPacket(pingSettings.packet_size, packet);
    if((status = setsockopt(sckt, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val))) != 0){
        cout<<"sockoptfail"<<endl;
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return;
    } else {
        cout<<"\nSocket set to TTL..."<<endl;
    }

    cout<<"break 1"<<endl;

    setsockopt(sckt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOut, sizeof(timeOut));

    cout<<"break 2"<<endl;

    while(executePing){
        bool packetSent = true;

        bzero(&packet, sizeof(packet));

        packet.hdr.type = ICMP_ECHO;
        packet.hdr.un.echo.id = getpid();

        for (int i = 0; i < msgLength - 2; ++i) {
            packet.msg[i] = i + '0';
        }

        packet.msg[msgLength - 1] = 0;
        packet.hdr.un.echo.sequence = msgCount++;
        packet.hdr.checksum = checksum(&packet, sizeof(packet));

        usleep(pingSettings.interval);
        clock_gettime(CLOCK_MONOTONIC, &curStart);
        if(sendto(sckt, &packet, sizeof(packet), 0, pingAddr, sizeof(pingAddr)) < 1){
            cout<<"\nPacket Sending Failure"<<endl;
        }

        addressLength = sizeof(returnAddr);

        if(recvfrom(sckt, &packet, sizeof(packet), 0, (struct sockaddr *)&returnAddr, (socklen_t *)&addressLength) < 1
            && msgCount > 1){
            cout<<"\nPacket receive failed"<<endl;
        } else {
            msgRecvCount++;
            clock_gettime(CLOCK_MONOTONIC, &curEnd);

            double elapsed = ((double)(curEnd.tv_nsec-curStart.tv_nsec))/1000000.;

            rtt_msec = (curEnd.tv_sec - curStart.tv_sec) * 1000. + elapsed;
            total_msec += rtt_msec;
            rtt_avg_msec = total_msec / msgRecvCount;
        }
    }


    freePacket(packet);
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
    cout<<sum<<" ";
    //reduce sum to 16 bits
    sum = (sum >> 16) + (sum & 0xffff);
    cout<<sum<<" ";
    sum += (sum >> 16);
    cout<<sum<<endl;
    result = ~sum;
    return result;
}

void initPacket(int packetSize, struct Packet &pkt){
    pkt.msg = new char[packetSize - sizeof(struct icmphdr)];
}

void freePacket(struct Packet &pkt){
    delete [] pkt.msg;
}
