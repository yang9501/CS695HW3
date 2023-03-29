#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/utsname.h>
#include <stdint.h>
#include <time.h>
#include <float.h>

//comment out to live run
//#define DEBUG 1

#define GPIO_PATH_44 "/sys/class/gpio/gpio44" //Green 1
#define GPIO_PATH_68 "/sys/class/gpio/gpio68" //Red 1

#define GPIO_PATH_66 "/sys/class/gpio/gpio66" //Start/Stop Button
#define GPIO_PATH_69 "/sys/class/gpio/gpio69" //Reset Button

#define BUTTON_PRESS_DURATION 5

//Writes specified value to specified GPIO directory
static void writeLED(char *filename, char *port, char *value);

//Reads input to GPIO pin
static uint32_t readGPIO(char *filename, char *port);

static void setInitialState(char *greenPort, char *redPort);

//Primary button press detection
void getButtonPressDuration(void *buttonPort);

void startWatch();
void stopWatch();
void msleep(long msec);
void updateTimerThread();
void displayTimerThread();

pthread_mutex_t timerMutex;
float timerInMilliseconds;

pthread_mutex_t runningStateMutex;
int watchRunningState;

int main(void) {
    //arrays containing GPIO port definitions, representing the green and red lights, and the start/stop and reset buttons
	char lightPorts[2][25] = {GPIO_PATH_44, GPIO_PATH_68}; //lightPorts[0] is the green light, lightPorts[1] is the red
    char buttonPorts[2][25] = {GPIO_PATH_66, GPIO_PATH_69}; //buttonPorts[0] is the start/stop, buttonPorts[1] is the reset

    #ifdef DEBUG
    (void) printf("DEBUG MODE\n");
    struct utsname sysInfo;
    (void) uname(&sysInfo);
    (void) printf("%s\n", sysInfo.sysname);
    (void) printf("%s\n", sysInfo.nodename);
    (void) printf("%s\n", sysInfo.machine);
    #else
    for (uint32_t i = 0; i < 2; i++) {
        (void) writeLED("/direction", lightPorts[i], "out");
    }

    for (uint32_t i = 0; i < 2; i++) {
        (void) writeLED("/direction", buttonPorts[i], "in");
    }
    #endif

    //Initialize mutexes
    (void) pthread_mutex_init(&runningStateMutex, NULL);
    (void) pthread_mutex_init(&timerMutex, NULL);

    //Initialize GPIO port values
    setInitialState(lightPorts[0], lightPorts[1]);

    // Create independent threads each of which will execute function
    pthread_t thread1, thread2, thread3, thread4;
    (void) pthread_create( &thread1, NULL, (void*) getButtonPressDuration, (void*) buttonPorts[0]);
    (void) pthread_create( &thread2, NULL, (void*) getButtonPressDuration, (void*) buttonPorts[1]);
    (void) pthread_create( &thread3, NULL, (void *) updateTimerThread, NULL);
    (void) pthread_create( &thread4, NULL, (void *) displayTimerThread, NULL);

    (void) pthread_join(thread3, NULL);

	return 0;
}

//Sleep for the requested number of milliseconds
void msleep(long milliseconds) {
    struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, &ts);
}

//https://stackoverflow.com/questions/10192903/time-in-milliseconds-in-c
//https://stackoverflow.com/questions/48711243/execute-action-every-x-milliseconds-in-while-loop
void updateTimerThread() {
    while(1) {
        (void) pthread_mutex_lock(&runningStateMutex);
        if(watchRunningState == 1) {
            (void) pthread_mutex_lock(&timerMutex);
            //If the counter value + 10 would be greater than the maximum value of float, rollover to zero
            if(FLT_MAX - 10 < timerInMilliseconds) {
                timerInMilliseconds = 0;
            }
            timerInMilliseconds = timerInMilliseconds + 10;
            (void) pthread_mutex_unlock(&timerMutex);
        }
        (void) pthread_mutex_unlock(&runningStateMutex);
        msleep(10);
    }
}

/////////////////////////////////
//TODO: Format display output into seconds:milliseconds
//TODO: Add RMS priority functionality to each of the threads
/////////////////////////////////
void displayTimerThread() {
    while(1) {
        (void) pthread_mutex_lock(&runningStateMutex);
        if (watchRunningState == 1) {
            (void) pthread_mutex_lock(&timerMutex);
            printf("%.6f", timerInMilliseconds);
            fflush(stdout);
            (void) pthread_mutex_unlock(&timerMutex);
        }
        (void) pthread_mutex_unlock(&runningStateMutex);
        msleep(100);
    }
}

