#ifndef BUZZ_UTILITY_H
#define BUZZ_UTILITY_H

#include <stdint.h>
#include <buzz/buzzvm.h>
extern int buzz_listen(const char* type,
                       int msg_size);

extern int buzz_script_set(const char* bo_filename,
                           const char* bdbg_filename);

extern void buzz_script_step();

extern void buzz_script_destroy();

extern int buzz_script_done();

//extern void camera_routine();

buzzvm_t get_vm();

int buzz_update_set(uint8_t* UP_BO_BUF, const char* bdbg_filename,size_t bcode_size);

int buzz_update_init_test(uint8_t* UP_BO_BUF, const char* bdbg_filename,size_t bcode_size);

int update_step_test();

//static int testing_buzz_register_hooks();

extern int dummy_closure(buzzvm_t vm);

#endif
