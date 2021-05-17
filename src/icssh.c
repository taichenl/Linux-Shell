#include "icssh.h"
#include <readline/readline.h>
#include "linkedList.h"
int children_flag = 0;

void sigusr2_handler(int sig){
	//int old = errno;
	fprintf(stdout, "Hi User! I am process %ld\n", (long)getpid());
	// errno = old;
}

void sigchild_handler(int sig){
	children_flag++;
}


int process_comparartor(void* lhs, void* rhs){
	bgentry_t* left = (bgentry_t *) lhs;
	bgentry_t* right = (bgentry_t *) rhs;

	return left->seconds-right->seconds;
}


int main(int argc, char* argv[]) {
	int exec_result;
	int exit_status;
	pid_t pid;
	pid_t wait_result;
	char* line;
	int fd[2];
	char prev_dir[10000];
#ifdef GS
    rl_outstream = fopen("/dev/null", "w");
#endif	

	List_t background;
	background.head = NULL;
	background.length = 0;
	background.comparator = &process_comparartor;

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGCHLD, sigchild_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

    // print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {
        // MAGIC HAPPENS! Command string is parsed into a job struct
        // Will print out error message if command string is invalid
		int built_in = 0;
		
		job_info* job = validate_input(line);

		//error checking for redirction
		if (job->procs->in_file!= NULL && job->procs->out_file!= NULL &&strcmp(job->procs->in_file, job->procs->out_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}

		if (job->procs->in_file!= NULL && job->procs->err_file!= NULL &&strcmp(job->procs->in_file, job->procs->err_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}

		if (job->procs->in_file!= NULL && job->procs->outerr_file!= NULL &&strcmp(job->procs->in_file, job->procs->outerr_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}

		if (job->procs->out_file!= NULL && job->procs->err_file!= NULL && strcmp(job->procs->err_file, job->procs->out_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}

		if (job->procs->out_file!= NULL && job->procs->outerr_file!= NULL && strcmp(job->procs->outerr_file, job->procs->out_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}

		if (job->procs->err_file!= NULL && job->procs->outerr_file!= NULL && strcmp(job->procs->outerr_file, job->procs->err_file) == 0){
			fprintf(stderr, RD_ERR);
			continue;
		}


        if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

        //Prints out the job linked list struture for debugging
        #ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
            debug_print_job(job);
        #endif

		//check children flag
		if (children_flag == 1){
			// finish all the children process
			//printf("%s", "flag has turned to one\n");
			wait(&exit_status);
			children_flag = 0;
		}

		// example built-in: exit
		if (strcmp(job->procs->cmd, "exit") == 0) {
			//kill all the background process
			node_t * iter = background.head;
			while(iter!= NULL){
				bgentry_t* temp = (bgentry_t *)iter->value;
				//kill(0, SIGKILL);
				job_info* hello = validate_input(temp->job->line);
				fprintf(stdout, BG_TERM, temp->pid, hello->procs->cmd);
				free_job(temp->job);
				free_job(hello);
				free(temp);
				iter = iter->next;
			}
			deleteList(&background);
			// Terminating the shell
			free(line);
			free_job(job);
            validate_input(NULL);
            return 0;
		}
		if (strcmp(job->procs->cmd, "bglist") == 0){
			built_in =1;
			node_t * iter = background.head;
			while(iter!= NULL){
				bgentry_t* temp = (bgentry_t *)iter->value;
				print_bgentry(temp);
				iter = iter->next;
			}
		}

		if (strcmp(job->procs->cmd, "ascii53") == 0){
			fprintf(stdout, "%s\n", "UUUUUUUU     UUUUUUUU             CCCCCCCCCCCCC     IIIIIIIIII\nU::::::U     U::::::U          CCC::::::::::::C     I::::::::I\nU::::::U     U::::::U        CC:::::::::::::::C     I::::::::I\nUU:::::U     U:::::UU       C:::::CCCCCCCC::::C     II::::::II\n U:::::U     U:::::U       C:::::C       CCCCCC       I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U:::::D     D:::::U      C:::::C                     I::::I  \n U::::::U   U::::::U       C:::::C       CCCCCC       I::::I  \n U:::::::UUU:::::::U        C:::::CCCCCCCC::::C     II::::::II\n  UU:::::::::::::UU          CC:::::::::::::::C     I::::::::I\n    UU:::::::::UU              CCC::::::::::::C     I::::::::I\n      UUUUUUUUU                   CCCCCCCCCCCCC     IIIIIIIIII\n");
			built_in = 1;
		}

		if (strcmp(job->procs->cmd, "fg") == 0){
			built_in =1;
			if (background.length==0){
				fprintf(stderr, PID_ERR);
			}
			else{
				int found = 0;
				if (job->procs->argc == 1){
					//bring the most recent
					node_t * iter = background.head;
					bgentry_t* temp;
					while(iter->next != NULL){
						iter = iter->next;
					}
					temp = (bgentry_t *)iter->value;
					pid_t t = temp->pid;
					waitpid(t, &exit_status, 0);
					removeByIndex(&background, background.length-1);
				}
				else{
					node_t * iter = background.head;
					bgentry_t* temp;
					int ind = 0;
					while(iter!= NULL){
						temp = (bgentry_t *)iter->value;
						if (atoi(job->procs->argv[1]) == temp->pid){
							found++;
							break;
						}
						ind++;
						iter = iter->next;
					}
					if (found == 0){
						fprintf(stderr, PID_ERR);
					}
					pid_t t = temp->pid;
					waitpid(t, &exit_status, 0);
					removeByIndex(&background, ind);
				}
			}
		}



		if (strcmp(job->procs->cmd, "cd") == 0){
			built_in = 1;
			char buf[10000];
				if (job->procs->argc == 1){
					getcwd(prev_dir, 10000);
					chdir("/home");int ccount = 0;
					getcwd(buf, 10000);
					fprintf(stdout, "%s\n", buf);
				}
				else if (strcmp(job->procs->argv[1], ".") == 0){
					getcwd(prev_dir, 10000);
					chdir(".");
					getcwd(buf, 10000);
					fprintf(stdout, "%s\n", buf);
				}
				else if (strcmp(job->procs->argv[1], "..") == 0){
					getcwd(prev_dir, 10000);
					chdir("..");
					getcwd(buf, 10000);
					fprintf(stdout, "%s\n", buf);
				}
				else if (strcmp(job->procs->argv[1], "-") == 0){
					chdir(prev_dir);
					getcwd(buf, 50);
					fprintf(stdout, "%s\n", buf);
				}
				else{
					getcwd(prev_dir, 10000);
					int result = chdir(job->procs->argv[1]);
					if (result < 0){
						fprintf(stderr, DIR_ERR);
					}
					else{
						fprintf(stdout, "%s\n", getcwd(buf, 10000));
					}
				}
			}
		if (strcmp(job->procs->cmd, "estatus") == 0){
			built_in = 1;
			waitpid(pid, &exit_status, 0);
			if ( WIFEXITED(exit_status) ){
				fprintf(stdout, "%d\n", WEXITSTATUS(exit_status));
			}
		}

		// example of good error handling!
		if (built_in == 0){
			if (job->nproc == 1){
				if ((pid = fork()) < 0) {
					perror("fork error");
					exit(EXIT_FAILURE);
				}
				if (pid == 0) {  //If zero, then it's the child process
					//get the first command in the job list
					proc_info* proc = job->procs;
					if (job->procs->in_file != NULL){
						int in = open(job->procs->in_file, O_RDONLY);
						if (in <0){
							fprintf(stderr, RD_ERR);
							exit(1);
						}
						dup2(in, 0);
						close(in);
					}

					if (job->procs->out_file != NULL){
						int out = open(job->procs->out_file, O_WRONLY|O_CREAT|O_TRUNC, 0600);
						if (out <0){
							fprintf(stderr, RD_ERR);
							exit(1);
						}
						dup2(out, 1);
						close(out);
					}

					if (job->procs->err_file!= NULL){
						int err = open(job->procs->err_file, O_WRONLY|O_CREAT|O_TRUNC, 0600);
						if (err <0){
							fprintf(stderr, RD_ERR);
							exit(1);
						}
						dup2(err, 2);
						close(err);
					}

					if (job->procs->outerr_file!= NULL){
						int err = open(job->procs->outerr_file, O_WRONLY|O_CREAT|O_TRUNC, 0600);
						if (err <0){
							fprintf(stderr, RD_ERR);
							exit(1);
						}
						dup2(err, 2);
						dup2(err, 1);
						close(err);
					}

					exec_result = execvp(proc->cmd, proc->argv);
					if (exec_result < 0) {  //Error checking
							printf(EXEC_ERR, proc->cmd);
							free_job(job);  
							free(line);
							validate_input(NULL);
							exit(EXIT_FAILURE);
					}
				} else {
					// As the parent, wait for the foreground job to finish
					if (job->bg){
						bgentry_t * temp = malloc(sizeof(bgentry_t));
						temp->job = job;
						temp->pid = pid;
						temp->seconds = time(NULL);

						insertInOrder(&background, (void *) temp);
					}
					else{
						wait_result = waitpid(pid, &exit_status, 0);
						children_flag--;
						//fprintf(stdout, "%s", "check here\n");
						if (wait_result < 0) {
							printf(WAIT_ERR);
							exit(EXIT_FAILURE);
						}
					}
				}
			}
			else{
				//fprintf(stdout, "%s", "check here\n");
				int fd[2];
				pipe(fd);

				if ((pid = fork()) < 0) {
					perror("fork error");
					exit(EXIT_FAILURE);
				}
				if (pid == 0) {  //If zero, then it's the child process
					//get the first command in the job list
					proc_info* proc = job->procs;
					close(fd[0]);
					dup2(fd[1], 1);
					close(fd[1]);
					exec_result = execvp(proc->cmd, proc->argv);
					if (exec_result < 0) {  //Error checking
							printf(EXEC_ERR, proc->cmd);
							free_job(job);  
							free(line);
							validate_input(NULL);
							exit(EXIT_FAILURE);
					}
				}
				else{
					pid_t p2 = fork();
					if ((p2 = fork()) < 0) {
						perror("fork error");
						exit(EXIT_FAILURE);
					}
					if (p2 ==0){
						close(fd[1]);
						dup2(fd[0], 0);
						close(fd[0]);

						proc_info* proc = job->procs->next_proc;
						exec_result = execvp(proc->cmd, proc->argv);
						if (exec_result < 0) {  //Error checking
							printf(EXEC_ERR, proc->cmd);
							free_job(job);  
							free(line);
							validate_input(NULL);
							exit(EXIT_FAILURE);
						}
					}
					else{
						wait_result = waitpid(pid, &exit_status, 0);
						wait_result = waitpid(pid, &exit_status, 0);
						
					}

				}
			}
		} //last lline
		if (job->bg == false) free_job(job);  // if a foreground job, we no longer need the data
		free(line);
	}

    // calling validate_input with NULL will free the memory it has allocated
    validate_input(NULL);

#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}
