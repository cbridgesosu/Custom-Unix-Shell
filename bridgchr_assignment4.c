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
};

int execute_process(struct command_line *command);

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
  struct command_line *curr_command;
  int last_status = 0; //exit status of most recent process

  while(true)
  {
    // prompts user and parses input
    curr_command = parse_input();

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
      last_status = execute_process(curr_command);
    }
  }

}


int execute_process(struct command_line *command)
{
  pid_t spawnpid = -5;

  spawnpid = fork();
  int childStatus;

  switch (spawnpid) 
  {
    case 0:
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
        waitpid(spawnpid, &childStatus, 0);
        
        if (WIFEXITED(childStatus))
        {
//          printf("Child exited normally with status %d\n", WEXITSTATUS(childStatus));
           return 1; 
        }
        else
        {
          printf("Child exited abnormally due to signal %d\n", WTERMSIG(childStatus));
        }
      }
      else
      {
        printf("background pid is %d\n", spawnpid);
        fflush(stdout);
      
        waitpid(spawnpid, &childStatus, WNOHANG);
      }

      break;
  }

 return 0; 
}
