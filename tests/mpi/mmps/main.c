/*
 Argonne Leadership Computing Facility benchmark
 BlueGene/P version
 Messaging rate
 Written by Vitali Morozov <morozov@anl.gov>
 Modified by: Daniel Faraj <faraja@us.ibm.com>
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXN 256   // repetition rate for a single pair test
#define MAXTASKS 64
#define MAXWIN 8
#define XNBORS 9

MPI_Comm comm = MPI_COMM_WORLD;
MPI_Request req[2 * XNBORS * MAXTASKS * MAXWIN];
int targets[XNBORS * MAXTASKS];
int iter = MAXN;

double times[2], mint[2];

inline void measureMMPS(int taskid, int source, int ppn, int num_targets, int receiver, int window) {
	int tag = 10, i, j, k, w;
	int total = 0;
	char sb, rb;
	double t0, t1, t2 = 0;
	times[0] = times[1] = 10000000000;
	mint[0] = mint[1] = 0;

	MPI_Barrier(comm);

	if (taskid >= source && taskid < (source + ppn)) {
		t0 = MPI_Wtime();
		for (i = 0; i < iter; i++) {
			total = window * num_targets;
			for (w = 0; w < window; w++)
				for (j = 0; j < num_targets; j++)
					MPI_Irecv(&rb, 0, MPI_CHAR, targets[j], tag, comm, &req[total++]);

			total = 0;
			for (w = 0; w < window; w++)
				for (j = 0; j < num_targets; j++)
					MPI_Isend(&sb, 0, MPI_CHAR, targets[j], tag, comm, &req[total++]);

			MPI_Waitall(2 * window * num_targets, req, MPI_STATUSES_IGNORE );
		}
		t0 = MPI_Wtime() - t0;

		times[0] = t0;
		times[1] = 0.;
	}

	// if I am a receiver
	else if (receiver) {
		for (i = 0; i < iter; i++) {
			total = window * ppn * receiver;

			for (w = 0; w < window; w++)
				for (j = 0; j < ppn; j++)
					for (k = 0; k < receiver; k++)
						MPI_Irecv(&rb, 0, MPI_CHAR, source + j, tag, comm, &req[total++]);

			total = 0;
			for (w = 0; w < window; w++)
				for (j = 0; j < ppn; j++)
					for (k = 0; k < receiver; k++)
						MPI_Isend(&sb, 0, MPI_CHAR, source + j, tag, comm, &req[total++]);

			MPI_Waitall(2 * window * ppn * receiver, req, MPI_STATUSES_IGNORE );
		}
	}

	MPI_Reduce(&times[0], &mint[0], 2, MPI_DOUBLE, MPI_MIN, 0, comm);
}

// this is uni mmps
inline double measureMMPS1(int taskid, int source, int ppn, int num_targets, int receiver, int window) {
	int iter = MAXN;
	int tag = 10, i, j, k, w;
	int total = 0;
	char sb, rb;
	double mrate, t0, t1 = 0, t2 = 0, maxt;

	if (ppn == 64)
		iter = 64;

	MPI_Barrier(comm);

	if (taskid >= source && taskid < (source + ppn)) {
		for (i = 0; i < iter; i++) {
			total = 0;
			MPI_Barrier(comm);
			MPI_Barrier(comm);
			t0 = MPI_Wtime();
			for (w = 0; w < window; w++)
				for (j = 0; j < num_targets; j++)
					MPI_Isend(&sb, 0, MPI_CHAR, targets[j], tag, comm, &req[total++]);

			MPI_Waitall(window * num_targets, req, MPI_STATUSES_IGNORE );
			t1 += (MPI_Wtime() - t0);
		}
	}

	// if I am a receiver
	else if (receiver) {
		for (i = 0; i < iter; i++) {
			total = 0;

			// we add another barrier below the loop to ensure no unexpected messages
			MPI_Barrier(comm);

			for (w = 0; w < window; w++)
				for (j = 0; j < ppn; j++)
					for (k = 0; k < receiver; k++)
						MPI_Irecv(&rb, 0, MPI_CHAR, source + j, tag, comm, &req[total++]);

			MPI_Barrier(comm);

			MPI_Waitall(window * ppn * receiver, req, MPI_STATUSES_IGNORE );
		}
	}
	else
		for (i = 0; i < iter; i++) {
			MPI_Barrier(comm);
			MPI_Barrier(comm);
		}

	MPI_Reduce(&t1, &maxt, 1, MPI_DOUBLE, MPI_MAX, 0, comm);

	mrate = (double) (ppn * iter * window * num_targets) / maxt / 1e6;
	return mrate;
}

int main(int argc, char *argv[]) {
	int ppn, num_targets, taskid, ntasks, source, i, j, window, receiver, best_win, provided, status;

	double best_mrate, mrate[MAXWIN + 1], mrateb[MAXWIN + 1];

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	MPI_Comm_rank(comm, &taskid);
	MPI_Comm_size(comm, &ntasks);

	/* Initialize MonEQ power monitoring */
	status = MonEQ_Initialize();
	if (0 != status) {
		printf("Error initializing MonEQ!\n");
		fflush(stdout);
	}

	init(&ppn, &source);

	if (provided != MPI_THREAD_MULTIPLE && ppn < 64) {
		if (taskid == 0)
			printf("cannot support thread_multiple, aborting\n");
		MPI_Finalize();
		exit(0);
	}

	if (taskid == 0) {
		printf("PPN   XNBORS   WIN   MMPS\n");
		printf("--------------------------\n");
	}
	if (ppn == 64)
		iter = 64;


	double max_rate = 0;
	for (i = 1; i <= XNBORS; i++) {
		num_targets = i * ppn;
		getranks(i, targets);

		for (receiver = 0, j = 0; j < num_targets; j++)
			if (taskid == targets[j])
				receiver++;

		for (window = 1; window <= MAXWIN; window++) {
			// warmup
			measureMMPS(taskid, source, ppn, num_targets, receiver, window);

			measureMMPS(taskid, source, ppn, num_targets, receiver, window);

			// measurement
			mrate[window] = (double) (2 * ppn * iter * window * num_targets) / mint[0] / 1e6;
			mrateb[window] = (double) (2 * ppn * iter * window * num_targets) / mint[1] / 1e6;

		}

		best_mrate = mrate[1];
		best_win = 1;
		for (j = 2; j < MAXWIN; j++)
			if (best_mrate < mrate[j]) {
				best_mrate = mrate[j];
				best_win = j;
			}
		if (taskid == 0)
			printf("%3d   %6d   %3d   %.2f\n", ppn, i, best_win, best_mrate);

		if (best_mrate > max_rate)
			max_rate = best_mrate;
	}


	if (taskid == 0)
		printf("Max Mrate for ppn %3d: %.2f\n", ppn, max_rate);

	freeMem();

#ifdef BVT
	{
		int _rank;
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
		if (_rank == 0)
		printf("Test passed functionally\n");
	}
#endif

	/* Finalize MonEQ power collection */
	status = MonEQ_Finalize();
	if (0 != status) {
		printf("Error finalizing MonEQ!\n");
		fflush(stdout);
	}

	MPI_Finalize();
	return 0;
}
