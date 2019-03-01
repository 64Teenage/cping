#include "ping.h"
#include <stdio.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

/*
struct PingStatus CPingStatus;
struct PingPacket CPingPacket[PACKET_SEND_MAX_NUM];
*/

//根据ICMP协议填充相应字段
void    icmpPackUp(struct icmp * pstIcmp, int iSeq, unsigned int dwLength, pid_t pid)
{
    pstIcmp->icmp_type = ICMP_ECHO;
    pstIcmp->icmp_code = 0;
    pstIcmp->icmp_cksum = 0;
    pstIcmp->icmp_seq = iSeq;
    pstIcmp->icmp_id = pid & 0xffff;

    for (unsigned int index=0; index<dwLength; ++index)
        pstIcmp->icmp_data[index] = index;

    pstIcmp->icmp_cksum = checkSum((unsigned short *)pstIcmp, dwLength);
}

//检验和计算
unsigned short  checkSum(unsigned short *pwBuff, const unsigned int dwLength)
{
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

//数据解包
int         icmpUnPack(char * psStream, int iLength, pid_t pid)
{
    struct ip * pstIp = (struct ip *)psStream;
    int     iHeadLen = pstIp->ip_hl * 4;
    struct icmp *pstIcmp = (struct icmp *)(psStream+iHeadLen);
    struct timeval  tv_begin, tv_recv, tv_offset;

    iLength -= iHeadLen;
    if (iLength<2)
    {
        fprintf(stderr, "Invalid ICMP Packet!\n");
        return -1;
    }

    if (pstIcmp->icmp_type == ICMP_ECHOREPLY && pstIcmp->icmp_id == (pid & 0xffff))
    {
        /*
        if (pstIcmp->icmp_seq < 0 || pstIcmp->icmp_seq >= PACKET_SEND_MAX_NUM)
        {
            fprintf(stderr, "ICMP Packet Seq is out of range !\n");
            return -1;
        }
        */

        /*pthread_mutex_lock(&CPingSync.mutex);
        for (int pos=0; pos<PACKET_SEND_MAX_NUM; ++pos)
            if (CPingPacket[pos].m_bGet && CPingPacket[pos].m_wSeq == pstIcmp->icmp_seq)
            {
                tv_begin = CPingPacket[pos].m_tvBegin;
                CPingPacket[pos].m_bGet = false;
                break;
            }
        if (CPingSync.nready == 0)
            pthread_cond_signal(&CPingSync.cond);
        ++CPingSync.nready;
        pthread_mutex_unlock(&CPingSync.mutex);
        */
       {
           //避免异常造成的锁未释放。
           std::unique_lock<std::mutex> syncMutex(CPingSync.m_mutex);
           for (int pos=0; pos<PACKET_SEND_MAX_NUM; ++pos)
            if (CPingPacket[pos].m_bGet && CPingPacket[pos].m_wSeq == pstIcmp->icmp_seq)
            {
                tv_begin = CPingPacket[pos].m_tvBegin;
                CPingPacket[pos].m_bGet = false;
                break;
            }
            if (CPingSync.nready == 0)
                CPingSync.m_cond.notify_one();
            ++CPingSync.nready;
       }
        
        gettimeofday(&tv_recv, NULL);
        tv_offset = timeOffset(tv_begin, tv_recv);

        int iRtt = tv_offset.tv_sec * 1000 + tv_offset.tv_usec/1000;

        printf("%d bytes from %s : ICMP_SEQ = %u TTL = %d RTT = %d ms\n", iLength, inet_ntoa(pstIp->ip_src), pstIcmp->icmp_seq, pstIp->ip_ttl, iRtt);

    }
    else
    {
        fprintf(stderr, "Invalid ICMP Packet ! It doesn't match!\n");
        return -1;
    }

    return 0;
}

//两个时间点之间的间隔
struct timeval  timeOffset(struct timeval tv_begin, struct timeval tv_end)
{
    struct timeval tv;
    tv.tv_sec = tv_end.tv_sec - tv_begin.tv_sec;
    tv.tv_usec = tv_end.tv_usec - tv_begin.tv_usec;

    if (tv.tv_usec < 0)
    {
        -- tv.tv_sec;
        tv.tv_usec += 1000000;
    }

    return tv;
}

//发包
void *    pingSend(void * args)
{
    char pszSendBuff[128];
    memset(pszSendBuff, 0, sizeof(pszSendBuff));
    gettimeofday(&CPingStatus.m_tvBegin, NULL);

    while (CPingStatus.m_bStatus)
    {
        /*pthread_mutex_lock(&CPingSync.mutex);
        while (CPingSync.nready == 0)
            pthread_cond_wait(&CPingSync.cond, &CPingSync.mutex);
        for (int pos=0; pos < PACKET_SEND_MAX_NUM; ++pos)
            if (CPingPacket[pos].m_bGet == false)
            {
                gettimeofday(&CPingPacket[pos].m_tvBegin, NULL);
                CPingPacket[pos].m_bGet = 1;
                CPingPacket[pos].m_wSeq = CPingStatus.m_iSendCnt;
                break;
            }
        --CPingSync.nready;
        pthread_mutex_unlock(&CPingSync.mutex);*/
        
        {
            //避免异常退出导致锁未释放。
            std::unique_lock<std::mutex>  syncMutex(CPingSync.m_mutex);
            while(CPingSync.nready == 0)
                CPingSync.m_cond.wait(syncMutex);
            for (int pos=0; pos < PACKET_SEND_MAX_NUM; ++pos)
                if (CPingPacket[pos].m_bGet == false)
                {
                    gettimeofday(&CPingPacket[pos].m_tvBegin, NULL);
                    CPingPacket[pos].m_bGet = 1;
                    CPingPacket[pos].m_wSeq = CPingStatus.m_iSendCnt;
                    break;
                }
            --CPingSync.nready;

        }


        icmpPackUp((struct icmp *)pszSendBuff, CPingStatus.m_iSendCnt, 64, CPingStatus.m_pid);
        ssize_t iSuccess = sendto(CPingStatus.m_iSockfd, pszSendBuff, 64, 0, (struct sockaddr *)&CPingStatus.m_sockServer, sizeof(CPingStatus.m_sockServer));
        ++CPingStatus.m_iSendCnt;
        if (iSuccess<0)
        {
            fprintf(stderr, "Send ICMP Packet Fail!\n");
            continue;
        }
        
        sleep(1);

    }
}

//收包
void *   pingRecv(void * args)
{
    struct timeval tv_overlap;
    tv_overlap.tv_sec = 10;
    tv_overlap.tv_usec = 20000;

    fd_set  fdRead;
    char    pszRecvBuff[512];
    memset(pszRecvBuff, 0, sizeof(pszRecvBuff));
    while (CPingStatus.m_bStatus)
    {
        FD_ZERO(&fdRead);
        FD_SET(CPingStatus.m_iSockfd, &fdRead);

        tv_overlap.tv_sec = 10;
        tv_overlap.tv_usec = 20000;
        int iState = select(CPingStatus.m_iSockfd+1, &fdRead, NULL, NULL, &tv_overlap);

        if (iState == -1)
            fprintf(stderr, "Fail to Select!\n");
        else if (iState == 0)
            fprintf(stderr, "Ping Select timeout!\n");
        else
        {
            int iSize = recv(CPingStatus.m_iSockfd, pszRecvBuff, sizeof(pszRecvBuff), 0);
            if (iSize < 0)
            {
                fprintf(stderr, "Receive ICMP Packet Fail!\n");
                //continue;
            }

            iState = icmpUnPack(pszRecvBuff, iSize, CPingStatus.m_pid);
            if (iState == -1)
            {
                fprintf(stderr, "Extract ICMP Packet Fail!\n");
                //continue;
            }

            ++CPingStatus.m_iRecvCnt;
        }

    }
}

//中断处理函数
void    icmpSigInt(int signo)
{
    CPingStatus.m_bStatus = false;
    gettimeofday(&CPingStatus.m_tvEnd, NULL);
    CPingStatus.m_tvOffset = timeOffset(CPingStatus.m_tvBegin, CPingStatus.m_tvEnd);
}

