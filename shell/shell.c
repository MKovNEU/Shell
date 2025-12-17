#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<unistd.h>
#include <sys/wait.h> 
#include<stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "vect.h"
#include "tokens.h"

const int MAX_SIZE = 256;
bool shellRunning = true;
char *previous;

void process(char *input);

//changes current working directory
void cd(vect_t *v){
  if (vect_size(v) < 2){
    fprintf(stderr, "No directory provided\n");
    return;
  }
  const char *dir = vect_get(v, 1);
  if (chdir(dir) != 0) {
    printf("%s", "h");
    perror("cd failed");
  }
}

//prints helpful message about built-in's
void help(){
  printf("cd (change directory)\n"
    "This Com should change the current working directory of the shell to\n"
    "the path specified as the argument.\n"
    "Tip: You can check what the current working directory is using the pwd\n"
    "Com (not a built-in).\n"
    "\n"
    "source\n"
    "Execute a script. Takes a filename as an argument and processes each line\n"
    "of the file as a Com, including built-ins. In other words, each line\n"
    "should be processed as if it was entered by the user at the prompt.\n"
    "\n"
    "prev\n"
    "Prints the previous Com line and executes it again, without befinalCommandming\n"
    "the new Com line. You do not have to support finalCommandbining prev with other\n"
    "Coms on a Com line.\n");
}

//Prints previous command, re-executes it
void prev(){
  printf("%s", previous);
  process(previous);
}

//Runs the contents of a file in the shell
//vect_t -> void
void source(vect_t *v){
  int fd = open(vect_get(v, 1), O_RDONLY);
  char buffer[MAX_SIZE];
  ssize_t bytes_read; 

  while((bytes_read = read(fd, buffer, MAX_SIZE - 1)) > 0) {
    buffer[bytes_read] = '\0';
  }

  char *line = strtok(buffer, "\n");
  while (line != NULL) {
    process(line);
    line = strtok(NULL, "\n");
  }
  close(fd);
}

// Checks and calls for built-ins, or runs program
//vect_t -> void
void run(vect_t *v) {
  //checks for build-ins
  if (strcmp(vect_get(v, 0), "help") == 0 || strcmp(vect_get(v, 0), "prev") == 0 || 
      strcmp(vect_get(v, 0), "source") == 0) {
        if (strcmp(vect_get(v, 0), "prev") == 0 ) {
          prev();
          return;
        }

        if (strcmp(vect_get(v, 0), "help") == 0) {
          help();
          return;
        }

        if (strcmp(vect_get(v, 0), "source") == 0) {
          source(v);
          return;
        }
      }
  //turns v into a **char for execvp
  const char **a = malloc((vect_size(v) + 1) * sizeof(char *));
  for (int i = 0; i < vect_size(v); i++) {
    a[i] = vect_get(v, i);
  }
  a[vect_size(v)] = NULL;
  //runs program, prints error if erros
  if (execvp(a[0], (char *const *)a) == -1) {
    fprintf(stderr, "%s :Com not found\n", a[0]);
    exit(1);
  }
  free(a);
  exit(0);
}

//Takes care of input and output redirection
//vect_t -> void
void run_command(vect_t *v) {
  //removes the output redir if exists, gets output file name
  vect_t *tempCom = vect_new();
  const char *outputFile;
  bool seenOutput= false;
  for (int i = 0; i < vect_size(v); i++) {
    if (strcmp(vect_get(v, i), ">") == 0 && !seenOutput) {
      seenOutput = true;
      outputFile = vect_get(v, i+1);
      break;
    }
    else {
      vect_add(tempCom, vect_get(v, i));
    }
  }
  //removes the input redir if exists, gets input file name
  vect_t *finalCommand = vect_new();
  const char *inputFile;
  bool seenInput = false;
  for (int i = 0; i < vect_size(tempCom); i++) {
    if (strcmp(vect_get(tempCom, i), "<") == 0 && !seenInput) {
      seenInput = true;
      inputFile = vect_get(tempCom, i+1);
      break;
    }
    else {
      vect_add(finalCommand, vect_get(tempCom, i));
    }
  }

  //forks to open files and call command.
  if (fork() == 0) {
    //opens file if has input redir
    if (seenInput) {
      int fd = open(inputFile, O_RDONLY);
      dup2(fd, 0);
      close(fd);
    }

    //opens file if has output rdir
    if (seenOutput) {
      int fd1 = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(fd1, 1);
      close(fd1);
    }

    //runs command
    run(finalCommand);
    return;
  }

  //waits til child is closed, 
  wait(NULL);
  vect_delete(tempCom);
  vect_delete(finalCommand);
}

