#include <stdio.h>  
#include <stdlib.h>   
#include <unistd.h>   
#include <sys/types.h>
#include "shell.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Global Variables */
int pidArr[100]; // allocate 100 bytes to keep track of processes
char *buffer = NULL; // set to NULL for the user's input
int childStat = 0; // keep track of children 
int changeMode = 1; // set the mode for foreground to true

void userInput() {
   struct Input *input = malloc(sizeof(struct Input)); // allocate memory into struct variable
   /* User input source: stackoverflow.com/questions/15539708/passing-an-array-to-execvp-from-the-users-input */
   char *saveptr = NULL; // set saveptr to NULL
   size_t len = 0; // set the length to 0
   size_t c = 0; // used to hold onto getline()
   
   printf(": "); fflush(stdout); 
   c = getline(&buffer, &len, stdin); // grab the user's input
   /* Comment and Blank space source: cboard.cprogramming.com/ c-programming/145793-checking-blank-line-when-reading-file.html */
   if (buffer[0] == '#') { // if the first index of the user's input is #
      userInput(); // start over
   }
   else if (strcmp(buffer, "\n") == 0) { // if the user enters a blank line
      userInput(); // start over
   }
}


void strReplace(char *target, char *needle, char *replace) {
   /* stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-cint childStat */
   char input[2049]; // allocate memory for input
   char *dest = &input[0]; // initilialize to the first index of input
   char *newTarget = target; // set to target (current string)
   size_t sourceLen = strlen(needle); // take the string length of what we need to find
   size_t targetLen = strlen(replace); // take the string length of what we need to replace

   while (strstr(buffer, "$$")) { // while 1 is returned
      char *newNeedle = strstr(newTarget, needle); // set to find the needle in the curent string
      if (newNeedle == NULL) { // if it doesn't exist
	 strcpy(dest, newTarget); // copy current  string into new stirng
	 break;
      }
      size_t nSize = newNeedle - newTarget; // set to the difference between $$ and th user input
      memmove(dest, newTarget, nSize); // copy charcters from newTarget into dest
      dest += nSize; // add the number of bytes into dest

      memmove(dest, replace, targetLen); // copy characters from replace into dest
      dest += targetLen; // add the number of bytes into dest
      
      newTarget = newNeedle + sourceLen; // set the new
   }
   strcpy(target, input); // copy the new input into the original output
}

void expandToken() {
   char *newString = malloc(sizeof(char)); // allocate memory for newString
   int pid = getpid(); // assign process id into pid
   sprintf(newString, "%d", getpid()); // format int into string
   if (strstr(buffer, "$$")) { // if "$$" is found in the input
      strReplace(buffer, "$$", newString); // replace getpid() with "$$"
   }
}

void exitCommand(int loop) {
   int i; // declare i 
   loop = 1; // set loop to 1 to end the program
   waitpid(-1, &childStat, WNOHANG); // set to the background
   for (i = 0; i < 100; i++) { // loop over the processes
      kill(pidArr[i], SIGTERM); // kill every processes kept track of
   }
}

void statusCommand() {
   if (WIFEXITED(childStat)) { // if the child terminated normally
      int exitStat = WEXITSTATUS(childStat); // set the exit value
      printf("exit value %d\n", exitStat); fflush(stdout);
   }
   else if (WIFSIGNALED(childStat)) { // if the child was terminated by a signal
      int exitStat = WTERMSIG(childStat); // set the exit value
      printf("terminated by signal %d", exitStat); fflush(stdout);
   }
}

void printBG(pid_t pid) {
   pid = waitpid(-1, &childStat, WNOHANG); // set to the background
   if (pid > 0) { // if there are any processes left over
      if (WIFEXITED(childStat)) { // if the child terminated normally
	 int exitStat = WEXITSTATUS(childStat); // set the exit value
	 printf("background pid is %d  exit value %d\n", pid, exitStat); fflush(stdout);
      }
      else if (WIFSIGNALED(childStat)) { // if the child termined by a signal
	 int exitStat = WTERMSIG(childStat); // set the exit value
	 printf("background pid is %d  terminated by signal %d\n", pid, exitStat); fflush(stdout);
      }
   }
}


