#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

// Written By Euijin Kang
// Contributor: Charlie Curtsinger, Jung Seok Choi
// Last Updated: 11/18/2021

// This is the maximum number of arguments your shell should handle for one command
#define MAX_ARGS 128

//declare variable for pid_t and status
pid_t result;
int status;

// thread that's responsible for checking whether a certain child process has ended
void *background_process_check(void* id){
  // revert input variable back into original form
  pid_t* child_pid = id;

      // wait until child has ended
      result = waitpid(*child_pid, &status, 0);

      // print status
      printf("[background process %d exited with status %d]\n", result, WEXITSTATUS(status));

    //end thread
    return 0;
}


// This is the code for running one individual program.
// Takes in a string representing command and it's arguments and a boolean (true = ';' and false = '&')

void runprogram(char* comm, bool flag) {
  // Initialize arrays for putting in command and arguments
  char *myargs[MAX_ARGS];
  char *input;
  int counter = 0;

  // Separates the command and it's inputs (by whitespace and newline)and puts them each into an array location
  while ((input = strsep(&comm, "\n ")) != NULL) {
    if ((strcmp(input, "") != 0)) {
      myargs[counter] = input;
      counter++;
    }
  }

  // If the command is exit, immediately exit from the program
  if (strcmp(myargs[0], "exit") == 0) {
      exit(EXIT_SUCCESS);
  }

  // Add a NULL character to the end of the array
  myargs[counter] = NULL;

  // If the command is cd, change the command to chdir to run
  if (strcmp(myargs[0],"cd") == 0) {
    int ch = chdir(myargs[1]);
      if (ch == -1) {
      perror("chdir failed");
    }
    return;
  }

  // Create a new process
  pid_t child_id = fork();
  if (child_id == -1) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  // Print messages from the parent and child processes
  if (child_id == 0) {
    // In the child
      //run the command
        int rc = execvp(myargs[0], myargs);
        if (rc == -1) {
          perror("exec failed");
          exit(EXIT_FAILURE);
        }
  } else {
    // In the parent
    // If ';' command, wait for child process.
    if (flag == true) {
      // Wait for the child process to exit
      int status;
      pid_t result = waitpid(child_id, &status, 0);

      // How did the child process die?
      if (WIFEXITED(status)) {
        // If the command is not cd, return a message
        if (strcmp(myargs[0], "cd")) {
          printf("[%s exited with status %d]\n",
              myargs[0], WEXITSTATUS(status));
        }
      } else if (WIFSIGNALED(status)) {
          printf("Child process %d died with signal %d\n",
              result, WTERMSIG(status));
      } else {
          printf("Something unexpected happened.\n");
      }
    } else { // else if background process ('&')
       // declare thread id
      pthread_t tid;
      // store child process id into a new location so it can reference without fear of overwriting
      pid_t local = child_id;
      //create thread to monitor this child process
      pthread_create(&tid, NULL, background_process_check, (void*) &local);
    }
  }
}



int main(int argc, char** argv) {
  // If there was a command line option passed in, use that file instead of stdin
  if (argc == 2) {
    // Try to open the file
    int new_input = open(argv[1], O_RDONLY);
    if (new_input == -1) {
      fprintf(stderr, "Failed to open input file %s\n", argv[1]);
      exit(1);
    }

    // Now swap this file in and use it as stdin
    if (dup2(new_input, STDIN_FILENO) == -1) {
      fprintf(stderr, "Failed to set new file as input\n");
      exit(2);
    }
  }

  char* line = NULL;     // Pointer that will hold the line we read in
  size_t line_size = 0;  // The number of bytes available in line

  // Loop forever
  while (true) {

    // Print the shell prompt
    printf("$ ");

    // Get a line of stdin, storing the string pointer in line
    if (getline(&line, &line_size, stdin) == -1) {
      if (errno == EINVAL) {
        perror("Unable to read command line");
        exit(2);
      } else {
        // Must have been end of file (ctrl+D)
        printf("\nShutting down...\n");

        // Exit the infinite loop
        break;
      }
    }

    // Execute the command instead of printing it below

    // Loop to run all the commands in the input
    while (true) {

      // If newline, check for background process and end loop.
      if (strcmp(line, "\n") == 0) {
        break;
      }

      // Check for ';' or '&'
      char *delim_position = strpbrk(line, ";&");

      // If there are no more ';' or '&' left, run program, check for background processes, and then end loop.
      if (delim_position == NULL) {
        runprogram(line, true);
        break;
      } else if (*delim_position == '&') {
        // If '&' is found, place '\0' to read line until that point, then run program without waiting for child process
        *delim_position = '\0';
        runprogram(line, false);
      } else if (*delim_position == ';') {
        // If ';' is found, place '\0' to read line until that point, then run program waiting for child process
        *delim_position = '\0';
        runprogram(line, true);
      }

      // Increase position by one to bypass '\0' and continue reading line
      line = delim_position + 1;

    }
  }

  // If we read in at least one line, free this space
  if (line != NULL) {
    free(line);
  }
  return 0;
}

