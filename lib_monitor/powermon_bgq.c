#ifndef __POWERMON__BGQ__H
#include "powermon_bgq.h"
#endif

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <mpi.h>
#include <signal.h>
#include <time.h>

#include "bgq_err.h"

#include <spi/include/kernel/location.h>
#include <hwi/include/common/uci.h>

// Needed for timebase
#include <hwi/include/bqc/A2_inlines.h>

#define EMON_DEFINE_GLOBALS
#include <spi/include/emon/emon.h>

// Variable to hold the last error
MonEQ_ERROR *moneq_err = 0;

// Flag for Monitoring Agent
int MonEQ_agent_on_rank = 0;

// MPI COMM_WORLD Rank
int MonEQ_world_rank = -1;

// MPI COMM_WORLD size
int MonEQ_world_size = -1;

// Flag to enable/disable background data collection
volatile int MonEQ_enable_collection = 1;

// Flag for Monitoring Agent
volatile int MonEQ_agent_alive = 1;

// Info about the co-ordinates of the process
unsigned row, col, midplane, nodeboard, computecard, core;

// Number of DOMAINS on BGQ
const int BGQ_MAX_DOMAINS = 7;

// Number of DCAs on BGQ
const int BGQ_NUM_DCA = 2;

// Number of Samples taken
static int measured_count = 0;

// Initial time
long long int start_ticks = 0;
struct timeval start_timeval;
double start_mpi_wtime;

// Maximum number of samples
int tot_samples = MAX_SAMPLES;

// Setting up the timer interrupt
struct sigaction sa;
struct itimerval timer;

/* Variables for collection intervals */

// Initial value defaults to 6.0 seconds
// This is because there is a 6-7 second delay in the data, so this shifts the window
// so the tags are more accurate.  Sure, the start and end tags could be off by as much
// as a whole second, but for now this is as good as it's going to get.
int initialCollection_seconds = 6;
int initialCollection_microSeconds = 0;

// Polling interval defaults to 560 msec
int polling_seconds = 0;
int polling_microSeconds = 560000;

/* Power tag related */
struct power_tag *power_tags_db = 0;

// Mainly just for sanity checking later, but could be useful in other instances
static int num_power_tags = 0;
static int num_used_power_tags = 0;

// Main power data structure
struct power_data {
	long long int m_time;
	double mpi_wtime;
	//struct timeval m_timeval;
	// Card power is the power of the node card.
	double m_card_power;
	double m_voltage[14];
	double m_current[14];
};

// Power database
struct power_data* power_db = 0;

int MonEQ_MonitorAgentOnRank(void) {
	return MonEQ_agent_on_rank;
}

void timer_handler(int signum) {
	if (MonEQ_agent_alive) {
		power_db[measured_count].m_card_power = MonEQ_GetVoltageAndCurrent(power_db[measured_count].m_voltage, power_db[measured_count].m_current);
		power_db[measured_count].m_time = GetTimeBase();
		power_db[measured_count].mpi_wtime = MPI_Wtime();

		++measured_count;
	}

	return;
}

/* Power Tagging Methods */

void push_power_tags_to_file() {
	FILE *tag_fp = fopen(POWER_TAG_FILENAME, "w");
	if (!tag_fp) {
		return;
	}
	else {
		int tag_num;
		for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
			struct power_tag *this_power_tag = &power_tags_db[tag_num];
			
			fprintf(tag_fp, "%s\n", this_power_tag->tag_name);
		}
		
		fclose(tag_fp);
	}
}

struct power_tag* find_start_power_tag_for_measured_count(int measured_count) {
	int tag_num;
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = &power_tags_db[tag_num];
		
		if (this_power_tag->start_measured_count == measured_count) {
			return this_power_tag;
		}
	}
	
	return 0;
}

struct power_tag* find_end_power_tag_for_measured_count(int measured_count) {
	int tag_num;
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = &power_tags_db[tag_num];
		
		if (this_power_tag->end_measured_count == measured_count) {
			return this_power_tag;
		}
	}
	
	return 0;
}

struct power_tag* find_power_tag_for_measured_count(int measured_count) {
	int tag_num;
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = &power_tags_db[tag_num];
		
