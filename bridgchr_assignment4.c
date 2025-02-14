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


struct command_line
{
  char *argv[MAX_ARGS + 1];
  int argc;
  char *input_file;
  char *output_file;
  bool is_bg;

  struct sigaction *sigint_enable;
  struct sigaction *sigint_disable;
};

int execute_process(struct command_line *command, pid_t *bgpids);
int checkbg(pid_t *bgpids);
void handle_sigint();
void child_handle_sigint();

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


int main()
{
  struct sigaction sigint_action = {0};
  struct sigaction sigint_default = {0};
  sigint_action.sa_handler = SIG_IGN;
  sigfillset(&sigint_action.sa_mask);
  sigint_action.sa_flags = 0;
  sigaction(SIGINT, &sigint_action, &sigint_default);

  struct command_line *curr_command;
  int last_status = 0; //exit status of most recent process

  pid_t bgpids[25] = {0};

  while(true)
  {
    int bg_status = checkbg(bgpids);
    if (bg_status != -1) 
    {
      last_status = bg_status; 
    }

    // prompts user and parses input
    curr_command = parse_input();
    curr_command->sigint_disable = &sigint_action;
    curr_command->sigint_enable = & sigint_default;


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


int execute_process(struct command_line *command, pid_t *bgpids)
{
  pid_t spawnpid = -5;

  spawnpid = fork();
  int childStatus;
    
 switch (spawnpid) 
  {
    case 0:
      {
      if (!command->is_bg)
      {
        command->sigint_disable->sa_handler = SIG_DFL;
        sigaction(SIGINT, command->sigint_enable, NULL);
      }

      if (command->output_file)
      {
        int outfd = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (outfd == -1)
        {
          printf("cannot open %s for output\n", command->output_file);
          exit(1);
        }
        dup2(outfd, 1);
      }
      if (command->input_file)
      {
        int infd = open(command->input_file, O_RDONLY);
        if (infd == -1)
        {
          printf("cannot open %s for input\n", command->input_file);
          exit(1);
        }
        dup2(infd, 0);
      }
 
      execvp(command->argv[0], command->argv);
      perror("execv");
      exit(1);
      break;
    default:
      if (!command->is_bg)
      {
        int child_pid = waitpid(spawnpid, &childStatus, 0);

       if (WIFEXITED(childStatus))
        {
          int status = WEXITSTATUS(childStatus);
          //printf("Child exited normally with status %d\n", status);
           return status; 
        }
       else if (WIFSIGNALED(childStatus))
        {
          int status = WTERMSIG(childStatus);
          printf("terminated by signal %d\n", status);
          return status;
        }
      }
      else
      {
        printf("background pid is %d\n", spawnpid);
        fflush(stdout);
     
        int i =0;
        while (bgpids[i] != 0)
        {
          i++;
        }
        bgpids[i] = spawnpid;
        waitpid(spawnpid, &childStatus, WNOHANG);
        sleep(1);
      }
      }
      break;
  }

 return 0; 
}


int checkbg(pid_t *bgpids)
{
  //printf("Active PIDs:\n");
  //fflush(stdout);
  int bg_status = -1;

  int childStatus;
  for (int i = 0; i < 25; i++)
  {
    if (bgpids[i] != 0)
    {
      //printf("%d\n", bgpids[i]);
      //fflush(stdout);
      if (waitpid(bgpids[i], &childStatus, WNOHANG) != 0)
      {
        if (WIFEXITED(childStatus))
        {
//          printf("Child exited normally with status %d\n", WEXITSTATUS(childStatus));
          printf("background pid %d is done: terminated by signal %d\n", bgpids[i], WEXITSTATUS(childStatus));
          bgpids[i] = 0;
          bg_status = WEXITSTATUS(childStatus);
        }
        else
        {
          printf("background pid %d is done: terminated by signal %d\n", bgpids[i], WTERMSIG(childStatus));
//          printf("Child exited abnormally due to signal %d\n", WTERMSIG(childStatus));
          bgpids[i] = 0;
          bg_status = WTERMSIG(childStatus);
        }
 
      }

    }
  }
  return bg_status;
}


void handle_sigint()
{
  // write(STDOUT_FILENO, "parent caught signint\n", 22);
}

void child_handle_sigint()
{
  write(STDOUT_FILENO, "child caught signint\n", 15);
  exit(2);
}


