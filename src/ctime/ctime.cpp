#include "ctime.h"
#include <sys/time.h>
#include <stddef.h>

using namespace std;

CTime::CTime(int tv_sec, int tv_usec)
{
    m_time.tv_sec = tv_sec;
    m_time.tv_usec = tv_usec;
}

CTime::CTime() : CTime(0,0)
{}

CTime::CTime(const timeval &time) : CTime(time.tv_sec, time.tv_usec)
{}

CTime   CTime::operator- (const CTime & time)
{
    struct timeval  tv(time.getTime());
    tv.tv_sec = m_time.tv_sec - tv.tv_sec;
    tv.tv_usec = m_time.tv_usec - tv.tv_usec;
    if (tv.tv_usec < 0)
    {
        --tv.tv_sec;
        tv.tv_usec += 1000000;
    }

    return CTime(tv);
}

CTime   CTime::operator +(const CTime & time)
{

    struct timeval  tv(time.getTime());
    tv.tv_sec = m_time.tv_sec + tv.tv_sec;
    tv.tv_usec = m_time.tv_usec + tv.tv_usec;
    if (tv.tv_usec >= 1000000)
    {
        ++tv.tv_sec;
        tv.tv_usec %= 1000000;
    }

    return CTime(tv);
}

void    CTime::gettimeofday()
{
    ::gettimeofday(&m_time, NULL);
}

struct timeval  CTime::getTime() const
{
    return m_time;
}

void    CTime::flush()
{
    m_total = m_time.tv_sec * 1000 + 1.0 * m_time.tv_usec / 1000;
}

double    CTime::getTotalUsec()
{
    flush();
    return m_total;
}

std::ostream & operator<<(std::ostream & os, const CTime & ctime) {
    struct timeval tv = ctime.getTime();
    os<<tv.tv_sec<<'\t'<<tv.tv_usec;
    return os;
}