		// Check both start and end.
		if (this_power_tag->start_measured_count == measured_count || this_power_tag->end_measured_count == measured_count) {
			return this_power_tag;
		}
	}
	
	return 0;
}

struct power_tag *find_power_tag_for_name(char *tag_name) {
	int tag_num;
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = &power_tags_db[tag_num];
		
		if (strcmp(this_power_tag->tag_name, tag_name) == 0) {
			return this_power_tag;
		}
	}
	
	return 0;
}

int MonEQ_StartPowerTag(char *tag_name) {
	if (!MonEQ_agent_on_rank) {
		moneq_err = new_moneq_error(ENOTAGENTONRANK, "This rank is not the agent rank.  This can be safely ignored.");
		return ENOTAGENTONRANK;
	}
	
	// Disallow spaces in tag names...for now.  Might remove this restriction at a later time.
	if (strchr(tag_name, ' ')) {
		moneq_err = new_moneq_error(EMKPWRTAGSP, "Invalid tag name, spaces are not supported.");
		return EMKPWRTAGSP;
	}
	
	struct power_tag *new_power_tag = (struct power_tag*) malloc(sizeof(struct power_tag));
	
	if (new_power_tag == 0) {
		moneq_err = new_moneq_error(EMKPWRTAG, "Malloc failed while creating tag, check errno.");
		return EMKPWRTAG;
	}
	
	new_power_tag->start_measured_count = measured_count;
	new_power_tag->tag_name = tag_name;
	new_power_tag->start_time = MPI_Wtime();
	
	power_tags_db[num_power_tags] = *new_power_tag;
	
	num_power_tags++;
	
	// Barrier everyone here to make sure the tag is in all threads.
	//MPI_Barrier(MPI_COMM_WORLD);
	
#if DEBUG
	printf("Created power tag: %s (time: %f, rank %d)\n", new_power_tag->tag_name, new_power_tag->start_time, MonEQ_world_rank);
#endif
	
	moneq_err = new_moneq_error(MonEQ_SUCCESS, "Sucessfully created start tag.");
	return MonEQ_SUCCESS;
}

int MonEQ_EndPowerTag(char *tag_name) {
	if (!MonEQ_agent_on_rank) {
		moneq_err = new_moneq_error(ENOTAGENTONRANK, "This rank is not the agent rank.  This can be safely ignored.");
		return ENOTAGENTONRANK;
	}
	
	struct power_tag *this_power_tag = find_power_tag_for_name(tag_name);
	
	if (this_power_tag == 0) {
		fprintf(
				stderr,
				"Failed to find power tag to end: %s\n",
				tag_name);
		moneq_err = new_moneq_error(EFINDPWRTAGNM, "Unable to find power tag to end.");
		return EFINDPWRTAGNM;
	}
	
	this_power_tag->end_measured_count = measured_count;
	this_power_tag->end_time = MPI_Wtime();
	
	
#if DEBUG
	printf("Ended power tag: %s (time: %f, rank: %d)\n", this_power_tag->tag_name, this_power_tag->end_time, MonEQ_world_rank);
#endif
	
	moneq_err = new_moneq_error(MonEQ_SUCCESS, "Sucessfully ended power tag.");
	return MonEQ_SUCCESS;
}


/* Primary MonEQ Methods */

