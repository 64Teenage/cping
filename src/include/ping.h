#ifndef _PING_H_
#define _PING_H_

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/ip_icmp.h>
#include <mutex>
#include <condition_variable>

#define PACKET_SEND_MAX_NUM     64

struct PingPacket {
    bool    m_bGet;
    short   m_wSeq;
    struct timeval  m_tvBegin;
    struct timeval  m_tvEnd;
};

struct PingSync {
    //pthread_mutex_t mutex;
    //pthread_cond_t  cond;
    
    std::mutex              m_mutex;
    std::condition_variable m_cond;
    unsigned int            nready;
};

struct PingStatus {
    bool    m_bStatus;
    int     m_iSendCnt;
    int     m_iRecvCnt;
    int     m_iSockfd;
    pid_t   m_pid;
    struct timeval  m_tvBegin;
    struct timeval  m_tvEnd;
    struct timeval  m_tvOffset;
    struct sockaddr_in  m_sockServer;
};


void    icmpPackUp(struct icmp * pstIcmp, int iSeq, unsigned int iLength, pid_t pid);
int     icmpUnPack(char * psStream, int iLength, pid_t pid);

unsigned short  checkSum(unsigned short *pwBuff, const unsigned int dwLength);

struct timeval  timeOffset(struct timeval tv_begin, struct timeval tv_end);

void *  pingSend(void *);
void *  pingRecv(void *);
void    icmpSigInt(int signo);

extern struct PingStatus CPingStatus;
extern struct PingPacket CPingPacket[PACKET_SEND_MAX_NUM];
extern struct PingSync   CPingSync;

#endif