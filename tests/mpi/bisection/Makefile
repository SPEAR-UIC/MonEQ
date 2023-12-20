ifeq ($(origin  CC), default)
  ifeq ($(origin DRV), undefined)
    export CC=/bgsys/drivers/ppcfloor/comm/xl.ndebug/bin/mpixlc
  else
    export CC=/bgsys/drivers/${DRV}/ppc64-rhel60/comm/xl.ndebug/bin/mpixlc
  endif
endif

LD     = $(CC)
FFLAGS = -g 
CFLAGS = -g 
DEFS= -D__INLINE__=inline $(BVT) $(PCHECK)
LDFLAGS = $(CFLAGS)

TARGET = aggregate
OBJS =  main.o getranks.o getticks.o

all: grid


%.o : %.c
	$(CC) $(CFLAGS) $(DEFS) -c $<

grid: $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(MPITRACE_LIB)

clean:
	rm -f $(TARGET) $(OBJS)

