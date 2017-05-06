#define _GNU_SOURCE
#include <stdio.h>

#include "buzz_utility.h"
#include "buzzkh4_closures.h"
#include <buzz/buzzdebug.h>
//#include "kh4_camera.h"
#include "buzz_update.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>

/****************************************/
/****************************************/

static buzzvm_t    VM              = 0;
static char*       BO_FNAME        = 0;
static char* 	   DBG_FNAME	   =0;
static uint8_t*    BO_BUF          = 0;
static buzzdebug_t DBG_INFO        = 0;
static int         MSG_SIZE        = -1;
static int         TCP_LIST_STREAM = -1;
static int         TCP_COMM_STREAM = -1;
static uint8_t*    STREAM_SEND_BUF = NULL;
//static int         blob_pos[4];
//static int         enable_cam=0;

#define TCP_LIST_STREAM_PORT "24580"
#define IDOFFSET 0

/* Pointer to a function that sends a message on the stream */
static void (*STREAM_SEND)() = NULL;
//int buzzutility_enable_camera(buzzvm_t vm);
/* PThread handle to manage incoming messages */
static pthread_t INCOMING_MSG_THREAD;
/* PThread handle to manage blob center */
//static pthread_t blob_manage;

//void set_enable_cam(int cam_enable);

//int get_enable_cam();
/****************************************/
/****************************************/

/* PThread mutex to manage the list of incoming packets */
static pthread_mutex_t INCOMING_PACKET_MUTEX;
/*MUtex for camera*/
//static pthread_mutex_t camera_mutex;
/*MUtex for camera enable*/
//static pthread_mutex_t camera_enable_mutex;

/* List of packets received over the stream */
struct incoming_packet_s {
   /* Id of the message sender */
   int id;
   /* Payload */
   uint8_t* payload;
   /* Next message */
   struct incoming_packet_s* next;
};

/* The list of incoming packets */
static struct incoming_packet_s* PACKETS_FIRST = NULL;
static struct incoming_packet_s* PACKETS_LAST  = NULL;

void incoming_packet_add(int id,
                         const uint8_t* pl) {
   /* Create packet */
   struct incoming_packet_s* p =
      (struct incoming_packet_s*)malloc(sizeof(struct incoming_packet_s));
   /* Fill in the data */
   p->id = id;
   p->payload = malloc(MSG_SIZE - sizeof(uint16_t));
   memcpy(p->payload, pl, MSG_SIZE - sizeof(uint16_t));
   p->next = NULL;
   /* Lock mutex */
   pthread_mutex_lock(&INCOMING_PACKET_MUTEX);
   /* Add as last to list */
   if(PACKETS_FIRST != NULL)
      PACKETS_LAST->next = p;
   else
      PACKETS_FIRST = p;
   PACKETS_LAST = p;
   /* Unlock mutex */
   pthread_mutex_unlock(&INCOMING_PACKET_MUTEX);
   /* fprintf(stderr, "[DEBUG]    Added packet from %d\n", id); */
}

/****************************************/
/****************************************/

void* buzz_stream_incoming_thread_tcp(void* args) {
   /* Create buffer for message */
   uint8_t* buf = calloc(MSG_SIZE, 1);
   /* Tot bytes left to receive, received up to now, and received at a
    * specific call of recv() */
   ssize_t left, tot, cur;
   while(1) {
      /* Initialize left byte count */
      left = MSG_SIZE;
      tot = 0;
      while(left > 0) {
         cur = recv(TCP_COMM_STREAM, buf + tot, left, 0);
         /* fprintf(stderr, "[DEBUG] Received %zd bytes", cur); */
         if(cur < 0) {
            fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
            free(buf);
            return NULL;
         }
         if(cur == 0) {
            fprintf(stderr, "Connection closed by peer\n");
            free(buf);
            return NULL;
         }
         left -= cur;
         tot += cur;
         /* fprintf(stderr, ", %zd left, %zd tot\n", left, tot); */
      }
      /* Done receiving data, add packet to list */
      incoming_packet_add(*(uint16_t*)buf,
                          buf + sizeof(uint16_t));
   }
}