void startWatch() {
    (void) pthread_mutex_lock(&runningStateMutex);
    watchRunningState = 1;
    #ifdef DEBUG
    (void) printf("Green on: %s\n", GPIO_PATH_44);
    (void) printf("Red off: %s\n", GPIO_PATH_68);
    #else
    (void) writeLED("/value", GPIO_PATH_44, "1");
    (void) writeLED("/value", GPIO_PATH_68, "0");
    #endif
    (void) pthread_mutex_unlock(&runningStateMutex);
}

void stopWatch() {
    (void) pthread_mutex_lock(&runningStateMutex);
    watchRunningState = 0;
    #ifdef DEBUG
    (void) printf("Green off: %s\n", GPIO_PATH_44);
    (void) printf("Red on: %s\n", GPIO_PATH_68);
    #else
    (void) writeLED("/value", GPIO_PATH_44, "0");
    (void) writeLED("/value", GPIO_PATH_68, "1");
    #endif
    (void) pthread_mutex_unlock(&runningStateMutex);
}

void getButtonPressDuration(void *buttonPort) {
    uint32_t pressedFlag = 0;
    uint32_t signalSentFlag = 0;
    uint32_t gpioValue;
    while(1) {
        gpioValue = readGPIO("/value", (char *) buttonPort);
        if(gpioValue == 1){
            //first press detected
            printf("HELLO\n");
            fflush(stdout);
            if(pressedFlag == 0) {
                pressedFlag = 1;
                if(signalSentFlag == 0) {
                    //If the buttonPort corresponds with start/stop
                    if(strcmp((char*) buttonPort,  GPIO_PATH_66) == 0) {
                        (void) pthread_mutex_lock(&runningStateMutex);
                        //if the watch is currently running, stop it
                        if(watchRunningState == 1) {
                            stopWatch();
                            signalSentFlag = 1;
                        }
                        //if the watch is currently stopped, start it
                        if(watchRunningState == 0) {
                            startWatch();
                            signalSentFlag = 1;
                        }
                        (void) pthread_mutex_unlock(&runningStateMutex);
                    }
                    //If the buttonPort corresponds with reset, set the counter to 0
                    if(strcmp((char*) buttonPort,  GPIO_PATH_69) == 0) {
                        (void) pthread_mutex_lock(&timerMutex);
                        timerInMilliseconds = 0;
                        (void) pthread_mutex_unlock(&timerMutex);
                        signalSentFlag = 1;
                    }
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                pressedFlag = 0;
                signalSentFlag = 0;
            }
        }
        msleep(10);
    }
}

static void setInitialState(char *greenPort, char *redPort) {
    #ifdef DEBUG
    (void) printf("Green off: %s\n", greenPort);
    (void) printf("Red on: %s\n", redPort);
    #else
    (void) pthread_mutex_lock(&runningStateMutex);
    watchRunningState = 0;
    (void) writeLED("/value", greenPort, "0");
    (void) writeLED("/value", redPort, "1");
    (void) pthread_mutex_unlock(&runningStateMutex);
    (void) pthread_mutex_lock(&timerMutex);
    timerInMilliseconds = 0;
    (void) pthread_mutex_unlock(&timerMutex);
    #endif
}

static uint32_t readGPIO(char *filename, char *port) {
    FILE* fp; //create file pointer
    char fullFileName[100]; //store path and filename
    uint32_t val;
    (void) sprintf(fullFileName, "%s%s", port, filename); //write path/name
    fp = fopen(fullFileName, "r"); //open file for writing
    (void) fscanf(fp, "%d", &val);
    (void) fclose(fp);
    return val;
}

static void writeLED(char *filename, char *port, char *value) {
	FILE* fp; //create file pointer
	char fullFileName[100]; //store path and filename
    (void) sprintf(fullFileName, "%s%s", port, filename); //write path/name
	fp = fopen(fullFileName, "w+"); //open file for writing
    (void) fprintf(fp, "%s", value); // send value to the file
    (void) fclose(fp); //close the file using the file pointer
}