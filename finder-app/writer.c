#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	FILE *fptr;

	openlog("writer", 0, LOG_USER);
	if(argc <= 2) {
		syslog(LOG_ERR, "Too few arguments");
	}
	
	fptr = fopen(argv[1], "w");
	if(fptr == NULL) {
		syslog(LOG_ERR, "Failed opening the file");
		return 1;
	}
	
	fprintf(fptr, "%s", argv[2]); 	
	syslog(LOG_DEBUG, "Writing %s to %s", argv[1], argv[2]);
	return 0;
}