void buzz_stream_send_tcp() {
   /* Tot bytes left to send, sent up to now, and sent at a specific
    * call of send() */
   ssize_t left, tot, cur;
   /* Initialize left byte count */
   left = MSG_SIZE;
   tot = 0;
   while(left > 0) {
      cur = send(TCP_COMM_STREAM, STREAM_SEND_BUF + tot, left, 0);
      /* fprintf(stderr, "[DEBUG] Sent %zd bytes", cur); */
      if(cur < 0) {
         fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
         exit(1);
      }
      if(cur == 0) {
         fprintf(stderr, "Connection closed by peer\n");
         exit(1);
      }
      left -= cur;
      tot += cur;
      /* fprintf(stderr, ", %zd left, %zd tot\n", left, tot); */
   }
}

/****************************************/
/****************************************/

int buzz_listen_tcp() {
   /* Used to store the return value of the network function calls */
   int retval;
   /* Get information on the available interfaces */
   struct addrinfo hints, *ifaceinfo;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;       /* Only IPv4 is accepted */
   hints.ai_socktype = SOCK_STREAM; /* TCP socket */
   hints.ai_flags = AI_PASSIVE;     /* Necessary for bind() later on */
   retval = getaddrinfo(NULL,
                        TCP_LIST_STREAM_PORT,
                        &hints,
                        &ifaceinfo);
   if(retval != 0) {
      fprintf(stderr, "Error getting local address information: %s\n",
              gai_strerror(retval));
      return 0;
   }
   /* Bind on the first interface available */
   TCP_LIST_STREAM = -1;
   struct addrinfo* iface = NULL;
   for(iface = ifaceinfo;
       (iface != NULL) && (TCP_LIST_STREAM == -1);
       iface = iface->ai_next) {
      TCP_LIST_STREAM = socket(iface->ai_family,
                               iface->ai_socktype,
                               iface->ai_protocol);
      if(TCP_LIST_STREAM > 0) {
         int true = 1;
         if((setsockopt(TCP_LIST_STREAM,
                        SOL_SOCKET,
                        SO_REUSEADDR,
                        &true,
                        sizeof(true)) != -1)
            &&
            (bind(TCP_LIST_STREAM,
                  iface->ai_addr,
                  iface->ai_addrlen) == -1)) {
            close(TCP_LIST_STREAM);
            TCP_LIST_STREAM = -1;
         }
      }
   }
   freeaddrinfo(ifaceinfo);
   if(TCP_LIST_STREAM == -1) {
      fprintf(stderr, "Can't bind socket to any interface\n");
      return 0;
   }
   /* Listen on the socket */
   fprintf(stdout, "Listening on port " TCP_LIST_STREAM_PORT "...\n");
   if(listen(TCP_LIST_STREAM, 1) == -1) {
      close(TCP_LIST_STREAM);
      TCP_LIST_STREAM = -1;
      fprintf(stderr, "Can't listen on the socket: %s\n",
              strerror(errno));
      return 0;
   }
   /* Accept incoming connection */
   struct sockaddr addr;
   socklen_t addrlen = sizeof(addr);
   TCP_COMM_STREAM = accept(TCP_LIST_STREAM, &addr, &addrlen);
   if(TCP_COMM_STREAM == -1) {
      close(TCP_LIST_STREAM);
      TCP_LIST_STREAM = -1;
      fprintf(stderr, "Error accepting connection: %s\n",
              strerror(errno));
      return 0;
   }
   fprintf(stdout, "Accepted connection from %s\n",
           inet_ntoa(((struct sockaddr_in*)(&addr))->sin_addr));
   /* Ready to communicate through TCP */
   STREAM_SEND = buzz_stream_send_tcp;
   STREAM_SEND_BUF = (uint8_t*)malloc(MSG_SIZE);
   if(pthread_create(&INCOMING_MSG_THREAD, NULL, &buzz_stream_incoming_thread_tcp, NULL) != 0) {
      fprintf(stderr, "Can't create thread: %s\n", strerror(errno));
      close(TCP_COMM_STREAM);
      TCP_COMM_STREAM = -1;
      return 0;
   }
   return 1;
}

