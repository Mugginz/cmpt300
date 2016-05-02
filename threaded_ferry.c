// Ferry program using multi-threading
// Compilation can be done with the command "gcc -pthread threaded_ferry.c"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/ipc.h>

// User input parameters.
int truck_prob = 50;
int spawn_interval = 300;
int ferry_interval = 500;

// Queues/counters for vehicle threads.
int early_trucks = 0, late_trucks = 0, early_cars = 0, late_cars = 0;
pthread_mutex_t lock_early_trucks, lock_early_cars;
pthread_mutex_t lock_late_trucks, lock_late_cars;

int boarded_cars = 0, boarded_trucks = 0;
pthread_mutex_t lock_boarded_cars, lock_boarded_trucks;

sem_t q_early_trucks;
sem_t q_late_trucks;
sem_t q_early_cars;
sem_t q_late_cars;
int boarding_call = 0;
pthread_mutex_t lock_boarding_call;

// Ferry-controlling semaphores
sem_t loaded;
sem_t unload;
sem_t disembarked;

pthread_t cap;
pthread_t vehicle;
void* arg;

int termination_flag = 0;

void* car(void* ptr){

	long int TID = (long int)pthread_self();
	pthread_mutex_lock(&lock_boarding_call);
// Enter queue dependant on captains signal.	
	if( !boarding_call ){
	pthread_mutex_unlock(&lock_boarding_call);
		pthread_mutex_lock(&lock_early_cars);
			early_cars++;
			printf("Car   id %ld has entered early queue.\n", TID);
		pthread_mutex_unlock(&lock_early_cars);
		sem_wait(&q_early_cars);
	}else{
	pthread_mutex_unlock(&lock_boarding_call);
		pthread_mutex_lock(&lock_late_cars);
			late_cars++;
			printf("Car   id %ld has entered late queue.\n", TID);
		pthread_mutex_unlock(&lock_late_cars);
		sem_wait(&q_late_cars);
	};
// Now wait to be called to board ferry.

// Tell captain it's boarded.
	pthread_mutex_lock(&lock_boarded_cars);
		boarded_cars++;
		printf("Car   id %ld is boarding the ferry.\n", TID);
		sem_post(&loaded);
	pthread_mutex_unlock(&lock_boarded_cars);

// Wait until ferry reaches destination to unload.
	sem_wait(&unload);
	pthread_mutex_lock(&lock_boarded_cars);
		boarded_cars--;
		printf("Car   id %ld is leaving the ferry.\n", TID);
	pthread_mutex_unlock(&lock_boarded_cars);
	
	sem_post(&disembarked);
	pthread_exit(NULL);
};

void* truck(void* ptr){

	long int TID = (long int)pthread_self();
// Enter queue dependant on captains signal.	
	pthread_mutex_lock(&lock_boarding_call);
	if( !boarding_call ){
	pthread_mutex_unlock(&lock_boarding_call);
		pthread_mutex_lock(&lock_early_trucks);
			early_trucks++;
			printf("Truck id %ld has entered early queue.\n", TID);
		pthread_mutex_unlock(&lock_early_trucks);
		sem_wait(&q_early_trucks);
	}else{
	pthread_mutex_unlock(&lock_boarding_call);
		pthread_mutex_lock(&lock_late_trucks);
			late_trucks++;
			printf("Truck id %ld has entered late queue.\n", TID);
		pthread_mutex_unlock(&lock_late_trucks);
		sem_wait(&q_late_trucks);
	};
// Now wait to be called to board ferry.

// Tell captain it's boarded.
	pthread_mutex_lock(&lock_boarded_trucks);
		boarded_trucks++;
		printf("Truck id %ld is boarding the ferry.\n", TID);
		sem_post(&loaded);
	pthread_mutex_unlock(&lock_boarded_trucks);

// Wait until ferry reaches destination to unload.
	sem_wait(&unload);
	pthread_mutex_lock(&lock_boarded_trucks);
		boarded_trucks--;
		printf("Truck id %ld is leaving the ferry.\n", TID);
	pthread_mutex_unlock(&lock_boarded_trucks);
	
	sem_post(&disembarked);
	pthread_exit(NULL);
};

