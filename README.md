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

* "file" : "path to file" - scheduler write to a file or send a udp stream
* "ip" : "ip" , "port" : port - ip address and a port to send a udp stream
* "bitrate" : bitrate - the bitrate of the stream, it must be the same for every file
* "cache size" : "size" - common cache size, you can use K to set the size in kilobytes and M in megabytes
* "accumul" : "size" - size of filled cache part, you can use K to set the size in kilobytes and M in megabytes
* "ttl" : "number" set ttl
* "priority" : number - set the process/thread priority
* "ts_in_udp" : number - the number of ts packets in one udp packet
* "stream" : [items] - contains items of a schedule

#### Parameters with "stream" parent:

* "file" : [files] - the sequence of files to play, it must the sequence, even if we play one file, "file" is used for a sequence with files that have different names
* "sequence" : [files] - the sequence of files to play, every file has a name concatenated with a number within a sequence
* "start_ind" : index - the started number of the sequence to play
* "end_ind" : index - the last number of the sequence to play
* "digits" : number - the number of digits described the file position in the sequence
* "shell_exec" : "shell command" - the shell's command to run before file playing
* "shell_file" : "path to file" - a file to be executed before file playing
* "schedule" : {parameters} - a description of time when to play files

#### Parameters with "schedule" parent:

* "day of week" : [days of week] - the list of days of week when to play files
* "day" : [dates] - the list of month days when to play files, it should be used instead of "day of week"
* "month" : [months] - the list of months when to play files, you can use key work "every" to play every month
* "year" : year - the year when to play fils, if the parameter is not defines, the scheduler play files every year

### Reread configuration:

You need to send SIGHUP signal to force the shedts to reread the json configuration. You may do from command-line:<br>
```
kill -1 pid
```
You can get a process pid with:<br>
```
ps -e | grep schedts
```


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

### Contribution

Code must be formatted with Artistic Style Utility: http://astyle.sourceforge.net/

Astyle is command-line utility and you have to use such options to format:
./astyle TimeItem.cpp TimeItem.h TimeStamp.cpp TimeStamp.h scheduler.cpp -p -c -xC110 -xe -k3 -A14

We reject contributions that aren't being formatted with astyle.

