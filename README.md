# Kwikflix Scheduler
## Scheduler

### Features

* Reading ts-files on selected time and sending to UDP
* 
### Build

Just run make in the project directory

### How to use

Run from command line: 
./scheduler some.json

### Example of json
<pre>
{
"ip" : "239.0.0.10",
"port" : 1234,
"accumul" : "4M",
"cache_size" : "8M",
"ttl" : 255,
"priority" : 99,
"dbg_msg" : 0,
"ts_in_udp" : 7,
"bitrate" : 2000320,
"stream" : [
   {
     "file" : ["/ProjectsP/ts/swm.ts", "/ProjectsP/ts/gt.ts"],
     "shell_exec" : "ps -e",
     "schedule" : {
       "repeat" : 1,
       "day of week" : ["Tue", "Sun"],
       "time" : ["12:26"],
       "month" : ["every"],
       "except" : [
          {"month" : [ "Jum", "Jul", "Aug"], "day" :  [ "5", "7", "18"]},
          {"month" : [ "Sep", "Oct"], "day of week" :  ["Mon"]}
       ]
      }
   },
   {
     "sequence" : "/ProjectsP/ts/sr_",
     "start_ind" : 1,
     "end_ind" : 3,
     "digits" : 2,
     "schedule" : {
       "repeat" : 1,
       "day" : ["5", "14"],
       "time" : ["15:01", "19:30"],
       "month" : ["Jan", "Nov"]
      }
    }
]
}
</pre>
