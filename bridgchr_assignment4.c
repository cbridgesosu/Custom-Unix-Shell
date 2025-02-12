#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

  while(true)
  {
    curr_command = parse_input();

    if (curr_command->argv[0] == NULL){
     continue; 
    }
    else if (!strcmp(curr_command->argv[0], "exit"))
    {
      return EXIT_SUCCESS;
    }
    else if (!strcmp(curr_command->argv[0], "#"))
    {
      continue; 
    }
    else if (!strcmp(curr_command->argv[0], "cd"))
    {
      if (curr_command->argv[1] == NULL)
      {
        char *home_directory = getenv("HOME");
        chdir(home_directory);
      }
      else
      {
        chdir(curr_command->argv[1]);
      }
    }
  }

}