void* captain(void* ptr){
// Vehicles counters for ferry loading logic.
	int numLoaded, numCars, numTrucks;
	int trips = 0;
	for( trips; trips < 11; trips++ ){
		numLoaded = 0; numCars = 0; numTrucks = 0;
		printf("\nLoading ferry for trip %d.\n\n", trips+1);
// Tell new vehicles to start entering the late queue.
		pthread_mutex_lock(&lock_boarding_call);
		boarding_call = 1; 
		pthread_mutex_unlock(&lock_boarding_call);
		while( numLoaded < 6 ){
			pthread_mutex_lock(&lock_early_trucks);
			while( (numTrucks < 2) && (numLoaded < 5) && (early_trucks > 0) ){
				early_trucks--;
				sem_post(&q_early_trucks);
				numTrucks++;
				numLoaded += 2;
			};
			pthread_mutex_unlock(&lock_early_trucks);
			pthread_mutex_lock(&lock_early_cars);
			while( (numLoaded < 6) && (early_cars > 0) ) { 
				early_cars--;
				sem_post(&q_early_cars);
				numCars++;
				numLoaded++;
			};
			pthread_mutex_unlock(&lock_early_cars);

			pthread_mutex_lock(&lock_late_trucks);
			while( (numTrucks < 2) && (numLoaded < 5) && (late_trucks > 0) ){
				late_trucks--;
				sem_post(&q_late_trucks);
				numTrucks++;
				numLoaded += 2;
			};
			pthread_mutex_unlock(&lock_late_trucks);
			pthread_mutex_lock(&lock_late_cars);
			while( (numLoaded < 6) && (late_cars > 0) ){ 
				late_cars--;
				sem_post(&q_late_cars);
				numCars++;
				numLoaded++;
			};
			pthread_mutex_unlock(&lock_late_cars);
		};

		int i = 0;
		for( i; i < (numCars + numTrucks); i++ ){
			sem_wait(&loaded);
		};
		pthread_mutex_lock(&lock_boarding_call);
		boarding_call = 0; 
		pthread_mutex_unlock(&lock_boarding_call);

		printf("All expected vehicles have been loaded.\n");
		printf("Ferry is now setting sail.\n\n");
		

		usleep((ferry_interval+1)*1000);
		printf("\nFerry is arriving at the destination.\n");
		printf("Vehicles will now disembark.\n");

// Signal to vehicles that they can leave.
		i = 0;
		for(i; i < (numCars + numTrucks); i++ ){
			sem_post(&unload);
		};

// Wait for vehicles to signal that they've left.
		i = 0;
		for(i; i < (numCars + numTrucks); i++ ){
			sem_wait(&disembarked);
		};
		printf("All vehicles successfully delivered.\n");
		printf("Ferry is now returning...\n\n");
		usleep((ferry_interval+1)*1000);
	};
	termination_flag = 1;
};

int main(){

	sem_init(&q_early_trucks, 0, 0);
	sem_init(&q_late_trucks, 0, 0);
	sem_init(&q_early_cars, 0, 0);
	sem_init(&q_late_cars, 0, 0);
	sem_init(&disembarked, 0, 0);
	sem_init(&loaded, 0, 0);
	sem_init(&unload, 0, 0);

	pthread_mutex_init(&lock_early_cars, NULL);
	pthread_mutex_init(&lock_early_trucks, NULL);
	pthread_mutex_init(&lock_late_cars, NULL);
	pthread_mutex_init(&lock_late_trucks, NULL);
	pthread_mutex_init(&lock_boarded_cars, NULL);
	pthread_mutex_init(&lock_boarded_trucks, NULL);

    printf("Enter a probability within [0..100] that a truck arrives in place of a car: ");
    scanf("%d", &truck_prob);
    printf("\nEnter the maximum time interval in milleseconds between vehicle arrivals: ");
    scanf("%d", &spawn_interval);
    printf("\nEnter the Ferry travel time in milleseconds(should be chosen close to previously entered value): ");
    scanf("%d", &ferry_interval);

	pthread_create(&cap, NULL, captain, arg);

	time_t t;
	time(&t);
	srand((unsigned)t);
	int spawn_time;

	while( !termination_flag ){
		spawn_time = rand()%spawn_interval + 1;
		if( (rand()%101) < truck_prob ){
			pthread_create( &vehicle, NULL, truck, arg);
		}else{
			pthread_create( &vehicle, NULL, car, arg);
		};	
		usleep(spawn_time*1000);
	};

	pthread_join(cap, NULL);

	sem_destroy(&q_early_trucks);
	sem_destroy(&q_late_trucks);
	sem_destroy(&q_early_cars);
	sem_destroy(&q_late_cars);
	sem_destroy(&disembarked);
	sem_destroy(&loaded);
	sem_destroy(&unload);

	pthread_mutex_destroy(&lock_early_cars);
	pthread_mutex_destroy(&lock_early_trucks);
	pthread_mutex_destroy(&lock_late_cars);
	pthread_mutex_destroy(&lock_late_trucks);
	pthread_mutex_destroy(&lock_boarded_cars);
	pthread_mutex_destroy(&lock_boarded_trucks);

	return 0;
}

