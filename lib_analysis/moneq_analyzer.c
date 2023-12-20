/*
 * moneq_analyzer.c
 *
 *      Author: Sean Wallace (swallac6@hawk.iit.edu)
 */

#include "gnuplot_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

struct domain {
	char *domain_name;
	int field_number;
	int included;
};

struct domain *create_domain(char *domain_name, int field_number, int included) {
	struct domain *this_domain = (struct domain*) malloc(sizeof(struct domain));

	this_domain->domain_name = domain_name;
	this_domain->field_number = field_number;
	this_domain->included = included;

	return this_domain;
}

struct domain *domains[9];

int push_results_to_file = 0;
char *directory = NULL;

gnuplot_ctrl *gnuplot;

// File format
//seconds, node_card_power, chip_core, dram, network, sram, optics, PCIexpress, link_chip_core

void print_usage() {
	printf("Usage: moneq_analyzer [option(s)]\n\n");
	printf("Options:\n");
	printf("  -f Push results to PNG files\n");
	printf("  -d Work in directory specified\n");
	printf("Individual Domains (default is to include all but the node card):\n");
	printf("  -a Include all domains\n");
	printf("  -n (repeated as necessary) Include domain n where n is:\n");
	printf("    1: Node Card\n");
	printf("    2: Chip Core\n");
	printf("    3: DRAM\n");
	printf("    4: Network\n");
	printf("    5: SRAM\n");
	printf("    6: Optics\n");
	printf("    7: PCIExpress\n");
	printf("    8: Link Chip Core\n");
}

void parse_args(int argc, char *argv[]) {
	int i, d;
	int include_all_domains = 0;
	for (i = 0; i < argc; i++) {
		if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0) {
			print_usage();
			exit(0);
		}
		if (strcmp("-f", argv[i]) == 0) {
			push_results_to_file = 1;
			printf("Pushing results to file.\n");
		}
		if (strcmp("-d", argv[i]) == 0) {
			directory = argv[i+1];
			printf("Working in directory: %s\n", directory);
		}
		if (strcmp("-a", argv[i]) == 0) {
			printf("Including all domains\n");

			for (d = 1; d <= 8; d++) {
				struct domain *this_domain = domains[d];

				this_domain->included = 1;
			}

			include_all_domains = 1;
		}
	}

	int include_a_domain = 0;
	if (include_all_domains == 0) {
		for (d = 1; d <= 8; d++) {
			struct domain *this_domain = domains[d];
			int should_include = 0;

			for (i = 0; i < argc; i++) {
				char this_flag[3];

				snprintf(this_flag, 3, "-%d", d);

				if (strcmp(argv[i], this_flag) == 0) {
					should_include = 1;
					include_a_domain = 1;
				}
			}

			this_domain->included = should_include;

			if (should_include) {
				printf("Including domain: %s\n", this_domain->domain_name);
			}
		}
	}

	if (!include_a_domain && !include_all_domains) {
		printf("Including all domains but node card\n");

		for (d = 2; d <= 8; d++) {
			struct domain *this_domain = domains[d];

			this_domain->included = 1;
		}
	}
}

char *get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

char *remove_ext (char* mystr, char dot, char sep) {
    char *retstr, *lastdot, *lastsep;

    // Error checks and allocate string.

    if (mystr == NULL)
        return NULL;
    if ((retstr = malloc(strlen(mystr) + 1)) == NULL)
        return NULL;

    // Make a copy and find the relevant characters.

    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

    // If it has an extension separator.

    if (lastdot != NULL) {
        // and it's before the extenstion separator.

        if (lastsep != NULL) {
            if (lastsep < lastdot) {
                // then remove it.

                *lastdot = '\0';
            }
        } else {
            // Has extension separator with no path separator.

            *lastdot = '\0';
        }
    }

    // Return the modified string.

    return retstr;
}

int main(int argc, char *argv[]) {
	domains[0] = 0;
	domains[1] = create_domain("Node Card", 2, 0);
	domains[2] = create_domain("Chip Core", 3, 1);
	domains[3] = create_domain("DRAM", 4, 1);
	domains[4] = create_domain("Network", 5, 1);
	domains[5] = create_domain("SRAM", 6, 1);
	domains[6] = create_domain("Optics", 7, 1);
	domains[7] = create_domain("PCI Express", 8, 1);
	domains[8] = create_domain("Link Chip Core", 9, 1);

	parse_args(argc, argv);

	DIR *dp;
	struct dirent *file;
	int d;

	if (directory == NULL) {
		directory = "./";
	}

	dp = opendir(directory);

	char plot_commands[2000];

	if (dp != NULL ) {
		while ((file = readdir(dp))) {
			char *extension = get_filename_ext(file->d_name);


			int matchesPattern = strcmp("dat", extension);

			if (matchesPattern == 0) {
				gnuplot = gnuplot_init();

				printf("Found file: %s\n", file->d_name);

				char *tag_name = remove_ext(file->d_name, '.', '/');

				gnuplot_set_xlabel(gnuplot, "Execution Time (Seconds)");
				gnuplot_set_ylabel(gnuplot, "Power (Watts)");

				if (push_results_to_file) {
					gnuplot_cmd(gnuplot, "set term png");
					gnuplot_cmd(gnuplot, "set output\'%s.png\'", tag_name);
				}

				char filename[256];
				snprintf(filename, sizeof(filename), "%s/%s", directory, file->d_name);

				if (strcmp(tag_name, "original") == 0) {
					gnuplot_cmd(gnuplot, "set title \"Overall\"");
				}
				else if (strcmp(tag_name, "untagged") == 0) {
					gnuplot_cmd(gnuplot, "set title \"Untagged\"");
				}
				else {
					gnuplot_cmd(gnuplot, "set title \"Tag Name: %s\"", tag_name);
				}

				// Prepare what we should pass to the command
				memset(plot_commands, '\0', 2000);

				strcat(plot_commands, "plot ");

				for (d = 1; d <= 8; d++) {
					struct domain *this_domain = domains[d];

					if (this_domain->included) {
						char this_command[255];

						//snprintf(this_command, 255, "\"./%s\" using 1:%d title '%s' with lines", filename, this_domain->field_number, this_domain->domain_name);

                        if (d != 8) {
						    snprintf(this_command, 255, "\"./%s\" using 1:%d title '%s' with lines,", filename, this_domain->field_number, this_domain->domain_name);
                        }
                        else {
                            snprintf(this_command, 255, "\"./%s\" using 1:%d title '%s' with lines", filename, this_domain->field_number, this_domain->domain_name);
                        }

						strcat(plot_commands, this_command);
					}
				}

				gnuplot_cmd(gnuplot, plot_commands);

				gnuplot_close(gnuplot);
			}
		}
	}

	return 0;
}
