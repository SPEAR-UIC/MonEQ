/*
 * moneq_reducer.c
 *
 *      Author: Sean Wallace (swallac6@hawk.iit.edu)
 */

#include "powermon_bgq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

int num_power_tags = 0;

struct power_tag *power_tags_db[MAX_TAGS];

struct tag_output_file {
	char *tag_name;
	FILE *fd;
	int num_lines;
	struct power_line *power_lines[1000000];
	
	struct tag_output_file *next;
};

struct power_line {
	int time_since_epoch;
	int ticks;
	double mpi_wtime;
	int row;
	int col;
	int midplane;
	int nodeboard;
	double node_card_power;
	double chip_core;
	double dram;
	double network;
	double sram;
	double optics;
	double PCIexpress;
	double link_chip_core;
};

struct tag_output_file *tag_output_file_head = 0;
struct tag_output_file *tag_output_file_ptr = 0;

struct power_line *untagged_power_lines[MAX_SAMPLES];

int num_files = 0;

void add_power_tag(char *tag_name) {
	struct power_tag *new_power_tag = (struct power_tag*) malloc(sizeof(struct power_tag));
	
	if (new_power_tag == 0) {
		printf("Error creating new power tag (%s)!\n", tag_name);
		
		return;
	}
	
	new_power_tag->tag_name = tag_name;
	
	power_tags_db[num_power_tags] = new_power_tag;
	
	num_power_tags++;
}

struct power_tag* find_power_tag_for_measured_count(int measured_count) {
	int tag_num;
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = power_tags_db[tag_num];
		
		// Check both start and end.
		if (this_power_tag->start_measured_count == measured_count || this_power_tag->end_measured_count == measured_count) {
			return this_power_tag;
		}
	}
	
	return 0;
}

void print_power_tag_list(void) {
	int tag_num;
	printf("\n-------Printing Power Tags Start-------");
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = power_tags_db[tag_num];
		
		printf("\nTag Name: %s", this_power_tag->tag_name);
	}
	printf("\n-------Printing Power Tags End-------\n\n");
	
	return;
}

/* Unused */
void print_usage() {
	printf("Usage: moneq_reducer [options]\n\n");
	printf("Options:\n");
	printf("  -h Print table headers");
	printf("  --prefix= Prefix for files");
}

void read_tags() {
	puts("Reading tags from file.\n");
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	
	fp = fopen(POWER_TAG_FILENAME, "r");
	if (fp == NULL ) {
		printf("Unable to open tag file.  Assuming no tags used.  If there are tags in the output files, this might result in problems later.\n");
	}
	else {
		while ((read = getline(&line, &len, fp)) != -1) {
			char *tag_name = strdup(line);
			tag_name[strlen(tag_name) - 1] = '\0';
			
			add_power_tag(tag_name);
		}
		
		if (line) {
			free(line);
		}
	}
}

int check_line_for_start_tag(char *line) {
	char lineSubString[26];
	strncpy(lineSubString, line, 25);
	lineSubString[25] = '\0';
	
	int is_start_tag = strcmp("---MonEQ_START_POWER_TAG=", lineSubString);
	
	return is_start_tag;
}

int check_line_for_end_tag(char *line) {
	char lineSubString[24];
	strncpy(lineSubString, line, 23);
	lineSubString[23] = '\0';
	
	int is_end_tag = strcmp("---MonEQ_END_POWER_TAG=", lineSubString);
	
	return is_end_tag;
}

struct power_line *parse_power_line(char *line, int line_number) {
	// Push out each field one at a time formatting and inserting extras as necessary
	char *thisLine = strdup(line);
	
	struct power_line *this_power_line = (struct power_line *) malloc(sizeof(struct power_line));
	
