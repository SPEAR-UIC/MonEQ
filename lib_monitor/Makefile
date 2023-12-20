#   ---------------------------
#   Compiler Flags
#   ---------------------------

MPICC = mpicc
MPICXX = mpicxx

#LIB_TIMEBASE_PATH = /home/morozov/lib
MPI_CFLAGS = -I. -O3 
Q_AR = /bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc64-bgq-linux-ar

all: libbgq_power.a

Q_POWER_C_OBJ = powermon_bgq.o 

$(Q_POWER_C_OBJ): %.o: %.c
	$(MPICC) $(MPI_CFLAGS)-c $< -o $@

libbgq_power.a: $(Q_POWER_C_OBJ)
	$(Q_AR) rcs $@ $(Q_POWER_C_OBJ)

clean:
	rm -f *.a *.o a.out core*
