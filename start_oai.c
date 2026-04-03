#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
//Socket
#define PORT 8099

//Aux for handling SIGINT or SIGTERM signal over keyboard
volatile sig_atomic_t interrupted = 0;
void handle_sig(int sig){
    interrupted=1;
}

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

//-------------------------------------------------------
// MAIN:
//-------------------------------------------------------

int main(int argc, char* argv[]){
    //store argvs passed
    char* args[argc];
    for(int i=1; i<argc; i++){
        args[i] = argv[i];
    }
    printf("Args passed: %s\n", *args);

    if(signal(SIGINT, handle_sig) == SIG_ERR){
        perror("Could not set up SIGINT handler!");
        exit(EXIT_FAILURE);
    }

    //PA startup
    enable_PA();

    //Create socket for restart signal
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = {0};
    // Creating socket file descriptor
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        fprintf(stdout, "%sSocket failed!%s\n", RED, DEFAULT);
        perror("Err:");
        exit(EXIT_FAILURE);
    }
    // Forcefully attaching socket to the port 8080
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        fprintf(stdout, "%ssetsockopt() failed!%s\n", RED, DEFAULT);
        perror("Err:");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0){
        fprintf(stdout, "%sBind socket failed!%s\n", RED, DEFAULT);
        perror("Err:");
        exit(EXIT_FAILURE);
    }
    if(listen(server_fd, 3) < 0){
        fprintf(stdout, "%sListen from socket failed!%s\n", RED, DEFAULT);
        perror("Err:");
        exit(EXIT_FAILURE);
    }

    // Changing the SIGCHLD handler to ignore to prevent Zombie processes from execpv
    signal(SIGCHLD, SIG_IGN);

    pid_t childID;
    do{
        childID = fork();
        if(childID == 0){
            execvp("/home/ue-5g/oai/openairinterface5g/cmake_targets/ran_build/build/nr-uesoftmodem", args);
        }
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0){
            perror("accept");
            return EXIT_FAILURE;
        }
        recv(new_socket, buffer, 1024, 0);
        sleep(1);
        printf("---------------------------------------------------------------------------------------\nReceived msg from socket: %s\n---------------------------------------------------------------------------------------\n", buffer);

        //wait(NULL);
        //join(childID, NULL);

        if (interrupted) {
           printf("\n[SIGINT detected] Graceful shutdown requested by user (Ctrl+C). Exiting now...\n");
        break;
        }
    } while(1);
    close(new_socket);

    close(server_fd);
    disablePA();

    return EXIT_SUCCESS;
}
