#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "openair1/PHY/defs_gNB.h"
#include "openair1/PHY/defs_RU.h"


#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return -1; } while (0)

int set_gNB_max_rxgain(char *buff, int debug, telnet_printfunc_t prnt)
{
int value;
  if (sscanf(buff, "%d", &value) == 1) {
    if (RC.ru && RC.ru[0]) {
      RC.ru[0]->max_rxgain = value;
      prnt("max_rxgain set to %d\n", value);
    } else {
      prnt("max_rxgain not found\n");
    }
  } else {
    prnt("Invalid value\n");
  }
  return 0;
}


int get_gNB_max_rxgain()
{
  if (RC.ru && RC.ru[0]) {
    return RC.ru[0]->max_rxgain;
  } else {
    return -1;
  }
}

int set_gNB_att_tx(char *buff, int debug, telnet_printfunc_t prnt)
{
  int value;
  if (sscanf(buff, "%d", &value) == 1) {
    if (RC.ru && RC.ru[0]) {
      RC.ru[0]->att_tx = value;
      prnt("att_tx set to %d\n", value);
    } else {
      prnt("att_tx not found\n");
    }
  } else {
    prnt("Invalid value\n");
  }
  return 0;
}

int get_gNB_att_tx()
{
  if (RC.ru && RC.ru[0]) {
    return RC.ru[0]->att_tx;
  } else {
    return -1;
  }
}


int set_gNB_att_rx(char *buff, int debug, telnet_printfunc_t prnt)
{
  int value;
  if (sscanf(buff, "%d", &value) == 1) {
    if (RC.ru && RC.ru[0]) {
      RC.ru[0]->att_rx = value;
      prnt("att_rx set to %d\n", value);
    } else {
      prnt("att_rx not found\n");
    }
  } else {
    prnt("Invalid value\n");
  }
  return 0;
}

int get_gNB_att_rx()
{
  if (RC.ru && RC.ru[0]) {
    return RC.ru[0]->att_rx;
  } else {
    return -1;
  }

}



static telnetshell_cmddef_t altice_cmds[] = {
    {"set_gNB_max_rxgain", "[value]", set_gNB_max_rxgain},
    {"get_gNB_max_rxgain", "", get_gNB_max_rxgain},
    {"set_gNB_att_tx", "[value]", set_gNB_att_tx},
    {"get_gNB_att_tx", "", get_gNB_att_tx},
    {"set_gNB_att_rx", "[value]", set_gNB_att_rx},
    {"get_gNB_att_rx", "", get_gNB_att_rx},
    {"", "", NULL},
};

static telnetshell_vardef_t altice_vars[] = {{"", 0, 0, NULL}};

void add_altice_cmds(void)
{
  add_telnetcmd("altice", altice_vars, altice_cmds);
}
