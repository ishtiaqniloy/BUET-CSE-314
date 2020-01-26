#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <iostream>
#include <queue>

#define q1_CAPACITY 5
#define q2_CAPACITY 5
#define q3_CAPACITY 5

#define chefX_SLEEP_TIME 1
#define chefY_SLEEP_TIME 1
#define chefZ_SLEEP_TIME 5
#define waiter1_SLEEP_TIME 15
#define waiter2_SLEEP_TIME 15


using namespace std;

class Cake{
public:
	int id;
	bool isChocolate;
	bool isDecorated;

	Cake(int n, bool isChoc, bool isDec = false){
		id = n;
		isChocolate = isChoc;
		isDecorated = isDec;
	}

	bool decorate(){
		isDecorated=true;
	}
	
};

//semaphore to control sleep and wake up

queue<Cake*> q1;	//all cakes
sem_t q1_empty;
sem_t q1_full;
pthread_mutex_t q1_lock;


queue<Cake*> q2;  //decorated vanilla cake
sem_t q2_empty;
sem_t q2_full;
pthread_mutex_t q2_lock;


queue<Cake*> q3;  //decorated chocolate cake
sem_t q3_empty;
sem_t q3_full;
pthread_mutex_t q3_lock;


pthread_mutex_t console_lock;

bool init_semaphore(){
	sem_init(&q1_empty,0,q1_CAPACITY);
	sem_init(&q1_full,0,0);
	pthread_mutex_init(&q1_lock,0);


	sem_init(&q2_empty,0,q2_CAPACITY);
	sem_init(&q2_full,0,0);
	pthread_mutex_init(&q2_lock,0);



	sem_init(&q3_empty,0,q3_CAPACITY);
	sem_init(&q3_full,0,0);
	pthread_mutex_init(&q3_lock,0);


	pthread_mutex_init(&console_lock,0);



	return true;

}


void * chefXFunc(void * arg){	//produces chocolate cake

	int id = 1;
	Cake* cake;

	pthread_mutex_lock(&console_lock);
	printf("%s\n",(char*)arg);
	pthread_mutex_unlock(&console_lock);

	while(true){

		pthread_mutex_lock(&console_lock);
		printf("ChefX waiting for q1 empty slots...\n");
		pthread_mutex_unlock(&console_lock);

		sem_wait(&q1_empty);	//if q1 has empty slots

		pthread_mutex_lock(&q1_lock);	//get lock on q1

		sleep(1);
		cake = new Cake(id, true);

		q1.push(cake);

		pthread_mutex_lock(&console_lock);
		printf("ChefX produced Choclate cake with ID: %d\nq1 new size = %d\n", id, q1.size());
		pthread_mutex_unlock(&console_lock);

		pthread_mutex_unlock(&q1_lock);

		sem_post(&q1_full);		//increases full slots

		id++;

		sleep(chefX_SLEEP_TIME);
	
	}


}


void * chefYFunc(void * arg){		//produces vanilla cake

	int id = 1;
	Cake* cake;

	pthread_mutex_lock(&console_lock);
	printf("%s\n",(char*)arg);
	pthread_mutex_unlock(&console_lock);

	while(true){
		pthread_mutex_lock(&console_lock);
		printf("ChefY waiting for q1 empty slots...\n");
		pthread_mutex_unlock(&console_lock);

		sem_wait(&q1_empty);	//if q1 has empty slots

		pthread_mutex_lock(&q1_lock);	//get lock on q1

		sleep(1);
		cake = new Cake(id, false);

		q1.push(cake);

		pthread_mutex_lock(&console_lock);
		printf("ChefY produced Vanilla cake with ID: %d\nq1 new size = %d\n", id, q1.size());
		pthread_mutex_unlock(&console_lock);

		pthread_mutex_unlock(&q1_lock);

		sem_post(&q1_full);		//increases q1 full slots


		id++;
		sleep(chefY_SLEEP_TIME);


	}

}


