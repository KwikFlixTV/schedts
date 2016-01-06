CC      = g++ 
LDFLAGS += -pthread
CFLAGS  = -g -MD -Wall -I. -I./jsoncpp/include/

OBJS = scheduler.o ./jsoncpp/src/lib_json/json_reader.o ./jsoncpp/src/lib_json/json_value.o ./jsoncpp/src/lib_json/json_writer.o 
TARGET = scheduler


all: $(TARGET)

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

