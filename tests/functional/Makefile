MPICC = mpicc

MPI_CFLAGS = -I. -I../../lib_monitor -O3
MonEQ_LIBRARY = ../../lib_monitor/libbgq_power.a
HPM_LIBRARY= /soft/perftools/hpctw/lib/libmpihpm.a /bgsys/drivers/ppcfloor/bgpm/lib/libbgpm.a  

TARGET = testpowercoll
OBJS = testpowercoll.o

all: testpowercoll

$(OBJS) : %.o : %.c
	$(MPICC) $(MPI_CFLAGS) -c $< $(MonEQ_LIBRARY) -o $@ 

testpowercoll: $(OBJS)
	$(MPICC) -o $(TARGET) $(OBJS) $(MonEQ_LIBRARY) $(HPM_LIBRARY)

clean:
	rm -f *.a *.o a.ou testpowercoll
