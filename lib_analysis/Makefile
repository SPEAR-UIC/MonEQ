GCC = gcc

GCC_CFLAGS = -I. -I../lib_monitor -lm

all: moneq_reducer moneq_analyzer

moneq_reducer:
	$(GCC) $(GCC_CFLAGS) -o moneq_reducer moneq_reducer.c

moneq_analyzer:
	$(GCC) $(GCC_CFLAGS) -o moneq_analyzer moneq_analyzer.c gnuplot_i.c

clean:
	rm -f *.a *.o a.ou moneq_analyzer moneq_reducer
