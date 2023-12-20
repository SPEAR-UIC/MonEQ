F90     = mpixlcxx_r
LD      = mpixlcxx_r

TARGET = dgemmtest

OBJS =  $(TARGET).o

#FFLAGS = -g -qsmp=omp 
FFLAGS = -g -O3 -qhot -qsmp=omp -I. -I../../lib_monitor
LDFLAGS = $(FFLAGS) #-qnostaticlink

# MonEQ Flags
MonEQ_LIBRARY = ../../lib_monitor/libbgq_power.a
HPM_LIBRARY = /soft/perftools/hpctw/lib/libmpihpm_smp.a /bgsys/drivers/ppcfloor/bgpm/lib/libbgpm.a

POWER_LIB_PATH = /home/swallace/PowerMonitoring/lib_monitor

INCLUDE=-I. -I$(POWER_LIB_PATH)
BLAS_LIB=-L/home/morozov/lib -ltimebase -L/home/morozov/ESSL5.1.1-20120305 -lesslsmpbg -L/soft/compilers/ibmcmp-feb2012/xlf/bg/14.1/bglib64 -lxlf90_r -lxlsmp -L/soft/compilers/ibmcmp-feb2012/xlmass/bg/7.3/bglib64 -lmass


all: grid 

%.o : %.C
	$(F90) $(FFLAGS) $(INCLUDE) $(HPCT_INC) -c $<

grid: $(OBJS)
	$(LD) $(LDFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)  $(HPCT_LIB) $(BLAS_LIB) $(LIBS) $(MonEQ_LIBRARY) $(HPM_LIBRARY)
        
clean:
	rm -f $(TARGET) $(TARGET).o

