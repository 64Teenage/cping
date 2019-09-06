#include "../ctime/ctime.h"

void    CPingPack::setBegTime()
{
    m_timeBeg.gettimeofday();
}

CTime   CPingPack::getBegTime()
{
    return m_timeBeg;
}

long    CPingPack::getTimeOffSet()
{
    CTime time = m_timeEnd - m_timeBeg;
    return time.getTotalUsec();
}

void    CPingPack::setStatus(bool flag)
{
    m_bGet = flag;
}

bool    CPingPack::getStatus() 
{
    return m_bGet;
}

void    CPingPack::setSeqID(int id)
{
    m_wSeq = id;
}

short   CPingPack::getSeqID()
{
    return m_wSeq;
}