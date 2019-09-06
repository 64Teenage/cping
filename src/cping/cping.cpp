#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "cping.h"

using namespace std;


const int PACKET_SEND_MAX_NUM = 64;

CPing * CPing::getInstance() {
    static CPing cping;
    return &cping;
}

void    CPing::initialize(const std::string & strDest) {
    m_strDest = strDest;

    struct protoent * protocol = getprotobyname("icmp");

    m_iSockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto);
    if (m_iSockfd == -1){
        printf("Create Socket Fail!\n");
        return ;
    }

    socklen_t   socklen = 128*1024;
    setsockopt(m_iSockfd, SOL_SOCKET, SO_RCVBUF, &socklen, sizeof(socklen));
    in_addr_t   inaddr = inet_addr(m_strDest.c_str());
    struct in_addr      inaddrServer;
    if (inaddr == INADDR_NONE)
    {
        struct hostent * host = gethostbyname(m_strDest.c_str());
        inaddrServer = *(struct in_addr *)host->h_addr;
    }
    else
        inet_aton(m_strDest.c_str(), &inaddrServer);

    bzero(&m_sockServer, sizeof(m_sockServer));
    m_sockServer.sin_addr.s_addr = (inaddrServer.s_addr);
    m_sockServer.sin_family = AF_INET;

    m_ready = PACKET_SEND_MAX_NUM;
    m_pid = getpid();
    m_status = true;
    m_pingPack = vector<CPingPack>(PACKET_SEND_MAX_NUM, CPingPack());
    
}

void    CPing::start() {

    printf("Ping %s, 56(24) bytes of data.\n", inet_ntoa(m_sockServer.sin_addr));
    signal(SIGINT, icmpSigInt);
    std::thread pingSendThread(&CPing::pingSend, this);
    std::thread pingReceThread(&CPing::pingRecv, this);

    pingSendThread.join();
    pingReceThread.join();

    
}

void    CPing::icmpPackUp(struct icmp * pstIcmp, unsigned int dwLength){
    pstIcmp->icmp_type = ICMP_ECHO;
    pstIcmp->icmp_code = 0;
    pstIcmp->icmp_cksum = 0;
    pstIcmp->icmp_seq = m_iSendCnt;
    pstIcmp->icmp_id = m_pid & 0xffff;

    for (unsigned int index=0; index<dwLength; ++index)
        pstIcmp->icmp_data[index] = index;

    pstIcmp->icmp_cksum = checkSum((unsigned short *)pstIcmp, dwLength);
}

unsigned short  CPing::checkSum(unsigned short *pwBuff, const unsigned int dwLength){
    int     iSum = 0;
    bool    bOdd = dwLength & 0x0;

    for (int pos=0; pos<dwLength/2; ++pos)
        iSum += pwBuff[pos];

    if (bOdd)
        iSum += ((((*(pwBuff+dwLength/2)) & 0x00ff)<<8) & 0xff00);

    iSum = (iSum >> 16) + (iSum & 0xffff);
    iSum += (iSum>>16);

    return ~iSum;
}


