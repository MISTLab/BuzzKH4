#ifndef BUZZKH4_CLOSURES_H
#define BUZZKH4_CLOSURES_H

#include <buzz/buzzvm.h>

/*
 * prextern int() function in Buzz
 */
extern int buzzkh4_print(buzzvm_t vm);

/*
 * set_wheels() function in Buzz
 * speeds are expressed in cm/s
 */
extern int buzzkh4_set_wheels(buzzvm_t vm);

/*
 * Updates battery information in Buzz
 */
extern int buzzkh4_update_battery(buzzvm_t vm);

/*
 * Updates IR information in Buzz
 * Proximity and ground sensors
 */
extern int buzzkh4_update_ir(buzzvm_t vm);

#endif