void * chefZFunc(void * arg){			//decorates cake

	pthread_mutex_lock(&console_lock);
	printf("%s\n",(char*)arg);
	pthread_mutex_unlock(&console_lock);

	while(true){

		int sz;

		pthread_mutex_lock(&console_lock);
		printf("ChefZ waiting for cakes in q1...\n");
		pthread_mutex_unlock(&console_lock);

		sem_wait(&q1_full);	//if q1 has cakes
		pthread_mutex_lock(&q1_lock);	//get lock on q1

		sleep(1);
		Cake* cake = q1.front();
		q1.pop();

		if(cake->isChocolate){
			pthread_mutex_lock(&console_lock);
			printf("ChefZ got Chocolate from q1 cake with ID: %d\nq1 new size = %d\n", cake->id, q1.size());
			pthread_mutex_unlock(&console_lock);
		}
		else{
			pthread_mutex_lock(&console_lock);
			printf("ChefZ got Vanilla cake from q1 with ID: %d\nq1 new size = %d\n", cake->id, q1.size());
			pthread_mutex_unlock(&console_lock);
		}

		pthread_mutex_unlock(&q1_lock);

		sem_post(&q1_empty);

		if(cake->isChocolate){
			
			pthread_mutex_lock(&console_lock);
			printf("ChefZ waiting for q3 empty slots...\n");
			pthread_mutex_unlock(&console_lock);	

			sem_wait(&q3_empty);	//if q3 has empty slots

			pthread_mutex_lock(&q3_lock);	//get lock on q3

			sleep(1);
			cake->decorate();

			q3.push(cake);

			pthread_mutex_lock(&console_lock);
			printf("ChefZ decorated Chocolate cake with ID: %d\nq3 new size = %d\n", cake->id, q3.size());
			pthread_mutex_unlock(&console_lock);

			pthread_mutex_unlock(&q3_lock);

			sem_post(&q3_full);		//increases q3 full slots



		}
		else{

			pthread_mutex_lock(&console_lock);
			printf("ChefZ waiting for q2 empty slots...\n");
			pthread_mutex_unlock(&console_lock);	

			sem_wait(&q2_empty);	//if q2 has empty slots

			pthread_mutex_lock(&q2_lock);	//get lock on q2

			sleep(1);
			cake->decorate();

			q2.push(cake);

			pthread_mutex_lock(&console_lock);
			printf("ChefZ decorated Vanilla cake with ID: %d\nq2 new size = %d\n", cake->id, q2.size());
			pthread_mutex_unlock(&console_lock);

			pthread_mutex_unlock(&q2_lock);

			sem_post(&q2_full);		//increases q2 full slots


		}
		sleep(chefZ_SLEEP_TIME);


		


	}
	
}


void * waiter1Func(void * arg){			//serves chocolate cake from q3

	pthread_mutex_lock(&console_lock);
	printf("%s\n",(char*)arg);
	pthread_mutex_unlock(&console_lock);

	while(true){
		pthread_mutex_lock(&console_lock);
		printf("Waiter1 waiting for cakes in q3...\n");
		pthread_mutex_unlock(&console_lock);

		sem_wait(&q3_full);	//if q3 has cakes
		pthread_mutex_lock(&q3_lock);	//get lock on q3

		sleep(1);
		Cake* cake = q3.front();
		q3.pop();

		pthread_mutex_lock(&console_lock);
		printf("Waiter1 served Chocolate cake from q3 with ID: %d\nq3 new size = %d\n", cake->id, q3.size());
		pthread_mutex_unlock(&console_lock);

		pthread_mutex_unlock(&q3_lock);

		sem_post(&q3_empty);

		delete cake;
		

		sleep(waiter1_SLEEP_TIME);
		

	}
	
}


void * waiter2Func(void * arg){			//serves vanilla cake from q2

	pthread_mutex_lock(&console_lock);
	printf("%s\n",(char*)arg);
	pthread_mutex_unlock(&console_lock);
	

	while(true){
		pthread_mutex_lock(&console_lock);
		printf("Waiter2 waiting for cakes in q2...\n");
		pthread_mutex_unlock(&console_lock);

		sem_wait(&q2_full);	//if q2 has cakes
		pthread_mutex_lock(&q2_lock);	//get lock on q2

		sleep(1);
		Cake* cake = q2.front();
		q2.pop();

		pthread_mutex_lock(&console_lock);
		printf("Waiter2 served Vanilla cake from q2 with ID: %d\nq2 new size = %d\n", cake->id, q2.size());
		pthread_mutex_unlock(&console_lock);

		pthread_mutex_unlock(&q2_lock);

		sem_post(&q2_empty);

		delete cake;

		sleep(waiter2_SLEEP_TIME);


	}


}




int main(){

	pthread_t chefX;
	pthread_t chefY;
	pthread_t chefZ;
	pthread_t waiter1;
	pthread_t waiter2;

	bool s = init_semaphore();
	if(!s){
		printf("Failed to initialize semaphores. Exiting...\n");
		exit(0);
	}


	char * messageChefX = "I am chefX";
	char * messageChefY = "I am chefY";
	char * messageChefZ = "I am chefZ";
	char * messageWaiter1 = "I am Waiter1";
	char * messageWaiter2 = "I am Waiter2";


	pthread_create(&chefX,NULL,chefXFunc,(void*)messageChefX);
	pthread_create(&chefY,NULL,chefYFunc,(void*)messageChefY);
	pthread_create(&chefZ,NULL,chefZFunc,(void*)messageChefZ);
	pthread_create(&waiter1,NULL,waiter1Func,(void*)messageWaiter1);
	pthread_create(&waiter2,NULL,waiter2Func,(void*)messageWaiter2);


	pthread_mutex_lock(&console_lock);
	printf("Delicious Cakes!!!\n");
	pthread_mutex_unlock(&console_lock);


	pthread_join(chefX, NULL);
	pthread_join(chefY, NULL);
	pthread_join(chefZ, NULL);
	pthread_join(waiter1, NULL);
	pthread_join(waiter2, NULL);


	printf("\nMain function Exiting...\n");

	return 0;
}