	char *pch = strtok(thisLine, ",");
	int i = 0;
	//int start_time = 0;
	while (pch != NULL) {
		if (i == 0) {
			// Time string, makes gnuplot freak out, so don't include it.
		}
		else if (i == 1) { // Time since epoch
			this_power_line->time_since_epoch = (int) strtol(pch, NULL, 0);
		}
		else if (i == 2) {	// Ticks
			this_power_line->ticks = (int) strtol(pch, NULL, 0);
		}
		else if (i == 3) {	// MPI Wtime
			// We know from hardware limitations that the closest value must be to the nearest 560 ms, so round.
			double mpi_wtime = (double) strtold(pch, NULL);
			double rounded = floorl(mpi_wtime * 100 + 0.56) / 100;
			
			this_power_line->mpi_wtime = rounded;
		}
		else if (i == 4) {	// Row
			this_power_line->row = (int) strtol(pch, NULL, 0);
		}
		else if (i == 5) {	// Column
			this_power_line->col = (int) strtol(pch, NULL, 0);
		}
		else if (i == 6) {	// Midplane
			this_power_line->midplane = (int) strtol(pch, NULL, 0);
		}
		else if (i == 7) {	// Node Card Power
			this_power_line->node_card_power = strtod(pch, NULL);
			
			if (this_power_line->node_card_power == 0) {
				puts("Invalid line!");
				return 0;
			}
		}
		else if (i == 8) {	// Chip Core
			this_power_line->chip_core = strtod(pch, NULL);
		}
		else if (i == 9) {	// DRAM
			this_power_line->dram = strtod(pch, NULL);
		}
		else if (i == 10) {	// Network
			this_power_line->network = strtod(pch, NULL);
		}
		else if (i == 11) {	// SRAM
			this_power_line->sram = strtod(pch, NULL);
		}
		else if (i == 12) {	// Optics
			this_power_line->optics = strtod(pch, NULL);
		}
		else if (i == 13) {	// PCIExpress
			this_power_line->PCIexpress = strtod(pch, NULL);
		}
		else if (i == 14) {	// Link Chip Core
			this_power_line->link_chip_core = strtod(pch, NULL);
		}
		else {
			printf("\n\nGot more columns than expected (i: %d, content: %s)!\n\n", i, pch);
		}
		
		pch = strtok(NULL, ",");
		
		i++;
	}
	
	//printf("Parsed power line - seconds: %d, node card power: %f\n", this_power_line->time_since_epoch - overall_start_time, this_power_line->node_card_power);
	
	return this_power_line;
}

struct tag_output_file *add_tag_output_file(char *tag_name, FILE *fd) {
	struct tag_output_file *this_tag_output_file = (struct tag_output_file*) malloc(sizeof(struct tag_output_file));
	
	this_tag_output_file->tag_name = tag_name;
	this_tag_output_file->fd = fd;
	this_tag_output_file->num_lines = 0;
	this_tag_output_file->next = 0;
	
	if (tag_output_file_head == 0) {
		// First tag file
		tag_output_file_head = this_tag_output_file;
	}
	else {
		tag_output_file_ptr->next = this_tag_output_file;
	}
	
	tag_output_file_ptr = this_tag_output_file;
	
	if (strcmp(tag_name, "untagged") == 0 && strcmp(tag_name, "original") == 0) {
		printf("Added tag output file for tag: %s\n", tag_output_file_ptr->tag_name);
	}
	
	return this_tag_output_file;
}

