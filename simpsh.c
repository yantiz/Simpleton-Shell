#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAX_FDS 1024
#define MAX_ARGV_NUM 1024
#define MAX_PROC_NUM 1024

// store all opts
char *OPTS[] = {
    "--append",
    "--cloexec",
    "--creat",
    "--directory",
    "--dsync",
    "--excl",
    "--nofollow",
    "--nonblock",
    "--rsync",
    "--sync",
    "--trunc",
    "--rdonly",
    "--rdwr",
    "--wronly",
    "--pipe",
    "--command",
    "--wait",
    "--close",
    "--verbose",
    "--profile",
    "--abort",
    "--catch",
    "--ignore",
    "--default",
    "--pause",
};

/**
 *  store child process info
 */
typedef struct {
    int pid;
    char *cmd_argv[MAX_ARGV_NUM];
    int cmd_argv_len;
    int status;
} proc_info;

/**
 * Judge if str is a opt, return 1 if true else return 0
 */
int is_opt(char *str) {
    int i = 0;
    for (i = 0; i < (int)(sizeof(OPTS) / sizeof(OPTS[0])); i++) {
        if (strcmp(str, OPTS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * signal handler
 */
char message[20] = "signal caught: ";
void catch(int sig) {
    char single_digit = (sig % 10) + '0';
    char ten_digit = ((sig / 10) % 10) + '0';
    char digits[] = {ten_digit, single_digit, '\n'};    
    strcat(message, digits);
    write(2, message, sizeof(message));
    _exit(sig);
}

/**
 * ignore handler
 */
jmp_buf env;
void ignore(int sig) {
  if (sig == SIGSEGV)
    longjmp(env, 1);
  else
    signal(sig, SIG_IGN);
}

int main(int argc, char *argv[])
{
    /* Parse argv directly is more simple than use getopt_long */

    // set up the jump value
    int val;

    // Indicate verbose
    int verbose = 0;

    // save file open flags, need clear after open
    int open_flags = 0, flag_index = 0;
    char *flag_list[11];

    // store all file descriptors
    int fds[MAX_FDS];
    int fds_len = 0;

    // store all child processes
    proc_info *procs = calloc(MAX_PROC_NUM, sizeof(proc_info));
    int procs_len = 0;

    // indicate if error happend
    int error = 0;

    int argc_n = 0;
    int i = 0, j = 0;
    for (argc_n = 0; argc_n < argc; argc_n++) {
        if (strcmp(argv[argc_n], "--verbose") == 0) {
	    open_flags = 0;
            // verbose opt, set verbose flag and print this opt
            verbose = 1;
            continue;
        }
        if (strcmp(argv[argc_n], "--append") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_APPEND;
            continue;
              
        }
        if (strcmp(argv[argc_n], "--cloexec") == 0) {         
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_CLOEXEC;
            continue;
        }
        if (strcmp(argv[argc_n], "--creat") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_CREAT;
            continue;
        }
        if (strcmp(argv[argc_n], "--directory") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_DIRECTORY;
            continue;
        }
        if (strcmp(argv[argc_n], "--dsync") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_DSYNC;
            continue;
        }
        if (strcmp(argv[argc_n], "--excl") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_EXCL;
            continue;
        }
        if (strcmp(argv[argc_n], "--nofollow") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_NOFOLLOW;
            continue;
        }
        if (strcmp(argv[argc_n], "--nonblock") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_NONBLOCK;
            continue;
        }
        if (strcmp(argv[argc_n], "--rsync") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_RSYNC;
            continue;
        }
        if (strcmp(argv[argc_n], "--sync") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_SYNC;
            continue;
        }
        if (strcmp(argv[argc_n], "--trunc") == 0) {
	    if (verbose) flag_list[flag_index++] = argv[argc_n];
            open_flags |= O_TRUNC;
            continue;
        }
        if (strcmp(argv[argc_n], "--rdonly") == 0
            || strcmp(argv[argc_n], "--rdwr") == 0
            || strcmp(argv[argc_n], "--wronly") == 0) {
            // need open file
            if (strcmp(argv[argc_n], "--rdonly") == 0) {
                open_flags |= O_RDONLY;
            } else if (strcmp(argv[argc_n], "--rdwr") == 0) {
                open_flags |= O_RDWR;
            } else {
                open_flags |= O_WRONLY;
            }
            if ((argc_n + 1) >= argc || is_opt(argv[argc_n + 1])) {
                // no file name
                fprintf(stderr, "file name not found after %s\n", argv[argc_n]);
                error = 1;
		open_flags = 0;
                continue;
            }
            // open file, argv[argc_n+1] if filename
            int fd = open(argv[argc_n + 1], open_flags, S_IRUSR | S_IWUSR);
            if (fd < 0) {
                fprintf(stderr, "open file failed %s: %s\n", argv[argc_n + 1], strerror(errno));
                error = 1;
		open_flags = 0;
		continue;
            } else {
                // append fd into fds
                fds[fds_len++] = fd;
            }
            // after open file, clear flags
            open_flags = 0;
	    if (verbose) {
	      for(i = 0; i < flag_index; i++)
		printf("%s ", flag_list[i]);
	      flag_index = 0;
	      printf("%s %s\n", argv[argc_n], argv[argc_n + 1]);
	    }
            // skip filename
            argc_n++;
            continue;
        }
        if (strcmp(argv[argc_n], "--pipe") == 0) {
	    open_flags = 0;
	    if(verbose) {
	        flag_index = 0;
	        printf("--pipe\n");
	    }
            // create pipe and append into fds
            int pfds[2];
            pipe(pfds);
            fds[fds_len++] = pfds[0];
            fds[fds_len++] = pfds[1];
            continue;
        }
        if (strcmp(argv[argc_n], "--command") == 0) {
	    open_flags = 0;
            // --command i o e cmd args
            int t = 1;
            while (argc_n + t < argc && is_opt(argv[argc_n + t]) == 0) {
                t++;
            }
	    // at least have 4 arguments, otherwise error
            if (t < 5) {
                fprintf(stderr, "command invalid\n");
                error = 1;
                argc_n += t - 1;
                continue;
            }
	    // if i o e are not pure digits, error
	    int format_error = 0;
	    char *in = argv[argc_n + 1];
	    char *out = argv[argc_n + 2];
	    char *err = argv[argc_n + 3];
	    while (*in != '\0') {
	        if(!isdigit(*in)){
		  format_error = 1;
		  goto format_error_detected;
	        }
	        in++;
	    }
	    while (*out != '\0') {
  	        if(!isdigit(*out)) {
		  format_error = 1;
		  goto format_error_detected;
	        }
	        out++;
	    }
	    while (*err != '\0') {
	        if(!isdigit(*err)) {
		  format_error = 1;
		  goto format_error_detected;
	        }
		err++;
	    }	    	    
	    format_error_detected:
	    if (format_error) {
	        fprintf(stderr, "i o e wrong format\n");
	        error = 1;
	        argc_n += t - 1;
	        continue;
	    }	    
            // get i o e
            int ifd = atoi(argv[argc_n + 1]);
            int ofd = atoi(argv[argc_n + 2]);
            int efd = atoi(argv[argc_n + 3]);	    
            // if i o e not in fds, error
            if (ifd >= fds_len || ofd >= fds_len || efd >= fds_len) {
                fprintf(stderr, "i o e invalid\n");
                error = 1;
                argc_n += t - 1;
                continue;
            }
	    if (fds[ifd] < 0 || fds[ofd] < 0 || fds[efd] < 0) {
	        fprintf(stderr, "access denied, file(s) already closed\n");
	        error = 1;
		argc_n += t - 1;
                continue;
	    }
            if (verbose) {
	        flag_index = 0;
    	        for(i = 0; i < t; i++)
		  printf("%s ", argv[argc_n + i]);
		printf("\n");
            }
            // save argv in proc
            proc_info proc;
            proc.cmd_argv_len = 0;
            for (i = 4; i < t; i++) {
                proc.cmd_argv[proc.cmd_argv_len++] = argv[argc_n + i];
            }
            proc.cmd_argv[proc.cmd_argv_len] = NULL;
            // execute command use execvp
            int pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork failed: %s\n", strerror(errno));
                error = 1;
		continue;
            } else if (pid == 0) {
                // child process
                // redirect ios
                for (i = 0; i < fds_len; i++) {
                    // close unused fd
                    if (i != ifd && i != ofd && i != efd) {
                        close(fds[i]);
			fds[i] = -1;
                    }
                }
                // dup stdin stdout stderr
                dup2(fds[ifd], 0);
                dup2(fds[ofd], 1);
                dup2(fds[efd], 2);
                execvp(proc.cmd_argv[0], proc.cmd_argv);
		// execvp fails if reaching the following two lines
		fprintf(stderr, "execvp failed: %s\n", strerror(errno));
		error = 1;
                _exit(errno);
            } else {
                // parent process
                proc.pid = pid;
                proc.status = 0;
                procs[procs_len++] = proc;
            }	   
	    argc_n += t - 1;
            continue;
        }
        if (strcmp(argv[argc_n], "--wait") == 0) {
	    open_flags = 0;
   	    if(verbose) {
	        flag_index = 0;
	        printf("--wait\n"); 
	    }
            // before wait, parent should close all fds(or pipes will not work)
            for (i = 0; i < fds_len; i++) {
                if (fds[i] >= 0) {
                    close(fds[i]);
                    fds[i] = -1;
                }
            }
            for (i = 0; i < procs_len; i++) {
                // wait each child process, and report status and cmd
                int ret = waitpid(procs[i].pid, &procs[i].status, 0);
		if (ret != procs[i].pid) {
		  fprintf(stderr, "wait failed on process %d: %s\n", procs[i].pid, strerror(errno));
		  error = 1;
		  continue;
		}
		if (!WIFEXITED(procs[i].status)) {
		  fprintf(stderr, "process %d exited abnormally: %s\n", procs[i].pid, strerror(errno));
		  error = 1;
		  continue;
		}
                printf("%d", WEXITSTATUS(procs[i].status));
                for (j = 0; j < procs[i].cmd_argv_len; j++) {
                    printf(" %s", procs[i].cmd_argv[j]);
                }
                printf("\n");
            }
            continue;
        }
	if (strcmp(argv[argc_n], "--close") == 0) {
	    open_flags = 0;
	    int filenum_error = 0;
	    char *fn = argv[argc_n + 1];
	    while (*fn != '\0') {
	        if (!isdigit(*fn)) {
		    filenum_error = 1;
		    break;
	        }	        
		fn++;
	    }
	    if (filenum_error) {
 	        fprintf(stderr, "file number invalid\n");
		error = 1;
		argc_n++;
		continue;
	    }
	    int filenum= atoi(argv[argc_n + 1]);
	    if (filenum >= fds_len) {
	        fprintf(stderr, "file number out of range\n");
		error = 1;
		argc_n++;
		continue;
	    }
	    if(verbose) {
	        flag_index = 0;
		printf("%s %s\n", argv[argc_n], argv[argc_n + 1]);
	    }	    
	    close(fds[filenum]);
	    fds[filenum] = -1;
	    argc_n++;
	    continue;
	}
        if (strcmp(argv[argc_n], "--profile") == 0) {
	    open_flags = 0;  
	    if(verbose) {
	        flag_index = 0;
	        printf("--profile\n");
	    }	    
	    // before wait, parent should close all fds(or pipes will not work)
	    for (i = 0; i < fds_len; i++) {
                if (fds[i] >= 0) {
                    close(fds[i]);
                    fds[i] = -1;
                }
            }
	    struct rusage usage;
            for (i = 0; i < procs_len; i++) {
	      wait4(procs[i].pid, &procs[i].status, 0, &usage);
	      getrusage(RUSAGE_CHILDREN, &usage);
              printf("resources used by child process %d:\n  ru_utime=%ldus ru_stime=%ldus ru_maxrss=%ld ru_ixrss=%ld ru_idrss=%ld "
                     "ru_isrss=%ld ru_minflt=%ld ru_majflt=%ld ru_nswap=%ld ru_inblock=%ld "
                     "ru_oublock=%ld ru_msgsnd=%ld ru_msgrcv=%ld ru_nsignals=%ld ru_nvcsw=%ld "
		     "ru_nivcsw=%ld\n", procs[i].pid, usage.ru_utime.tv_sec*1000000+usage.ru_utime.tv_usec,
		      usage.ru_stime.tv_sec*1000000+usage.ru_stime.tv_usec, usage.ru_maxrss,
                      usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss, usage.ru_minflt, usage.ru_majflt,
                      usage.ru_nswap, usage.ru_inblock, usage.ru_oublock, usage.ru_msgsnd, usage.ru_msgrcv,
                      usage.ru_nsignals, usage.ru_nvcsw, usage.ru_nivcsw);
	    }
	    continue;
        }
        if (strcmp(argv[argc_n], "--abort") == 0) {
	    open_flags = 0;  
	    if(verbose) {
	        flag_index = 0;
	        printf("--abort\n");
	    }
	    val = setjmp(env);
	    if (!val) {
	      char *p = NULL;
	      *p = 1;
	    }
	    continue;
        }
	if (strcmp(argv[argc_n], "--catch") == 0
            || strcmp(argv[argc_n], "--ignore") == 0
            || strcmp(argv[argc_n], "--default") == 0) {
	    open_flags = 0;  	 	    
            if (argc_n + 1 >= argc || is_opt(argv[argc_n + 1])) {
                // no signal
                fprintf(stderr, "signal not found after %s\n", argv[argc_n]);
		error = 1;
                continue;
            }
	    // if signal not digits, error
	    int signal_error = 0;
	    char *sig = argv[argc_n + 1];
	    while (*sig != '\0') {
	        if (!isdigit(*sig)) {
		    signal_error = 1;
		    break;
	        }	        
		sig++;
	    }
	    if (signal_error) {
 	        fprintf(stderr, "signal format error\n");
		error = 1;
		argc_n++;
		continue;
	    }
	    if(verbose) {
	        flag_index = 0;
		printf("%s %s\n", argv[argc_n], argv[argc_n + 1]);
	    }
	    void (*handler)(int);
	    // choose appropriate handler for each case
	    if (strcmp(argv[argc_n], "--catch") == 0)
	      handler = &catch;
	    else if (strcmp(argv[argc_n], "--ignore") == 0)
	      handler = &ignore;
	    else
	      handler = SIG_DFL;
            signal(atoi(argv[argc_n + 1]), handler);
            argc_n++;
            continue;
        }
        if (strcmp(argv[argc_n], "--pause") == 0) {
	    open_flags = 0;  
	    if(verbose) {
	        flag_index = 0;
	        printf("--pause\n");
	    }
            pause();
            continue;
        }
    }
    // parent child should close all fds(or pipes will not work)
    for (i = 0; i < fds_len; i++) {
        if (fds[i] >= 0) {
            close(fds[i]);
            fds[i] = -1;
        }
    }
    // get the max status
    int max_status = 0;
    for (i = 0; i < procs_len; i++) {
        if (procs[i].status > max_status) {
            max_status = WEXITSTATUS(procs[i].status);
        }
    }
    free(procs);
    return max_status ? max_status : error;
}