int buzz_listen_bt() {
   return 0;
}

int buzz_listen(const char* type,
                int msg_size) {
   /* Set the message size */
   MSG_SIZE = msg_size;
   /* Create the mutex */
   if(pthread_mutex_init(&INCOMING_PACKET_MUTEX, NULL) != 0) {
      fprintf(stderr, "Error initializing the incoming packet mutex: %s\n",
              strerror(errno));
      return 0;
   }
   /* Listen to connections */
   if(strcmp(type, "tcp") == 0)
      return buzz_listen_tcp();
   else if(strcmp(type, "bt") == 0)
      return buzz_listen_bt();
   return 0;
}


/****************************************/
/****************************************/
//TODO check this out
static const char* buzz_error_info() {
 char* msg;
  /* buzzdebug_entry_t dbg = *buzzdebug_info_get_fromoffset(DBG_INFO, &VM->pc);
   
   if(dbg != NULL) {
      asprintf(&msg,
               "%s: execution terminated abnormally at %s:%" PRIu64 ":%" PRIu64 " : %s\n\n",
               BO_FNAME,
               dbg->fname,
               dbg->line,
               dbg->col,
               VM->errormsg);
   }
   else {
      asprintf(&msg,
               "%s: execution terminated abnormally at bytecode offset %d: %s\n\n",
               BO_FNAME,
               VM->pc,
               VM->errormsg);
   }*/
   return msg;
}

/****************************************/
/****************************************/

static int buzz_register_hooks() {
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "print", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_print));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "log", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_print));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "set_wheels", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_set_wheels));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "set_leds", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_set_leds));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM, buzzvm_string_register(VM, "goto", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, BuzzGoTo));
   buzzvm_gstore(VM);
   //buzzvm_pushs(VM,  buzzvm_string_register(VM, "enable_camera", 1));
   //buzzvm_pushcc(VM, buzzvm_function_register(VM, dummy_closure));//TODO replaced with dummy closure
   //buzzvm_gstore(VM);
   return BUZZVM_STATE_READY;
}

/****************************************/
/****************************************/

static int testing_buzz_register_hooks() {
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "print", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_print));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "log", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_print));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "set_wheels", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, dummy_closure));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "set_leds", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, dummy_closure));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM, buzzvm_string_register(VM, "goto", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, dummy_closure));
   buzzvm_gstore(VM);
   //buzzvm_pushs(VM,  buzzvm_string_register(VM, "enable_camera", 1));
   //buzzvm_pushcc(VM, buzzvm_function_register(VM, dummy_closure));
   //buzzvm_gstore(VM);
   return BUZZVM_STATE_READY;
}

/****************************************/
/****************************************/

