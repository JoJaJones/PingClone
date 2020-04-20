/******************************************************************************
 * Created by Joshua Jones on 4/18/2020.
 * Copyright (c) 2020
 * All rights reserved.
 ******************************************************************************/
#include "Ping.hpp"

#define FLOOD 1
#define QUIET 2
#define ADAPTIVE 4

//struct declaration for enabling additional features for ping.


bool executePing = true;



//int main(int argc, char *argv[]) {
int main() {
    int echoSocket;
    char ip[INET6_ADDRSTRLEN];
    int ipType, icmpType;
//    char *host;
    char host[] = "google.com";
    int pingSettingsOn = 0;
    struct Options pingSettings;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

//    if(argc < 2){
//        std::cout<<"usage: ./ping <hostname>"<<std::endl;
//        return -1;
//    }

//    host = argv[1];
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
//    for (int i = 2; i < argc; ++i) {
//        if(argv[i][0] == '-'){
//            //set
//        }
//    }

    // set up interrupt to end the std::endless loop
    signal(SIGINT, finish);

    std::cout<<ip<<std::endl;

    // begin pinging the target address
    if(icmpType == IPPROTO_ICMP) {
        sa.sin_addr.s_addr = htonl(sa.sin_addr.s_addr);
        pingAddr(echoSocket, host, ip, pingSettingsOn, pingSettings, (struct sockaddr *) &sa, sizeof(sa));
    } else {
        pingSettings.ipv6 = true;
        pingAddr(echoSocket, host, ip, pingSettingsOn, pingSettings, (struct sockaddr *) &sa6, sizeof(sa6));
    }

}

/**********************************************************************************************************
 * Function to end the std::endless ping loop upon receiving an interrupt signal
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

/**********************************************************************************************************
 * Function to send and recieve ICMP requests and track relevant data about the process
 * @param sckt
 * @param host
 * @param ip
 * @param settings
 * @param pingSettings
 * @param pingTarget
 * @param targetSize
 **********************************************************************************************************/
