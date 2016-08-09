#include <buzz/buzzasm.h>
#include <buzz_utility.h>
#include <kh4_utility.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static int done = 0;

/*
 * Print usage information
 */
void usage(const char* path, int status) {
   fprintf(stderr, "Usage:\n\t%s <file.bo> <file.bdb>\n\n", path);
   exit(status);
}

static void ctrlc_handler(int sig) {
   done = 1;
   /* static int already_called = 0; */
   /* if(!already_called) { */
   /*    already_called = 1; */
   /*    printf("\nEXITING\n"); */
   /*    usleep(1000000); */
   /*    printf("SCRIPT DESTROY\n"); */
   /*    buzz_script_destroy(); */
   /*    printf("KH4 DESTROY\n"); */
   /*    kh4_done(); */
   /*    printf("DONE\n"); */
   /* } */
}

int main(int argc, char** argv) {
   /* The bytecode filename */
   char* bcfname;
   /* The debugging information file name */
   char* dbgfname;
   /* Parse command line */
   if(argc != 3) usage(argv[0], 0);
   bcfname = argv[1];
   dbgfname = argv[2];
   /* Set CTRL-C handler */
   signal(SIGTERM, ctrlc_handler);
   signal(SIGINT, ctrlc_handler);
   /* Initialize the robot */
   kh4_setup();
   /* Set the Buzz bytecode */
   if(buzz_script_set(bcfname, dbgfname)) {
      /* Main loop */
      while(!done && !buzz_script_done())
         buzz_script_step();
      /* Cleanup */
      printf("SCRIPT DESTROY\n");
      buzz_script_destroy();
   }
   
   /* Stop the robot */
   printf("KH4 DESTROY\n");
   kh4_done();
   /* All done */
   printf("DONE\n");
   return 0;
}
