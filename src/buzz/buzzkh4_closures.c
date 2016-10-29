#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include "buzzkh4_closures.h"
#include "kh4_utility.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int TurningMechanism = 0;
/****************************************/
/****************************************/

int buzzkh4_print(buzzvm_t vm) {
   int i;
   for(i = 1; i < buzzdarray_size(vm->lsyms->syms); ++i) {
      buzzvm_lload(vm, i);
      buzzobj_t o = buzzvm_stack_at(vm, 1);
      buzzvm_pop(vm);
      switch(o->o.type) {
         case BUZZTYPE_NIL:
            fprintf(stdout, "[nil]");
            break;
         case BUZZTYPE_INT:
            fprintf(stdout, "%d", o->i.value);
            break;
         case BUZZTYPE_FLOAT:
            fprintf(stdout, "%f", o->f.value);
            break;
         case BUZZTYPE_TABLE:
            fprintf(stdout, "[table with %d elems]", (buzzdict_size(o->t.value)));
            break;
         case BUZZTYPE_CLOSURE:
            if(o->c.value.isnative)
               fprintf(stdout, "[n-closure @%d]", o->c.value.ref);
            else
               fprintf(stdout, "[c-closure @%d]", o->c.value.ref);
            break;
         case BUZZTYPE_STRING:
            fprintf(stdout, "%s", o->s.value.str);
            break;
         case BUZZTYPE_USERDATA:
            fprintf(stdout, "[userdata @%p]", o->u.value);
            break;
         default:
            break;
      }
   }
   fprintf(stdout, "\n");
   return buzzvm_ret0(vm);
}

void WrapValue(float *t_value) {
         while(*t_value > 3.1416) *t_value -= 2*3.1416;
         while(*t_value < -3.1416) *t_value += 2*3.1416;
}

void SetWheelSpeedsFromVector(float* vec) {
   float HardTurnOnAngleThreshold = 0.1745; //10.0 deg
   float SoftTurnOnAngleThreshold = 1.2217; //70.0 deg
   float NoTurnAngleThreshold = 1.57; //90.0 deg
   float MaxSpeed = 20.0;
   /* Get the heading angle */
   //CRadians cHeadingAngle = c_heading.Angle().SignedNormalize();
   float cHeadingAngle = atan2 (vec[1],vec[0]);
   WrapValue(&cHeadingAngle);
   /* Get the length of the heading vector */
   float fHeadingLength = vec[0]*vec[0]+vec[1]*vec[1];
   fHeadingLength = sqrt(fHeadingLength);
   /* Clamp the speed so that it's not greater than MaxSpeed */
   float fBaseAngularWheelSpeed = MIN((float)fHeadingLength, (float)MaxSpeed);

   /* Turning state switching conditions */
   if(abs(cHeadingAngle) <= NoTurnAngleThreshold) {
      /* No Turn, heading angle very small */
      TurningMechanism = 0;
   }
   else if(abs(cHeadingAngle) > HardTurnOnAngleThreshold) {
      /* Hard Turn, heading angle very large */
      TurningMechanism = 2;
   }
   else if(TurningMechanism == 0 &&
           abs(cHeadingAngle) > SoftTurnOnAngleThreshold) {
      /* Soft Turn, heading angle in between the two cases */
      TurningMechanism = 1;
   }

   /* Wheel speeds based on current turning state */
   float fSpeed1 = 0 , fSpeed2 = 0;
   switch(TurningMechanism) {
      case 0: {
         /* Just go straight */
         fSpeed1 = fBaseAngularWheelSpeed;
         fSpeed2 = fBaseAngularWheelSpeed;
         break;
      }

      case 1: { //soft turn
         /* Both wheels go straight, but one is faster than the other */
         float fSpeedFactor = (HardTurnOnAngleThreshold - abs(cHeadingAngle)) / HardTurnOnAngleThreshold;
         fSpeed1 = fBaseAngularWheelSpeed - fBaseAngularWheelSpeed * (1.0 - fSpeedFactor);
         fSpeed2 = fBaseAngularWheelSpeed + fBaseAngularWheelSpeed * (1.0 - fSpeedFactor);
         break;
      }

      case 2: { //hard turn
         /* Opposite wheel speeds */
         fSpeed1 = -MaxSpeed;
         fSpeed2 =  MaxSpeed;
         break;
      }
   }

   /* Apply the calculated speeds to the appropriate wheels */
   float fLeftWheelSpeed, fRightWheelSpeed;
   if(cHeadingAngle > 0) {
      /* Turn Left */
      fLeftWheelSpeed  = fSpeed1;
      fRightWheelSpeed = fSpeed2;
   }
   else {
      /* Turn Right */
      fLeftWheelSpeed  = fSpeed2;
      fRightWheelSpeed = fSpeed1;
   }
   /* Finally, set the wheel speeds */
  kh4_set_speed(fLeftWheelSpeed, fRightWheelSpeed,DSPIC);
}
/****************************************/
int BuzzGoTo(buzzvm_t vm) {
   buzzvm_lnum_assert(vm, 2);
   /* Push the vector components */
   buzzvm_lload(vm, 1);
   buzzvm_lload(vm, 2);
   buzzvm_type_assert(vm, 2, BUZZTYPE_FLOAT);
   buzzvm_type_assert(vm, 1, BUZZTYPE_FLOAT);
   /* Create a new vector with that */
   float vect[2];
   vect[0]=buzzvm_stack_at(vm, 2)->f.value;
   vect[1]=buzzvm_stack_at(vm, 1)->f.value;
   //CVector2 cDir(buzzvm_stack_at(vm, 2)->f.value,
   //              buzzvm_stack_at(vm, 1)->f.value);
   buzzvm_gload(vm);
   SetWheelSpeedsFromVector(vect);
   return buzzvm_ret0(vm);
}

