#ifndef __dectsync_h__
#define __dectsync_h__

#define DECT_SYNCGPIO       CONFIG_AVM_DECT_SYNC

void start_dectsync(int start, int ignore_run);
int idle_dectsynchandler(int inactiv);
unsigned long timer_dectsynchandler(unsigned long cycles_per_unit);
int updaterun_dectsync(int run);
int display_dectsync(int loadcontrol);

extern unsigned int avm_power_disp_loadrate;
#endif/*--- #ifndef __dectsync_h__ ---*/