int MonEQ_Initialize(void) {
	Personality_t mypers;
	int status, all_status;
	uint32_t procIDOnNode;

	MPI_Comm_size(MPI_COMM_WORLD, &MonEQ_world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &MonEQ_world_rank);

	// Retreive the Personality Info
	status = Kernel_GetPersonality(&mypers, sizeof(Personality_t));

	// This is needed to scale to 64RPN
	procIDOnNode = Kernel_ProcessorID(); // HW thread ID 0-67

	bg_decodeComputeCardCoreOnNodeBoardUCI(mypers.Kernel_Config.UCI, &row, &col, &midplane, &nodeboard, &computecard, &core);

	// Assign compute card 0 and procIDOnNode 0 to do monitoring
	if ((0 == computecard) && (0 == procIDOnNode)) {
		MonEQ_agent_on_rank = 1;
		//printf ("Rank: %d R: %d C: %d M: %d N: %d CC: %d C: %u \n", \
		//		MonEQ_world_rank, row, col, midplane, nodeboard, computecard, procIDOnNode);
	}

	// Initialize EMON Setup on the rank
	// Initialize the Starting Timers
	status = 0;
	if (MonEQ_agent_on_rank) {
		start_mpi_wtime = MPI_Wtime();
		gettimeofday(&start_timeval, NULL);
		start_ticks = GetTimeBase();

		status = EMON_SetupPowerMeasurement();
		
#if DEBUG
		printf("Starting at %f for rank %d.\n", start_mpi_wtime, MonEQ_world_rank);
#endif
	}

	MPI_Allreduce(&status, &all_status, 1, MPI_INT, MPI_BOR, MPI_COMM_WORLD);

	if (0 != all_status) {
		moneq_err = new_moneq_error(EEMONERR, "One or more agent ranks failed to setup EMON power measurment.");
		return EEMONERR;
	}

	// Allocate the Power Database
	if (MonEQ_agent_on_rank && MonEQ_enable_collection) {
		power_db = (struct power_data*) malloc(tot_samples * sizeof(struct power_data));
		memset(power_db, 0, tot_samples * sizeof(struct power_data));
		
		// Allocate the power tags database
		power_tags_db = (struct power_tag *) malloc(MAX_TAGS * sizeof(struct power_tag));
		memset(power_tags_db, 0, MAX_TAGS * sizeof(struct power_tag));

		// Launch the Monitor handler
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = &timer_handler;

		status = sigaction(SIGALRM, &sa, NULL);
		if (0 != status) {
			fprintf(stderr, "Failed to set SIGACTION: %s\n", strerror(errno));
			moneq_err = new_moneq_error(ESIGACTION, "Failed to set SIGACTION.");
			return ESIGACTION;
		}

		// Start timer delay
		timer.it_value.tv_sec = initialCollection_seconds;
		timer.it_value.tv_usec = initialCollection_microSeconds;

		// Delay for subsequent polls
		timer.it_interval.tv_sec = polling_seconds;
		timer.it_interval.tv_usec = polling_microSeconds;

		//Start a virtual timer. It counts down whenever this process is executing.
		status = setitimer(ITIMER_REAL, &timer, NULL);
		if (0 != status) {
			fprintf(stderr, "Failed to set timer: %s\n", strerror(errno));
			moneq_err = new_moneq_error(ESETTIMER, "Failed to set timer.");
			return ESETTIMER;
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);

	moneq_err = new_moneq_error(MonEQ_SUCCESS, "Sucessfully initialized MonEQ.");
	return MonEQ_SUCCESS;
}

int MonEQ_Finalize(void) {
	moneq_err = new_moneq_error(MonEQ_SUCCESS, "Sucessfully finialized MonEQ.");
	char *message;

	MPI_Barrier(MPI_COMM_WORLD);

	// Sleep for 8 seconds to make sure we have all the records read
	if (MonEQ_enable_collection) {
		sleep(8);
		MonEQ_agent_alive = 0;

		// This is a hack around for now.
		sleep(1);
	}

	if (MonEQ_agent_on_rank && MonEQ_enable_collection) {
		char* prefix = 0;
		FILE* fp;
		double time_from_start, frac, intpart;
		int64_t fracpart;
		struct timeval cur_time;

		char pathname[8192];
		memset(pathname, '\0', 8192);

		char time_str[8192];
		memset(time_str, '\0', 8192);
		char *time_str_ptr;

		prefix = getenv("MonEQ_PREFIX");
		if (prefix) {
			char* slash = "/";
			if (0 ==  (&prefix[strlen(prefix) - 1], slash, 1)) {
				snprintf(pathname, sizeof(pathname), "%sMonEQ-R%d-C%d-M%d-N%d.txt", prefix, row, col, midplane, nodeboard);
			}
			else {
				snprintf(pathname, sizeof(pathname), "%s/MonEQ-R%d-C%d-M%d-N%d.txt", prefix, row, col, midplane, nodeboard);
			}
		}
		else {
			snprintf(pathname, sizeof(pathname), "./MonEQ-R%d-C%d-M%d-N%d.txt", row, col, midplane, nodeboard);
		}

		fp = fopen(pathname, "w");
		if (!fp) {
			fprintf(stderr, "Failed to open: %s: %s\n", pathname, strerror(errno));
			moneq_err = new_moneq_error(EOPENOUTPUT, "Failed to open output file for writing.  Check errno for more information.");
		}
		else {
			for (int i = 0; i < measured_count; i++) {
				if (num_power_tags > 0) {
					// Check to see if there is a power tag associated with this measured count.
					struct power_tag *this_start_power_tag = find_start_power_tag_for_measured_count(i);
					struct power_tag *this_end_power_tag = find_end_power_tag_for_measured_count(i + 1);
					
					if (this_end_power_tag != 0) {
						fprintf(fp, "---MonEQ_END_POWER_TAG=%s\n", this_end_power_tag->tag_name);
						
						// Keep track of how many power tags we end up using--sanity check.
						num_used_power_tags++;
					}
					if (this_start_power_tag != 0) {
						fprintf(fp, "---MonEQ_START_POWER_TAG=%s\n", this_start_power_tag->tag_name);
					}
				}
				
				time_from_start = ((double) power_db[i].m_time - (double) start_ticks) / 1600e6;

				frac = modf(time_from_start, &intpart);
				cur_time.tv_sec = start_timeval.tv_sec + (int64_t) intpart;
				fracpart = (int32_t) (frac * 1000000.0);

				if ((start_timeval.tv_usec + fracpart) >= 1e6) {
					cur_time.tv_usec = (start_timeval.tv_usec + fracpart - 1e6);
					cur_time.tv_sec++;
				}
				else {
					cur_time.tv_usec = start_timeval.tv_usec + fracpart;
				}
				
				time_t rawtime = (time_t)cur_time.tv_sec;
				struct tm *timeinfo = localtime(&rawtime);
				char time_str_buffer[20];
				
				strftime (time_str_buffer, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
				
				// Compute time since start
				double time_since_start = power_db[i].mpi_wtime - start_mpi_wtime;

				// Print out the time Info
				//	Time string, cur time since epoch <sec>, ticks, MPI wall time
				fprintf(fp, "%s,%lld,%llu,%f", time_str_buffer, cur_time.tv_sec, power_db[i].m_time, time_since_start);
				fprintf(fp, "%d,%d,%d,%d,%.4f", row, col, midplane, nodeboard, power_db[i].m_card_power);

				double watt = 0.0;
				for (int dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
					watt = (power_db[i].m_voltage[dom * 2] * power_db[i].m_current[dom * 2]) + (power_db[i].m_voltage[(dom * 2) + 1] * power_db[i].m_current[(dom * 2) + 1]);

					watt = watt * (double) domain_info[dom].k_const;
					fprintf(fp, ",%.4f", watt);
				}
				
				fprintf(fp, "\n");
			}

			fclose(fp);
		}
		
		if (num_power_tags > 0) {
			if (num_used_power_tags != num_power_tags) {
				fprintf(stderr, "Did not end all started tags...are you sure you ended every start tag?\n");
			}
			
			// Push the tags to file if rank 0.
			if (MonEQ_world_rank == 0) {
				push_power_tags_to_file();
			}
		}
		
		if (power_db) {
			free(power_db);
			power_db = 0;
		}
		
		if (power_tags_db) {
			free(power_tags_db);
			power_tags_db = 0;
		}
	}

	return moneq_err->err_num;
}

void MonEQ_DisableAutoCollection(void) {
	MonEQ_enable_collection = 0;
}

int MonEQ_GetVTMRatio(uint32_t* arr) {
	int dom;

	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		arr[dom] = domain_info[dom].k_const;
	}
	return 0;
}

int MonEQ_GetDomainIDs(int *arr) {
	int dom;
	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		arr[dom] = domain_info[dom].domain_id;
	}

	return 0;
}

