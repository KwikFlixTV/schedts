# Kwikflix Scheduler
## Scheduler

### Features

* Reading ts-files on selected time and sending to UDP
 
### Build

Just run make in the project directory

### How to use

Run from command line: 
./scheduler some.json

### Json format description:

#### Parameters with no parents:

* "file" - scheduler write to a file or send a udp stream
* "ip", "port" - ip address and a port to send a udp stream
* "bitrate" - the bitrate of the stream, it must be the same for every file
* "cache size" - common cache size, you can use K to set the size in kilobytes and M in megabytes
* "accumul" - size of filled cache part, you can use K to set the size in kilobytes and M in megabytes

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
       "day of week" : ["Tue", "Sun"],
       "time" : ["12:26"],
       "month" : ["every"],
      }
   },
   {
     "sequence" : "/ProjectsP/ts/sr_",
     "start_ind" : 1,
     "end_ind" : 3,
     "digits" : 2,
     "schedule" : {
       "day" : ["5", "14"],
       "time" : ["15:01", "19:30"],
       "month" : ["Jan", "Nov"]
      }
    }
]
}
</pre>
