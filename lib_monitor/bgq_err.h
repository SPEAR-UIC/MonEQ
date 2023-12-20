#ifndef __POWERMON__BGQ__ERR__H
#define __POWERMON__BGQ__ERR__H

/* MonEQ Error Handling */

// Error numbers.
#define MonEQ_SUCCESS	0		/* General success */
#define ENOTAGENTONRANK	-1		/* This rank is not the agent rank */
#define EMKPWRTAGSP		-2		/* Unable to create a new power tag because of a space in the name */
#define EMKPWRTAG		-3		/* Unable to create a new power tag  */
#define EFINDPWRTAGNM	-4		/* Unable to find a power tag by name */
#define ESIGACTION		-5		/* Failed to set SIGACTION */
#define ESETTIMER		-6		/* Failed to set timer */
#define EOPENOUTPUT		-7		/* Failed to open output file for writing */
#define EEMONERR		-8		/* Status allreduction failed on EMON setup */
#define EFSTCOLLSHORT	-9		/* First collection delay too short */

// General error structure.
typedef struct MonEQ_ERROR {
	int err_num;
	char *error_message;
} MonEQ_ERROR;

// Method definitions

MonEQ_ERROR *new_moneq_error(int a_err_num, char *a_error_message);
char *MonEQ_GetMessageForError(MonEQ_ERROR error);
char *MonEQ_GetLastErrorMessage();

#endif