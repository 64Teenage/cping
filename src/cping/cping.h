#ifndef CPING_H
#define CPING_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <vector>
#include <string>

#include <errno.h>
#include <sys/types.h>
#include <netinet/ip_icmp.h>

#include "cping.h"
#include "../ctime/ctime.h"

class CPing
{
private:
    unsigned int            m_ready;
    std::mutex              m_mutex;
    std::condition_variable m_cond;

    std::vector<CPingPack>  m_pingPack;

    std::string m_strDest;

    bool        m_status;
    int         m_iSendCnt;
    int         m_iReceCnt;
    int         m_iSockfd;
    pid_t       m_pid;
    CTime       m_timeBeg;
    CTime       m_timeEnd;
    CTime       m_timeOffSet;
    struct sockaddr_in  m_sockServer;

private:
    CPing(){}
    CPing(const CPing & );

public:
    static  CPing *  getInstance();
    void    initialize(const std::string &);
    void    start();

    void    icmpPackUp(struct icmp * pstIcmp, unsigned int iLength);
    int     icmpUnPack(char * psStream, int iLength);

    unsigned short  checkSum(unsigned short *pwBuff, const unsigned int dwLength);

    void    pingSend(void);
    void    pingRecv(void);
    static void    icmpSigInt(int signo);  

};

#endif // CPING_H
