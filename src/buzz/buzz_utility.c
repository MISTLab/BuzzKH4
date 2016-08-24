#define _GNU_SOURCE
#include <stdio.h>

#include "buzz_utility.h"
#include "buzzkh4_closures.h"
#include <buzz/buzzdebug.h>

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
static uint8_t*    BO_BUF          = 0;
static buzzdebug_t DBG_INFO        = 0;
static int         MSG_SIZE        = -1;
static int         TCP_LIST_STREAM = -1;
static int         TCP_COMM_STREAM = -1;

#define TCP_LIST_STREAM_PORT "24580"

/* Pointer to a function that sends a message on the stream */
void (*STREAM_SEND)() = NULL;

/* PThread handle to manage incoming messages */
pthread_t INCOMING_MSG_THREAD;

/****************************************/
/****************************************/

/* PThread mutex to manage the list of incoming packets */
pthread_mutex_t INCOMING_PACKET_MUTEX;

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
static struct incoming_packet_s* PACKETS = NULL;

void incoming_packet_add(int id,
                         const uint8_t* pl) {
   /* Create packet */
   struct incoming_packet_s* p =
      (struct incoming_packet_s*)malloc(sizeof(struct incoming_packet_s));
   /* Fill in the data */
   p->id = id;
   memcpy(p->payload, pl, MSG_SIZE);
   /* Lock mutex */
   pthread_mutex_lock(&INCOMING_PACKET_MUTEX);
   /* Add as first to list */
   p->next = PACKETS;
   PACKETS = p;
   /* Unlock mutex */
   pthread_mutex_unlock(&INCOMING_PACKET_MUTEX);
}

/****************************************/
/****************************************/

static void* buzz_stream_incoming_thread_tcp(void* args) {
   /* Create buffer for message */
   uint8_t* buf = calloc(MSG_SIZE + sizeof(uint16_t), 1);
   /* Tot bytes left to receive, received up to now, and received at a
    * specific call of recv() */
   ssize_t left, tot, cur;
   while(1) {
      /* Initialize left byte count */
      left = MSG_SIZE + sizeof(uint16_t);
      tot = 0;
      while(left > 0) {
         cur = recv(TCP_COMM_STREAM, buf + tot, left, 0);
         if(cur < 0) {
            fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
            return NULL;
         }
         left -= cur;
         tot += cur;
      }
      /* Done receiving data, add packet to list */
      incoming_packet_add(*(uint16_t*)buf,
                          buf + sizeof(uint16_t));
   }
}

static void buzz_stream_send_tcp(void* payload) {
   /* Buffer for the data to send */
   uint8_t* buf = (uint8_t*)malloc(MSG_SIZE + sizeof(uint16_t));
   *(uint16_t*)buf = VM->robot;
   memcpy(buf + sizeof(uint16_t), payload, MSG_SIZE);
   /* Tot bytes left to send, sent up to now, and sent at a specific
    * call of send() */
   ssize_t left, tot, cur;
   /* Initialize left byte count */
   left = MSG_SIZE + sizeof(uint16_t);
   tot = 0;
   while(left > 0) {
      cur = send(TCP_COMM_STREAM, buf + tot, left, 0);
      if(cur < 0) {
         fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
         exit(1);
      }
      left -= cur;
      tot += cur;
   }
   /* Cleanup */
   free(buf);
}

/****************************************/
/****************************************/

static int buzz_listen_tcp() {
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
   if(pthread_create(&INCOMING_MSG_THREAD, NULL, &buzz_stream_incoming_thread_tcp, NULL) != 0) {
      fprintf(stderr, "Can't create thread: %s\n", strerror(errno));
      close(TCP_COMM_STREAM);
      TCP_COMM_STREAM = -1;
      return 0;
   }
   return 1;
}

static int buzz_listen_bt() {
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

static const char* buzz_error_info() {
   buzzdebug_entry_t dbg = *buzzdebug_info_get_fromoffset(DBG_INFO, &VM->pc);
   char* msg;
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
   }
   return msg;
}

/****************************************/
/****************************************/

static int buzz_register_hooks() {
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "print", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_print));
   buzzvm_gstore(VM);
   buzzvm_pushs(VM,  buzzvm_string_register(VM, "set_wheels", 1));
   buzzvm_pushcc(VM, buzzvm_function_register(VM, buzzkh4_set_wheels));
   buzzvm_gstore(VM);
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
   int id = strtol(hstnm + 1, NULL, 10);
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
   /* Execute the global part of the script */
   buzzvm_execute_script(VM);
   /* Call the Init() function */
   buzzvm_function_call(VM, "init", 0);
   /* All OK */
   return 1;
}

/****************************************/
/****************************************/

extern void buzz_script_step() {
   /*
    * Process incoming messages
    */
   /* Reset neighbor information */
   buzzneighbors_reset(VM);
   /* Lock mutex */
   pthread_mutex_lock(&INCOMING_PACKET_MUTEX);
   /* Go through messages and add them to the FIFO */
   struct incoming_packet_s* p = PACKETS;
   struct incoming_packet_s* n;
   while(p) {
      /* Save next packet */
      n = p->next;
      /* Update Buzz information */
      buzzneighbors_add(VM, p->id, 0.0, 0.0, 0.0);
      buzzinmsg_queue_append(VM->inmsgs,
                             buzzmsg_payload_frombuffer(p->payload,
                                                        MSG_SIZE));
      /* Erase packet */
      free(p);
      /* Go to next packet */
      p = n;
   }
   /* Unlock mutex */
   pthread_mutex_unlock(&INCOMING_PACKET_MUTEX);
   /* Process messages */
   buzzvm_process_inmsgs(VM);
   /*
    * Update sensors
    */
   buzzkh4_update_battery(VM);
   buzzkh4_update_ir(VM);
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
   while(!buzzoutmsg_queue_isempty(VM->outmsgs)) {
      /* Get message */
      buzzmsg_payload_t m = buzzoutmsg_queue_first(VM->outmsgs);
      /* Send message */
      STREAM_SEND(m->data);
      /* Get rid of message */
      buzzoutmsg_queue_next(VM->outmsgs);
      buzzmsg_payload_destroy(&m);
   }
}

/****************************************/
/****************************************/

void buzz_script_destroy() {
   /* Cancel thread */
   pthread_cancel(INCOMING_MSG_THREAD);
   pthread_join(INCOMING_MSG_THREAD, NULL);
   /* Get rid of virtual machine */
   if(VM) {
      buzzvm_function_call(VM, "destroy", 0);
      buzzvm_destroy(&VM);
      free(BO_FNAME);
      buzzdebug_destroy(&DBG_INFO);
   }
}

/****************************************/
/****************************************/

int buzz_script_done() {
   return VM->state != BUZZVM_STATE_READY;
}

/****************************************/
/****************************************/
