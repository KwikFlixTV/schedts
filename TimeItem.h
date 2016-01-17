typedef struct TIME_S {
    int        Hour;
    int        Min;
    int        Sec;
    int        Ms;
};
class TimeDescr {
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
class TimeItem {
    friend int      ParseJson(char *Path);
  private:

    int        ProcessTimes(int CurDay, int CurMonth, int CurYear,  tm *tm);
    int        ProcessDays(int CurDay, int CurMonth, int CurYear,  tm *tm);
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