/****************************************/

int buzzkh4_set_wheels(buzzvm_t vm) {
   buzzvm_lnum_assert(vm, 2);
   buzzvm_lload(vm, 1); /* Left speed */
   buzzvm_lload(vm, 2); /* Right speed */
   buzzvm_type_assert(vm, 2, BUZZTYPE_FLOAT);
   buzzvm_type_assert(vm, 1, BUZZTYPE_FLOAT);
   kh4_set_speed(buzzvm_stack_at(vm, 2)->f.value * 10.0f, // Left speed
                 buzzvm_stack_at(vm, 1)->f.value * 10.0f, // Right speed
                 DSPIC);
   return buzzvm_ret0(vm);
}

/****************************************/
/****************************************/

int buzzkh4_set_leds(buzzvm_t vm) {
   buzzvm_lnum_assert(vm, 3);
   buzzvm_lload(vm, 1); /* Red */
   buzzvm_lload(vm, 2); /* Green */
   buzzvm_lload(vm, 3); /* Blue */
   buzzvm_type_assert(vm, 3, BUZZTYPE_INT);
   buzzvm_type_assert(vm, 2, BUZZTYPE_INT);
   buzzvm_type_assert(vm, 1, BUZZTYPE_INT);
   int32_t r = buzzvm_stack_at(vm, 3)->i.value;
   int32_t g = buzzvm_stack_at(vm, 2)->i.value;
   int32_t b = buzzvm_stack_at(vm, 1)->i.value;
   kh4_SetRGBLeds(r,g,b, // Left
                  r,g,b, // Right
                  r,g,b, // Back
                  DSPIC);
   return buzzvm_ret0(vm);
}

/****************************************/
/****************************************/

int buzzkh4_update_battery(buzzvm_t vm) {
   static char BATTERY_BUF[256];
   kh4_battery_status(BATTERY_BUF, DSPIC);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "battery", 1));
   buzzvm_pusht(vm);
   buzzvm_dup(vm);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "voltage", 1));
   buzzvm_pushf(vm, (BATTERY_BUF[10] | BATTERY_BUF[11] << 8) * .00975f);
   buzzvm_tput(vm);
   buzzvm_dup(vm);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "current", 1));
   buzzvm_pushf(vm, (BATTERY_BUF[6] | BATTERY_BUF[7] << 8) * .07813f);
   buzzvm_tput(vm);
   buzzvm_dup(vm);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "capacity", 1));
   buzzvm_pushf(vm, BATTERY_BUF[3]);
   buzzvm_tput(vm);
   buzzvm_gstore(vm);
   return vm->state;
}

/****************************************/
/****************************************/

int buzzkh4_update_ir(buzzvm_t vm) {
   static char PROXIMITY_BUF[256];
   int i;
   kh4_proximity_ir(PROXIMITY_BUF, DSPIC);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "proximity_ir", 1));
   buzzvm_pusht(vm);
   for(i = 0; i < 8; i++) {
      buzzvm_dup(vm);
      buzzvm_pushi(vm, i+1);
      buzzvm_pushi(vm, (PROXIMITY_BUF[i*2] | PROXIMITY_BUF[i*2+1] << 8));
      buzzvm_tput(vm);
   }
   buzzvm_gstore(vm);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "ground_ir", 1));
   buzzvm_pusht(vm);
   for(i = 8; i < 12; i++) {
      buzzvm_dup(vm);
      buzzvm_pushi(vm, i-7);
      buzzvm_pushi(vm, (PROXIMITY_BUF[i*2] | PROXIMITY_BUF[i*2+1] << 8));
      buzzvm_tput(vm);
   }
   buzzvm_gstore(vm);
   return vm->state;
}

int buzzkh4_camera_updateblob(buzzvm_t vm, int* blob){
   //int* blob =get_available_blob();
   buzzvm_pushs(vm, buzzvm_string_register(vm, "camera_blob", 1));
   buzzvm_pusht(vm);
      //push x center
   buzzvm_dup(vm);
   buzzvm_pushi(vm, 1);
   buzzvm_pushi(vm, blob[0]);
   buzzvm_tput(vm);
   // push y center of blob
   buzzvm_dup(vm);
   buzzvm_pushi(vm, 2);
   buzzvm_pushi(vm, blob[1]);
   buzzvm_tput(vm);
   // push blob size
   buzzvm_dup(vm);
   buzzvm_pushi(vm, 3);
   buzzvm_pushi(vm, blob[2]);
   buzzvm_tput(vm);
   // push blob size
   buzzvm_dup(vm);
   buzzvm_pushi(vm, 4);
   buzzvm_pushi(vm, blob[3]);
   buzzvm_tput(vm);

   buzzvm_gstore(vm);
   return vm->state;
}


/****************************************/
/****************************************/