int buzz_script_set(const char* bo_filename,
                    const char* bdbg_filename) {
   /* Get hostname */
   char hstnm[30];
   gethostname(hstnm, 30);
   /* Make numeric id from hostname */
   /* NOTE: here we assume that the hostname is in the format Knn */
   int id = strtol(hstnm + 1, NULL, 10) + IDOFFSET;	//CHANGES FOR OFFROBOTS TESTS!!!!
   /* Reset the Buzz VM */
   if(VM) buzzvm_destroy(&VM);
   VM = buzzvm_new(id);
   /* Get rid of debug info */
   if(DBG_INFO) buzzdebug_destroy(&DBG_INFO);
   DBG_INFO = buzzdebug_new();
   /* Read bytecode and fill in data structure */
   FILE* fd = fopen(bo_filename, "rb");
   if(!fd) {
      perror(bo_filename);
      return 0;
   }
   fseek(fd, 0, SEEK_END);
   size_t bcode_size = ftell(fd);
   rewind(fd);
   BO_BUF = (uint8_t*)malloc(bcode_size);
   if(fread(BO_BUF, 1, bcode_size, fd) < bcode_size) {
      perror(bo_filename);
      buzzvm_destroy(&VM);
      buzzdebug_destroy(&DBG_INFO);
      fclose(fd);
      return 0;
   }
   fclose(fd);
   /* Read debug information */
   if(!buzzdebug_fromfile(DBG_INFO, bdbg_filename)) {
      buzzvm_destroy(&VM);
      buzzdebug_destroy(&DBG_INFO);
      perror(bdbg_filename);
      return 0;
   }
   /* Set byte code */
   if(buzzvm_set_bcode(VM, BO_BUF, bcode_size) != BUZZVM_STATE_READY) {
      buzzvm_destroy(&VM);
      buzzdebug_destroy(&DBG_INFO);
      fprintf(stdout, "%s: Error loading Buzz script\n\n", bo_filename);
      return 0;
   }
   /* Register hook functions */
   if(buzz_register_hooks() != BUZZVM_STATE_READY) {
      buzzvm_destroy(&VM);
      buzzdebug_destroy(&DBG_INFO);
      fprintf(stdout, "%s: Error registering hooks\n\n", bo_filename);
      return 0;
   }
   /* Save bytecode file name */
   BO_FNAME = strdup(bo_filename);
   DBG_FNAME = strdup(bdbg_filename);
   /* Execute the global part of the script */
   buzzvm_execute_script(VM);
   /* Call the Init() function */
   buzzvm_function_call(VM, "init", 0);
   /* All OK */
   return 1;
}

/****************************************/
/*Sets a new update    :TODO multiple functions with same code reduce to 1*/
/****************************************/
int buzz_update_set(uint8_t* UP_BO_BUF, const char* bdbg_filename,size_t bcode_size){
/* Get hostname */
char hstnm[30];
gethostname(hstnm, 30);
int id = strtol(hstnm + 1, NULL, 10) + IDOFFSET;	//CHANGES FOR OFFROBOTS TESTS!!!!
// Reset the Buzz VM
if(VM) buzzvm_destroy(&VM);
VM = buzzvm_new(id);
// Get rid of debug info
if(DBG_INFO) buzzdebug_destroy(&DBG_INFO);
DBG_INFO = buzzdebug_new();

// Read debug information
if(!buzzdebug_fromfile(DBG_INFO, DBG_FNAME)) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	perror(DBG_FNAME);
	return 0;
 }
//BO_BUF = (uint8_t*)malloc(bcode_size);
//memcpy(UP_BO_BUF, BO_BUF, bcode_size);
// Set byte code
if(buzzvm_set_bcode(VM, UP_BO_BUF, bcode_size) != BUZZVM_STATE_READY) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	fprintf(stdout, "%s: Error loading Buzz script\n\n", BO_FNAME);
	return 0;
 }
// Register hook functions
if(buzz_register_hooks() != BUZZVM_STATE_READY) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	fprintf(stdout, "%s: Error registering hooks\n\n", BO_FNAME);
	return 0;
}

// Execute the global part of the script
buzzvm_execute_script(VM);
// Call the Init() function
buzzvm_function_call(VM, "init", 0);
// All OK
return 1;
}

/****************************************/
/*Performs a initialization test        */
/****************************************/
int buzz_update_init_test(uint8_t* UP_BO_BUF, const char* bdbg_filename,size_t bcode_size){
/* Get hostname */
char hstnm[30];
gethostname(hstnm, 30);
int id = strtol(hstnm + 1, NULL, 10) + IDOFFSET;	//CHANGES FOR OFFROBOTS TESTS!!!!
// Reset the Buzz VM
if(VM) buzzvm_destroy(&VM);
VM = buzzvm_new(id);
// Get rid of debug info
if(DBG_INFO) buzzdebug_destroy(&DBG_INFO);
DBG_INFO = buzzdebug_new();

// Read debug information
if(!buzzdebug_fromfile(DBG_INFO, DBG_FNAME)) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	perror(DBG_FNAME);
	return 0;
 }
