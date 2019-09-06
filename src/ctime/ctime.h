#ifndef CTIME_H
#define CTIME_H

#include <sys/time.h>
#include <ostream>

class CTime
{
public:

    CTime();
    CTime(int tv_sec, int tv_usec);

public:
    CTime   operator- (const CTime & time);
    CTime   operator+(const CTime & time);
    friend std::ostream & operator<<(std::ostream &, const CTime &);

public:
    void    gettimeofday();
    double  getTotalUsec();

private:
    CTime(const struct timeval & time);
    struct timeval  getTime() const;
    void    flush();



private :
    struct timeval  m_time;
    double    m_total;
};


class CPingPack 
{
private:
    bool    m_bGet;
    short   m_wSeq;
    CTime   m_timeBeg;
    CTime   m_timeEnd;

public:
    long    getTimeOffSet();
    void    setBegTime();
    CTime   getBegTime();

    void    setStatus(bool);
    bool    getStatus();

    void    setSeqID(int);
    short   getSeqID();
};

#endif // CTIME_H
