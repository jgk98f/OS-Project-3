/**
 * Description: The logging mechanism provides functionality such as clearing, adding a log message, getlog, and savelog.
 * This is utilized via the main function which uses getopt and perror to parse the command line for arguments and the master process
 * then creates slaves and the slaves are subject to race conditions while trying to capture a two resources which are a shared
 * variable and a file to write a message to named 'test.out'.
 *
 *  Author: Jason Klamert
 *  Date: 9/25/2016
 *  Log: logfile.txt
 */

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
#include <sys/stat.h>
#include <sys/msg.h>
#include "header.h"

using namespace std;

#define DEBUG false
#define MAX 20
#define TOTAL_SLAVES 100
#define INCREMENTER 10000

int logMessage(char*,char*);
void usage();
void usage2();
void signal_callback_handler(int signum);
void cleanEnv();
void createProcesses(int numberOfProcesses);
void childDeathFunction(int qid, int msgtype);
void sendMessage(int queue, int typeOfMessage);
int cleanSharedMemory(int shmid, int shmid2, sharedStruct *s);

volatile sig_atomic_t cleanupCalled = 0;
pid_t master, slave;
int sId;
int mId;
int nextToSend = 1;
int processCreated = 1;
int messageReceived = 0;
struct msqid_ds msqid_ds_buf;
char* standard = "logfile.txt";
int returnStatus;
int terminationTime = 20;
int numberSlaves = 5;
struct sharedStruct *SharedStruct;
int shmid,shmid2;
char* processX = (char*) malloc(25);
char* timeArg = (char*) malloc(25);
char* cshmid = (char*) malloc(25);

int cleanSharedMemory(int shmid,int shmid2, sharedStruct *s) {
	
	printf("Kernel: Currently Removing Shared Resources!\n");
  	
	int error = 0;
  	
	//Deallocate Shared Struct and other shared resources.
	if(shmdt(s) == -1) {
    		error = errno;
  	}
  	if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    		error = errno;
  	}
  	if((shmctl(shmid2, IPC_RMID, NULL) == -1) && !error) {
		error = errno;
  	}
  	if(!error) {
    		return 0;
  	}

  	return -1;
}

/**
 * Description: Function to send message reguardless of queue..
 * Author: Jason Klamert
 * Date: 10/19/2016
 **/
