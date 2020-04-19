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
#define MAX_PACKET_LENGTH   64
#endif

#define FLOOD 1
#define QUIET 2
#define ADAPTIVE 4

using namespace std;


//struct declaration for enabling additional features for ping.
struct Options {
    int deadline = 0;
    int timeout = 10;
    int timeToLive = 64;
    int interval = 1000000;  //time between packets in milliseconds
    int maxCount = 0;
    int packet_size = 64;
    bool ipv6 = false;
};

struct Packet {
    struct icmphdr hdr;

    char msg[MAX_PACKET_LENGTH];
};

bool executePing = true;

void finish(int);
void setIP(char *host, char *ip, int &ipType, int &icmpType);
void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr *pingTarget, int targetSize);
unsigned short checksum(void *b, int len);

int main() {
    int echoSocket;
    char ip[INET6_ADDRSTRLEN];
    int ipType, icmpType;
    char host[] = "duckduckgo.com";
    int pingSettingsOn = 0;
    struct Options pingSettings;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

    // load ip, ip format, and proper ICMP proto for the format
    setIP(host, ip, ipType, icmpType);

    //load appropriate address variable based on received IP type
    if(ipType == AF_INET){
        inet_pton(ipType, ip, &(sa.sin_addr));
        sa.sin_family = ipType;
    } else {
        inet_pton(ipType, ip, &(sa6.sin6_addr));
        sa6.sin6_family = ipType;
    }

    // generate appropriate raw socket for ICMP requests
    echoSocket = socket(ipType, SOCK_RAW, icmpType);

    // set up interrupt to end the endless loop
    signal(SIGINT, finish);

//    cout<<ip<<endl;

    if(icmpType == IPPROTO_ICMP) {
        pingAddr(echoSocket, host, ip, pingSettingsOn, pingSettings, (struct sockaddr *) &sa, sizeof(sa));
    } else {
        pingSettings.ipv6 = true;
        pingAddr(echoSocket, host, ip, pingSettingsOn, pingSettings, (struct sockaddr *) &sa6, sizeof(sa6));
    }

}

/**********************************************************************************************************
 * Function to end the endless ping loop upon receiving an interrupt signal
 **********************************************************************************************************/
void finish(int code){
    executePing = false;
}


/**********************************************************************************************************
 * Function to convert the passed host name to an IP address, its type and the corresponding ICMP protocol
 * type.
 * @param host
 * @param ip
 * @param ipType
 * @param icmpType
 **********************************************************************************************************/
void setIP(char *host, char *ip, int &ipType, int &icmpType) {
    int status;                  // variable to hold error flag for debugging
    struct addrinfo ipSettings;  // settings for getaddrinfo funciont
    char res[INET6_ADDRSTRLEN];
    struct addrinfo *target;

    //init struct to 0s
    memset(&ipSettings, 0, sizeof(ipSettings));

    // change this line to enable Ipv6 ips (not functional yet)
    ipSettings.ai_family = AF_INET;

    ipSettings.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(host, 0, &ipSettings, &target)) != 0) {
       fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
       exit(1);
    }

    // load ip address character array
    if (target->ai_family == AF_INET){
       inet_ntop(AF_INET, &((struct sockaddr_in*)target->ai_addr)->sin_addr, res, sizeof res);
    } else{
       inet_ntop(AF_INET6, &((struct sockaddr_in6*)target->ai_addr)->sin6_addr, res, sizeof res);
    }

    // load format of ip address and corresponding ICMP protocols
    ipType = target->ai_family;
    if(ipType == AF_INET){
       icmpType = IPPROTO_ICMP;
    } else {
       icmpType = IPPROTO_ICMPV6;
    }

    strcpy(ip, res);
}

