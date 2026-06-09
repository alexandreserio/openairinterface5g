#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <wiringPi.h>
#include <errno.h>
#include <time.h>

//INCLUDES FROM OAI
#include "openair1/PHY/defs_nr_UE.h"


//colors
#define CYAN "\033[1;36m"
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define WHITE "\033[1;37m"
#define DEFAULT "\033[0m"
//GPIO RaspPi CM5
#define RELE_5V 16
#define RELE_2V 12
#define LED_5V 17
#define LED_2V 27
#define LOW 0
#define HIGH 1

int enable_PA(void){
    int ret;
    //Pinout mapping
    ret = wiringPiSetupPinType(WPI_PIN_BCM);
    if(ret != 0){
        fprintf(stdout, "GPIO configuration error (errno: %d) ", errno);
        perror("Err:");
        exit(EXIT_FAILURE);
    }
    //Pinout configuration
    pinMode(RELE_5V, OUTPUT);
    pinMode(RELE_2V, OUTPUT);
    pinMode(LED_5V, OUTPUT);
    pinMode(LED_2V, OUTPUT);

    //Activate 5V:
    printf("%sActivating 5V for Power Amplifier...%s\n", WHITE, DEFAULT);
    digitalWrite(LED_5V, HIGH);
    digitalWrite(RELE_5V, HIGH);
    sleep(3);
    printf("%sActivating 2V for Power Amplifier...%s\n", WHITE, DEFAULT);
    digitalWrite(LED_2V, HIGH);
    digitalWrite(RELE_2V, HIGH);
    sleep(2);
    return 0;
}

int disable_PA(void){
    fprintf(stdout, "%sDeactivating 2V for Power Amplifier...%s\n", WHITE, DEFAULT);
    digitalWrite(LED_2V, LOW);
    digitalWrite(RELE_2V, LOW);
    sleep(2);
    fprintf(stdout, "%sDeactivating 5V for Power Amplifier...%s\n", WHITE, DEFAULT);
    digitalWrite(LED_5V, LOW);
    digitalWrite(RELE_5V, LOW);
    sleep(1);
    return 0;
}

FILE *logfile = NULL;
int meas_log_init(){
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    int year = local->tm_year + 1900;
    int month = local->tm_mon + 1;
    int day = local->tm_mday;
    int hour = local->tm_hour;
    int min = local->tm_min;

    char fname_logs[100];
    snprintf(fname_logs, sizeof(fname_logs), "../../UE_logs/ue_meas_%02d-%02d-%04d_%02d-%02d.log", day, month, year, hour, min);
    logfile = fopen(fname_logs, "wa");
    if(logfile == NULL){
        LOG_E(UTIL, "Error opening file for measurements storage");
        return 1;
    }
    LOG_A(UTIL, "Measurements being stored (fname = %s)", fname_logs);
    return 0;
}

void saveMeasurements(const char *string){
    time_t tmr;
    time(&tmr);
    struct tm *tstamp = localtime(&tmr);
    int hour = tstamp->tm_hour;
    int min = tstamp->tm_min;
    int sec = tstamp->tm_sec;
    if(string == NULL) {
        return;
    }
    fprintf(logfile, "[%02d:%02d:%02d] %s", hour, min, sec, string);
    fflush(logfile);
    return;
}

int meas_log_close(){
    if(logfile == NULL){
        LOG_E(UTIL, "Log filename not found to close...\n");
        return 1;
    }
    fclose(logfile);
    return 0;
}

