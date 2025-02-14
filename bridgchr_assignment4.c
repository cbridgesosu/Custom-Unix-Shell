#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS 512

bool bg_mode = true;  // tracks if background mode is enabled
bool switch_sigtstp = false; // tracks if background mode is switched to print notification

/* 
 * This struct is based upon the provided parser starter code
 */
struct command_line
{
  char *argv[MAX_ARGS + 1]; // argument list
  int argc; // argument count
  char *input_file;  // input file for < redirection
  char *output_file; // output file for > redirection
  bool is_bg; // status of fg/bg process

  struct sigaction *sigint_enable;  // sigaction handler for ctrl-c
  
  struct sigaction *sigtstp_control;  // sigaction handler for ctrl-z bg mode
};

int execute_process(struct command_line *command, pid_t *bgpids);
int checkbg(pid_t *bgpids);
void handle_sigtstp();
void child_handle_sigint();
void inform_sigtstp();

/*
 * This function based upon the provided parser starter code 
 */
struct command_line *parse_input()
{
  char input[INPUT_LENGTH];
  struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

  // get input
  printf(": ");
  fflush(stdout);
  fgets(input, INPUT_LENGTH, stdin);

  // tokenize input
  char *token = strtok(input, " \n");
  while(token)
  {
    if(!strcmp(token, "<"))
    {
      curr_command->input_file = strdup(strtok(NULL, " \n"));
    } else if (!strcmp(token, ">"))
    {
      curr_command->output_file = strdup(strtok(NULL, " \n"));
    } else if(!strcmp(token, "&"))
    {
      curr_command->is_bg = true;
    } else
    {
      curr_command->argv[curr_command->argc++] = strdup(token);
    }
    token=strtok(NULL, " \n");
  }
  return curr_command;
}

/*
 * Program name: Smallsh
 * Author: Chris Bridges
 *
 * This program implements a basic shell that has the built in commands exit, cd, and status.
 * Additional commands are passed on to the exec() function and handled by the os. The shell
 * supports ctrl-c interupts of foreground processes and utilizes ctrl-z as a toggle between
 * enabling and disabling background processes.
 *
 * Arguments: none
 */
int main()
{
  // creates signal handler for ctrl-c
  struct sigaction sigint_action = {0};
  sigint_action.sa_handler = SIG_IGN;
  sigfillset(&sigint_action.sa_mask);
  sigint_action.sa_flags = 0;
  sigaction(SIGINT, &sigint_action, NULL);

  // creates signal handler for ctrl-z bg toggle
  struct sigaction sigtstp_control = {0};
  sigtstp_control.sa_handler = handle_sigtstp;
  sigfillset(&sigtstp_control.sa_mask);
  sigtstp_control.sa_flags = 0;
  sigaction(SIGTSTP, &sigtstp_control, NULL);

  struct command_line *curr_command;
  int last_status = 0; //exit status of most recent process

  pid_t bgpids[25] = {0}; // array to track bg pids

  // loops for cli prompt until exit 
  while(true)
  {
    // checks if any bg processes have terminated
    int bg_status = checkbg(bgpids);
    if (bg_status != -1)   // updates status with bg exit code
    {
      last_status = bg_status; 
    }

    // checks if bg/fg mode has been toggled
    if (switch_sigtstp)
    {
      inform_sigtstp(); 
    }

    // prompts user and parses input
    curr_command = parse_input();

    // register signal handles inside command struct
    curr_command->sigint_enable = &sigint_action;
    curr_command->sigtstp_control = &sigtstp_control;


    if (curr_command->argv[0] == NULL) // handles empty line
    {
     continue; 
    }
    else if (!strcmp(curr_command->argv[0], "exit")) // handles exit cmd
    {
      return EXIT_SUCCESS;
    }
    else if (!strncmp(curr_command->argv[0], "#", 1)) // handles comments
    {
      continue; 
    }
    else if (!strcmp(curr_command->argv[0], "cd")) // handles cd cmd
    {
      if (curr_command->argv[1] == NULL) // if no filepath
      {
        char *home_directory = getenv("HOME");
        chdir(home_directory);
      }
      else // if filepath provided
      {
        chdir(curr_command->argv[1]);
      }
    }
    else if (!strcmp(curr_command->argv[0], "status")) // handles status cmd
    {
      printf("exit value %d\n", last_status);
    }
    else // handles other cmds
    {
      last_status = execute_process(curr_command, bgpids);
    }
  }

}


/*
 * Function to execute commands from the shell.
 *
 * Arguments: struct commandline * command - current command
 *            pid_t *bgpids - array of current active background processes
 *
 * Return: int command exit status
 */