void sendMessage(int queue, int messageType){
        
	struct msgformat msg;

        msg.mType = messageType;

        sprintf(msg.mText, "Kernel: starting up user queue\n");

        if(msgsnd(queue, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
                perror("Kernel msgsnd error");
                cleanEnv();
        }
}

/**
 * Description: Function to handle the child's death message and print it in the kernel.
 * Author: Jason Klamert
 * Date: 10/19/2016
 **/
void childDeathFunction(int queue, int messageType) {

        struct msgformat msg;

        char* message = (char*) malloc(1000);

        if(msgrcv(queue, (void *) &msg, sizeof(msg.mText), messageType, MSG_NOERROR | IPC_NOWAIT) == -1) {

                if(errno != ENOMSG) {
                        perror("Kernel msgrcv");
                        cleanEnv();
                }

        }
        else {
                msgctl(mId, IPC_STAT, &msqid_ds_buf);

                messageReceived++;

                sprintf(message,"%03d - Master: User %d terminating at my time %llu.%09llu because slave reached %s\n",
                        messageReceived, msqid_ds_buf.msg_lspid, SharedStruct->ossTimer / NANOCONVERTER, SharedStruct->ossTimer % NANOCONVERTER, msg.mText);
                logMessage(standard,message);

                fprintf(stderr, "Master: %d/%d children completed!\n", messageReceived, TOTAL_SLAVES);

                sendMessage(sId, 2);

                nextToSend++;

                if(processCreated <= TOTAL_SLAVES) {
                        createProcesses(1);
                }
        }

	free(message);
}

/**
 * Description: Function to clean up the work environment. This includes cleaning any shared memory
 * or residual queueing facilities.
 *
 * Author: Jason Klamert
 * Date: 10/19/2016
 **/
void cleanEnv(){

	signal(SIGQUIT, SIG_IGN);
  	SharedStruct->sigNotReceived = 0;

  	kill(-getpgrp(), SIGQUIT);

	//Wait for the term of slaves.
  	slave = wait(&returnStatus);
  
	//Detach and remove the shared memory after all child process have died
  	if(cleanSharedMemory(shmid, shmid2, SharedStruct) == -1) {
        	perror("cleanSharedMemory fail");
  	}

  	//Clean the message queues
  	msgctl(sId, IPC_RMID, NULL);
  	msgctl(mId, IPC_RMID, NULL);
	
	printf("Kernel: Terminating!\n");

        kill(getpid(), SIGKILL);

}

/**
 * Description: function that logs the given message to given file.
 *
 * Author: Jason Klamert
 * Date: 9/25/2016
 */
int logMessage(char* fileName, char* message){

        bool status = true;

        ofstream outfile;

        outfile.open(fileName, ios::out | ios::app);

        if(outfile){

             outfile << message;

         }else{

               status = false;

         }

         outfile.close();

        //If log filewrite was successful, return 0.
        if(status){

        	if(DEBUG == true)
		printf("Success: savelog successful.\n");

        return 0;

        }
        //If log filewrite was unsuccessful, return -1.
        else{

             errno = EIO;

             perror("Error: savelog unsuccessful. File open unsuccessful.");

             cleanEnv();

             return -1;
        }
}

 /**
 * Description: function that prints the proper usage of the routine.
 * Author: Jason Klamert
 * Date: 9/25/2016
 */
void usage(){
     perror("Previous unrecognized argument. Please retry the program.\nUse the format master [-h, -s #slaves, -i #critical section iterations, -t #seconds till master termination, or -l fileName] \n");
}

 /**
 * Description: function that prints the proper usage of the routine.
 * Author: Jason Klamert
 * Date: 9/25/2016
 */
void usage2(){
     cout << "Usage format: master [-h, -s #slaves, -i #critical section iterations, -t #seconds till master termination, or -l fileName] \n";
}

/**
 * Author: Jason Klamert
 * Date: 9/25/2016
 * Description: Signal Handler for master.
 */
void signal_callback_handler(int signum)
{
        signal(SIGQUIT, SIG_IGN);
        signal(SIGINT, SIG_IGN);

        if(signum == SIGINT) {
                fprintf(stderr, "\nCtrl-C signal passed. Terminating! \n");
        }

        if(signum == SIGALRM) {
                fprintf(stderr, "Kernel's time has expired. Cleaning up Env!\n");
        }

        if(!cleanupCalled) {
                cleanupCalled = 1;
                printf("Kernel: cleanup called from because of signal!\n");
                cleanEnv();
        }

}

/**
 * Description: function to create X number of processes.
 * Author: Jason Klamert
 * Date: 10/19/2016
 **/
void createProcesses(int numberOfProcesses){
int x;

for(x  = 0; x < numberOfProcesses; x++)
                {
                        cout << "Creating child number " << processCreated << "!" << endl;

                        if((slave = fork()) == -1)
                        {
                                fprintf(stderr, "Error: fork failed!\n");
                                cleanEnv();
                                exit(-2);
                        }

                        if(slave == 0)
                        {
                                slave = getpid();
                                pid_t pGroup = getpgrp();
                                sprintf(processX, "%d", x+1);
                                sprintf(timeArg, "%d", terminationTime);
				sprintf(cshmid, "%d",shmid2);

                                if(DEBUG == true){
                                        cout << "Kernel processX: " << *processX << endl;
                                        cout << "Kernel timeArg: " << *timeArg << endl;
                                }

                                char *slaveArgs[] = {"./user", "-s", processX, "-t", timeArg, "-i", cshmid,  NULL};
                                const char execute[10] = "./user";


                                int status = execvp(execute,slaveArgs);

                                if(status < 0)
                                {
                                        perror("execvp");
                                        char* message = (char*) malloc(100);
                                        sprintf(message,"Execvp failed for process %d!", x);
                                        logMessage(standard,message);
                                        free(message);
                                        cleanEnv();
                                        exit(-1);
                                }

                        }
		
		//We must increment a counter to keep track of number processes.
                processCreated++;

                }
}

/**
 * Description: Routine to use every function of the logging utility at least twice.
 * This parses the commandline for flags regarding the functions to trigger.
 * Author: Jason Klamert
 * Date: 9/6/2016
 */
int main(int argc, char **argv)
{
        signal(SIGINT, signal_callback_handler);

        extern char *optarg;
        extern int optind;
        struct msgformat msg;
        char c;
        key_t timerKey = 13000;
        key_t kernelKey = 13010;
        key_t userKey = 13020;
        key_t sharedKey = 13030;
        struct shmid_ds* buffer;

        //Make timer in shared memory.
        if((shmid = shmget(timerKey, sizeof(long long)*2, IPC_CREAT | 0777)) == -1) {
                perror("Kernel timerKey shmget");
                cleanEnv();
                exit(-1);
        }

	//Make shared struct.
        if((shmid2 = shmget(sharedKey, sizeof(struct sharedStruct)*2, IPC_CREAT | 0777)) == -1){
                perror("Kernel sharedKey shmget");
                cleanEnv();
                exit(-1);
        }

        //Retrieve the structure from shared memory.
        if((SharedStruct = (sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
                perror("Kernel SharedStruct");
                cleanEnv();
                exit(-1);
        }

        if((sId = msgget(userKey, IPC_CREAT | 0777)) == -1) {
                perror("");
                cleanEnv();
                exit(-1);
        }

        if((mId = msgget(kernelKey, IPC_CREAT | 0777)) == -1) {
                perror("Kernel msgget for master queue");
                cleanEnv();
                exit(-1);
        }

        SharedStruct->ossTimer = 0;
        SharedStruct->sigNotReceived = 1;

while ((c = getopt(argc, argv, ":ht:l:s:")) != -1)
{
        switch (c)
        {

                case 'h':
                {
                 if(DEBUG == true){
                         cout << "Inside case h for help\n";
                 }
		 usage();
                
                 break;
                }

                case 'l':
                {
                        if(DEBUG == true){
                                cout << "Inside of case: l \n";
                                cout << "OPTARG: ";
                                cout << optarg << "\n";
                        }

                        standard = (char*)optarg;
                        cout << "-----------------------------------------------------------------------\n";
                        cout << "Log File: " << standard << "\n";
                        cout << "-----------------------------------------------------------------------\n";
                        break;
                }
		case 's':
		{
			if(DEBUG == true){
				cout << "inside of case s\n";
			}
			numberSlaves = atoi(optarg);
		 	 cout << "-----------------------------------------------------------------------\n";
                         cout << "Number slaves: " << numberSlaves << "\n";
                         cout << "-----------------------------------------------------------------------\n";
			break;
		}

                case 't':
                {
                         if(DEBUG == true){
                         cout << "Inside of case: t \n";
                         cout << "OPTARG: ";
                         cout << atoi(optarg) << "\n";
                         }
                         terminationTime = atoi(optarg);
                         cout << "-----------------------------------------------------------------------\n";
                         cout << "Termination Time: " << terminationTime << " seconds\n";
                         cout << "-----------------------------------------------------------------------\n";
                         break;
                }

                default:
                 if(DEBUG == true){
                         cout << "Inside of case: default \n";
                 }
                         errno = EINVAL;
                         perror("GetOpt Default");
                         cout << "The flag argument did not match the approved flags.\n";
                         char* str = (char*) malloc(50);
                         sprintf(str,"The flag argument did not match the approved flags.\n");
                         logMessage(standard,str);
                         usage2();
                         free(str);
                         return -1;
                         break;
                }
        }


                //Override the alarm signal and ignore SIGCHLD and SIGQUIT
                signal(SIGALRM, signal_callback_handler);
                signal(SIGCHLD, SIG_IGN);
                signal(SIGQUIT, SIG_IGN);
		signal(SIGINT, signal_callback_handler);

                //set alarm to ring in terminationTime seconds
                alarm(terminationTime);

                createProcesses(numberSlaves);
                
		//Kick off our message passing.
		sendMessage(sId,2);

                //While the number of messages received are less than the total number
                //of slaves are supposed to send back messages
                while(messageReceived < TOTAL_SLAVES && SharedStruct->ossTimer < 2000000000 && SharedStruct->sigNotReceived) {
                        SharedStruct->ossTimer = SharedStruct->ossTimer + INCREMENTER;
                        childDeathFunction(mId, 3);
                }

		sleep(5);
		
		//call cleanup if not true.
		if(!cleanupCalled) {
    			cleanupCalled = 1;
    			printf("Kernel: cleanup called from main!\n");
    			cleanEnv();
  		}

                return 0;

}

