/*
Copyright by Michael Korneev 2015
*/

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <json/json.h>
#include <signal.h>

#include "TimeStamp.h"

static const int N_DAYS_IN_MONTH     [] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const int N_DAYS_IN_LEAP_MONTH[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int TimeStamp::GetDayOfWeek(int Day, int Month, int Year) {
    tm timeStruct = {};
    timeStruct.tm_year = Year - 1900;
    timeStruct.tm_mon = Month - 1;
    timeStruct.tm_mday = Day;
    timeStruct.tm_hour = 12;    //  To avoid any doubts about summer time, etc.
    mktime(&timeStruct);
    return timeStruct.tm_wday;
}

void TimeStamp::SetDate(int day, int month, int year, int hour, int min, int sec) {
    Dw1 = GetDw1(year, month, day, hour, min, sec);
    Dw2 = 0;
}
//month is zero-based as elsewhere in the programm
int TimeStamp::GetDw1( const int year, int month, const int day, const int hour, const int minute,
                       const int second) {
    const int *const DAYS_PER_MONTH = isLeapYear(year) ? N_DAYS_IN_LEAP_MONTH : N_DAYS_IN_MONTH;
    month++;
    if(year   < 0  ||  year   > 3000 ||
            month  < 1  ||  month  > 12   ||
            day    < 1  ||  day    > DAYS_PER_MONTH[ month - 1 ] ||
            hour   < 0  ||  hour   > 24 ||
            minute < 0  ||  minute > 60 ||
            second < 0  ||  second > 60)
        return 0;
    // it is problematic to do error handling with the return value, as all possible return values could be valid epochs as well.
    // at least by taking 0 you can use a boolean check like if( x = getUnixTimeStamp(...) ) -> valid.
    // The check will fail for 1970-01-01-00-00-00
    // Calculate the seconds since or to epoch
    // Formula for dates before epoch will subtract the full years first, and then add the days passed in that year.
    int epoch       = 2015                  ; // new age epoch 01/01/2015
    int sign        = year > epoch ? 1 : -1 ; // So we know whether to add or subtract leap days
    int nleapdays   = 0                     ; // The number of leap days
    int monthsNDays = 0                     ; // The number of days passed in the current year
    // Count the leap days
    for(int i = std::min( year, epoch ); i < std::max(year, epoch);  i++)
        if(isLeapYear( i ))
            ++nleapdays;
    // Calculate the number of days passed in the current year
    for(int i = 1;  i < month;  i++)
        monthsNDays += DAYS_PER_MONTH[ i - 1 ];
    return (
               ( year - epoch )  * 365         // add or subtract the full years
               +   sign          * nleapdays   // add or subtract the leap days
               +   monthsNDays                 // The number of days since the beginning of the year for full months
               +   day - 1                     // The number of full days this month
           ) * 86400                       // number of seconds in 1 day
           + hour    * 3600
           + minute  * 60
           + second;
}


inline bool TimeStamp::isLeapYear(const int Year) {
    return  Year % 400 == 0 ||
            (Year % 4   == 0  &&  Year % 100 != 0);
}


void TimeStamp::GetLocalTime() {
    time_t    t;
    tm        tm;
    t = time(NULL);
    tm = *localtime(&t);
    Dw1 = GetDw1(tm.tm_year + 1900, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    Dw2 = 0;
}

int TimeStamp::GetDiff(TimeStamp *MinTime, DWORD *Sec, DWORD *Ns) {
    Sec[0] = MinTime->Dw1 - Dw1;
    //Ns[0] = MinTime->Dw2 - Dw1;
    return 0;
}

TimeStamp &TimeStamp::operator = (TimeStamp &f) {
    Dw1 = f.Dw1;
    Dw2 = f.Dw2;
    return *this;
}

bool TimeStamp::operator == (TimeStamp &f) {
    if(Dw1 == f.Dw1 && Dw2 == f.Dw2)return true;
    return false;
}

bool TimeStamp::operator != (TimeStamp &f) {
    if(Dw1 == f.Dw1 && Dw2 == f.Dw2)return false;
    return true;
}

bool TimeStamp::operator >= (TimeStamp &f) {
    if(Dw1 > f.Dw1)return true;
    if(Dw1 == f.Dw1) {
        if(Dw2 >= f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator <= (TimeStamp &f) {
    if(Dw1 < f.Dw1)return true;
    if(Dw1 == f.Dw1) {
        if(Dw2 <= f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator > (TimeStamp &f) {
    if(Dw1 > f.Dw1)return true;
    if(Dw1 == f.Dw1) {
        if(Dw2 > f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator < (TimeStamp &f) {
    if(Dw1 < f.Dw1)return true;
    if(Dw1 == f.Dw1) {
        if(Dw2 < f.Dw2)return true;
    }
    return false;
}