//Recursive function, runs LHS of a pipe and the recalls on RHS, multiple pipe support
//calls run_Com for indiviual commands
//vect_t -> void
void run_seq(vect_t *v) {
  bool seenPipe = false;
  vect_t *LHPipe = vect_new();
  vect_t *RHPipe = vect_new();
  //find the left hand side and right hand side of the first pipe.
  for (int i = 0; i < vect_size(v); i++) {
    if (!seenPipe && strcmp(vect_get(v, i), "|") == 0) {
      seenPipe = true;
    }
    else {
      if (!seenPipe){
        vect_add(LHPipe, vect_get(v, i));
      }
      else {
        vect_add(RHPipe, vect_get(v, i));
      }
    }
  }
  //if no pipe, runs the command
  if (!seenPipe) {
    run_command(LHPipe);
    vect_delete(LHPipe);
    vect_delete(RHPipe);
    exit(0);
  }
  //creates pipe
  int pipe_fd[2];
  pipe(pipe_fd);
  int write_fd = pipe_fd[1];
  int read_fd = pipe_fd[0];

  //forks and runs the LHS pipe, the writing end.
  pid_t writing_child = fork();
  if (writing_child == 0) {
    close(read_fd); 
    close(1);
    assert(dup(write_fd) == 1);
    run_command(LHPipe);
    exit(0);
  }

  //forks and runs the RHS pipe, the reading end.
  pid_t reading_child = fork();
  if (reading_child == 0){
    close(0);
    close(write_fd);
    assert(dup(read_fd) == 0);
    //Recursivly calls RHS, looks for more pipes.
    run_seq(RHPipe);
    exit(0);
  }

  //Closes pipes, waits for children to end, frees memory
  close(write_fd);
  close(read_fd);
  wait(NULL);
  wait(NULL);
  vect_delete(LHPipe);
  vect_delete(RHPipe);
}

//Tokenizes input and runs each sequence (or cd) split by semicolon
//Forks for each sequence and runs in child process
//*char -> void
void process(char *input) {
  //tokenizes input, turns into usable tokens
  vect_t *tokens = tokenize(input);
  //keeps track of the current sequence
  vect_t *singleSeq = vect_new();

  //gets each current sequence seperated by ';'
  for (int i = 0; i < vect_size(tokens); i++) {
    if (strcmp(vect_get(tokens, i), ";") == 0){
      //if sequence is cd call
      if (strcmp(vect_get(singleSeq, 0), "cd") == 0) {
        cd(singleSeq);
        vect_delete(singleSeq);
        singleSeq= vect_new();
      }
      // fork and run the single sequence in the child. 
      else {
        if (fork() == 0) {
          run_seq(singleSeq);
          exit(0);
        }
        else {
          wait(NULL);
          vect_delete(singleSeq);
          singleSeq = vect_new();
        }
      }
    }
    //add token to current sequence
    else {
      vect_add(singleSeq, vect_get(tokens, i));
    }
  }
  //if last sequence is cd
  if (strcmp(vect_get(singleSeq, 0), "cd") == 0){
    cd(singleSeq);
    vect_delete(singleSeq);
    vect_delete(tokens);
  }
  //fork and run the last sequence
  else {
    if (fork() == 0) {
      run_seq(singleSeq);
      exit(0);
    }
    else {
      wait(NULL);
      vect_delete(singleSeq);
      vect_delete(tokens);
    }
  }
}

//Main function for the shell, takes in user input and calls process on the input
//Updates previous for prev, responsible for exiting shell. 
//int, **char  --> int 
int main(int argc, char **argv) {
  // TODO: Implement your shell's main
  printf("Welcome to mini-shell.\n");

  while(shellRunning) {
    printf("shell $ ");
    char input[MAX_SIZE];
    //gets user input
    if (!fgets(input, MAX_SIZE, stdin) || strcmp(input, "exit\n") == 0){
      shellRunning = false;
      printf("%s", "Bye bye.");
    }
    else {
      //cals process on user input
      process(input);
      //updates previous
      if (strcmp(input, "prev\n") != 0){
        if (previous != NULL) {
          free(previous);
        }
        previous = strdup(input);
      }
    }
  }
}
