#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <wiringPi.h>
#include <errno.h>
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

int disablePA(void){
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