// Set byte code
if(buzzvm_set_bcode(VM, UP_BO_BUF, bcode_size) != BUZZVM_STATE_READY) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	fprintf(stdout, "%s: Error loading Buzz script\n\n", BO_FNAME);
	return 0;
 }
// Register hook functions
if(testing_buzz_register_hooks() != BUZZVM_STATE_READY) {
	buzzvm_destroy(&VM);
	buzzdebug_destroy(&DBG_INFO);
	fprintf(stdout, "%s: Error registering hooks\n\n", BO_FNAME);
	return 0;
}

// Execute the global part of the script
buzzvm_execute_script(VM);
// Call the Init() function
buzzvm_function_call(VM, "init", 0);
// All OK
return 1;
}

/*****************************************/
/*****************************************/

int update_step_test() {
   /*
    * Update sensors
    */
   buzzkh4_update_battery(VM);
   buzzkh4_update_ir(VM);

   int a = buzzvm_function_call(VM, "step", 0);
	if(a != BUZZVM_STATE_READY) {
		fprintf(stdout, "step test VM state %i\n",a);
		fprintf(stdout, " execution terminated abnormally\n\n");
	}
	return a == BUZZVM_STATE_READY;


}

/****************************************/
/****************************************/

struct buzzswarm_elem_s {
   buzzdarray_t swarms;
   uint16_t age;
};
typedef struct buzzswarm_elem_s* buzzswarm_elem_t;

void check_swarm_members(const void* key, void* data, void* params) {
   buzzswarm_elem_t e = *(buzzswarm_elem_t*)data;
   int* status = (int*)params;
   if(*status == 3) return;
   fprintf(stderr, "CHECKING SWARM MEMBERS:%i\n",buzzdarray_get(e->swarms, 0, uint16_t));
   if(buzzdarray_size(e->swarms) != 1) {
      fprintf(stderr, "Swarm list size is not 1\n");
      *status = 3;
   }
   else {
      int sid = 1;
      if(!buzzdict_isempty(VM->swarms)) {
        if(*buzzdict_get(VM->swarms, &sid, uint8_t) &&
           buzzdarray_get(e->swarms, 0, uint16_t) != sid) {
           fprintf(stderr, "I am in swarm #%d and neighbor is in %d\n",
                   sid,
                   buzzdarray_get(e->swarms, 0, uint16_t));
           *status = 3;
           return;
         }
         if(buzzdict_size(VM->swarms)>1) {
           sid = 2;
           if(*buzzdict_get(VM->swarms, &sid, uint8_t) &&
             buzzdarray_get(e->swarms, 0, uint16_t) != sid) {
             fprintf(stderr, "I am in swarm #%d and neighbor is in %d\n",
                     sid,
                     buzzdarray_get(e->swarms, 0, uint16_t));
             *status = 3;
             return;
            }
          }
       }
   }
}