void MonEQ_PrintDomainInfo(void) {
	int dom;

	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		printf(" Index: %d, Q_Domain_ID: %d, Details: %s \n", dom, domain_info[dom].domain_id, bgq_dom_str[dom]);
	}

}

void MonEQ_PrintVTMRatio(void) {
	int dom;
	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		printf(" Index: %d Q_DomainID: %d VTM: %u\n", dom, domain_info[dom].domain_id, domain_info[dom].k_const);
	}
	return;
}

// There are 7 domains in the BGQ design
int MonEQ_GetNumDomains(void) {
	int num_domains = BGQ_MAX_DOMAINS;
	return num_domains;
}

int MonEQ_GetNumDCA(void) {
	int num_dca = BGQ_NUM_DCA;
	return num_dca;
}

double MonEQ_GetPower(void) {
	if (!MonEQ_agent_on_rank)
		return -1;

	return EMON_GetPower();
}

// Return the combined power consumed by each Domain
double MonEQ_GetDomainPower(double* arr) {
	double voltage[14], current[14];
	double power, watt;
	unsigned k_const;
	int dom;

	if (!MonEQ_agent_on_rank)
		return -1;

	power = MonEQ_GetVoltageAndCurrent(voltage, current);

	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		watt = (voltage[dom * 2] * current[dom * 2]) + (voltage[(dom * 2) + 1] * current[(dom * 2) + 1]);

		watt = watt * (double) domain_info[dom].k_const;

		arr[dom] = watt;
	}

	return power;
}

