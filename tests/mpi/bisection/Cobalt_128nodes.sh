#!/bin/bash -x
export L1P_POLICY=std
export BG_THREADLAYOUT=1   # 1 - default next core first; 2 - my core first

export PROG=aggregate
export NODES=128
export RANKS_PER_NODE=2

rm -f core.* 



for RANKS_PER_NODE in 1 2 4 8 16 32 64 
do
    NPROCS=$((NODES*RANKS_PER_NODE)) 
    OUTPUT=N${NODES}_R${RANKS_PER_NODE}_aggregate_${NPROCS}.log 
    VARS="BG_SHAREDMEMSIZE=64"
    rm -f $OUTPUT
    qsub -n ${NODES} --mode c${RANKS_PER_NODE} -t 1:10:00 -o $OUTPUT --env ${VARS} $PROG
    touch $OUTPUT
done

