#include <stdio.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

#include "ping.h"

struct PingPacket   CPingPacket[PACKET_SEND_MAX_NUM];
struct PingStatus   CPingStatus;
struct PingSync     CPingSync = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PACKET_SEND_MAX_NUM};

int main(int argc, char ** argv)
{
    int         iState;
    pthread_t   pid_Send, pid_Recv;
    in_addr_t   inaddr;
    struct in_addr      inaddrServer;
    struct sockaddr_in  serverAddr;

    struct protoent * protocol = getprotobyname("icmp");
    if (protocol == NULL)
    {
        printf("Fail to get ICMP Protocol!\n");
        return -1;
    }

    CPingStatus.m_iSockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto);
    if (CPingStatus.m_iSockfd == -1)
    {
        printf("Create Socket Fail!\n");
        return -1;
    }

    socklen_t   socklen = 128*1024;
    setsockopt(CPingStatus.m_iSockfd, SOL_SOCKET, SO_RCVBUF, &socklen, sizeof(socklen));
    inaddr = inet_addr(argv[1]);
    if (inaddr == INADDR_NONE)
    {
        struct hostent * host = gethostbyname(argv[1]);
        inaddrServer = *(struct in_addr *)host->h_addr;
    }
    else
        inet_aton(argv[1], &inaddrServer);

    bzero(&CPingStatus.m_sockServer, 0);
    CPingStatus.m_sockServer.sin_addr.s_addr = (inaddrServer.s_addr);
    CPingStatus.m_sockServer.sin_family = AF_INET;

    printf("Ping %s, 56(24) bytes of data.\n", inet_ntoa(inaddrServer));

    CPingStatus.m_bStatus = true;
    CPingStatus.m_pid = getpid();
    signal(SIGINT, icmpSigInt);

    iState = pthread_create(&pid_Send, NULL, pingSend, NULL);
    if (iState == -1)
    {
        printf("Fail to create Ping Send Thread!\n");
        return -1;
    }

    iState = pthread_create(&pid_Recv, NULL, pingRecv, NULL);
    if (iState == -1)
    {
        printf("Fail to create Ping Receive Thread!\n");
        return -1;
    }

    pthread_join(pid_Send, NULL);
    pthread_join(pid_Recv, NULL);

    printf("%d packets transmitted, %d packet recieved, %d packets loss, time %ld ms\n", CPingStatus.m_iSendCnt,
    CPingStatus.m_iRecvCnt, CPingStatus.m_iSendCnt-CPingStatus.m_iRecvCnt, CPingStatus.m_tvOffset.tv_sec*1000+CPingStatus.m_tvOffset.tv_usec/1000);


    close(CPingStatus.m_iSockfd);
    return 0;
}