void buzz_script_step() {
   /*
    * Process incoming messages
    */
   /* Reset neighbor information */
   buzzneighbors_reset(VM);
   /* Lock mutex */
   /* fprintf(stderr, "[DEBUG] Processing incoming packets...\n"); */
   pthread_mutex_lock(&INCOMING_PACKET_MUTEX);
   /* Go through messages and add them to the FIFO */
   struct incoming_packet_s* n;
   while(PACKETS_FIRST) {
      /* Save next packet */
      n = PACKETS_FIRST->next;
      /* Update Buzz neighbors information */
      uint8_t* pl = (uint8_t*)PACKETS_FIRST->payload;
      float x=0.0,y=0.0,t=0.0;
      size_t tot = 0;
      memcpy(&x, pl+tot, sizeof(float));
      tot += sizeof(float);
      memcpy(&y, pl+tot, sizeof(float));
      tot += sizeof(float);
      memcpy(&t, pl+tot, sizeof(float));
      tot += sizeof(float);
      /*if updater message present append it to its queue*/
      fprintf(stderr, "[DEBUG]received   updater message present = %i\n", *(uint8_t*)(pl+tot)); 
     if(*(uint8_t*)(pl+tot)){
	tot +=sizeof(uint8_t);
	uint16_t upMsgSize = *(uint16_t*)(pl+tot);
	tot+=sizeof(uint16_t);	
	code_message_inqueue_append((uint8_t*)(pl+tot),upMsgSize);
	code_message_inqueue_process();
	tot+=upMsgSize;
      }
      else tot +=sizeof(uint8_t);	
      //fprintf(stdout,"got neighbors position: %.2f,%.2f,%.2f\n",x,y,t);
      buzzneighbors_add(VM, PACKETS_FIRST->id, x, y, t);
      /* Go through the payload and extract the messages */
      uint16_t msgsz;
      /* fprintf(stderr, "[DEBUG] Processing packet %p from %d\n",
               PACKETS_FIRST,
               PACKETS_FIRST->id);*/
      /* fprintf(stderr, "[DEBUG] recv sz = %u\n", */
      /*         *(uint16_t*)pl); */
      do {
         /* Get payload size */
         msgsz = *(uint16_t*)(pl + tot);
         tot += sizeof(uint16_t);
         /* fprintf(stderr, "[DEBUG]    msg size = %u, tot = %zu\n", msgsz, tot); */
         /* Make sure the message payload can be read */
         if(msgsz > 0 && msgsz <= MSG_SIZE - tot) {
            /* Append message to the Buzz input message queue */
            buzzinmsg_queue_append(
               VM,
               buzzmsg_payload_frombuffer(pl + tot, msgsz));
            tot += msgsz;
            /* fprintf(stderr, "[DEBUG]    appended message, tot = %zu\n", tot); */
         }
      }
      while(MSG_SIZE - tot > sizeof(uint16_t) && msgsz > 0);
      /* Erase packet */
      /* fprintf(stderr, "[DEBUG] Done processing packet %p from %d\n", */
      /*         PACKETS_FIRST, */
      /*         PACKETS_FIRST->id); */
      free(PACKETS_FIRST->payload);
      free(PACKETS_FIRST);
      /* Go to next packet */
      PACKETS_FIRST = n;
   }
   /* The packet list is now empty */
   PACKETS_LAST = NULL;
   /* Unlock mutex */
   pthread_mutex_unlock(&INCOMING_PACKET_MUTEX);
   /* fprintf(stderr, "[DEBUG] Done processing incoming packets.\n"); */
   /* Process messages */
   buzzvm_process_inmsgs(VM);
   /*
    * Update sensors
    */
   buzzkh4_update_battery(VM);
   buzzkh4_update_ir(VM);

   //pthread_mutex_lock(&camera_mutex);
   //buzzkh4_camera_updateblob(VM,blob_pos);
   //pthread_mutex_unlock(&camera_mutex);

   /*
    * Call Buzz step() function
    */
   if(buzzvm_function_call(VM, "step", 0) != BUZZVM_STATE_READY) {
      fprintf(stderr, "%s: execution terminated abnormally: %s\n\n",
              BO_FNAME,
              buzz_error_info());
      buzzvm_dump(VM);
   }
   /*
    * Broadcast messages
    */
   /* Prepare buffer */
   memset(STREAM_SEND_BUF, 0, MSG_SIZE);
   fprintf(stderr, "[DEBUG]msg_size = %i\n", MSG_SIZE); 
   *(uint16_t*)STREAM_SEND_BUF = VM->robot;
   fprintf(stderr, "[DEBUG]from vm , %i ,rid = %i\n",VM->robot, *(uint16_t*)STREAM_SEND_BUF); 
   
   ssize_t tot = sizeof(uint16_t);
      /* add local position*/
      float x=0;
      //memset(STREAM_SEND_BUF + tot, 0, sizeof(float));
      *(float*) (STREAM_SEND_BUF+tot) = x;
      tot += sizeof(float);
      *(float*)(STREAM_SEND_BUF+tot) = x;
      tot += sizeof(float);
      *(float*)(STREAM_SEND_BUF+tot) = x;
      tot += sizeof(float);
      /*if updater message present send it TODO add check for available message size and code size from updater*/
      if((int)get_update_mode()!=CODE_RUNNING && is_msg_present()==1){
	uint8_t up_message_present=1;        
         *(uint8_t*)(STREAM_SEND_BUF+tot) =up_message_present;
	tot += sizeof(uint8_t);
	uint16_t updater_msgSize=*(uint16_t*) (getupdate_out_msg_size());
	fprintf(stdout,"[Debug]Transfering code \n");
	 *(uint16_t*)(STREAM_SEND_BUF + tot)=updater_msgSize;
         tot += sizeof(uint16_t);
	 fprintf(stdout,"[Debug]Updater sent msg size : %i \n", (int)updater_msgSize);
	 // Append updater msgs
   	 memcpy(STREAM_SEND_BUF + tot, (uint8_t*)(getupdater_out_msg()), updater_msgSize);
   	 tot += updater_msgSize;
         destroy_out_msg_queue();
      }
      else{
         uint8_t up_message_present=0;
         fprintf(stdout,"[Debug]no update message \n");
         *(uint8_t*)(STREAM_SEND_BUF+tot) =up_message_present;
	 fprintf(stderr, "[DEBUG]sent update message present = %i\n", *(uint8_t*)(STREAM_SEND_BUF+tot)); 
         tot += sizeof(uint8_t);
      }
	
      
      fprintf(stdout,"sending neighbors position: %.2f,%.2f,%.2f\n",x,x,x);
   do {
      /* Are there more messages? */
      if(buzzoutmsg_queue_isempty(VM)) break;
      /* Get first message */
      buzzmsg_payload_t m = buzzoutmsg_queue_first(VM);
      /* Make sure it fits the data buffer */
      if(tot + buzzmsg_payload_size(m) + sizeof(uint16_t)
         >
         MSG_SIZE) {
         buzzmsg_payload_destroy(&m);
         break;
      }
      /* Add message length to data buffer */
      /* fprintf(stderr, "[DEBUG] send before sz = %u\n", */
      /*         *(uint16_t*)(STREAM_SEND_BUF + 2)); */
      *(uint16_t*)(STREAM_SEND_BUF + tot) = (uint16_t)buzzmsg_payload_size(m);
      tot += sizeof(uint16_t);
      /* fprintf(stderr, "[DEBUG] send after sz = %u\n", */
      /*         *(uint16_t*)(STREAM_SEND_BUF + 2)); */
      /* Add payload to data buffer */
      memcpy(STREAM_SEND_BUF + tot, m->data, buzzmsg_payload_size(m));
      tot += buzzmsg_payload_size(m);
      fprintf(stderr, "[DEBUG] send before sz = %u\n",
               *(uint16_t*)(STREAM_SEND_BUF + 2));
      /* Get rid of message */
      buzzoutmsg_queue_next(VM);
      buzzmsg_payload_destroy(&m);
   } while(1);
   /* fprintf(stderr, "[DEBUG] send id = %u, sz = %u\n", */
   /*         *(uint16_t*)STREAM_SEND_BUF, */
   /*         *(uint16_t*)(STREAM_SEND_BUF + 2)); */
   /* Send messages */
      /*float xo=0.0,yo=0.0,to=0.0;
      memcpy(&xo, STREAM_SEND_BUF+sizeof(uint16_t), sizeof(float));
      memcpy(&yo, STREAM_SEND_BUF+sizeof(uint16_t)+sizeof(float), sizeof(float));
      memcpy(&to, STREAM_SEND_BUF+sizeof(uint16_t)+2*sizeof(float), sizeof(float));
      fprintf(stdout,"retrieving neighbors position: %.2f,%.2f,%.2f\n",xo,yo,to);*/
   buzzvm_process_outmsgs(VM);
   STREAM_SEND();
   /* Sleep */
   //usleep(100000);
   /* Print swarm */
   buzzswarm_members_print(stdout, VM->swarmmembers, VM->robot);
   /* Check swarm state */
  /* int status = 1;
   buzzdict_foreach(VM->swarmmembers, check_swarm_members, &status);
   if(status == 1 &&
      buzzdict_size(VM->swarmmembers) < 9)
      status = 2;
   buzzvm_pushs(VM, buzzvm_string_register(VM, "swarm_status", 1));
   buzzvm_pushi(VM, status);
   buzzvm_gstore(VM);*/
}

