#include <pthread.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <time.h>
#include <sys/time.h>
#include <iomanip>
#include <sys/stat.h>
#include <vector>

int TransSave = 0;

void Trans( int n ) {
	long i, j;

	// Use CPU cycles 
	j = 0;
	for( i = 0; i < n * 100000; i++ ) {
		j += i ^ (i+1) % (i+n);
	}
	TransSave += j;
	TransSave &= 0xff;
}

void Sleep( int n ) {
	struct timespec sleep;

	// Make sure pass a valid nanosecond time to nanosleep
	if( n <= 0 || n >= 100 ) {
		n = 1;
	}

	// Sleep for less than one second
	sleep.tv_sec  = 0;
        sleep.tv_nsec = n * 10000000 + TransSave;
	if( nanosleep( &sleep, NULL ) < 0 ) {
		perror ("NanoSleep" );
	}
}

double wall_time(){
	struct timeval time;
	gettimeofday(&time,NULL);
	return((double)time.tv_sec + (double)time.tv_usec * .000001);
}

//mutex for queue 
pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
//mutex for file io
pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;

std::queue<int> work_queue;
std::vector<int> jobs_completed;
bool workCompleted = false;
double startTime = wall_time();
int askCount = 0;

// global output stream for log file
std::ofstream output;

void* consumer_thread(void* idpoint){
	
	int n;
	int id = *(int *)idpoint;
	pthread_mutex_lock(&io_mutex);
	//ASK
	output << std::fixed << std::setprecision(3) << wall_time() - startTime << "  " << "ID=" << id << "          ASK" << std::endl;
	askCount++;
	pthread_mutex_unlock(&io_mutex);
	while(!workCompleted){
		n = -1;
		pthread_mutex_lock(&q_mutex);
		if(!work_queue.empty()){
			n = work_queue.front();
			work_queue.pop();
			pthread_mutex_lock(&io_mutex);
			//RECIEVE
			output << wall_time() - startTime << "  " << "ID=" << id << "   Q=" << work_queue.size() << "    RECIEVE   " << n << std::endl;
			pthread_mutex_unlock(&io_mutex);
			
		}
		pthread_mutex_unlock(&q_mutex);
		if(n != -1){
			Trans(n);
			pthread_mutex_lock(&io_mutex);
			//COMPLETE
			jobs_completed.at(id - 1)++;
			output << wall_time() - startTime << "  " << "ID=" << id  << "          COMPLETE  " << n << std::endl;
			//ASK
			output << wall_time() - startTime << "  " << "ID=" << id << "          ASK" << std::endl;
			askCount++;
			pthread_mutex_unlock(&io_mutex);
		}
	}		
}





void printSummary(int sleeps){
	int totalJobs = 0;
	for(int i = 1; i <= jobs_completed.size(); i++){
		totalJobs += jobs_completed.at(i - 1);

	}
	double transactionRate = totalJobs/(wall_time() - startTime);
	output << "Summary:" << std::endl;
	output << "	Work:      " << totalJobs << std::endl;
	output << "	Ask:       " << askCount << std::endl;
	output << "	Recieve:   " << totalJobs << std::endl;
	output << "	Complete:  " << totalJobs << std::endl;
	output << "	Sleep:     " << sleeps << std::endl;
	for(int i = 1; i <= jobs_completed.size(); i++){
		output << "	Thread " << i << ":  " << jobs_completed.at(i - 1) << std::endl;
	}
	output << "Transactions per second: " << std::fixed << std::setprecision(2) << transactionRate << std::endl;
	output << "----------------------------------" << std::endl << std::endl << std::endl;
}




int main(int argc, char* argv[]){
	int log_num = 0;
	int Nthreads = 0;
	int sleepCount = 0;

	//read command line args
	if(argc == 1){
		std::cout << "Not enought input arguments" << std::endl;
		return(-1);
	}
	else if(argc == 2){
		Nthreads = atoi(argv[1]);
	}
	else if(argc == 3){
		Nthreads = atoi(argv[1]);
		log_num = atoi(argv[2]);
	}
	else{
		std::cout << "Too many input arguments" << std::endl;
	}
	
	std::string logfile = "prodcon." + std::to_string(log_num) + ".log";
	std::cout << "# of threads: " << Nthreads << std::endl;
	std::cout << "Log file: " << logfile << std::endl;
	
	//create file stream for log
	std::string cwd = getcwd(NULL, 0);
	output.open(logfile, std::ios_base::app);
	
	
	//create N amount of threads
	pthread_t consumers[Nthreads];
	int consumer_ids[Nthreads];
	for(int i = 0; i < Nthreads; i++){
		consumer_ids[i] = i+1;
		pthread_create(&consumers[i], NULL, consumer_thread, &consumer_ids[i]);
		jobs_completed.push_back(0);			
	}
	//Threads are made, start adding to Q
	int id = 0;
	std::string task;
	int Qmax = 2*Nthreads;
	for(task; std::getline(std::cin, task);){
		if(task.substr(0,1) == "T"){
			int n = std::stoi(task.substr(1,task.size() - 1));
			//wait for queue space
			bool Qfull = true;
			while(Qfull){
				pthread_mutex_lock(&q_mutex);
				if(work_queue.size() < Qmax){
					Qfull = false;
				}
				pthread_mutex_unlock(&q_mutex);
			}
			pthread_mutex_lock(&q_mutex);
			work_queue.push(n);
			pthread_mutex_unlock(&q_mutex);
			pthread_mutex_lock(&io_mutex);
			//WORK
			output << std::fixed << std::setprecision(3) << wall_time() - startTime << "  " << "ID=" << id << "   Q=" << work_queue.size() << "    WORK      " << n << std::endl; 
			pthread_mutex_unlock(&io_mutex);
		}	
		else{
			int n = std::stoi(task.substr(1, task.size() - 1));
			pthread_mutex_lock(&io_mutex);
			output << std::fixed << std::setprecision(3) << wall_time() - startTime << "  " << "ID=" << id << "          SLEEP     " << n << std::endl;
			pthread_mutex_unlock(&io_mutex);
			//SLEEP
			Sleep(n);
			sleepCount++;
		}
	}
	pthread_mutex_lock(&io_mutex);
	output << wall_time() - startTime << "  " << "ID=" << id << "          END" << std::endl;
	pthread_mutex_unlock(&io_mutex);
	//END
	bool isEmpty = false;
	while(!isEmpty){
		pthread_mutex_lock(&q_mutex);
		if(work_queue.empty()){
			isEmpty = true;
		}
		pthread_mutex_unlock(&q_mutex);
	}
	workCompleted = true;
	for(int i = 0; i < Nthreads; i++){
		pthread_join(consumers[i], NULL);
	}
	printSummary(sleepCount);
	
	return(0);

}





























