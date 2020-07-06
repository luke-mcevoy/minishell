/*******************************************************************************
 * Name        : minishell.c
 * Author      : Luke McEvoy
 * Date        : April 15, 2020
 * Description : Replica of Linux shell in C.
 * Pledge      : I pledge my Honor that I have abided by the Stevens Honor System.
 ******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <wait.h>

#define BRIGHTBLUE 	"\x1b[34;1m"
#define DEFAULT		"\x1b[0m"
#define BUFSIZE 128

sigjmp_buf jmpbuf;

/* atomic signal catcher that checks if a process is running */
volatile sig_atomic_t child_running = false;

void catch_signal(int sig) {
	if (!child_running) {
		write(STDOUT_FILENO, "\n", 1);
	}
	siglongjmp(jmpbuf, 1);
}

int main(int argc, char **argv) {

	/*	Usage error handler */
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}

	/*	Copies an absolute pathname of the cwd of the calling process.	*/
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "Error: cwd() failed. %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	/*	Signal handler */
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = catch_signal;
	action.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &action, NULL) == -1) {
		fprintf(stderr, "Cannot register signal handler. %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	char buf[PATH_MAX];
	char child_buf[PATH_MAX];
	/* 	child_buf declared to keep buf data intact before strtok mutation.
	 *
	 *		without child_buf, no further computation could be done with input
	 *		from STDIN_FILENO (which is read to buf) because strtok operation 
	 * 		will have removed all " "s. This would result in buf turning into 
	 *		a string of letters instead of a legible sequence of words.
	 *
	 *		Example:	
	 *			INPUT:	echo Hello World!
	 *			OUTPUT: echoHelloWorld!
	 *
	 *		Obviously this data is not ideal to work with.	*/

	sigsetjmp(jmpbuf, 1);
	do {

		child_running = false;

		printf("[%s%s%s]$ ", BRIGHTBLUE, cwd, DEFAULT);
		fflush(stdout);

		ssize_t bytes_read = read(STDIN_FILENO, buf, PATH_MAX - 1);
		if (bytes_read > 0) {
			buf[bytes_read - 1] = '\0';
		}
		if (bytes_read == 1) {
			continue;
		}

		/*	child_buf copied with contents of buf	*/
		memcpy(child_buf, buf, sizeof(buf));

		/*	char* vect[BUFSIZE] used to store parameters of user input	
		 * 		" "s are taken out of string using strtok so they can be 
		 *		passed to execvp in future fork.	*/
		char *vect[BUFSIZE];
		char *tmp;
		memset(vect, 0, sizeof(char *) * BUFSIZE);
		tmp = (char *)strtok(buf, " ");
		int vect_length = 0;

		for (int i = 0; tmp != NULL; i++, tmp = (char *)strtok(NULL, " ")) {
			vect[i] = tmp;
			vect_length++;
		}

		/*	cd cases	*/
		if (strncmp(vect[0], "cd", 2) == 0) {
			/*	Gets a pointer to a structure containing the broken-out fields
				of the record in the password database that matches the username
				name.	*/
			uid_t uid = getuid();
			struct passwd *pwd;
			if ((pwd = getpwuid(uid)) == NULL) {
				fprintf(stderr, "Error: Cannot get passwd. %s\n", strerror(errno));
				return EXIT_FAILURE;
			}

			/*	cd || cd ~	*/
			if ((vect_length == 1) || strncmp(vect[1], "~", 1) == 0) {
				/*	Changes the current working directory (cwd) of the calling 
				process to the directory specified in the path, which is
				the home directory - provided by *pwd.	*/
				if (chdir(pwd->pw_dir) != 0) {
					fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n",
						pwd->pw_dir, strerror(errno));
					return EXIT_FAILURE;
				}

				/*	Copies an absolute pathname of the cwd of the calling process.	*/
				if (getcwd(cwd, sizeof(cwd)) == NULL) {
					fprintf(stderr, "Error: Cannot get current working directory %s. \n",
						strerror(errno));
					return EXIT_FAILURE;
				}
				continue;
			}

			/*	Changes cwd to second char* from vect	*/
			if (chdir(vect[1]) != 0) {
				fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n",
					vect[1], strerror(errno));
			}

			/*	Copies an absolute pathname of the cwd of the calling process.	*/
			if (getcwd(cwd, sizeof(cwd)) == NULL) {
				fprintf(stderr, "Error: Cannot get current working directory %s. \n",
					strerror(errno));
				return EXIT_FAILURE;
			}

		/* 	Termination of program via user entry	*/
		} else if (strncmp(vect[0], "exit", 4) == 0){	
			break;

		} else {
			child_running = true;

			pid_t pid;
			if ((pid = fork()) < 0) {
				fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
				return EXIT_FAILURE;
			} else if (pid > 0) {

				/*	We're in the parent.
				 *	pid is the process id of the child 	*/
				int status;
				pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
				if (w == -1) {
					/*	waitpid failed.	*/
					fprintf(stderr, "Error: waitpid() failed. %s.\n",
						strerror(errno));
					exit(EXIT_FAILURE);
				}

			} else {
				/* We're in the child.	*/

				/*	char* c_vect[BUFSIZE] used to store parameters of user input	
		 		 * 		" "s are taken out of string using strtok so they can be 
		 		 *		passed to execvp in future fork.	
		 		 *	child_buf is used instead of buf because it is untouched.	*/
				char *c_vect[BUFSIZE];
				char *c_tmp;
				memset(c_vect, 0, sizeof(char *) * BUFSIZE);
				c_tmp = (char *)strtok(child_buf, " ");

				for (int i = 0; c_tmp != NULL; i++, c_tmp = (char *)strtok(NULL, " ")) {
					c_vect[i] = c_tmp;
				}

				if (execvp(c_vect[0], c_vect) == -1) {
					fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
					return EXIT_FAILURE;
				} 
			}
		}
	} while (true);
	return EXIT_SUCCESS;
}










