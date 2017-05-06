#include <buzz/buzzasm.h>
#include <buzz_utility.h>
#include <kh4_utility.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "buzz_update.h"

static int done = 0;
//static struct timeval t1, t2;
//double elapsedTime;

/*
 * Print usage information
 */
void usage(const char* path, int status) {
   fprintf(stderr, "Usage:\n");
   fprintf(stderr, "\t%s <stream> <msg_size> <file.bo> <file.bdb>\n\n", path);
   fprintf(stderr, "== Options ==\n\n");
   fprintf(stderr, "  stream        The stream type: tcp or bt\n");
   fprintf(stderr, "  msg_size      The message size in bytes\n");
   fprintf(stderr, "  file.bo       The Buzz bytecode file\n");
   fprintf(stderr, "  file.bdbg     The Buzz debug file\n\n");
   fprintf(stderr, "  number of robots    For the Barrier\n\n");
   fprintf(stderr, "  standby script-file.bo    To be executed during Update\n\n");
   exit(status);
}

static void ctrlc_handler(int sig) {
   done = 1;
}

int main(int argc, char** argv) {
   /* Parse command line */
   if(argc != 7) usage(argv[0], 0);
   /* The stream type */
   char* stream = argv[1];
   if(strcmp(stream, "tcp") != 0 &&
      strcmp(stream, "bt") != 0) {
      fprintf(stderr, "%s: unknown stream type '%s'\n", argv[0], stream);
      usage(argv[0], 0);
   }
   /* The message size */
   char* endptr;
   int msg_sz = strtol(argv[2], &endptr, 10);
   if(endptr == argv[2] || *endptr != '\0') {
      fprintf(stderr, "%s: can't parse '%s' into a number\n", argv[0], argv[2]);
      return 0;
   }
   if(msg_sz <= 0) {
      fprintf(stderr, "%s: invalid value %d for message size\n", argv[0], msg_sz);
      return 0;
   }
   /* barrier number*/
   int barrier = strtol(argv[5], &endptr, 10);
   if(endptr == argv[5] || *endptr != '\0') {
      fprintf(stderr, "%s: can't parse '%s' into a number\n", argv[0], argv[5]);
      return 0;
   }
   if(barrier <= 0) {
      fprintf(stderr, "%s: invalid value %d for barrier size\n", argv[0], barrier);
      return 0;
   }
   /* The bytecode filename */
   char* bcfname = argv[3];
   /* The debugging information file name */
   char* dbgfname = argv[4];
   /*standby code file name*/
   char* stdbyname = argv[6];
   /* Wait for connection */
   if(!buzz_listen(stream, msg_sz)) return 1;
   /* Set CTRL-C handler */
   signal(SIGTERM, ctrlc_handler);
   signal(SIGINT, ctrlc_handler);
   /* Initialize the robot */
   kh4_setup();
   /*initialize camera thread*/
   //camera_routine();
   /*set file name to monitor inside updater*/
   char ext_rmv[strlen(bcfname)+2];
   memcpy(ext_rmv, bcfname, strlen(bcfname)+2);
   char* lastdot = strrchr (ext_rmv, '.');
    if (lastdot != NULL)
        *lastdot = '\0';
   //remove_extension(ext_rmv,bcfname);
   const char* bzz_name=strcat(ext_rmv,".bzz");
   fprintf(stdout,bzz_name);
    fprintf(stdout,"\n");
   //free(ext_rmv);
   set_bzz_file(bzz_name);
   /*initialize update monitor*/
   init_update_monitor(bcfname,stdbyname);
   /*set number of robots inside update monitor*/
   updates_set_robots(barrier);
   /* Set the Buzz bytecode */
   if(buzz_script_set(bcfname, dbgfname)) {
      /* Main loop */
      while(!done && !buzz_script_done()){
         /*Check updater state and step code*/
  	 update_routine(bcfname, dbgfname);
         //gettimeofday(&t1, NULL);
         buzz_script_step();
         usleep(100000);
	 //gettimeofday(&t2, NULL);
	 /*elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      
   	 elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // in milli second
	 elapsedTime = elapsedTime*1000;
         double sleepTime = 120000-elapsedTime;
	 fprintf(stderr,"elapsed time : %.4f.\n",elapsedTime);
	 if(sleepTime<0){
	   fprintf(stderr,"[Warning] Program running slower than 10 Hz : elapsed time : %.4f.\n",elapsedTime);
	 }
	 else
	     usleep(sleepTime);*/
      }
      /* Cleanup */
      buzz_script_destroy();
   }
   
   /* Stop the robot */
   kh4_done();
   destroy_updater();
   /* All done */
   return 0;
}