double MonEQ_GetDomainPowerByDCA(double* dca0, double* dca1) {
	double voltage[14], current[14];
	double power, watt0, watt1;
	unsigned k_const;
	int dom;

	if (!MonEQ_agent_on_rank)
		return -1;

	power = MonEQ_GetVoltageAndCurrent(voltage, current);

	for (dom = 0; dom < BGQ_MAX_DOMAINS; dom++) {
		watt0 = (voltage[dom * 2] * current[dom * 2]);
		watt1 = (voltage[(dom * 2) + 1] * current[(dom * 2) + 1]);

		watt0 = watt0 * (double) domain_info[dom].k_const;
		watt1 = watt1 * (double) domain_info[dom].k_const;

		dca0[dom] = watt0;
		dca1[dom] = watt1;
	}

	return power;
}

double MonEQ_GetVoltageAndCurrent(double* v, double* a) {
	double pw;

	if (!MonEQ_agent_on_rank)
		return -1;

	pw = EMON_GetPower_impl(v, a);
	return pw;
}

int MonEQ_SetFirstCollectionDelay(int seconds, int microSeconds) {
	// For some strange reason, 3 seconds seems to be near the lowest starting interval that should exist.
	// More investigation into this will be necessary.
	if (seconds < 3) {
		moneq_err = new_moneq_error(EFSTCOLLSHORT, "First collection delay too short.  Must be greater than or equal to 3 seconds.");
		return EFSTCOLLSHORT;
	}
	
	initialCollection_seconds = seconds;
	initialCollection_microSeconds = microSeconds;
	
	moneq_err = new_moneq_error(MonEQ_SUCCESS, "First collection delay sucessfully set.");
	return MonEQ_SUCCESS;
}

int MonEQ_SetCollectionInterval(int seconds, int microSeconds) {
	polling_seconds = seconds;
	polling_microSeconds = microSeconds;
	
	moneq_err = new_moneq_error(MonEQ_SUCCESS, "Collection interval sucessfully set.");
	return MonEQ_SUCCESS;
}


/* MonEQ Error Handling */

MonEQ_ERROR *new_moneq_error(int a_err_num, char *a_error_message) {
	MonEQ_ERROR *error = malloc(sizeof(MonEQ_ERROR));
	
	error->err_num = a_err_num;
	error->error_message = a_error_message;
	
	return error;
}

char *MonEQ_GetMessageForError(MonEQ_ERROR error) {
	return error.error_message;
}

char *MonEQ_GetLastErrorMessage() {
	if (moneq_err != 0) {
		return moneq_err->error_message;
	}
	
	return "No last error message.";
}