#ifndef SHELL
#define SHELL


struct Input {
   char *command;
   char *args[513];
   char *inFile;
   char *outFile;
   char *background;
   char *expVar;
   struct Input *next;

};
#endif
