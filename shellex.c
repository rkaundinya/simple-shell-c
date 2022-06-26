/* $begin shellmain */
#include "csapp.h"

#define MAXARGS   128

/* function prototypes */
void eval(char*cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void child_handler(int sig);
void debugPrintArgv(char **argv);

int main() 
{
    char cmdline[MAXLINE]; /* command line */

    while (1) {
	/* read */
	printf("> ");                   
	Fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* argv for execve() */
    char buf[MAXLINE];   /* holds modified command line */
    char *inputFiles[MAXARGS] = { NULL }; /* stores input file names */
    char *outputFiles[MAXARGS] = { NULL }; /* stores output file names */
    int bg;              /* should the job run in bg or fg? */
    pid_t pid, pid2;           /* process id */
    int bHasPipe = 0;    /* do we have a pipe */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* ignore empty lines */

    int count = 0;
    int inputFilesIdx = 0;
    int outputFilesIdx = 0;
    while(argv[count] != NULL)
    {
		// Need to add logic to handle case where file to read/write from
		// comes right after > or <; in this case parsetext keeps that text in the same arv index rather than
		// putting it in the next one
		if(argv[count][0] == '<')
		{
			// If the next char after is not a new line
			if(argv[count][1] != '\0')
			{
				inputFiles[inputFilesIdx++] = argv[count] + 1;
				argv[count] = "\0";
			}
			// If next char is new line, our input file is in the next
			// argv element
			else
			{
			inputFiles[inputFilesIdx++] = argv[count + 1];
			argv[count] = "\0";
			argv[count + 1] = "\0";
			}
		}
        else if(argv[count][0] == '>')
        {
			// If the next char after is not a new line
			if(argv[count][1] != '\0')
			{
				outputFiles[outputFilesIdx++] = argv[count] + 1;
					argv[count] = "\0";
			}
			// If next char is new line, our input file is in the next
			// argv element
			else
			{
			outputFiles[outputFilesIdx++] = argv[count + 1];
			argv[count] = "\0";
			argv[count + 1] = "\0";
			}
        }

		count++;
    }

    count = 0;

    char *argv2[MAXARGS] = {NULL};
    for (int i = 0; argv[i]; i++)
    {
		if (argv[i][0] != '\0')
		{
			argv2[count++] = argv[i];
		}
    }

    count = 0;
    for (; argv2[count]; count++)
    {
		argv[count] = argv2[count];
    }
    argv[count] = NULL;

    count = 0;
    int argv2Count = 0;

    // Search for pipe and terminate argv if pipe found
    // by replacing pipe with NULL
    // NOTE - this program currently only supports one pipe
    while (argv[count] != NULL)
    {
		if (argv[count][0] == '|')
		{
			argv[count] = NULL;
			bHasPipe = 1;
			break;
		}
		count++;
	}
		
	// Copy command to right of pipe to argv2
	if (bHasPipe)
	{
		count++;
		int j = 0;
		for (; argv[count]; count++)
		{
			argv2[j++] = argv[count];
		}

		argv2[j] = NULL;
	}
	else
	{
		argv2[0] = NULL;
	}

	count = 0;

    if (!builtin_command(argv)) { 
	// If we don't have any pipes
		if (!bHasPipe)
		{
			if ((pid = Fork()) == 0) /* child runs user job */
			{
				int fdw, fdr = 0;
				if (inputFiles[0] != NULL)
				{
					fdr = open(inputFiles[0], O_RDONLY);
					dup2(fdr, 0);
					close(fdr);
				}
				if (outputFiles[0] != NULL)
				{
					fdw = open(outputFiles[0], O_WRONLY);
					dup2(fdw, 1);
					close(fdw);
				}
				if (execve(argv[0], argv, environ) < 0)
				{
					printf("%s: Command not found.\n", argv[0]);
					exit(0);
				}
				else
				{
					printf("started %d %s", pid, cmdline);
				}
	    	}
		}
		// There are some pipes to process
		else
		{
			int pipefd[2];
			pipe(pipefd);
			if ((pid = Fork()) == 0)
			{
			int fdin, fdout;
			
			if (inputFiles[0] != NULL)
			{
				fdin = open(inputFiles[0], O_RDONLY);
				dup2(fdin, 0);
				close(fdin);
			}
			dup2(pipefd[1], 1); // make stdout same as pipefd 1
			close(pipefd[0]); // Don't need this
			
			if (execve(argv[0], argv, environ) < 0)
			{
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}
			}
			// We are the parent
			else
			{
				printf("started %6d: ", pid);
				
				for (int i = 0; argv[i]; i++)
				{
					printf(" %s", argv[i]);
				}
				
				printf("\n");

				if ((pid2 = Fork()) == 0)
				{
					int fdin, fdout;
					if (outputFiles[0] != NULL)
					{
						fdout = open(outputFiles[0], O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
						dup2(fdout, 1);
						close(fdout);
					}
					
					dup2(pipefd[0], 0); // make stdin same as pfds[0]
					close(pipefd[1]);

					if (execve(argv2[0], argv2, environ) < 0)
					{
						printf("%s: Command not found. \n", argv2[0]);
						exit(0);
					}
				}
				else
				{
					printf("Started %6d: ", pid2);
					
					for (int i = 0; argv2[i]; i++)
					{
						printf(" %s", argv2[i]);
					}
					
					printf("\n");

					close(pipefd[0]);
					close(pipefd[1]);
				}
			}
		}
    }
    /* parent waits for foreground job to terminate */
    if (!bg) 
    {
		int status;
		
		if (waitpid(pid, &status, 0) < 0)
		{
			unix_error("waitfg: waitpid error");
		}
		
		printf("%d exited\n", pid);
		
		if (bHasPipe)
		{
			if(waitpid(pid2, &status, 0) < 0)
			{
				unix_error("waitfg: waitpid error");
			}
			printf("%d exited\n", pid2);
		}
    }
    else
    {
		printf("%d %s", pid, cmdline);
		if (bHasPipe)
		{
			printf("%d %s", pid2, cmdline);
		}
    }
    
	signal(SIGCHLD, child_handler);

    return;
}

/* if first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (argv[0] == NULL || !strcmp(argv[0], "quit")) /* quit command */
	exit(0);  
    if (!strcmp(argv[0], "&"))    /* ignore singleton & */
	return 1;
    return 0;                     /* not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* points to first space delimiter */
    int argc;            /* number of args */
    int bg;              /* background job? */

    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

void child_handler(int sig)
{
    int child_status;
    pid_t pid;
    
	while ((pid = waitpid(-1, &child_status, WNOHANG)) > 0)
    {
		printf("Received signal %d from process %d\n", sig, pid);
    }
}

void debugPrintArgv(char **argv)
{
    while(*argv != NULL)
    {
		printf("%s\n", *argv);
		argv++;
    }
}
