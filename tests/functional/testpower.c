#include "powermon_bgq.h"

const int buf_size = (1024 * 1024);

int main (int argc, char** argv)
{
	int status, myrank, numtasks, itr;
	int* arr;
	double power;
	long long int tm1, tm2;
	double lat;

    status = MPI_Init(&argc, &argv);
    if ( MPI_SUCCESS != status)
    {
        printf(" Error Starting the MPI Program \n");
        MPI_Abort(MPI_COMM_WORLD, status);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* Setup Power	*/
	MonEQ_DisableCollection ();
	status = MonEQ_Initialize();

	/*	Report the Current Power */

	/*	-------------	Create The Array	-------------- */
	arr = (int*) malloc ( sizeof(int) * buf_size);
	if (0 == arr) {
		printf("Error allocating Array \n");
		fflush(stdout);
	}
	memset(arr, 0, buf_size * sizeof(int));

	/*	-------------	Populate the Array	-------------- */
	for (itr = 0; itr < buf_size; itr++) {
		arr[itr] = 7 +  itr;
	}

	if (0 == myrank){
		MonEQ_PrintDomainInfo ();
		MonEQ_PrintVTMRatio ();
	}

	if (MonEQ_MonitorAgentOnRank()) {
		tm1 = GetTimeBase();
		power =  MonEQ_GetPower();
		tm2 = GetTimeBase();

		lat = ((double)tm2 - (double) tm1) / 1600e6;

		printf (" Power is %f w, call latency: %f sec \n", power, lat);
	}

	/* Finalize Power	*/
	status = MonEQ_Finalize();
	if (0 != status)
	{
		printf("Error Initializing MonEQ \n");
		fflush(stdout);
	}

	free(arr);
	arr = 0;

	MPI_Finalize();

	return 0;
}
