/*
   Argonne Leadership Computing Facility benchmark
   BlueGene/P version
   Aggregate bandwidth
   Written by Vitali Morozov <morozov@anl.gov>
   */
#include "mpi.h"
#include "<powermon_bgq.h>"
#include <stdio.h>
#include <stdlib.h>

#define MAXN 100       // repetition rate for a single pair test
#define MAXSKIP 0     // skip first tests to warm-up
#define N 10

#define LENGTH 524288
//#define LENGTH 2*1048576 // 2MB

unsigned long long getticks();
double timer();

int main( int argc, char *argv[] )
{
    MPI_Comm comm = MPI_COMM_WORLD;
    int taskid, ntasks, is1, *ir1, i, L, k, Receiver, ppn;
    double *sb, *rb, d;
    MPI_Status stat[2*N];
    MPI_Request req[2*N];
    double t1;

    int status = MPI_Init( &argc, &argv );

    if (MPI_SUCCESS != status) {
        printf("Error starting the MPI Program!\n");
        MPI_Abort(MPI_COMM_WORLD, status);
    }

    MPI_Comm_rank( comm, &taskid );
    MPI_Comm_size( comm, &ntasks );

    status = PMQ_Initialize();

    if ( getranks(N, &is1, &ir1, &ppn ) == 0 )
    {

        Receiver = 0;
        for ( i = 0; i < N; i++ )
            if ( taskid == ir1[i] )
                Receiver++;

        d = 0.0;

        posix_memalign( (void **)&sb, 16, sizeof( char ) * LENGTH );
        posix_memalign( (void **)&rb, 16, sizeof( char ) * LENGTH );

        MPI_Barrier (comm);

        if ( taskid == is1 )
        {
            //for ( i = 0; i < N; i++ )
            //printf("%d => %d\n", is1, ir1[i] );

            // heating up! //
            for ( k = 0; k < MAXSKIP; k++ )
            {
                for ( i = 0; i < N; i++ )
                {
                    MPI_Isend( sb, LENGTH, MPI_BYTE, ir1[i], is1, comm, &req[i] );
                    MPI_Irecv( rb, LENGTH, MPI_BYTE, ir1[i], ir1[i], comm, &req[i+N] );
                }

                MPI_Waitall( 2*N, req, stat );
            }

            t1 = timer();

            for ( k = 0; k < MAXN; k++ )
            {
                for ( i = 0; i < N; i++ )
                {
                    MPI_Isend( sb, LENGTH, MPI_BYTE, ir1[i], is1,    comm, &req[i] );
                    MPI_Irecv( rb, LENGTH, MPI_BYTE, ir1[i], ir1[i], comm, &req[i+N] );
                }
                MPI_Waitall( 2*N, req, stat );
            }

            t1 = timer() - t1;


            d = ( (2.0 * (double)LENGTH * (double)N * (double)MAXN ) / (t1) );
            d /= 1e9;
            // in MB/s, 1MB = 1e6 B

            //d = (double)(2 * MAXN * N * LENGTH) / (double)t1 * 850. / 1e6; // in MB/s, 1MB = 1e6 B
        }

        if ( Receiver )
        {
            for ( k = 0; k < MAXN+MAXSKIP; k++ )
            {
                int total = 0;
                for ( i = 0; i < Receiver; i++ )
                {
                    MPI_Isend( sb, LENGTH, MPI_BYTE, is1, taskid, comm, &req[total++] );
                    MPI_Irecv( rb, LENGTH, MPI_BYTE, is1, is1,    comm, &req[total++] );
                }

                MPI_Waitall( total, req, stat );

            }
        }

        MPI_Reduce( &d, &d, 1, MPI_DOUBLE, MPI_SUM, 0, comm );
        if ( taskid == 0 )
            printf("PPN: %d Aggregate BW (GB/s): %18.2lf\n", ppn, d );

        free( sb );
        free( rb );
    }

    free(ir1);
#ifdef BVT
    {
        int _rank;
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
        if (_rank == 0)
            printf("Test passed functionally\n");
    }
#endif

    MPI_Finalize();
    return 0 ;
}