void pingAddr(int sckt, char *host, char *ip, const int &settings, struct Options &pingSettings,
              struct sockaddr *pingTarget, int targetSize) {
    struct timespec pingStart, pingEnd, curStart, curEnd;    // structs for timing ping process
    struct Packet packetOut, packetIn;                                    // packet struct
    struct sockaddr_in returnAddr;
    struct sockaddr_in6 returnAddr6;
    struct timeval timeOut;
    int count;

    long double rtt_msec = 0., rtt_avg_msec = 0., total_msec = 0., total_time = 0.;
    double elapsed;

    unsigned short msgCount = 0;
    int msgRecvCount = 0, addressLength,
            msgLength = pingSettings.packet_size - 8;

    // set timeout time from pingSettings struct
    timeOut.tv_sec = (int)(pingSettings.timeout / 1000);
    timeOut.tv_usec = (int)(pingSettings.timeout * 1000)%1000000;

    // set time to live for packets
//    if(setsockopt(sckt, IPPROTO_IP, IP_TTL, &pingSettings.timeToLive, sizeof(pingSettings.timeToLive)) != 0){
//        std::cout<<"sockoptfail"<<std::endl;
//        return;
//    } else {
//        std::cout << "\nSocket set to TTL..." << std::endl;
//    }

    // set timeout for receiving a response
    if (setsockopt (sckt, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(timeOut)) < 0) {
        std::cout<<"Setting Receive Time Out Failed"<<std::endl;
    }

    if (setsockopt (sckt, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeOut, sizeof(timeOut)) < 0) {
        std::cout<<"Setting Send Time Out Failed"<<std::endl;
    }

    std::cout<<sckt<<std::endl;
    clock_gettime(CLOCK_MONOTONIC, &pingStart);
    while(executePing){
        bool packetSent = true;
        bool packetReceived = true;

        //initialize the outbound packet
        memset(&packetOut, 0, sizeof(packetOut));

        if(pingSettings.ipv6){
            packetOut.type = 128;
        } else {
            packetOut.type = ICMP_ECHO;
        }
        packetOut.code = 0;
        packetOut.id = getpid();
        packetOut.seq = msgCount++;
        packetOut.checksum = 0;

        for (int i = 0; i < msgLength - 1; ++i) {
            packetOut.msg[i] = i + '0';
        }
        packetOut.checksum = checksum(&packetOut, sizeof(packetOut));

        packetOut.msg[msgLength] = 0;

        usleep(pingSettings.interval);
        clock_gettime(CLOCK_MONOTONIC, &curStart);

        // send packet and detect errors in sending
        count = sendto(sckt, &packetOut, sizeof(packetOut), 0, pingTarget, targetSize);
        if(count <= 0){
            std::cout<<"\nFailed to send packet, error number: "<<strerror(errno)<<std::endl;
            packetSent = false;
        }

        //attempt tp receive packet
        if(pingSettings.ipv6){
            addressLength = sizeof(returnAddr6);
            count = recvfrom(sckt, &packetIn, sizeof(packetIn), 0, (struct sockaddr*)&returnAddr6, (socklen_t *)&addressLength);
            if(count <= 0 && msgCount > 1) {
                packetReceived = false;
            } else {
                msgRecvCount++;
            }
        } else{
            addressLength = sizeof(returnAddr);
            count = recvfrom(sckt, &packetIn, sizeof(packetIn), 0, (struct sockaddr*)&returnAddr, (socklen_t *)&addressLength);
            if(count <= 0) {
                std::cout<<count<<" "<<strerror(errno)<<std::endl;
                packetReceived = false;
            } else {
                msgRecvCount++;
            }
        }

        // if successfully received packet calculate rtt and format display
        if(packetReceived) {
            clock_gettime(CLOCK_MONOTONIC, &curEnd);

            elapsed = ((double)(curEnd.tv_nsec-curStart.tv_nsec))/1000000.;

            rtt_msec = (curEnd.tv_sec - curStart.tv_sec) * 1000. + elapsed;
            total_msec += rtt_msec;
            rtt_avg_msec = total_msec / msgRecvCount;

            // if a packet was successfully sent output relevate results of the receive operation
            if(packetSent){
                if(!(packetIn.type == 0 && packetIn.code == 0)){
                    printf("Error, TCMP type %d code %d\n", packetOut.type, packetOut.code);
                } else if (executePing) {
                    std::cout<<pingSettings.packet_size<<" bytes from "<<host<<" ("<<ip<<") message seq = "<<msgCount
                        <<" ttl = "<<pingSettings.timeToLive<<" rtt = "<<rtt_msec<<" ms."<<std::endl;
                }
            }
        } else {
            std::cout<<pingSettings.packet_size<<" bytes from "<<host<<" ("<<ip<<") message seq = "<<msgCount
                <<" packet lost."<<std::endl;
        }

        // calc time elapsed so far for deadline check
        clock_gettime(CLOCK_MONOTONIC, &pingEnd);
        elapsed = ((double)(pingEnd.tv_nsec-pingStart.tv_nsec))/1000000.;
        total_time = elapsed + (1000.*(pingEnd.tv_sec-pingStart.tv_sec));

        if((msgCount == pingSettings.maxCount && pingSettings.maxCount != 0) ||
           (total_time > pingSettings.deadline && pingSettings.deadline != 0)){
            executePing = false;
        }

    }

    // calculate total time elapsed and output session statistics
    clock_gettime(CLOCK_MONOTONIC, &pingEnd);
    elapsed = ((double)(pingEnd.tv_nsec-pingStart.tv_nsec))/1000000.;
    total_time = elapsed + (1000.*(pingEnd.tv_sec-pingStart.tv_sec));
    for (int j = 0; j < 50; ++j) {
        std::cout<<"-";
    }
    std::cout<<std::endl;
    std::cout<<" Ping session summary:\n\t "
             <<msgCount<<" packets sent. "<<msgRecvCount<<" packets received."
             <<100.-(100.*msgRecvCount/msgCount)<<"% packets lost.\n\t "
             <<"Average rtt: "<<rtt_avg_msec<<"ms. "
             <<"Total time elapsed: "<<total_time<<"ms."<<std::endl;
}

/**********************************************************************************************************
 * Function to create checksum value for simple data validity checking.
 *
 * @param b
 * @param len
 * @return
 **********************************************************************************************************/
unsigned short checksum(void *b, int len){
    unsigned short *ref = (unsigned short *)b;
    unsigned short result;
    unsigned int sum = 0;

    sum += htons(*ref);
    ref++;
    len-=2;

    // sequential words of data in the packet
    while(len > 1){
        sum += *ref;
        ref++;
        len-=2;
    }

    if(len == 1){ //handle remaining byte of data if present
        unsigned short num;
        *(char*)&num = *(char *)ref;
        sum += num;
    }
    //reduce sum to 16 bits
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

