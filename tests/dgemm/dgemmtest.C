#include <stdio.h>
#include <assert.h>
#include <omp.h>
#include <mpi.h>
//#define BLKSIZE 4096
#define BLKSIZE 1024 

#define NUM_ITER 10000

#define _ESVCPTR
#include <complex>
#include <essl.h>


//A class for randomly generating matrix elements' value
#define MAXINT (~(1<<31))
class MatGen {
private:
  //variables for generating the sequence of random numbers
  int curRnd;
  int rndA;
  int rndQ;
  int rndR;

public:
  MatGen(int seed) {
    curRnd = seed;
    rndA = 48271;
    rndQ = MAXINT/rndA;
    rndR = MAXINT%rndA;
  }

  int nextRndInt() {
    curRnd = rndA*(curRnd%rndQ) - rndR*(curRnd/rndQ);
    if (curRnd<0) curRnd += MAXINT;
    return curRnd;
  }

  //The range of the returned double random number is [-0.5, 0.5]
  double toRndDouble(int rndInt) {
    return (double)rndInt/MAXINT - 0.5;
  }

  void getNRndInts(int num, int *d) {
    for (int i=0; i<num; i++)
      d[i] = nextRndInt();
  }
};

extern "C" long long int timebase();
extern "C" int MonEQ_Initialize ();
extern "C" int MonEQ_Finalize ();


double testdgemm(int iter)
{
	double *m1 = new double[BLKSIZE*BLKSIZE]; 
	double *m2 = new double[BLKSIZE*BLKSIZE]; 
	double *m3 = new double[BLKSIZE*BLKSIZE]; 
 
	MatGen rnd(0); 
	for (int i=0; i<BLKSIZE*BLKSIZE; i++) 
	{ 
        m1[i] = rnd.toRndDouble(rnd.nextRndInt()); 
        m2[i] = rnd.toRndDouble(rnd.nextRndInt()); 
        m3[i] = rnd.toRndDouble(rnd.nextRndInt()); 
	} 

	long long int tm1 = timebase();    
   
	// Loop through the dgemm iterations reusing original matrices except C
	for ( int i =0; i < iter; i++) 
	{
		 dgemm( "N", "N",
			 BLKSIZE, BLKSIZE, BLKSIZE,
		     -1.0, m1,
			BLKSIZE, m2, BLKSIZE,
			1.0, m3, BLKSIZE);

	}

	long long int tm2 = timebase();

	double duration = ( (double)( tm2 - tm1 ) ) / 1600e6;
	double B = (double)BLKSIZE / 1000; /* croot( 1 000 000 000 ) = 1000 */
    double tot_iter =  (double) iter;  
	
	double gflopcount = 2.0 * B * B * B * tot_iter;
      
    double gflopPerSec = gflopcount / duration; 
   
	//printf("The dgemm is %g GFlop/sec\n", gflopPerSec); 
 
	delete[] m1; 
	delete[] m2; 
	delete[] m3;       

	return gflopPerSec;
}


int main(int argc, char** argv)
{
	int numtasks, myrank, status;
	int nthreads, tid;
	double my_flops_count, tot_flops_count;

	status = MPI_Init(&argc, &argv);
    if ( MPI_SUCCESS != status)
    {
        printf(" Error Starting the MPI Program \n");
        MPI_Abort(MPI_COMM_WORLD, status);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	// Initialize Power Monitoring
	MonEQ_Initialize();

    my_flops_count = testdgemm(NUM_ITER);


	MPI_Reduce (&my_flops_count, &tot_flops_count, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	if (0 == myrank)
	{
		printf(" Aggregate Dgemm: %g GFLOPS \n", tot_flops_count);
	}

	// Finalize Power Monitoring
	MonEQ_Finalize();

	MPI_Finalize();
    return 0;
}