void catchSIGINT(int signo) {
   char *message = "terminated by signal 2\n"; // define message
   write(STDOUT_FILENO, message, 22); fflush(stdout); // write instead of printfto reentrant
   sleep(5); // hold for 5 seconds
}

void catchSIGTSTP(int signo) {
   if (changeMode == 1) { // if the program is on foreground mode
      char *message = "Entering foreground-only mode (& is now ignored)\n"; // define message
      write(STDOUT_FILENO, message, 49); fflush(stdout); // use write in stead of printf to reentrant
      changeMode = 0; // set the mode to false
   }
   else if (changeMode == 0) { // leave foreground only mode if set to false
      char *message = "Exiting foreground-only mode\n"; // define message
      write(STDOUT_FILENO, message, 29); fflush(stdout); // use write instead of printf to reentrant
      changeMode = 1; // set the mode back to true
   }
   sleep(5); // hold for 5 seconds
}


void catchSignal() {
   /* Initalize to be empty */
   struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};
   
   /* SIGNINT is used to catch ^C */
   SIGINT_action.sa_handler = catchSIGINT; 
   sigfillset(&SIGINT_action.sa_mask);
   SIGINT_action.sa_flags = SA_RESTART;
   ignore_action.sa_handler = SIG_IGN;
   sigaction(SIGINT, &SIGINT_action, NULL);

   /* SIGTSTP is used to catch ^Z */
   SIGTSTP_action.sa_handler = catchSIGTSTP;
   sigfillset(&SIGTSTP_action.sa_mask);
   SIGTSTP_action.sa_flags = SA_RESTART;
   sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

