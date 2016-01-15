typedef unsigned int DWORD;
class TimeStamp {
  protected:

  public:
    DWORD      Dw1;
    DWORD      Dw2;
    TimeStamp() {
        Dw1 = 0;
        Dw2 = 0;
    };
    ~TimeStamp() {};
    int        GetDw1( const int year, const int month, const int day, const int hour, const int minute,
                       const int secondW);
    void       SetDate(int day, int month, int year, int hour, int min, int sec);
    void       AddMicro(int ms, int ns);
    void       GetLocalTime();
    int        GetDiff(TimeStamp *MinTime, DWORD *Sec, DWORD *Ns);
    int        GetDayOfWeek(int Day, int Month, int Year);
    TimeStamp  &operator = (TimeStamp &d);
    bool       operator == (TimeStamp &d);
    bool       operator != (TimeStamp &d);
    bool       operator >= (TimeStamp &d);
    bool       operator <= (TimeStamp &d);
    bool       operator >  (TimeStamp &d);
    bool       operator <  (TimeStamp &d);
    inline bool isLeapYear(const int Year);
};


