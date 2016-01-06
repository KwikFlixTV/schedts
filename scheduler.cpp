/*
3Copyright by Michael Korneev 2015
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

#define TS_PACKET_SIZE 188
#define PKT_ACCUMUL_NUM 10000
#define PKT_FULL_NUM    30000
typedef unsigned int DWORD;


unsigned char*      send_buf;
pthread_mutex_t     c_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned int        snd_pkt_num;       //the size of ts packets, that sended per one operation
unsigned int        packet_size;       //the size of udp packet, that sended per one operation
unsigned char       *cache_buf;        //the cache buffer
unsigned int        pkt_full = PKT_FULL_NUM;          //the quantity of all packets in the chache buffer
unsigned int        pkt_num = 0;       //the quanitity of filled packets in the chache buffer
unsigned int        start_pkt = 0;     //the number of the first packet in cache buffer where is the buffer is filled
unsigned int        last_pkt = 0;      //the number of the last packet in cache buffer where is the buffer is filled
unsigned int        file_fin = 0;      //file reading finisched
unsigned int        bitrate = 0;
int                 bCacheReady = 0;
int                 bNoMoreFile = 0;
int                 transport_fd = 0;
int                 sockfd;
int                 outfd;
struct timespec     nano_sleep_packet;
struct timespec     nano_sleep_packet_r;
struct sockaddr_in  addr;
sched_param         prio_param;
int                 bPrint = 0;
int                 bMsg = 0;
FILE                *LogFileD = NULL;
unsigned int        PktAccumulNum = PKT_ACCUMUL_NUM;
unsigned int        BufDelay = 0;
int                 bDontExit = 0;
int                 bSend = 0;          //==0 output to a file, ==1 send to udp
char                *FilePath;
Json::Reader        reader;
Json::Value         root;
static const int N_DAYS_IN_MONTH     [] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const int N_DAYS_IN_LEAP_MONTH[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
int      ParseJson(char *Path);

int CheckIp(char *Value){
     int bWasChifr = 0;
     int NumOfChifr = 0;
     int DigNum = 0;
     int i;
     int len = strlen(Value);
     for(i=0; i<len; i++){
         if(Value[i] == '.'){
            if(bWasChifr == 0)return 0;
            bWasChifr = 0;
            NumOfChifr = 0;
            DigNum++;
            continue;
         }
         if(!(Value[i] >=0x30 && Value[i] <=0x39))return 0;
         NumOfChifr++;
         if(NumOfChifr > 3)return 0;
         bWasChifr = 1;
     }
     if(DigNum != 3)return 0;
     if(NumOfChifr > 3 || NumOfChifr == 0)return 0;
     return 1;
}

int ReadSizeInPkt(char *Value){
     char st[100];
     int len = strlen(Value);
     if(Value[len-1] == 'M' || Value[len-1] == 'K'){
        memcpy(st, Value, len - 1);
        int Val = atoi(st);
        if(Value[len-1] == 'M')return Val * 1024 * 1024 / TS_PACKET_SIZE;
        return Val * 1024 / TS_PACKET_SIZE;
     }
     return atoi(Value);
}

class TimeStamp
{
protected:

public:
    DWORD      Dw1;
    DWORD      Dw2;
    TimeStamp()
    {
        Dw1 = 0;
        Dw2 = 0;
    };
    ~TimeStamp() {};
    int        GetDw1( const int year, const int month, const int day, const int hour, const int minute, const int secondW);
    void       SetDate(int day, int month, int year, int hour, int min, int sec);
    void       AddMicro(int ms, int ns);
    void       GetLocalTime();
    int        GetDiff(TimeStamp*MinTime, DWORD *Sec, DWORD *Ns);
    int        GetDayOfWeek(int Day, int Month, int Year);
    TimeStamp  &operator = (TimeStamp & d);
    bool       operator == (TimeStamp & d);
    bool       operator != (TimeStamp & d);
    bool       operator >= (TimeStamp & d);
    bool       operator <= (TimeStamp & d);
    bool       operator >  (TimeStamp & d);
    bool       operator <  (TimeStamp & d);
    inline bool isLeapYear(const int Year);
};

int TimeStamp::GetDayOfWeek(int Day, int Month, int Year)
{
    tm timeStruct = {};
    timeStruct.tm_year = Year - 1900;
    timeStruct.tm_mon = Month - 1;
    timeStruct.tm_mday = Day;
    timeStruct.tm_hour = 12;    //  To avoid any doubts about summer time, etc.
    mktime(&timeStruct);
    return timeStruct.tm_wday;
}

void TimeStamp::SetDate(int day, int month, int year, int hour, int min, int sec)
{
    Dw1 = GetDw1(year, month, day, hour, min, sec);
    Dw2 = 0;
}
//month is zero-based as elsewhere in the programm
int TimeStamp::GetDw1( const int year, int month, const int day, const int hour, const int minute, const int second)
{
    const int* const DAYS_PER_MONTH = isLeapYear(year) ? N_DAYS_IN_LEAP_MONTH : N_DAYS_IN_MONTH;
    month++;
    if(year   < 0  ||  year   > 3000 ||
            month  < 1  ||  month  > 12   ||
            day    < 1  ||  day    > DAYS_PER_MONTH[ month-1 ] ||
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
    for(int i = std::min( year, epoch ); i<std::max(year, epoch);  i++)
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


inline bool TimeStamp::isLeapYear(const int Year)
{
    return  Year % 400 == 0 ||
            (Year % 4   == 0  &&  Year % 100 != 0);
}


void TimeStamp::GetLocalTime()
{
    time_t    t;
    tm        tm;
    t = time(NULL);
    tm = *localtime(&t);
    Dw1 = GetDw1(tm.tm_year + 1900, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    Dw2 = 0;
}

int TimeStamp::GetDiff(TimeStamp *MinTime, DWORD *Sec, DWORD *Ns)
{
    Sec[0] = MinTime->Dw1 - Dw1;
    //Ns[0] = MinTime->Dw2 - Dw1;
    return 0;
}

TimeStamp & TimeStamp::operator = (TimeStamp & f)
{
    Dw1 = f.Dw1;
    Dw2 = f.Dw2;
    return *this;
}

bool TimeStamp::operator == (TimeStamp & f)
{
    if(Dw1 == f.Dw1 && Dw2 == f.Dw2)return true;
    return false;
}

bool TimeStamp::operator != (TimeStamp & f)
{
    if(Dw1 == f.Dw1 && Dw2 == f.Dw2)return false;
    return true;
}

bool TimeStamp::operator >= (TimeStamp & f)
{
    if(Dw1 > f.Dw1)return true;
    if(Dw1 == f.Dw1)
    {
        if(Dw2 >= f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator <= (TimeStamp & f)
{
    if(Dw1 < f.Dw1)return true;
    if(Dw1 == f.Dw1)
    {
        if(Dw2 <= f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator > (TimeStamp & f)
{
    if(Dw1 > f.Dw1)return true;
    if(Dw1 == f.Dw1)
    {
        if(Dw2 > f.Dw2)return true;
    }
    return false;
}

bool TimeStamp::operator < (TimeStamp & f)
{
    if(Dw1 < f.Dw1)return true;
    if(Dw1 == f.Dw1)
    {
        if(Dw2 < f.Dw2)return true;
    }
    return false;
}

typedef struct TIME_S
{
    int        Hour;
    int        Min;
    int        Sec;
    int        Ms;
};
class TimeDescr
{
public:
    TimeDescr() {};
    ~TimeDescr();
    int        bWeekDay;
    int        bRepeat;
    int        Year;            //0 - any year
    int        MonthArr[12];    //enumeration of used months
    int        DayArr[32];      //enumeration of used days
    int        WDayArr[7];      //for days of week
    TIME_S     **TimeArr;       //array of times
    TimeStamp  Stamp;           //calculated stamp
};

TimeDescr::~TimeDescr()
{
    if(MonthArr != NULL)free(MonthArr);
    if(DayArr != NULL)free(DayArr);
    for(int i=0; TimeArr[i] != NULL; i++)free(TimeArr[i]);
    if(TimeArr != NULL)free(TimeArr);
}

class TimeItem
{
    friend int      ParseJson(char *Path);
private:

    int        ProcessTimes(int CurDay, int CurMonth, int CurYear,  tm *tm);
    int        ProcessDays(int CurMonth, int CurYear,  tm *tm);
public:
    TimeItem() {};
    ~TimeItem();
    int        CalcNearest();
    TimeItem   *Next;
    char       **FileArr;
    TimeDescr  Descr;          //the schedule description
    TimeDescr  **Exceptions;   //array of exceptions from the description
    TimeStamp  ExecStamp;
    char       *Shell;         //run some shell command on execution(path or string)
    int         bShellFile;    //Shell is file or command
    int         bSeq;          //we're playing a sequence of file, f.e vid01.ts, vid02.ts, vid03.ts
    int         SeqStart;      //sequence start index (f.e 01)
    int         SeqEnd;        //sequence end index (f.e 03)
    int         SeqDigits;     //digits in index, f.e 03 or 003 or 004
};

TimeItem  *FirstItem;
char      *OutputFile;
TimeItem::~TimeItem()
{
    int  i;
    for(i=0; Exceptions[i]!=(TimeDescr*)NULL; i++)Exceptions[i]->~TimeDescr();
    for(i=0; FileArr[i]!=NULL; i++)free(FileArr[i]);
    free(FileArr);
    free(Exceptions);
}
const char *MonArr[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
const char *DayWArr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};
int TimeItem::ProcessTimes(int CurDay, int CurMonth, int CurYear,  tm *tm)
{
    TIME_S    *CurTime;
    DWORD     LocDw1, CurDw1, Dw1;
    LocDw1 = 0;
    int bSuc = false;
    CurDw1 = Descr.Stamp.GetDw1(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    for(int i=0; Descr.TimeArr[i] != NULL; i++)
    {
        Dw1 = Descr.Stamp.GetDw1(CurYear, CurMonth, CurDay, Descr.TimeArr[i]->Hour, Descr.TimeArr[i]->Min, Descr.TimeArr[i]->Sec);
        if(Dw1 < CurDw1 && CurYear == tm->tm_year && CurMonth == tm->tm_mon && CurDay == tm->tm_mday)continue;
        if(Dw1 - ExecStamp.Dw1 < 24 * 60 * 60)continue;  //don't run in the same day agan
        bSuc = true;
        if(LocDw1 == 0)
        {
            LocDw1 = Dw1;
            CurTime = Descr.TimeArr[i];
        }
        else
        {
            if(Dw1 < LocDw1)
            {
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
int TimeItem::ProcessDays(int CurMonth, int CurYear,  tm *tm)
{
    TimeStamp  Stamp;
    int        StartDay;
    int        i, i2;
    if(CurYear == tm->tm_year + 1900 && CurMonth == tm->tm_mon)StartDay = tm->tm_mday;
    else StartDay = 0;
    if(Descr.bWeekDay == true)
    {
        //cacluating the array of month day
        int FWDay = Stamp.GetDayOfWeek(1, CurMonth, CurYear);
        for(i=0; i<31; i++)
        {
            Descr.DayArr[i] = 0;
            for(i2=0; i2<7; i2++)
            {
                if(Descr.WDayArr[i2] == 1 && i2 == FWDay)
                {
                    Descr.DayArr[i] = 1;
                    break;
                }
            }
            FWDay++;
            if(FWDay == 7)FWDay = 0;
        }
    }
    for(i=StartDay; i<31; i++)
    {
        if(Descr.DayArr[i] == 1)
        {
            if(ProcessTimes(i, CurMonth, CurYear, tm) == true)
            {
                return true;
            }
        }
    }
    return false;
}
int TimeItem::CalcNearest()
{
    time_t    t;
    tm        tm;
    int       i;
    int       bSuc;
    int       CurMonth = 0;
    int       StartMonth = 0;  //month to start look
    t = time(NULL);
    tm = *localtime(&t);
    int CurYear = Descr.Year;
    if(Descr.Year == 0)CurYear = tm.tm_year + 1900;
    if(CurYear < tm.tm_year + 1900)return false;
    if(CurYear == tm.tm_year + 1900)StartMonth = tm.tm_mon;
//tm.tm_month zero-based??

    bSuc = false;

    //looking for month
    for(;;){
        for(i=StartMonth; i<12; i++)
        {
            if(Descr.MonthArr[i] == 1)
            {
               CurMonth = i;
               if(ProcessDays(CurMonth, CurYear, &tm) == true)
               {
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


int StrFromArr(char **StrArr, char *Str)
{
    for(int i=0; StrArr[i]!=NULL; i++)
    {
        if(strcmp(StrArr[i], Str) == 0)return i;
    }
    return -1;
}

char *ReadFile(char *Path, DWORD *SizeW){
    FILE *f = fopen(Path, "rb");
    if(f == NULL){
       printf("file reading error\n");
       return 0;
    }
    fseek(f, 0, SEEK_END);
    DWORD Size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *pBuf = (char*)malloc(Size + 1);
    fread(pBuf, Size, 1, f);
    fclose(f);
    pBuf[Size] = 0;
    if(SizeW != NULL)
       SizeW[0] = Size;
    return pBuf;
}

int ParseJson(char *Path)
{
    char                   *ip, *output;
    int                    port;
    int                    ttl;
    Json::Value            Value;
    DWORD                  Size;
    char                   start_addr[4];
    int                    is_multicast = 0;
    int                    rt;
    char *pBuf = ReadFile(Path, &Size);
    if(pBuf == NULL)return 0;


    if(reader.parse( pBuf, pBuf+Size, root) == false)
    {
        printf("json parsing error\n");
        exit(0);
    }
    if(root.isMember("bitrate") == false){
       printf("no bitrate defined\n");
       exit(0);
    }
    Value = root.get("bitrate", "");
    bitrate = Value.asInt();
    if(root.isMember("ts_in_udp") == false){
       packet_size = 7 * TS_PACKET_SIZE;
       snd_pkt_num = 7;
    }else{
       Value = root.get("ts_in_udp", "");
       snd_pkt_num = Value.asInt();
       packet_size = snd_pkt_num * TS_PACKET_SIZE;
    }
    if(root.isMember("accumul") == true){
       Value = root.get("accumul", "");
       PktAccumulNum = ReadSizeInPkt((char*)Value.asCString());
    }
    if(root.isMember("cache_size") == true){
       Value = root.get("cache_size", "");
       pkt_full = ReadSizeInPkt((char*)Value.asCString());
    }
    if(root.isMember("priority") == true){
       Value = root.get("priority", "");
       prio_param.sched_priority = Value.asInt();
    }
    if(root.isMember("dbg_msg") == true){
       Value = root.get("dbg_msg", "");
       BufDelay = Value.asInt();
    }

    if(root.isMember("output") == true){
       bSend = 0;
       Value = root.get("output", "");
       output = (char*)Value.asCString();
       outfd = open(output, O_APPEND);
       if(outfd == NULL){
          perror("couldn't open the output file\n");
          exit(0);
       }
       printf("output: %s\n", output);
    }else{
       bSend = 1;
       if(root.isMember("ip") == false || root.isMember("port") == false){
          printf("ip address isn't correct\n");
          exit(0);
       }
       Json::Value IpValue = root.get("ip", "");
       ip = (char*)IpValue.asCString();
       if(CheckIp(ip) == 0){
          printf("incorrect ip address: %s\n", ip);
          exit(0);
       }
       Value = root.get("port", "");
       port = Value.asInt();
       memcpy(start_addr, ip, 3);
       start_addr[3] = 0;
       is_multicast = atoi(start_addr);
       is_multicast = (is_multicast >= 224) || (is_multicast <= 239);
       addr.sin_family = AF_INET;
       addr.sin_addr.s_addr = inet_addr(ip);
       addr.sin_port = htons(port);
       sockfd = socket(AF_INET, SOCK_DGRAM, 0);
       if(sockfd < 0){
          perror("socket(): error ");
          //if(LogFileD != NULL)fclose(LogFileD);
          exit(0);
       }
       if(root.isMember("ttl") == true){
          Value = root.get("ttl", "");
          ttl = Value.asInt();
          if(is_multicast){
             rt = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
          }else{
             rt = setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
          }
          if(rt < 0){
             perror("ttl configuration fail");
          }
       }
    }
    Json::Value s_stream = root.get("stream", "");
    TimeItem                 *NewItem;
    int                      i, i2, i3;
    int                      Num;
    int                      PrevInd, Ind;
    int                      bIntrv;
    int                      bSuc;
    char                     st[5];
    const char               *sVal;
    Json::Value              s_day, s_mon, s_time;
    Json::Value              s_file;
    Json::Value              s_repeat;
    Json::Value              s_schedule;
    Json::Value              s_shell;
    Json::ValueConstIterator it_f;
    int                      iShell = 0;
    for(Json::ValueConstIterator it = s_stream.begin(); it != s_stream.end(); ++it)
    {
        const Json::Value& cur_stream = *it;
        NewItem = new TimeItem;
        NewItem->Next = FirstItem;
        FirstItem = NewItem;
        if(cur_stream.isMember("schedule") == false)continue;
        if(cur_stream.isMember("shell_exec") == true){
           iShell = 1;
           s_shell = cur_stream.get("shell_exec", 0);
        }else if(cur_stream.isMember("shell_file") == true){
           iShell = 2;
           s_shell = cur_stream.get("shell_file", 0);
        }
        if(iShell > 0){
           sVal = (char*)s_shell.asCString();
           rt = strlen(sVal);
           NewItem->Shell = (char*)malloc(rt + 1);
           memcpy(NewItem->Shell, sVal, rt);
           if(iShell == 2)NewItem->bShellFile = true;
        }
        s_schedule = cur_stream.get("schedule", 0);
        s_repeat = s_schedule.get("repeat", 0);
        NewItem->Descr.bRepeat = s_repeat.asInt();
        if(cur_stream.isMember("file"))
        {
            s_file = cur_stream.get("file", "");
            i = 0;
            for(it_f = s_file.begin(); it_f != s_file.end(); ++it_f)
            {
                i++;
            }
            NewItem->FileArr = (char**)malloc(sizeof(char*) * (i +1));
            i = 0;
            for(it_f = s_file.begin(); it_f != s_file.end(); ++it_f)
            {
                const Json::Value& cur_file = *it_f;
                char *file = (char*)cur_file.asCString();
                rt = strlen(file);
                NewItem->FileArr[i] = (char*)malloc(rt + 1);
                memcpy(NewItem->FileArr[i], file, rt);
                NewItem->FileArr[i][rt] = 0;
                printf("file: %s\n", file);
                i++;
            }
            NewItem->FileArr[i] = NULL;
        }
        else
        {
            s_file = cur_stream.get("sequence", "");
            char *seq = (char*)s_file.asCString();
            rt = strlen(seq);
            printf("sequence: %s\n", seq);
            NewItem->FileArr = (char**)malloc(sizeof(char*) * 2);
            NewItem->FileArr[0] = (char*)malloc(rt + 1);
            memcpy(NewItem->FileArr[0], seq, rt);
            NewItem->FileArr[0][rt] = 0;
            NewItem->FileArr[1] = NULL;
            NewItem->bSeq = true;
            if(cur_stream.isMember("start_ind")){
               Value = cur_stream.get("start_ind", "");
               NewItem->SeqStart = Value.asInt();
            }else NewItem->SeqStart = 1;
            if(cur_stream.isMember("end_ind")){
               Value = cur_stream.get("end_ind", "");
               NewItem->SeqEnd = Value.asInt();
            }else NewItem->SeqEnd = 10;
            if(cur_stream.isMember("digits")){
               Value = cur_stream.get("digits", "");
               NewItem->SeqDigits = Value.asInt();
            }else NewItem->SeqDigits = 2;
        }
        if(s_schedule.isMember("day of week"))
        {
            NewItem->Descr.bWeekDay = true;
            s_day = s_schedule.get("day of week", "");
        }
        else
        {
            s_day = s_schedule.get("day", "");
        }
        //DayArr
        i = 0;
        for(it_f = s_day.begin(); it_f != s_day.end(); ++it_f)
        {
            const Json::Value& cur_day = *it_f;
            sVal = cur_day.asCString();
            rt = strlen(sVal);
            //reading day of month or week
            i3 = 0;
            bIntrv = 0;
            bSuc = 1;
            for(i2=0; i2<rt; i2++)
            {
                if(sVal[i2] == '-')
                {
                    strncpy(st, sVal, i2-1);
                    if(NewItem->Descr.bWeekDay == true)
                    {
                        Ind = StrFromArr((char**)DayWArr, st);
                        if(Ind != -1)NewItem->Descr.WDayArr[Ind] = 1;
                    }
                    else
                    {
                        Ind = atoi(st);
                        if(Ind >= 0 && Ind <= 31)
                            NewItem->Descr.DayArr[Ind] = 1;
                    }
                    PrevInd = Ind;
                    i3 = 0;
                    bIntrv = 1;
                    continue;
                }
                else if(i3 > 2 || (NewItem->Descr.bWeekDay == false && (sVal[i2] < 0x30 || sVal[i2] > 0x39)))
                {
                    printf("Json Value error\n");
                    bSuc = 0;
                    break;
                }
                i3++;
            }
            if(bIntrv == 1 && bSuc == 1)
            {
                strncpy(st, sVal+i2-i3, i3);
                if(NewItem->Descr.bWeekDay == true)
                {
                    Ind = StrFromArr((char**)DayWArr, st);
                    if(Ind > PrevInd && Ind < 7)
                    {
                        for(i2=PrevInd; i2<=Ind; i2++)
                            NewItem->Descr.WDayArr[i2] = 1;
                    }
                }
                else
                {
                    Ind = atoi(st);
                    if(Ind > PrevInd && Ind < 32)
                    {
                        for(i2=PrevInd; i2<=Ind; i2++)
                            NewItem->Descr.DayArr[i2] = 1;
                    }
                }
            }
            else if(bSuc == 1)
            {
                if(NewItem->Descr.bWeekDay == true)
                {
                    Ind = StrFromArr((char**)DayWArr, (char*)sVal);
                    NewItem->Descr.WDayArr[Ind] = 1;
                }
                else
                {
                    Ind = atoi(sVal);
                    NewItem->Descr.DayArr[Ind] = 1;
                }
            }
            i++;
        }
        //MonthArr
        s_mon = s_schedule.get("month", "");
        i = 0;
        for(it_f = s_mon.begin(); it_f != s_mon.end(); ++it_f)
        {
            const Json::Value& cur_mon = *it_f;
            sVal = cur_mon.asCString();
            rt = strlen(sVal);
            if(strncmp(sVal, "every", rt) == 0){
               for(i2=0; i2<12; i2++){
                   NewItem->Descr.MonthArr[i2] = 1;
               }
               break;
            }
            //reading months
            i3 = 0;
            bIntrv = 0;
            bSuc = 1;
            for(i2=0; i2<rt; i2++)
            {
                if(sVal[i2] == '-')
                {
                    strncpy(st, sVal, i2-1);
                    Ind = StrFromArr((char**)MonArr, st);
                    if(Ind != -1 && Ind < 12)
                        NewItem->Descr.MonthArr[Ind] = 1;
                    PrevInd = Ind;
                    i3 = 0;
                    bIntrv = 1;
                    continue;
                }
                else if(i3 > 2)
                {
                    printf("Json Value error\n");
                    bSuc = 0;
                    break;
                }
                i3++;
            }
            if(bIntrv == 1 && bSuc == 1)
            {
                strncpy(st, sVal+i2-i3, i3);
                Ind = StrFromArr((char**)MonArr, st);
                if(Ind > PrevInd && Ind < 12)
                {
                    for(i2=PrevInd; i2<=Ind; i2++)
                        NewItem->Descr.MonthArr[i2] = 1;
                }
            }
            else if(bSuc == 1)
            {
                Ind = StrFromArr((char**)MonArr, (char*)sVal);
                NewItem->Descr.MonthArr[Ind] = 1;
            }
            i++;
        }
        //TimeArr
        s_time = s_schedule.get("time", "");
        Num = 0;
        for(it_f = s_time.begin(); it_f != s_time.end(); ++it_f)Num++;
        NewItem->Descr.TimeArr = (TIME_S**)malloc(sizeof(TIME_S*) * (Num + 1));
        i = 0;
        for(it_f = s_time.begin(); it_f != s_time.end(); ++it_f)
        {
            const Json::Value& cur_time = *it_f;
            sVal = cur_time.asCString();
            rt = strlen(sVal);
            NewItem->Descr.TimeArr[i] = (TIME_S*)calloc(1, sizeof(TIME_S));
            printf("Zero time: %d:%d:%d\n", NewItem->Descr.TimeArr[i]->Hour, NewItem->Descr.TimeArr[i]->Min, NewItem->Descr.TimeArr[i]->Sec);
            i3 = 0;
            int i_prev = 0;
            for(i2=0; i2<=rt; i2++)
            {
                if(sVal[i2] == ':' || sVal[i2] == 0)
                {
                    strncpy(st, sVal+i_prev, i2);
                    printf("st: %s %d\n", st, i);
                    if(i3 == 0)NewItem->Descr.TimeArr[i]->Hour = atoi(st);
                    else if(i3 == 1)NewItem->Descr.TimeArr[i]->Min  = atoi(st);
                    else if(i3 == 2)NewItem->Descr.TimeArr[i]->Sec  = atoi(st);
                    printf("Added time: %d:%d:%d\n", NewItem->Descr.TimeArr[i]->Hour, NewItem->Descr.TimeArr[i]->Min, NewItem->Descr.TimeArr[i]->Sec);
                    i3++;
                    i_prev = i2 + 1;
                    continue;
                }
                if(sVal[i2] < 0x30 || sVal[i2] > 0x39)
                {
                    printf("Json Value error\n");
                    break;
                }
            }
            i++;
        }
        NewItem->Descr.TimeArr[i] = NULL;
    }
    return 1;
}

void PrintMsg(char *msg)
{
    time_t    t;
    struct tm tm;
    if(bMsg == 1)printf("%s", msg);
    if(LogFileD != NULL)
    {
        t = time(NULL);
        tm = *localtime(&t);
        fprintf(LogFileD, "[%d-%.2d-%.2d %.2d:%.2d:%.2d] %s", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
    }
}
void ProcessFile(char *tsfile)
{
    char          st[100];
    unsigned char temp_buf[TS_PACKET_SIZE];
    int len;
    transport_fd = open(tsfile, O_RDONLY);
    printf("Processing: %s\r\n", tsfile);
    if(transport_fd < 0)
    {
        fprintf(stderr, "couldn't open file: %s\n", tsfile);
        return;
    }
    unsigned int read_bytes = 0;  //number of bytes that read from file
    for(;;)
    {
        if(pkt_num + 1 >= pkt_full)
        {
            //the chache buffer is full
            if(bPrint == 1)
                PrintMsg("Cache is full\r\n");
            nanosleep(&nano_sleep_packet_r, 0);
            continue;
        }
        len = read(transport_fd, temp_buf, TS_PACKET_SIZE);
        if(len == 0 || len < TS_PACKET_SIZE)
        {
            if(len < TS_PACKET_SIZE && len != 0)
                fprintf(stderr, "read < TS_PACKET_SIZE while reading: %d\n", len);
            //file reading completed
            close(transport_fd);
            printf("Processed: %s\r\n", tsfile);
           // bCacheReady = 0;
            return;

        }
        read_bytes += len;
        pthread_mutex_lock( &c_mutex );
        unsigned char *src_buf = cache_buf + last_pkt * TS_PACKET_SIZE;
        memcpy(src_buf, temp_buf, TS_PACKET_SIZE);
        last_pkt++;
        pkt_num++;
        if(bPrint == 1)
        {
            sprintf(st, "reading file %d\n", pkt_num);
            PrintMsg(st);
        }
        if(last_pkt == pkt_full)last_pkt = 0;
        if(pkt_num > PktAccumulNum)bCacheReady = 1;
        pthread_mutex_unlock( &c_mutex );
        if(pkt_num  > PktAccumulNum)
            nanosleep(&nano_sleep_packet_r, 0);
    }
}
void *TimerThread( void *ptr )
{
    TimeStamp  LocTime;
    TimeItem  *CurItem;
    TimeItem  *MinItem = NULL;
    DWORD     Sec, Ns;
    memset(&nano_sleep_packet, 0, sizeof(nano_sleep_packet));
    for(;;)
    {
        LocTime.GetLocalTime();
        CurItem = FirstItem;
        while(CurItem != NULL)
        {
            printf("Bef Calc Nearest: %s \n", CurItem->FileArr[0]);
            if(CurItem->CalcNearest() == true)
            {
                printf("nearest\n");
                if(MinItem == NULL)MinItem = CurItem;
                else if(MinItem->Descr.Stamp > CurItem->Descr.Stamp && CurItem->Descr.Stamp > LocTime)
                {
                    MinItem = CurItem;
                }
            }
            CurItem = CurItem->Next;
        }
        if(MinItem == NULL)
        {
            printf("panic, nothing to execute\n");
            return 0;
        }
        LocTime.GetLocalTime();
        if(LocTime.GetDiff(&MinItem->Descr.Stamp, &Sec, &Ns) < 0)
        {
            printf("too late too show\n");
        }
        else
        {
            printf("Sleep: %s %d\n", MinItem->FileArr[0], Sec);
            nano_sleep_packet.tv_sec = Sec;
            nano_sleep_packet.tv_nsec = Ns;
            //nanosleep(&nano_sleep_packet, 0);
            sleep(Sec);
            printf("Executing!! \n");
            MinItem->ExecStamp = MinItem->Descr.Stamp;
            if(MinItem->Shell != NULL){
               printf("%s\n", MinItem->Shell);
               if(MinItem->bShellFile == true){
                  char *pBuf = ReadFile(MinItem->Shell, NULL);
                  if(pBuf != NULL){
                     system(pBuf);
                     free(pBuf);
                  }
               }else{
                  system(MinItem->Shell);
               }
            }
            if(MinItem->bSeq == false){
               for(int i=0; MinItem->FileArr[i] != 0; i++){
                   ProcessFile(MinItem->FileArr[i]);
               }
            }else{
               char st2[10];
               char st0[20];
               sprintf(st2, "%d", MinItem->SeqDigits);
               char *st = (char*)malloc(strlen(MinItem->FileArr[0]) + MinItem->SeqDigits + 3 + 4);
               sprintf(st0, "%s%sd.ts", "%s%.", st2);
               for(int i=MinItem->SeqStart; i<=MinItem->SeqEnd; i++){
                   sprintf(st, st0, MinItem->FileArr[0], i);
                   ProcessFile(st);
               }
            }
            MinItem = NULL;
            sleep(10000000);
        }

    }
}
void SendEmptyPacket(){
     int i;
     int sent;
     unsigned char *buf = send_buf;
     for(i=0; i<snd_pkt_num; i++){
         //memset(buf, 0x00, TS_PACKET_SIZE);
         buf[0] = 0x47;
         ((unsigned short*)(buf+1))[0] = 0x1fff;
         buf[1] = 0x1F;
         buf[2] = 0xFF;
         buf[3] = 0x10;
         buf += TS_PACKET_SIZE;
     }
     //printf("Sending empty packet\r\n");
     if(bSend == false){
        sent = (int)write(outfd, send_buf, packet_size);
     }else{
        sent = sendto(sockfd, send_buf, packet_size, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
     }
     if(sent <= 0)perror("send() in SendEmpty: error ");
}

void SendPacket(){
     char  Msg[150];
     unsigned long long int packet_size2 = 0;
     int i;
     int bShow;
     int sent;
     unsigned char *dst_buf = send_buf;
     for(i=0; i<snd_pkt_num; i++){
         pthread_mutex_lock( &c_mutex );
         unsigned char *src_buf = cache_buf + start_pkt * TS_PACKET_SIZE;
         memcpy(dst_buf, src_buf, TS_PACKET_SIZE);
         if(bPrint == 1){
            sprintf(Msg, "Sending packet: %d of %d, start: %d, last: %d\r\n", start_pkt, pkt_num, start_pkt, last_pkt);
            PrintMsg(Msg);
         }
         packet_size2 += TS_PACKET_SIZE;
         pkt_num--;
         start_pkt++;
         if(start_pkt == pkt_full){
            start_pkt = 0;
         }
         //if(pkt_num == 0 && bAccumulOnZero == 1){
         //   bCacheReady = 0;
         //}
         pthread_mutex_unlock( &c_mutex );
         if(pkt_num <= 0)break;
         dst_buf += TS_PACKET_SIZE;
     }
     if(bSend == false){
        sent = (int)write(outfd, send_buf, packet_size2);
     }else{
        sent = sendto(sockfd, send_buf, packet_size2, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
     }
     if(sent <= 0)perror("send() in SendPacket: error ");
}
void *buf_info_thread( void *ptr ){
     for(;;){
         printf("Buffer: %d of %d, start: %d, last: %d\r\n", pkt_num, pkt_full, start_pkt, last_pkt);
         sleep(BufDelay);
     }
}
long long int usecDiff(struct timespec* time_stop, struct timespec* time_start)
{
     long long int temp = 0;
     long long int utemp = 0;

     if(time_stop && time_start){
	    if(time_stop->tv_nsec >= time_start->tv_nsec){
	       utemp = time_stop->tv_nsec - time_start->tv_nsec;
	       temp = time_stop->tv_sec - time_start->tv_sec;
        }else{
    	   utemp = time_stop->tv_nsec + 1000000000 - time_start->tv_nsec;
	       temp = time_stop->tv_sec - 1 - time_start->tv_sec;
        }
        if(temp >= 0 && utemp >= 0){
	       temp = (temp * 1000000000) + utemp;
        }else{
	       fprintf(stderr, "start time %ld.%ld is after stop time %ld.%ld\n", time_start->tv_sec, time_start->tv_nsec, time_stop->tv_sec, time_stop->tv_nsec);
	       temp = -1;
        }
     }else{
	    fprintf(stderr, "memory is garbaged?\n");
	    temp = -1;
     }
     return temp / 1000;
}
void *sending_thread( void *ptr ){
     unsigned long long int packet_time = 0;
     unsigned long long int real_time = 0;
     struct                 timespec time_start;
     struct timespec        time_stop;
     memset(&time_start, 0, sizeof(time_start));
     memset(&time_stop, 0, sizeof(time_stop));
     clock_gettime(CLOCK_MONOTONIC, &time_start);
     for(;;){
         clock_gettime(CLOCK_MONOTONIC, &time_stop);
         real_time = usecDiff(&time_stop, &time_start);
         while(real_time * bitrate > packet_time * 1000000){
               if((bCacheReady == 1 || bNoMoreFile == 1) && pkt_num > 0){
                  SendPacket();
                  packet_time += packet_size * 8;
                  continue;
               }
               SendEmptyPacket();
               packet_time += packet_size * 8;
         }//while(real_time * bitrate > packet_time * 1000000)
         nanosleep(&nano_sleep_packet, 0);
     }//for(;;)
}
int main (int argc, char *argv[])
{
    pthread_t              thread1, s_thread;
    pthread_attr_t         attr;
    pthread_attr_t         attr_d;
    struct sched_param     param;
    int                    rt;
    int                    policy = SCHED_FIFO;
    if(argc != 2){
       printf("Use: scheduler <some.json>\n");
       exit(0);
    }
    memset(&addr, 0, sizeof(addr));
    memset(&nano_sleep_packet, 0, sizeof(nano_sleep_packet));
    memset(&nano_sleep_packet_r, 0, sizeof(nano_sleep_packet_r));
    prio_param.sched_priority = 50;
    ParseJson(argv[1]);
    cache_buf = (unsigned char*)malloc(pkt_full * TS_PACKET_SIZE);
    send_buf = (unsigned char*)malloc(packet_size);
    rt = pthread_attr_init(&attr);
    if(rt != 0)
       perror("pthread_attr_init");
    rt = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if(rt != 0)
       perror("pthread_attr_setinheritsched");
    rt = pthread_create( &thread1, &attr, TimerThread, (void*) 2);
    if(rt){
       fprintf(stderr,"Error - pthread_create(TimerThread) return code: %d\n",rt);
       if(LogFileD != NULL)fclose(LogFileD);
       exit(EXIT_FAILURE);
    }
    if(BufDelay != 0){
        pthread_attr_init(&attr_d);
        pthread_attr_setinheritsched(&attr_d, PTHREAD_EXPLICIT_SCHED);
        rt = pthread_create( &thread1, &attr_d, buf_info_thread, (void*) 2);
        if(rt){
           fprintf(stderr,"Error - pthread_create(buf_info_thread) return code: %d\n",rt);
           if(LogFileD != NULL)fclose(LogFileD);
           exit(EXIT_FAILURE);
        }
    }
    nano_sleep_packet.tv_nsec = 665778; // 1 packet at 100mbps
    nano_sleep_packet_r.tv_nsec = 665778; // 1 packet at 100mbps
    rt = pthread_create( &s_thread, &attr, sending_thread, (void*) 2);
    if(rt){
       fprintf(stderr,"Error - pthread_create(sending_thread) return code: %d\n",rt);
       //if(LogFileD != NULL)fclose(LogFileD);
       exit(EXIT_FAILURE);
    }
    for(;;)
    {
        sleep(10);
    }
}