void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr *pingTarget, int targetSize) {
    struct timespec pingStart, pingEnd, curStart, curEnd;    // structs for timing ping process
    struct Packet packet;                                    // packet struct
    struct sockaddr_in returnAddr;
    struct sockaddr_in6 returnAddr6;
    struct timeval timeOut;
    int count;

    long double rtt_msec = 0., rtt_avg_msec = 0., total_msec = 0., total_time = 0.;
    double elapsed;

    int msgCount = 0, msgRecvCount = 0, addressLength,
                msgLength = pingSettings.packet_size - sizeof(icmphdr);

    timeOut.tv_sec = pingSettings.timeout;
    timeOut.tv_usec = 0;

    if(setsockopt(sckt, SOL_IP, IP_TTL, &pingSettings.timeToLive, sizeof(pingSettings.timeToLive)) != 0){
        cout<<"sockoptfail"<<endl;
        return;
    } else {
        cout << "\nSocket set to TTL..." << endl;
    }
    setsockopt(sckt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOut, sizeof(timeOut));


//    clock_gettime(CLOCK_MONOTONIC, &pingStart);
    while(executePing){ // 1187016144
        bool packetSent = true;
        bool packetReceived = true;
        memset(&packet, 0, sizeof(packet));

        if(pingSettings.ipv6){
           packet.hdr.type = 128;
        } else {
           packet.hdr.type = ICMP_ECHO;
        }
        packet.hdr.un.echo.id = getpid();

        for (int i = 0; i < msgLength - 1; ++i) {
           packet.msg[i] = i + '0';
        }

        packet.msg[msgLength] = 0;
        packet.hdr.un.echo.sequence = msgCount++;
        packet.hdr.checksum = checksum(&packet, sizeof(packet));

        usleep(pingSettings.interval);
        clock_gettime(CLOCK_MONOTONIC, &curStart);
        count = sendto(sckt, &packet, sizeof(packet), 0, pingTarget, targetSize);
        cout<<count<<endl;
        if(count <= 0){
            cout<<"\nPacket Sending Failure "<<count<<endl;
            fprintf(stderr, "send error: %d\n", errno);
            packetSent = false;
        }


        if(pingSettings.ipv6){
            addressLength = sizeof(returnAddr6);
            count = recvfrom(sckt, &packet, sizeof(packet), 0, (struct sockaddr*)&returnAddr6, (socklen_t *)&addressLength);
            if(count <= 0 && msgCount > 1) {
                packetReceived = false;
                cout<<"\nPacket receive failed "<<count<<endl;
                fprintf(stderr, "receive error: %d\n", errno);
            }
        } else{
            addressLength = sizeof(returnAddr);
            count = recvfrom(sckt, &packet, sizeof(packet), 0, (struct sockaddr*)&returnAddr, (socklen_t *)&addressLength);
            if(count <= 0 && msgCount > 1) {
                packetReceived = false;
                cout<<"\nPacket receive failed "<<count<<endl;
                fprintf(stderr, "receive error: %d\n", errno);
            }
        }
        if(packetReceived) {
            cout<<count<<endl;
            msgRecvCount++;
            clock_gettime(CLOCK_MONOTONIC, &curEnd);

            elapsed = ((double)(curEnd.tv_nsec-curStart.tv_nsec))/1000000.;

            rtt_msec = (curEnd.tv_sec - curStart.tv_sec) * 1000. + elapsed;
            total_msec += rtt_msec;
            rtt_avg_msec = total_msec / msgRecvCount;

            if(packetSent){
                if(!(packet.hdr.type == 69 && packet.hdr.code == 0)){
                    printf("Error, TCMP type %d code %d\n", packet.hdr.type, packet.hdr.code);
                } else {
                    cout<<pingSettings.packet_size<<" bytes from "<<host<<" ("<<ip<<") message seq = "<<msgCount
                        <<" ttl = "<<pingSettings.timeToLive<<" rtt = "<<rtt_msec<<" ms."<<endl;
                    msgRecvCount++;
                }
            }
        }

        if(msgCount == pingSettings.maxCount && pingSettings.maxCount != 0){
            executePing = false;
        }

    }

//    clock_gettime(CLOCK_MONOTONIC, &pingEnd);
    elapsed = ((double)(pingEnd.tv_nsec-pingStart.tv_nsec))/1000000.;
    total_time = elapsed + (1000.*(pingEnd.tv_sec-pingStart.tv_sec));
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