int     CPing::icmpUnPack(char * psStream, int iLength){
    struct ip * pstIp = (struct ip *)psStream;
    int     iHeadLen = pstIp->ip_hl * 4;
    struct icmp *pstIcmp = (struct icmp *)(psStream+iHeadLen);

    iLength -= iHeadLen;
    if (iLength<2)
    {
        fprintf(stderr, "Invalid ICMP Packet!\n");
        return -1;
    }

    if (pstIcmp->icmp_type == ICMP_ECHOREPLY && pstIcmp->icmp_id == (m_pid & 0xffff))
    {
        CTime timeBeg, timeEndg, timeOffSet;
       {
           std::unique_lock<std::mutex> syncMutex(m_mutex);
           for (int pos=0; pos<PACKET_SEND_MAX_NUM; ++pos)
            if (m_pingPack[pos].getStatus() && m_pingPack[pos].getSeqID() == pstIcmp->icmp_seq)
            {
                m_pingPack[pos].setSeqID(false);
                timeBeg = m_pingPack[pos].getBegTime();
                break;
            }
            if (m_ready == 0)
                m_cond.notify_one();
            ++m_ready;
       }
        
        timeEndg.gettimeofday();
        timeOffSet = timeEndg - timeBeg;


        printf("%d bytes from %s : ICMP_SEQ = %u TTL = %d RTT = %d ms\n", iLength, inet_ntoa(pstIp->ip_src), pstIcmp->icmp_seq, pstIp->ip_ttl, timeOffSet.getTotalUsec());

    }
    else
    {
        fprintf(stderr, "Invalid ICMP Packet ! It doesn't match!\n");
        return -1;
    }

    return 0;
}

void    CPing::pingSend(void)
{
    char pszSendBuff[128];
    memset(pszSendBuff, 0, sizeof(pszSendBuff));
    //gettimeofday(&CPingStatus.m_tvBegin, NULL);
    m_timeBeg.gettimeofday();

    while (m_status)
    {
        
        {
            //避免异常退出导致锁未释放。
            std::unique_lock<std::mutex>  syncMutex(m_mutex);
            while(m_ready == 0)
                m_cond.wait(syncMutex);
            for (int pos=0; pos < PACKET_SEND_MAX_NUM; ++pos)
                if (m_pingPack[pos].getStatus() == false)
                {
                    m_pingPack[pos].setStatus(true);
                    m_pingPack[pos].setSeqID(m_iSendCnt);
                    m_pingPack[pos].setBegTime();
                    break;
                }
            --m_ready;

        }


        icmpPackUp((struct icmp *)pszSendBuff, 64);
        ssize_t iSuccess = sendto(m_iSockfd, pszSendBuff, 64, 0, (struct sockaddr *)&m_sockServer, sizeof(m_sockServer));
        ++m_iSendCnt;
        if (iSuccess<0)
        {
            fprintf(stderr, "Send ICMP Packet Fail!\n");
            continue;
        }
        
        sleep(1);

    }
}

//收包
void   CPing::pingRecv(void )
{
    struct timeval tv_overlap;
    tv_overlap.tv_sec = 10;
    tv_overlap.tv_usec = 20000;

    fd_set  fdRead;
    char    pszRecvBuff[512];
    memset(pszRecvBuff, 0, sizeof(pszRecvBuff));
    while (m_status)
    {
        FD_ZERO(&fdRead);
        FD_SET(m_iSockfd, &fdRead);

        tv_overlap.tv_sec = 5;
        tv_overlap.tv_usec = 100;
        int iState = select(m_iSockfd+1, &fdRead, NULL, NULL, &tv_overlap);

        if (iState == -1)
            fprintf(stderr, "Fail to Select!\n");
        else if (iState == 0)
            fprintf(stderr, "Ping Select timeout!\n");
        else
        {
            int iSize = recv(m_iSockfd, pszRecvBuff, sizeof(pszRecvBuff), 0);
            if (iSize < 0)
            {
                fprintf(stderr, "Receive ICMP Packet Fail!\n");
                //continue;
            }

            iState = icmpUnPack(pszRecvBuff, iSize);
            if (iState == -1)
            {
                fprintf(stderr, "Extract ICMP Packet Fail!\n");
                //continue;
            }

            ++m_iReceCnt;
        }

    }
}

void    CPing::icmpSigInt(int signo)
{
    CPing * handle = CPing::getInstance();
    handle->m_status = false;
    handle->m_timeEnd.gettimeofday();
    handle->m_timeOffSet = handle->m_timeEnd - handle->m_timeBeg;
    //CPingStatus.m_tvOffset = timeOffset(CPingStatus.m_tvBegin, CPingStatus.m_tvEnd);
}