/****************************************/
/****************************************/

void buzz_script_destroy() {
//printf("buzz_script_destroy\n");
   /* Cancel thread */
   pthread_cancel(INCOMING_MSG_THREAD);
   pthread_join(INCOMING_MSG_THREAD, NULL);
   /*camera thread*/
//printf("PRECANCEL\n");
//   pthread_cancel(blob_manage);
//printf("PREJOIN\n");
//   pthread_join(blob_manage, NULL);
//printf("AFTERJOIN\n");
   //stop_camera();
//printf("STOPCAM\n");
   
   /* Get rid of stream buffer */
   free(STREAM_SEND_BUF);
   /* Get rid of virtual machine */
   if(VM) {
      if(VM->state != BUZZVM_STATE_READY) {
         fprintf(stderr, "%s: execution terminated abnormally: %s\n\n",
                 BO_FNAME,
                 buzz_error_info());
         buzzvm_dump(VM);
      }
      buzzvm_function_call(VM, "destroy", 0);
      buzzvm_destroy(&VM);
      free(BO_FNAME);
      buzzdebug_destroy(&DBG_INFO);
   }
   fprintf(stdout, "Script execution stopped.\n");
}

/****************************************/
/****************************************/

int buzz_script_done() {
   return VM->state != BUZZVM_STATE_READY;
}

