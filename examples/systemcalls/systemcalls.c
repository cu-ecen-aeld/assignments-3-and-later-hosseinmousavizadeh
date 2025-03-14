#include "systemcalls.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/wait.h>
#include <errno.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    int ret_val = system(cmd);
    if((ret_val == -1) || (!WIFEXITED(ret_val))) {
	    syslog(LOG_ERR, "system() failed: %X", ret_val);
	    return false;
    }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more  arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    syslog(LOG_INFO, "Start exec call");
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int ret = fork();
    if(ret < 0)
	    return false;
    if(ret == 0) {
        //The child process
        execv(command[0], command);
        exit(1);
    } 

    int status;
    ret = wait(&status);
    syslog(LOG_INFO, "Wait returned: %d status: %X",  ret, status);
    if(ret < 0) {
	    syslog(LOG_ERR, "wait function failed");
        return false;
    }
    if(WIFEXITED(status)) {
	    if(!WEXITSTATUS(status)) {
	        syslog(LOG_INFO, "Child returned successful");
            va_end(args);
	        return true;
        } else {
            syslog(LOG_ERR, "Child returned unsuccessful");
            va_end(args);
            return false;
        }
    }else {
        syslog(LOG_ERR, "wait failed");
        va_end(args);
	    return false;
    }
    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    syslog(LOG_INFO, "Start exec call redirect");
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    syslog(LOG_INFO, "Logging command arguments:");
    for (int i = 0; i < count; i++) {
        syslog(LOG_INFO, "Arg[%d]: %s", i, command[i]);
    }
/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/ 
    int fd = open(outputfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd < 0)
	    return false;
    int err = fork();
    if(err < 0)
	    return false;
    else if(err == 0) {
	    err = dup2(fd, 1);
        close(fd);
	    if(err < 0)
		    return false;
	    execv(command[0], command);
	    exit(-1);
    }

    int status;
    int ret = wait(&status);
    
    if(!ret) {
        return false;
    }
    if(WIFEXITED(status)) {
	    if(!WEXITSTATUS(status)) {
	        syslog(LOG_INFO, "Child returned successful");
            va_end(args);
            syslog(LOG_INFO, "Succeeded");
	        return true;
        } else {
            syslog(LOG_ERR, "Child returned unsuccessful");
            va_end(args);
            return false;
        }
    }else {
        syslog(LOG_ERR, "wait failed");
        va_end(args);
	    return false;
    }
    syslog(LOG_INFO, "Succeeded");
    va_end(args);
    return true;
}