void parse_moneq_files() {
	puts("Parsing MonEQ files.");
	
	int tag_num, i, j;
	
	// Make the directory
	if (mkdir("moneq_output", S_IRWXU) == -1) {
		if (errno != EEXIST) {
			printf("Error creating output directory!\n");
		}
	}
	
	// First, open a fd for each tag
	for (tag_num = 0; tag_num < num_power_tags; tag_num++) {
		struct power_tag *this_power_tag = power_tags_db[tag_num];
		
		int filename_length = (int)strlen(this_power_tag->tag_name) + 18;
		char this_tag_filename[filename_length];
		int cx;
		
		cx = snprintf(this_tag_filename, filename_length, "moneq_output/%s.dat", this_power_tag->tag_name);
		
		FILE *this_fd = fopen(this_tag_filename, "w");
		
		if (!this_fd) {
			printf("Failed to open %s!\n", this_tag_filename);
		}
		else {
			add_tag_output_file(this_power_tag->tag_name, this_fd);
		}
	}
	
	FILE *original_fd = fopen("moneq_output/original.csv", "w");
	
	fprintf(original_fd, "date_time,time_since_epoch,ticks,mpi_wtime,row,col,midplane,nodecard,chip_core,dram,network,sram,optics,pciexpress,link_chip_core\n");
	
	FILE *untagged_fd = NULL;
	FILE *original_dat_fd = fopen("moneq_output/original.dat", "w");
	
	// And add the original FD to the tag list, just so we're always making a complete graph.
	struct tag_output_file *overall_tag_output_file = add_tag_output_file("original", original_dat_fd);
	
	// Add the untagged FD to the tag list only if there are tags (otherwise this untagged file will contain the exact same information as the original).
	struct tag_output_file *untagged_tag_output_file = NULL;
	if (num_power_tags > 0) {
		untagged_fd = fopen("moneq_output/untagged.dat", "w");
		untagged_tag_output_file = add_tag_output_file("untagged", untagged_fd);
	}
	
	DIR *dp;
	struct dirent *file;
	
	int num_untagged_power_lines = 0;
	
	dp = opendir("./");
	if (dp != NULL ) {
		while ((file = readdir(dp))) {
			char subString[7];
			strncpy(subString, file->d_name, 6);
			subString[6] = '\0';
			
			int matchesPattern = strcmp("MonEQ-", subString);
			
			if (matchesPattern == 0) {
				num_files++;
				// Open the file, read through each line until we hit special characters which signify tag,
				// push everything from that line forward to the next tag occurance to the tag output file
				//
				printf("\n---Working with: %s\n\n", file->d_name);
				
				FILE *this_input_file_fd;
				char *line = NULL;
				size_t len = 0;
				ssize_t read;
				int line_num = 1;
				
				this_input_file_fd = fopen(file->d_name, "r");
				if (this_input_file_fd == NULL) {
					printf("Unable to open input file %s!\n", file->d_name);
				}
				
				int untagged_line_number = 1;
				
				struct power_line *this_overall_power_line = 0;
				
				while ((read = getline(&line, &len, this_input_file_fd)) != -1) {
					int is_start_tag_line = check_line_for_start_tag(line);
					int is_end_tag_line = check_line_for_end_tag(line);
					
					// Unless this is an actual tag line, push this to the original file.
					if (is_start_tag_line != 0 && is_end_tag_line != 0) {
						fprintf(original_fd, "%s", line);
					}
					
					this_overall_power_line = parse_power_line(line, line_num);
					
					if (this_overall_power_line != 0) {
						// Regardless, push this line to the overall data structure for later output.
						overall_tag_output_file->power_lines[overall_tag_output_file->num_lines] = this_overall_power_line;
						overall_tag_output_file->num_lines++;
						
						// Not a tag line, but still want to process it so it's standardized for gnuplot
						if (is_start_tag_line != 0) {
							if (num_power_tags > 0) {
								struct power_line *this_power_line = parse_power_line(line, untagged_line_number);
								
								untagged_tag_output_file->power_lines[num_untagged_power_lines] = this_power_line;
								untagged_tag_output_file->num_lines++;
								
								untagged_line_number++;
								num_untagged_power_lines++;
							}
						}
						else { //This is a tag line
							int tag_start_pos = 25;
							size_t tag_length = len - tag_start_pos;
							char tag_name[tag_length];
							
							int i;
							for (i = tag_start_pos; i < len; i++) {
								if (line[i] == ' ' || line[i] == '\n') {
									break;
								}
								
								tag_name[i - tag_start_pos] = line[i];
							}
							tag_name[i - tag_start_pos] = '\0';
							
							printf("Found tag \'%s\' on line %d\n", tag_name, line_num);
							
							int found_tag_output_file = 0;
							struct tag_output_file *this_tag_output_file = tag_output_file_head;
							
							if (this_tag_output_file == 0) {
								printf("No tag output files!");
								exit(EXIT_FAILURE);
							}
							
							while (this_tag_output_file != 0) {
								if (strcmp(this_tag_output_file->tag_name, tag_name) == 0) { // Matching tag output file
									found_tag_output_file = 1;
									break;
								}
								else {
									this_tag_output_file = this_tag_output_file->next;
								}
							}
							
							// Check to make sure we found an output file
							if (!found_tag_output_file) {
								printf("Unable to find output file for tag: %s!", tag_name);
								exit(EXIT_FAILURE);
							}
							
							// Continue reading the file and pushing line from the input to the output
							// until EOF or another tag.
							
							// For better output on graphs, let's calculate seconds of execution.
							int thisTag_line_num = 1;
							
							while ((read = getline(&line, &len, this_input_file_fd)) != -1 && check_line_for_end_tag(line) != 0) {
								// Need to make sure that we're still outputing this file to the overall file.
								this_overall_power_line = parse_power_line(line, line_num);
								
								if (this_overall_power_line != 0) {
									overall_tag_output_file->power_lines[overall_tag_output_file->num_lines] = this_overall_power_line;
									overall_tag_output_file->num_lines++;
									
									fprintf(original_fd, "%s", line);
									
									
									// Now we can worry about the tag.
									struct power_line *this_power_line = parse_power_line(line, thisTag_line_num);
									
									this_tag_output_file->power_lines[this_tag_output_file->num_lines] = this_power_line;
									this_tag_output_file->num_lines++;
								}
								
								line_num++;
								thisTag_line_num++;
							}
						}
					}
					
					line_num++;
				}
			}
		}
		
		// Done with the files, close everything up.
		(void) closedir(dp);
		fclose(original_fd);
		
		// Now process the lines loaded into memory.
		struct tag_output_file *this_tag_output_file = tag_output_file_head;
		
		while (this_tag_output_file != 0) {
			if (this_tag_output_file == overall_tag_output_file) {
				printf("\n\nNumber of overall lines: %d\n", this_tag_output_file->num_lines);
			}
			FILE *output_file_fd = this_tag_output_file->fd;
			
			// Print table headers
			fprintf(output_file_fd, "#seconds\tnode_card_power\tchip_core\tdram\tnetwork\tsram\toptics\tPCIexpress\tlink_chip_core\n");
			
			double node_card_power	= -1;
			double chip_core		= -1;
			double dram				= -1;
			double network			= -1;
			double sram				= -1;
			double optics			= -1;
			double PCIexpress		= -1;
			double link_chip_core	= -1;
			
			double this_second = -1;
			double last_second = -1;
			
			for (i = 0; i < this_tag_output_file->num_lines; i++) {
				struct power_line *this_power_line = this_tag_output_file->power_lines[i];
				
				if (this_power_line->node_card_power > 0) {
					this_second = this_power_line->mpi_wtime;
					
					// Check to see if we've moved onto the next file.
					// If this_second is less than last_second, we know that we've moved onto the next node card's data.
					// This might be cheating...
					if (this_second < last_second) {
						break;
					}
					
					// We've already processed everything with this second, now we just need to seek to the next "second"
					while (this_second == last_second && (i + 1) < this_tag_output_file->num_lines) {
						i++;
						this_power_line = this_tag_output_file->power_lines[i];
						
						last_second = this_second;
						this_second = this_power_line->mpi_wtime;
					}
					
					// Need one last increment.
					i++;
					
					// Gather data for this second of this line
					node_card_power = this_power_line->node_card_power;
					chip_core = this_power_line->chip_core;
					dram = this_power_line->dram;
					network = this_power_line->network;
					sram = this_power_line->sram;
					optics = this_power_line->optics;
					PCIexpress = this_power_line->PCIexpress;
					link_chip_core = this_power_line->link_chip_core;
					
					int num_samples_at_this_second = 1;
					
					// Walk through the remaining lines looking for matching seconds meaning data should be averaged for this time, so sum it up.
					for (j = i+1; j < this_tag_output_file->num_lines; j++) {
						struct power_line *this_potential_power_line = this_tag_output_file->power_lines[j];
						
						if (this_potential_power_line->mpi_wtime == this_power_line->mpi_wtime) {
							node_card_power += this_potential_power_line->node_card_power;
							chip_core += this_potential_power_line->chip_core;
							dram += this_potential_power_line->dram;
							network += this_potential_power_line->network;
							sram += this_potential_power_line->sram;
							optics += this_potential_power_line->optics;
							PCIexpress += this_potential_power_line->PCIexpress;
							link_chip_core += this_potential_power_line->link_chip_core;
							
							num_samples_at_this_second++;
						}
					}
					
					// Now the variables all contain the sum from lines of matching seconds,
					// divide by the number of lines at this second to get average.
					node_card_power /= num_samples_at_this_second;
					chip_core /= num_samples_at_this_second;
					dram /= num_samples_at_this_second;
					network /= num_samples_at_this_second;
					sram /= num_samples_at_this_second;
					optics /= num_samples_at_this_second;
					PCIexpress /= num_samples_at_this_second;
					link_chip_core /= num_samples_at_this_second;
					
					// Finally, all values are averages for this second, so we can push this to the output file.
					fprintf(output_file_fd, "%.2f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", this_second, node_card_power, chip_core, dram, network, sram, optics, PCIexpress, link_chip_core);
					
					last_second = this_second;
				}
			}
			
			this_tag_output_file = this_tag_output_file->next;
		}
	}
	else {
		perror("Couldn't open the directory");
	}
}

void close_output_files() {
	struct tag_output_file *previous_tag_output_file = 0;
	struct tag_output_file *this_tag_output_file = tag_output_file_head;
	
	while (this_tag_output_file != 0) {
		previous_tag_output_file = this_tag_output_file;
		this_tag_output_file = this_tag_output_file->next;
		
		fclose(previous_tag_output_file->fd);
		free(previous_tag_output_file);
	}
	
	tag_output_file_head = 0;
	tag_output_file_ptr = 0;
}

int main(int argc, char *argv[]) {
	read_tags();
	print_power_tag_list();
	parse_moneq_files();
	close_output_files();
	
	return 0;
}