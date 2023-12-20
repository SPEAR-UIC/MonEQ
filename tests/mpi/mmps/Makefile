MPICC		= mpixlc_r
CFLAGS = -g  -O3 -I../../../lib_monitor
LDFLAGS = $(CFLAGS)

PMQ_LIBRARY = ../../../lib_monitor/libbgq_power.a

TARGET = mmps
OBJS =  main.o getranks.o getticks.o

all: mmps 

$(OBJS): %.o : %.c
	$(MPICC) $(CFLAGS) -c $< $(PMQ_LIBRARY) -o $@ 

mmps: $(OBJS)
	$(MPICC) -g -o $(TARGET) $(OBJS) $(PMQ_LIBRARY)

clean:
	rm -f *.o core mmps 

