/**
 * Author: Jason Klamert
 * Date: 10/8/2016
 * Description: Slave program code to implement message passing and introduce race conditions.
 **/

#include <sys/wait.h>
#include <fstream>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <errno.h>
#include <ctime>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include "header.h" 
#include <sys/msg.h>
using namespace std;

#define SHMSZ 1024
#define DEBUG false
#define TIMEOUT 10

void signal_callback_handler(int signum);
void killChild(int signum);
void logMessage(char* fileName, string message);
void sendMessage(int, int, long long);
void getMessage(int,int);

pid_t slave;
long long *ossTimer;
struct sharedStruct *SharedStruct;
int processNumber = 0;
int sId;
int mId;
struct msqid_ds msqid_buf;
volatile sig_atomic_t sigNotRecieved = 1;
int shmid;

int main(int argc, char** argv){
	   
	signal(SIGINT, signal_callback_handler);
	   
	int timeoutValue = 30;
  	long long start;
  	long long current;
  	 
        key_t timerKey = 13000;
        key_t kernelKey = 13010;
        key_t userKey = 13020;
        key_t sharedKey = 13030;

	int Num;
	int processID;
	slave = getpid();
	int timeArg = 20;
   	char c;
	extern int optind;
	extern char* optarg;
	struct shmid_ds* buffer;

	     while ((c = getopt(argc, argv, "s:t:i:")) != -1)
             {
                switch (c)
                {
			case 's':
			{
				if(DEBUG == true){
					cout << "Inside of case: s \n";
					cout << "optarg: " << optarg << endl;
				}
				
				processNumber = atoi(optarg);
				
				if(DEBUG == true){
                			cout << "-----------------------------------------------------------------------\n";
                			cout << "PID: " << processNumber << "\n";
                			cout << "-----------------------------------------------------------------------\n";
				}
				break;
			}
			case 't':
			{
				if(DEBUG == true){
					cout << "Inside of case: t \n";
					cout << "optarg: " << optarg << endl;
				}

				timeArg = atoi(optarg);
	
				if(DEBUG == true){
					cout << "-----------------------------------------------------------------------\n";
                         		cout << "Time Arg: " << timeArg << "\n";
                         		cout << "-----------------------------------------------------------------------\n";
                        	}
				break;
			}
			case 'i':
			{
				shmid = atoi(optarg);
				break;
			}
	
	                 default:
			{
	                 	if(DEBUG == true){
	                        	cout << "Inside of case: default \n";
	                 	}

				errno = EINVAL;
				perror("GetOpt Slave Default");
	                        cout << "Error: Unrecognized flag passed to slave! Failing silently.\n";
	                        return 2;
	                        break;
	
	
	                }
	            }
		}
	
		if(DEBUG == true){
			cout << "User timeArg: " << timeArg << endl;
			cout << "User ProcessID: " << processNumber << endl;
		}


		srand(getpid());

		//Try to attach to shared memory
		if((SharedStruct = (sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
			perror("User process shmat");
			return(1);
		}
			  
		//Ignore SIGINT so that it can be handled below
		signal(SIGINT, signal_callback_handler);
			  
		//Set the sigquitHandler for the SIGQUIT signal
		signal(SIGQUIT, signal_callback_handler);
			  
		//Set the alarm handler
		signal(SIGALRM, killChild);
		
		//Set the default alarm time
		alarm(TIMEOUT);
			  
		if((sId = msgget(userKey, IPC_CREAT | 0777)) == -1) {
			perror("User process msgget for s queue");
			return(-1);
		}
		if((mId = msgget(kernelKey, IPC_CREAT | 0777)) == -1) {
			perror("User process msgget for m queue");
			return(-1);
		}

  		int i = 0;
  		int j;
		//Name this duration like in the requirements!
  	  	long long duration = 1 + rand() % 100000;

		//Create the start reference timer.
		start = SharedStruct->ossTimer;
		//Retrieve the current instant of time.
		current = SharedStruct->ossTimer - start;

		//Perform the most simple mutual exclusion and busy wait while the processess doesnt have a message.
		//Keep going until timer expires.
		while(true) {
    			if(SharedStruct->sigNotReceived) {
      			getMessage(sId, 2);
      				if(current = (SharedStruct->ossTimer - start) >= duration) {
        				break;
      				}
      				sendMessage(sId, 2, SharedStruct->ossTimer);
    			}
    			else {
      				break;
    			}
  		}

		//Send termination message to kernel of child about to be killed.
  		sendMessage(mId, 3, SharedStruct->ossTimer);

		//Remove child shared memory reference.
  		if(shmdt(SharedStruct) == -1) {
    			perror("Shmdt error");
  		}

  		msgctl(mId, IPC_STAT, &msqid_buf);

  		if(SharedStruct->sigNotReceived) {
    			while(msqid_buf.msg_qnum != 0) {
      				msgctl(mId, IPC_STAT, &msqid_buf);
     			}
  		}

		printf("User Notification: user process %d exiting!\n", processNumber);
 
		//We must sleep for a second to give the process time to exit gracefully!
		kill(slave, SIGTERM);
		sleep(1);
		kill(slave, SIGKILL);

		return 0;
}
     
/**
 * Author: Jason Klamert
 * Date: 9/25/2016
 * Description: Signal Handler for children.
 */
void signal_callback_handler(int signum)
{
   	printf("User process %d has received signal %s (%d)\n", processNumber, strsignal(signum), signum);
  	sigNotRecieved = 0;

	if(shmdt(SharedStruct) == -1) {
    		perror("User process shmdt");
  	}

 	kill(slave, SIGKILL);

  	//The slaves have at most 5 more seconds to exit gracefully.
  	//This is highly desireable.
  	alarm(5);
   	exit(signum);           
}

/**
 * Author: Jason Klamert
 * Date: 9/25/2016
 * Description: Function to kill child whose parent has timed out.
 **/
void killChild(int signum) {
  	printf("User process %d will terminate from running out of time!\n", processNumber);
	
	//Do not forget to sleep one sec for another chance to exit gracefully!
	kill(slave, SIGTERM);
	sleep(1);
	//Last chance has past. Kill immediately.
	kill(slave, SIGKILL);
}

/**
 * Author: Jason Klamert
 * Description: Function to send a message to either the master or the slave queues.
 * Date: 10/21/2016
 **/
void sendMessage(int queue, int messageType, long long finish) {

  	struct msgformat msg;

  	msg.mType = messageType;

  	if(queue == sId) {
    		sprintf(msg.mText, "Consumed message from a user process %d\n", processNumber);
  	}

  	if(queue == mId) {
    		sprintf(msg.mText, "%llu.%09llu\n", finish / NANOCONVERTER, finish % NANOCONVERTER);
  	}	

  	if(msgsnd(queue, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    		perror("User process msgsnd error");
  	}
}

/**
 * Author: Jason Klamert
 * Date: 10/21/2016
 * Description: Function that gets a message from the corresponding passed queue.
 **/
void getMessage(int queue, int messageType) {

  	struct msgformat msg;

  	if(msgrcv(queue, (void *) &msg, sizeof(msg.mText), messageType, MSG_NOERROR) == -1) {

    		if(errno != ENOMSG) {
      			perror("User process msgrcv");
    		}

    		printf("User process: No message available for consumption\n");

  	}
	else {

		printf("User process: Message received by user process %d: %s\n", processNumber, msg.mText);

	}
}

/**
 * Description: function that logs the given message to given file.
 *
 * Author: Jason Klamert
 * Date: 9/25/2016
 **/
void logMessage(char* fileName, string message){

        bool status = true;
        ofstream outfile;
        outfile.open(fileName, ios::out | ios::app);

        if(outfile){
             outfile << message;
         }
	else{
               status = false;
         }
         outfile.close();

        //If log filewrite was successful, return 0.
        if(status){
        	if(DEBUG == true)
		printf("Success: savelog successful.\n");
        }
        //If log filewrite was unsuccessful, return -1.
        else{
               errno = EIO;
               perror("Error: savelog unsuccessful. File open unsuccessful.");
        }
}
