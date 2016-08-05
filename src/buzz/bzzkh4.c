#include <buzz/buzzasm.h>
#include <buzz_utility.h>
#include <kh4_utility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Print usage information
 */
void usage(const char* path, int status) {
   fprintf(stderr, "Usage:\n\t%s <file.bo> <file.bdb>\n\n", path);
   exit(status);
}

int main(int argc, char** argv) {
   /* The bytecode filename */
   char* bcfname;
   /* The debugging information file name */
   char* dbgfname;
   /* Parse command line */
   if(argc != 2) usage(argv[0], 0);
   bcfname = argv[1];
   dbgfname = argv[2];
   /* Initialize the robot */
   kh4_setup();
   /* Set the Buzz bytecode */
   if(buzz_script_set(bcfname, dbgfname)) {
      /* Main loop */
      while(!buzz_script_done())
         buzz_script_step();
      /* Cleanup */
      buzz_script_destroy();
   }
   /* Stop the robot */
   kh4_done();
   /* All done */
   return 0;
}
