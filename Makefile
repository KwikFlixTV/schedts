CC      = g++ 
LDFLAGS += -pthread
CFLAGS  = -g -MD -Wall -I. -I./jsoncpp/include/

OBJS = TimeItem.o TimeStamp.o scheduler.o ./jsoncpp/src/lib_json/json_reader.o ./jsoncpp/src/lib_json/json_value.o ./jsoncpp/src/lib_json/json_writer.o 
TARGET = shedts


all: $(TARGET)

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

