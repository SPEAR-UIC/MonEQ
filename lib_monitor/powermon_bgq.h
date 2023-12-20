#ifndef __POWERMON__BGQ__H
#define __POWERMON__BGQ__H

#ifndef __POWERMON__BGQ__ERR_H
#include "bgq_err.h"
#endif

#include <stdint.h>

// Needed for the timeval on the measurment
#include <sys/time.h>

/* Global variables for use in other programs */

// Maximum number of samples
#define  MAX_SAMPLES  16384

// Maximum number of tags
#define	 MAX_TAGS	  1000

// Details of the various Domains
// Taken from BGQ_power_modeling_LLNL.pdf
static const char *bgq_dom_str[] = {
	"Chip Core Voltage (0.9V)",
	"Chip Memory Interface and DRAM Voltage (1.35V)",
	"HSS network transceiver voltage compute+link chip (1.5V)",
	"Chip SRAM voltage (0.9 + 0.15V)",
	"Optics (2.5V)",
	"Optics + PCIexpress (3.5V)",
	"Link Chip Core (1V)"
};
// Short version great for use in CSV files
static const char *bgq_dom_headers[] = {
	"timestr",
	"time_since_epoch",
	"ticks",
	"row",
	"col",
	"midplane",
	"nodeboard",
	"node_card_power",
	"chip_core",
	"dram",
	"network",
	"sram",
	"optics",
	"PCIexpress,link_chip_core"
};
// Power tags filename
static const char POWER_TAG_FILENAME[] = "MonEQTags.txt";
// Power tag structure
struct power_tag {
	char *tag_name;
	int start_measured_count;
	int end_measured_count;
	double start_time;
	double end_time;
};

// Initialize the power monitoring mechanism
int MonEQ_Initialize(void);

// Finalize the power monitoring mechanism
int MonEQ_Finalize(void);

// The above two calls are sufficient for collecting data
// Below are calls needed if we want to instrument the
// application at a finer granularity.

// Disable the Data Collection
// This is enabled by default
// and needed for finegrained collection
void MonEQ_DisableAutoCollection(void);

// Function to identify if the monitoring agent is running
// on the current rank.
// Calls should be performed on the ranks where there is an agent
// 1 -> agent on rank
// 0 -> no agent on rank
int MonEQ_MonitorAgentOnRank(void);

// Get the total
double MonEQ_GetPower(void);

// Number of Domains for which measurement is available
// For BGQ, we currently have 7 domains
int MonEQ_GetNumDomains(void);

// Number of DCAs for which measurement is available
// For BGQ, we currently have 2 DCAs
int MonEQ_GetNumDCA(void);


// Returns the total power
// Populates v, and a with the voltage and current of the domains for both DCAs
// v and a must be an array of 14 elements each (# DCAs *  #NUM DOMAINS)
double MonEQ_GetVoltageAndCurrent(double* v, double* a);
double MonEQ_GetDomainPower(double* arr);
double MonEQ_GetDomainPowerByDCA(double* dca0, double* dca1);
int MonEQ_GetVTMRatio(uint32_t* arr);
void MonEQ_PrintVTMRatio(void);
int MonEQ_GetDomainIDs(int* arr);
void MonEQ_PrintDomainInfo(void);



/* Full customization methods */

// Set delay of first collection
int MonEQ_SetFirstCollectionDelay(int seconds, int microSeconds);

// Set the collection interval
int MonEQ_SetCollectionInterval(int seconds, int microSeconds);


/* Tagging */

// Start tagging
int MonEQ_StartPowerTag(char *tag_name);
// End tagging
int MonEQ_EndPowerTag(char *tag_name);
// Print tags
void MonEQ_PrintPowerTagsList(void);

#endif