/****************************************/
/****************************************/
/*
void* camera_thread(void *args){
int* blob;
   while(1){
      int cam_enable=get_enable_cam();
      pthread_testcancel();
      if(cam_enable == 1){   
      //blob = get_blob_pos();
      pthread_testcancel();
      pthread_mutex_lock(&camera_mutex);
      blob_pos[0] =blob[0];
      blob_pos[1] =blob[1];
      blob_pos[2] =blob[2];
      blob_pos[3] =blob[3];
      pthread_mutex_unlock(&camera_mutex);
      pthread_testcancel();
      }
   }


}*/
/**************************************************/
/*************************************************/
/*void camera_routine(){
  //  initialize_camera();
   
  // pthread_create(&blob_manage, NULL, &camera_thread, NULL);
   
   
}
int buzzutility_enable_camera(buzzvm_t vm){
   buzzvm_lnum_assert(vm, 1);
   buzzvm_lload(vm, 1); // 0 disable 1 enable 
   buzzvm_type_assert(vm, 1, BUZZTYPE_INT);
   set_enable_cam(buzzvm_stack_at(vm, 1)->i.value);
   printf("enablecam_val = \n, buzz returned value = \n");
   return buzzvm_ret0(vm);
}

void set_enable_cam(int cam_enable){
	//pthread_mutex_lock(&camera_enable_mutex);
	enable_cam=cam_enable;
	//pthread_mutex_unlock(&camera_enable_mutex);
}

int get_enable_cam(){
	int cam_enable;	
	//pthread_mutex_lock(&camera_enable_mutex);
	cam_enable=enable_cam;
	//pthread_mutex_unlock(&camera_enable_mutex);
return cam_enable;
}*/

buzzvm_t get_vm() {
return VM;
}

extern int dummy_closure(buzzvm_t vm){
return buzzvm_ret0(vm);
}