void parseInput(int loop) {
   struct Input *input = malloc(sizeof(struct Input)); // allocate memory for struct variable
   char *saveptr = NULL; // set saveptr to NULL
   int i = 0; // set to 0
   expandToken(); // call function to expand "$$" to pid
 
   /* The section below is the parsing of the user input. When the program takes the user's
   * input, it's parsed by being tokenized using strtok_r and in order, it'll store 
   * the command, two arguments for execvp, "<" (input), ">" (output), and "&" (background) 
   * in a struct */

   char *token = strtok_r(buffer, " \n", &saveptr); // tokenize the first argument
   while (token != NULL) { // while the token is not empty
      if (strcmp(token, "<") == 0) { // if "<" is found in the token 
 	   token = strtok_r(NULL, " \n", &saveptr); // move to the next argument
	   input->inFile = calloc(strlen(token) + 1, sizeof(char)); // allocate memory into inFile
	   strcpy(input->inFile, token); // copy "<" into inFile
       }
       else if (strcmp(token, ">") == 0) { // if ">" is found in the token
	   token = strtok_r(NULL, " \n", &saveptr); // move to the next argument
	   input->outFile = calloc(strlen(token) + 1, sizeof(char)); // allocate memory into outFile
	   strcpy(input->outFile, token); // copy ">" into outFile
       }
       else if (strcmp(token, "&") == 0) { // if "&" is found in the token
	   input->background = calloc(strlen(token) + 1, sizeof(char)); // allocate memory into background
	   strcpy(input->background, token); // copy "&" into background
       }
       else {
	  input->args[i] = calloc(strlen(token) + 1, sizeof(char)); // allocate memory into our args to keep track of them
	  strcpy(input->args[i], token); // store them in our array
	  i++; // increment to move one by one
       }
       token = strtok_r(NULL, " \n", &saveptr); // move to the next token
   }

   /* The section below is when the user enters the three built in-commands including
    * "exit", "cd", and "status". When the user enters one, a function will be called
    * for that command.*/
 
   if (strstr(input->args[0], "exit") && input->background == NULL) { // if the first argument is "exit"
      exitCommand(loop); // call exitComman
   }
   else if (strstr(input->args[0], "cd") && input->background == NULL) {// && input->background == NULL) { // if the first argument is "cd"
      // stackoverflow.com/questions/33485011/chdirgetenvhome-prompts-error-no-such-file-or-directoryi/cdCommand(); // call cdComman 
      if (input->args[1] == NULL) { // if a second argument is not entered
	 if (chdir(getenv("HOME")) != 0) { //change directory to HOME and if it's not true
	    perror("chdir()\n"); fflush(stdout);// print error
	 }
      }
      else if (chdir(input->args[1]) != 0) { // if a second argument is entered dollowing "cd"
	 perror("chdir()\n"); fflush(stdout); // print error
      }
   }
   else if (strstr(input->args[0], "status")) { // if the first argument is "status"
      statusCommand(); // call statusCommand
   }
  
   /* The section below is the creation of the parent and child process. Before the fork, the
    * function to catch the signals is called, and below that we have our cases. If fork returns
    * -1 then it fails, and by default it'll start with the parent, then jump to the child. The
    *  parent process takes care of the foreground and background if "&" has been entered or not,
    *  and the child handles the redirection of "<" and ">", and executing non built in commands. */

   catchSignal(); // call function for signal handlers
   pid_t pass; // arbritary use (nothing used for)
   printBG(pass); // call function to print background pid 
   pid_t spawnPid = fork(); // create a parent and child process
   switch(spawnPid) { // if spawnpid exist
      case -1: // if -1 is returned
	 perror("fork()\n"); fflush(stdout); // print an error
	 _exit(1); // safely exit
	 break; 
      case 0:
	 /* Redirect in */
	 if (input->inFile != NULL) { // if the user entered "<"
	    int inFD = open(input->inFile, O_RDONLY); // open a file that's read ony
	    if (inFD == -1) { // if -1 is returned
	       perror("open()\n"); fflush(stdout); // print an error
	       _exit(1); // safely exit
	    }
	    int result = dup2(inFD, 0); // redirect to stdin
	    if (result == -1) { // if -1 is returned 
	       perror("dup2\n"); fflush(stdout); // print an error
	       _exit(1); // exit safely
	    }
	    fcntl(inFD, F_SETFD, FD_CLOEXEC); // close the file
	 }
	 else if (input->inFile == NULL) { // if the user doesn't enter "<"
	    int inFD = open("/dev/null", O_RDONLY); // open file into dev/null
	    if (inFD == -1) { // if -1 is returned
	       perror("open()\n"); fflush(stdout); // print an error
	       _exit(1); // exit safely
	    }
	    int result = dup2(inFD, 0); // redirect to stdin
	    if (result == -1) { // if -1 is returned
	       perror("dup2\n"); fflush(stdout); // print an error
	       _exit(1); // exit safely
	    }
	    fcntl(inFD, F_SETFD, FD_CLOEXEC); // close the file
	 }

	 /* Redirect out below*/
	 if (input->outFile != NULL) { // if the user enters ">"
	    int outFD = open(input->outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); // create a file that's write only
	    if (outFD == -1) { // if -1 is returned
	       perror("open()\n"); fflush(stdout); // print an error
	       _exit(1); // safely exit
	    }
	    int result = dup2(outFD, 1); // redirect to stdout
	    if (result == -1) { // if -1 is returned
	       perror("dup2\n"); fflush(stdout); // print and error
	       _exit(1); // safely exit
	    }
	    fcntl(outFD, F_SETFD, FD_CLOEXEC); // close the file
	 }

	 /* Execute non built-in commands*/
	 if (execvp(input->args[0], input->args)) {  // execute non-built in commands
            fprintf(stderr, "%s: No such file or directory\n", input->args[0]); //fflush(stdout);	 
	    _exit(1);
	 }
      default:
	 if (input->background != NULL) { // if the user enteres "&"
            printBG(spawnPid); // pass in variable to print the background pid
 	    printf("background pid is %d\n", spawnPid); fflush(stdout); // print the background child
	 }
	 else if (input->background == NULL) { // if the user doesn't enter "&"
	    spawnPid = waitpid(spawnPid, &childStat, 0); // set spawnpid to foreground
	 }
	 break;
   }
}


int main () {
   /* The flow of the program */
   int loop = 0;
   do {
      userInput(); // call function for user input
      parseInput(loop); // call function to run the program
   } while (loop == 0); // loop as long as it's 0
   return 0;
}







