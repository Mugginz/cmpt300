// Ferry program using multiple processes.
// Compile using the command "gcc ferry.c"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/ipc.h>
#include <linux/msg.h>
#include <sys/types.h>
#include <signal.h>

// Message types/contents.
#define BOARDED 3
#define WAITING 4
#define CAR 5
#define TRUCK 6
#define ARRIVED 9

// Message structure. 
typedef struct mbuf{
	long mtype;
	int info;
	int pid;
} msg_t;

void sigHand(int sig){
	if( sig == SIGTERM ){
		exit(0);
	};
}

int spawnVehicle(int prob){
	time_t t;
	srand((unsigned) time(&t));
	if(	(rand()%101) < prob ){
		return TRUCK;
	}else{
		return CAR;
	};
};
	
int vehicle(int prob, int q_arrivals, int q_boarded, size_t len){

	signal(SIGTERM, sigHand);
	msg_t buffer;

	int vtype = spawnVehicle(prob);
	buffer.mtype = ARRIVED;
	buffer.info = vtype;
	pid_t procID = getpid();
	buffer.pid = procID;

	if( vtype == TRUCK ){
		printf("A truck id %d has arrived at the dock\n", procID);
	}else{
		printf("A car id %d has arrived at the dock\n", procID);
	};
	msgsnd(q_arrivals, &buffer, len, 0);

// Vehicle will intermittently check if it's been called to ferry deck.
	while( msgrcv(q_boarded, &buffer, len, procID, IPC_NOWAIT) == -1 ){
		usleep(50*1000);
	};
	buffer.mtype = BOARDED;
	msgsnd(q_boarded, &buffer, len, 0);
	if( vtype == TRUCK ){
		printf("%s id %d is boarding the ferry.\n", "Truck", procID);
	}else{
		printf("%s id %d is boarding the ferry.\n", "Car", procID);
	};
	
// Wait to be killed.
	while( 1 ){
		sleep(5);
	};

	return 0;
}

int captain( int travelTime, int q_arrivals, int q_early, int q_late, int q_ferry, int q_boarded, size_t len){
	printf("Captain is on duty!\n");
	printf("Ferry is currently docked.\n");
	msg_t buffer;
	int waitflag = 0;
	int trips = 0;
	int numLoaded, numCars, numTrucks = 0;

	for( trips; trips < 11; trips++ ){
		numLoaded = 0; numCars = 0; numTrucks = 0;
		printf("\nLoading ferry for trip. %d\n\n", trips+1);
		while( numLoaded < 6 ){
			while( ((numTrucks < 2) && (numLoaded < 5)) && 
			(msgrcv(q_early, &buffer, len, TRUCK, IPC_NOWAIT) != -1) ){
				msgsnd(q_ferry, &buffer, len, 0);
				buffer.mtype = buffer.pid;
				msgsnd(q_boarded, &buffer, len, 0);
				numTrucks++;
				numLoaded +=2;
			};
			while( (numLoaded < 6) && (msgrcv(q_early, &buffer, len, CAR, IPC_NOWAIT) != -1) ){
				msgsnd(q_ferry, &buffer, len, 0);
				buffer.mtype = buffer.pid;
				msgsnd(q_boarded, &buffer, len, 0);
				numCars++;
				numLoaded++;
			};
			while( ((numTrucks < 2) && (numLoaded < 5)) && 
			(msgrcv(q_late, &buffer, len, TRUCK, IPC_NOWAIT) != -1) ){
				msgsnd(q_ferry, &buffer, len, 0);
				buffer.mtype = buffer.pid;
				msgsnd(q_boarded, &buffer, len, 0);
				numTrucks++;
				numLoaded +=2;
			};
			while( (numLoaded < 6) && (msgrcv(q_late, &buffer, len, CAR, IPC_NOWAIT) != -1) ){
				msgsnd(q_ferry, &buffer, len, 0);
				buffer.mtype = buffer.pid;
				msgsnd(q_boarded, &buffer, len, 0);
				numCars++;
				numLoaded++;
			};
			usleep(20*1000);
// Check for late arrivals.
			if( (numLoaded < 6) && (waitflag == 0) ){
				printf("Ferry not yet full; waiting for arrivals...\n");
				waitflag = 1;
			};
			while( msgrcv(q_arrivals, &buffer, len, ARRIVED, IPC_NOWAIT) != -1 ){
				buffer.mtype = buffer.info;
				msgsnd(q_late, &buffer, len, 0);
			};
			
		};
		waitflag = 0;

// Captain needs to acknowledge that vehicles know they're ready for termination.
		while( (numTrucks + numCars) > 0 ){
			// Give vehicles a limited time to respond.
			usleep(1000*100);
			if( msgrcv(q_boarded, &buffer, len, BOARDED, IPC_NOWAIT) == -1){
				printf("Vehicles are missing, gather a search party!");
				exit(0);
			};
			if( buffer.info == TRUCK ){
				numTrucks--;
				printf("Captain has verified that %s id %d is now on the ferry deck.\n", "Truck", buffer.pid );
			}else{
				numCars--;
				printf("Captain has verified that %s id %d is now on the ferry deck.\n", "Car", buffer.pid );
			};
		};
		printf("All expected vehicles have been loaded.\n");
		printf("Ferry is now sailing from the dock...\n\n");

// Late vehicles can now move up to the early queue for the next load.
		while( msgrcv(q_late, &buffer, len, 0, IPC_NOWAIT) != -1 ){
			msgsnd(q_early, &buffer, len, 0);
		};

// Terminate the processes in the ferry queue.
		usleep(1000*travelTime);
		printf("\nFerry has reached its destination.\n");
		printf("Vehicles will now disembark.\n");
		while( msgrcv(q_ferry, &buffer, len, 0, IPC_NOWAIT) != -1 ){
			printf("Vehicle id %d has been unloaded.\n", buffer.pid);
			kill(buffer.pid, SIGTERM);
		};
		printf("Ferry is now sailing back...\n\n");
		usleep(1000*travelTime);
		printf("\nFerry has returned to the dock.\n");
	};
	return 0;
};