int execute_process(struct command_line *command, pid_t *bgpids)
{
  pid_t spawnpid = -5; // process id init to arbitrary invalid value

  // forks process for parent shell and child command 
  spawnpid = fork();
  int childStatus; // status of child process
    
 switch (spawnpid) 
  {
    case 0: // child fork
    {
          // updates signal handler to ignore ctrl-z for both fg/bg processes
        command->sigtstp_control->sa_handler = SIG_IGN;
        sigaction(SIGTSTP, command->sigtstp_control, NULL);

        // updates signal handler to enable ctrl-c on fg processes
        if (!command->is_bg)
        {
          command->sigint_enable->sa_handler = SIG_DFL;
          sigaction(SIGINT, command->sigint_enable, NULL);
        }

        // redirects output fd if provided
        if (command->output_file)
        {
          int outfd = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
          if (outfd == -1)
          {
            printf("cannot open %s for output\n", command->output_file);
            exit(1);
          }
          dup2(outfd, 1);
        } else if (command->is_bg) // redirects bg process stdout to /dev/null if no user redirect
        {
          int outfd = open("/dev/null", O_WRONLY, 0640);
          dup2(outfd, 1);
        }

        // redirects input fd if provided
        if (command->input_file)
        {
          int infd = open(command->input_file, O_RDONLY);
          if (infd == -1)
          {
            printf("cannot open %s for input\n", command->input_file);
            exit(1);
          }
          dup2(infd, 0);
        } else if (command->is_bg) // redirects bg process stdin to /dev/null is no user redirect
        {
          int infd = open("/dev/null", O_RDONLY);
          dup2(infd, 0);
        }
  
        // executes user command
        execvp(command->argv[0], command->argv);
        
        // prints error on exec failure
        perror("execv");
        exit(1);
        break;
    }
    default: // parent fork
    {
      if (!command->is_bg || !bg_mode) // if cmd is not bg or bg mode disabled
      {
        // parent waits for child process to complete
        int child_pid = waitpid(spawnpid, &childStatus, 0); 

       if (WIFEXITED(childStatus)) // if child exits normally
        {
          // returns child exit status
          int status = WEXITSTATUS(childStatus);
          return status; 
        }
       else if (WIFSIGNALED(childStatus)) // if child exits with signal
        {
          // returns child termination signal
          int status = WTERMSIG(childStatus);
          printf("terminated by signal %d\n", status);
          return status;
        }
      }
      else // if cmd is bg process
      {
        // prints child pid for bg process
        printf("background pid is %d\n", spawnpid);
        fflush(stdout);
    
        // adds child pid to actice process array
        int i =0;
        while (bgpids[i] != 0)
        {
          i++;
        }
        bgpids[i] = spawnpid;

        //waitpid(spawnpid, &childStatus, WNOHANG);
      }
    }
      
    break;
  }

 return 0; 
}

/*
 * Function to monitor active background processes and report their exit status back to the shell.
 * Returns -1 if no active process returns.
 *
 * Arguments: pid_t *bgpids - array of active background pids
 * Return: int exit status value
 */
int checkbg(pid_t *bgpids)
{
  int bg_status = -1; // default invalid exit status

  int childStatus; // exit status of child process

  // iterates through array of active bg pids
  for (int i = 0; i < 25; i++)
  {
    if (bgpids[i] != 0) // if array index not empty
    {
      if (waitpid(bgpids[i], &childStatus, WNOHANG) != 0) // if child process has ended
      {
        if (WIFEXITED(childStatus)) // if child exited
        {
          // prints exit status
          printf("background pid %d is done: exit value %d\n", bgpids[i], WEXITSTATUS(childStatus));
          bg_status = WEXITSTATUS(childStatus);
        }
        else // if child was terminated
        {
          // prints termination signal
          printf("background pid %d is done: terminated by signal %d\n", bgpids[i], WTERMSIG(childStatus));
          bg_status = WTERMSIG(childStatus);
        }
 
        // removes terminated pid from array
        bgpids[i] = 0;
      }

    }
  }
  return bg_status;
}


/*
 * Signal handler function to toggle background mode boolean.
 *
 * Arguments: none
 * Return: none
 */
void handle_sigtstp()
{
  switch_sigtstp = true;
}


/*
 * Function to display current status of background toggle setting.
 *
 * Arguments: none
 * Return: none
 */
void inform_sigtstp()
{
  // reset boolean after toggle handled
  switch_sigtstp = false;

  if (bg_mode)
  {
    // disables background mode and displays status
    bg_mode = false;
    char *message = "\nEntering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, strlen(message));
    fflush(stdout);
  }
  else
  {
    // enable background mode and displays status
    bg_mode = true;
    char *message = "\nExiting foreground-only mode\n";
    write(STDOUT_FILENO, message, strlen(message));
    fflush(stdout);
  }
}
