PROG = server
CPPFLAGS += -I.
CFLAGS = -g -O2 -Wall -std=gnu99

all: $(PROG)

OBJS += my_signal.o
OBJS += my_socket.o
OBJS += readn.o
OBJS += logUtil.o
OBJS += get_num.o
OBJS += bz_usleep.o

OBJS += $(PROG).o

$(PROG): $(OBJS)

clean:
	rm -f *.o $(PROG)
