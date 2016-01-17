x/*
Cyopyright by Michael Korneev 2015
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
#include "TimeItem.h"

TimeDescr::~TimeDescr() {
    if(TimeArr != NULL) {
        for(int i = 0; TimeArr[i] != NULL; i++)free(TimeArr[i]);
        free(TimeArr);
    }
}
TimeItem::~TimeItem() {
    int  i;
    for(i = 0; FileArr[i] != NULL; i++)free(FileArr[i]);
    if(Shell != NULL)free(Shell);
    free(FileArr);
    if(Exceptions != NULL) {
        for(i = 0; Exceptions[i] != (TimeDescr *)NULL; i++)Exceptions[i]->~TimeDescr();
        free(Exceptions);
    }
}
int TimeItem::ProcessTimes(int CurDay, int CurMonth, int CurYear,  tm *tm) {
    TIME_S    *CurTime;
    DWORD     LocDw1, CurDw1, Dw1;
    LocDw1 = 0;
    int bSuc = false;
    CurDw1 = Descr.Stamp.GetDw1(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    for(int i = 0; Descr.TimeArr[i] != NULL; i++) {
        Dw1 = Descr.Stamp.GetDw1(CurYear, CurMonth, CurDay, Descr.TimeArr[i]->Hour, Descr.TimeArr[i]->Min,
                                 Descr.TimeArr[i]->Sec);
        if(Dw1 < CurDw1 && CurYear == (tm->tm_year + 1900) && CurMonth == tm->tm_mon && CurDay == tm->tm_mday)continue;
        if(Dw1 - ExecStamp.Dw1 < 24 * 60 * 60)continue;  //don't run in the same day agan
        bSuc = true;
        if(LocDw1 == 0) {
            LocDw1 = Dw1;
            CurTime = Descr.TimeArr[i];
        } else {
            if(Dw1 < LocDw1) {
                LocDw1 = Dw1;
                CurTime = Descr.TimeArr[i];
            }
        }
    }
    if(bSuc == false)return false;
    Descr.Stamp.Dw1 = Descr.Stamp.GetDw1(CurYear, CurMonth, CurDay, CurTime->Hour, CurTime->Min, CurTime->Sec);
    Descr.Stamp.Dw2 = 0;
    return true;
}
int TimeItem::ProcessDays(int CurDay, int CurMonth, int CurYear,  tm *tm) {
    TimeStamp  Stamp;
    int        StartDay;
    int        i, i2;
    if(CurYear == tm->tm_year + 1900 && CurMonth == tm->tm_mon)StartDay = tm->tm_mday;
    else StartDay = 0;
    if(Descr.bWeekDay == truey) {
        //cacluating the array of month day
        for(i = 0; i < 31; i++) {
            int FWDay = Stamp.GetDayOfWeek(i+1, CurMonth, CurYear);
            Descr.DayArr[i+1] = 0;
            for(i2 = 0; i2 < 7; i2++) {
                if(Descr.WDayArr[i2] == 1 && i2 == FWDay) {
                    Descr.DayArr[i+1] = 1;
                    break;
                }
            }
            FWDay++;
            if(FWDay == 7)FWDay = 0;
        }
    }
    for(i = StartDay; i < 31; i++) {
        if(Descr.DayArr[i] == 1) {
            if(ProcessTimes(i, CurMonth, CurYear, tm) == true) {
                return true;
            }
        }
    }
    return false;
}
int TimeItem::CalcNearest() {
    time_t    t;
    tm        tm;
    int       i;
    int       bSuc;
    int       CurMonth = 0;
    int       StartMonth = 0;  //month to start look
    t = time(NULL);
    tm = *localtime(&t);
    int CurDay = tm.tm_mday;
    int CurYear = Descr.Year;
    if(Descr.Year == 0)CurYear = tm.tm_year + 1900;
    if(CurYear < tm.tm_year + 1900)return false;
    if(CurYear == tm.tm_year + 1900)StartMonth = tm.tm_mon;
//tm.tm_month zero-based??
    bSuc = false;
    //looking for month
    for(;;) {
        for(i = StartMonth; i < 12; i++) {
            if(Descr.MonthArr[i] == 1) {
                CurMonth = i;
                if(ProcessDays(CurDay, CurMonth, CurYear, &tm) == true) {
                    bSuc = true;
                    break;
                }
            }
        }
        if(bSuc == false && (Descr.Year != 0 || StartMonth == 0))return 0;
        if(bSuc == true)break;
        StartMonth = 0;
        CurYear++;
    }
    return true;
}
