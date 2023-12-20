#include "powermon_bgq.h"
#include <stdio.h>
#include <unistd.h>
#include <mpi.h>
#include <time.h>

#define MAX_PRIMES 150000

void work(int myrank) {
	clock_t start, end;
    double runTime;
    start = clock();
    int i, num = 1, primes = 0;
	
    while (num <= MAX_PRIMES) {
        i = 2;
        while (i <= num) {
            if(num % i == 0)
                break;
            i++;
        }
        if (i == num)
            primes++;
		
        num++;
    }
	
    end = clock();
    runTime = (end - start) / (double) CLOCKS_PER_SEC;
    //printf("This rank (%d) calculated all %d prime numbers under %d in %g seconds\n", myrank, primes, MAX_PRIMES, runTime);
}

int main (int argc, char** argv) {
	int status, myrank, numtasks;

	status = MPI_Init(&argc, &argv);
	if (MPI_SUCCESS != status) {
		printf(" Error Starting the MPI Program \n");
		MPI_Abort(MPI_COMM_WORLD, status);
	}

	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* Setup Power	*/
	//MonEQ_SetFirstCollectionDelay(3, 0);
	//MonEQ_SetCollectionInterval(1, 0);
	status = MonEQ_Initialize();

	if (0 != status) {
		printf("Error initializing MonEQ\n");
		fflush(stdout);
	}

	MonEQ_StartPowerTag("prime");
	
	
	work(myrank);
	

	MonEQ_EndPowerTag("prime");

	/* Finalize Power	*/
	status = MonEQ_Finalize();

	if (0 != status) {
		printf("Error finializing MonEQ\n");
		fflush(stdout);
	}
	
	MPI_Finalize();

	return 0;
}