int main(){

// Initializations.
	msg_t buffer;
	int len = sizeof(msg_t) - sizeof(long);

	int truck_prob = 50;
	int spawn_interval = 300;
	int ferry_travel = 500;

	printf("Enter a probability within [0..100] that a truck arrives in place of a car: ");
	scanf("%d", &truck_prob);
	printf("\nEnter the maximum time interval in milleseconds between vehicle arrivals: ");
	scanf("%d", &spawn_interval);
	printf("\nEnter the Ferry travel time in milleseconds : ");
	scanf("%d", &ferry_travel);

	key_t key_arrivals;
	key_arrivals = ftok(".","arr");
	int q_arrivals = msgget(key_arrivals, IPC_CREAT | 0660);
	printf("Arrival queue   : %d\n", q_arrivals);

	key_t key_early;
	key_early = ftok(".","ear");
	int q_early = msgget(key_early, IPC_CREAT | 0660);
	printf("Early queue     : %d\n", q_early);

	key_t key_late;
	key_late = ftok(".","lat");
	int q_late = msgget(key_late, IPC_CREAT | 0660);
	printf("Late queue      : %d\n", q_late);

	key_t key_ferry;
	key_ferry = ftok(".","fer");
	int q_ferry = msgget(key_ferry, IPC_CREAT | 0660);
	printf("Ferry deck      : %d\n", q_ferry);

	key_t key_boarded;
	key_boarded = ftok(".","boa");
	int q_boarded = msgget(key_boarded, IPC_CREAT | 0660);
	printf("Boarded vehicles: %d\n", q_boarded);

	pid_t capt;
// Spawn captain process to manage the ferry.
	if( (capt = fork()) == 0 ) captain( ferry_travel+1, q_arrivals, q_early, q_late, q_ferry, q_boarded, len );
// End Initializations.

// Keep track of children that captain did not terminate.
	int n = 1000;
	pid_t* childArray = malloc(n*sizeof(pid_t));
	pid_t child;
	int child_counter = 0;

// Start vehicle spawning until ferry loads are done.
	int timeGap;
	while( !waitpid(capt, NULL, WNOHANG) ){
		timeGap = (rand()%spawn_interval) + 1;
		usleep(timeGap*1000);
		if( (child = fork()) == 0 ){
			vehicle(truck_prob, q_arrivals, q_boarded, len);
		}else{
			childArray[child_counter] = child;
		};
		child_counter++;
		if( child_counter == n ){
			n = n*2;
			childArray = realloc(childArray, n*sizeof(pid_t));
		};
	};

// Remove all child processes.
	int v = 0;
	for( v; v < child_counter; v++ ){
		kill(childArray[v], SIGTERM);
	};
	free(childArray);

// Remove all queues.
	msgctl(q_arrivals, IPC_RMID, 0);
	msgctl(q_late, IPC_RMID, 0);
	msgctl(q_early, IPC_RMID, 0);
	msgctl(q_ferry, IPC_RMID, 0);
	msgctl(q_boarded, IPC_RMID, 0);

	return 0;
}

