#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "openair1/PHY/defs_gNB.h"
#include "openair1/PHY/defs_RU.h"
#include "radio/COMMON/common_lib.h"


#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return -1; } while (0)

int usrp_get_actual_gains(openair0_device *device,
                          double *rx_gains,
                          double *tx_gains,
                          int max_channels,
                          int *num_rx,
                          int *num_tx);

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

int set_gNB_ta_offset(char *buff, int debug, telnet_printfunc_t prnt)
{
  int value;
  if (sscanf(buff, "%d", &value) == 1) {
    if (RC.ru && RC.ru[0]) {
      RC.ru[0]->N_TA_offset = value;
      prnt("N_TA_offset set to %d\n", value);
    } else {
      prnt("RU not found\n");
    }
  } else {
    prnt("Invalid value\n");
  }
  return 0;
}

int get_gNB_ta_offset(char *buff, int debug, telnet_printfunc_t prnt)
{
  (void)buff;
  (void)debug;
  if (RC.ru && RC.ru[0]) {
    prnt("N_TA_offset %d\n", RC.ru[0]->N_TA_offset);
  } else {
    prnt("RU not found\n");
  }
  return 0;
}

int get_usrp_actual_gains(char *buff, int debug, telnet_printfunc_t prnt)
{
  (void)buff;
  (void)debug;
  if (!RC.ru || !RC.ru[0]) {
    prnt("RU not found\n");
    return -1;
  }

  openair0_device *device = &RC.ru[0]->rfdevice;
  if (device->type != USRP_B200_DEV &&
      device->type != USRP_X300_DEV &&
      device->type != USRP_N300_DEV &&
      device->type != USRP_X400_DEV) {
    prnt("RF device is not USRP (type %d)\n", device->type);
    return -1;
  }

  double rx_gains[8] = {0};
  double tx_gains[8] = {0};
  int num_rx = 0;
  int num_tx = 0;
  int ret = usrp_get_actual_gains(device, rx_gains, tx_gains, 8, &num_rx, &num_tx);
  if (ret != 0) {
    prnt("Failed to read USRP gains (%d)\n", ret);
    return ret;
  }

  prnt("USRP actual gains:\n");
  for (int i = 0; i < num_rx; ++i) {
    prnt("  RX[%d]: %.2f dB\n", i, rx_gains[i]);
  }
  for (int i = 0; i < num_tx; ++i) {
    prnt("  TX[%d]: %.2f dB\n", i, tx_gains[i]);
  }
  return 0;
}

int get_gNB_max_rxgain(char *buff, int debug, telnet_printfunc_t prnt)
{
  (void)buff;
  (void)debug;
  if (RC.ru && RC.ru[0]) {
    prnt("max_rxgain %d\n", RC.ru[0]->max_rxgain);
    return RC.ru[0]->max_rxgain;
  } else {
    prnt("max_rxgain not found\n");
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

int get_gNB_att_tx(char *buff, int debug, telnet_printfunc_t prnt)
{
  (void)buff;
  (void)debug;
  if (RC.ru && RC.ru[0]) {
    prnt("att_tx %d\n", RC.ru[0]->att_tx);
    return RC.ru[0]->att_tx;
  } else {
    prnt("att_tx not found\n");
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

int get_gNB_att_rx(char *buff, int debug, telnet_printfunc_t prnt)
{
  (void)buff;
  (void)debug;

  if (RC.ru && RC.ru[0]) {
    prnt("att_rx %d\n", RC.ru[0]->att_rx);
    return RC.ru[0]->att_rx;
  } else {
    prnt("att_rx not found\n");
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
    {"set_gNB_ta_offset", "[value]", set_gNB_ta_offset},
    {"get_gNB_ta_offset", "", get_gNB_ta_offset},
    {"get_usrp_gains", "", get_usrp_actual_gains},
    {"", "", NULL},
};

static telnetshell_vardef_t altice_vars[] = {{"", 0, 0, NULL}};

void add_altice_cmds(void)
{
  add_telnetcmd("altice", altice_vars, altice_cmds);
}
