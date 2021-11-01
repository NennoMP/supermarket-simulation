#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif /* _DEFAULT_SOURCE */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>


#include "./include/cashier.h"
#include "./include/customer.h"
#include "./include/utils.h"
#include "./include/myqueue.h"
#include "./include/mylist.h"
#include "./include/mylist2.h"


/* config values SM */
int K;			// n max cashiers
int K_start;		// n start cashiers
int C;			// n max customers inside
int E;			// (C-E) target for new customers
int P;			// n prodotti massimo per cliente
int T;			// time max customer
int T_product;		// time cashier x product
int I;			// interval cashier-manager notifications
int S;			// interval customer queue change
int S2;		// n max customers in a queue to addCashier
char* LOG_FILE;	// log file for output data

/* auxiliary values*/
int n_tot_customers;		// n total customer supermarket 
int n_tot_products;		// n total products supermarket

int n_cashiers_open	= 0;	// n cashiers open
int n_customers_in	= 0;	// n customers inside
int id_c		= 0;	// id customers counter
int closeID		= -1;	// id cashier to close for closeCashier

int* do_join;			// array for pthread_join (0: already done, 1: to do)
int* c_status; 		// cashiers status (0: closed, 1:open)

int addCashier		= 0;	// flag for adding cashier
int closeCashier	= 0;	// flag for closing cashier
int quit		= 0;	// flag for SIGQUIT
int closing		= 0;	// flag for SIGHUP

/* data structures */
list_customerData datacustomers;	// list of customers data for log file
cashierArgs_h* datasm; 		// array arguments for cashiers */
queue_h* c_queues;  			// cashiers queues

/* threads */
pthread_t* th_cashiers;
pthread_t* th_handlers;


/* mutex */
static pthread_mutex_t* status_lock;						// mutex on status cashiers
static pthread_mutex_t customers_data 	= PTHREAD_MUTEX_INITIALIZER;	// mutex for customers data list
static pthread_mutex_t tot_customers 		= PTHREAD_MUTEX_INITIALIZER;	// mutex for n_tot_customers
static pthread_mutex_t tot_products 		= PTHREAD_MUTEX_INITIALIZER;	// mutex for n_tot_products
static pthread_mutex_t cashiers_open 		= PTHREAD_MUTEX_INITIALIZER;	// mutex for n_cashiers_open
static pthread_mutex_t mutex_customers_in	= PTHREAD_MUTEX_INITIALIZER;	// mutex on n_customers_in
static pthread_mutex_t	spawn_cashiers		= PTHREAD_MUTEX_INITIALIZER;	// mutex for closeCashier && addCashier flags
/* cond */
static pthread_cond_t cond_customers_in	= PTHREAD_COND_INITIALIZER;	// cond on n_customers_in
static pthread_cond_t 	cond_spawn_cashiers 	= PTHREAD_COND_INITIALIZER;	// cond for spawn Cashier_Spawner


/* utility function for loading default values from config.txt */
void parse_config (void) {
	FILE* fd_config;
	if ((fd_config=fopen("./config/config.txt", "r"))==NULL) {
		perror("fopen");
		fprintf(stderr, "cannot open config.txt\n");
		exit(EXIT_FAILURE);
	}
	int value;
	char buff[BUFSIZE];
	char* line;
	while (fgets(buff, sizeof(buff), fd_config)!=NULL) {
		if (strstr(buff, "K=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			K	= value;
		}
		if (strstr(buff, "K_start=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			K_start = value;
		}
		if (strstr(buff, "C=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			C	= value;
		}
		if (strstr(buff, "E=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			E	= value;
		}
		if (strstr(buff, "P=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			P	= value;
		}
		if (strstr(buff, "T=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			T 	= value;
		}
		if (strstr(buff, "T_product=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			T_product = value;
		}
		if (strstr(buff, "I=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			I	= value;
		}
		if (strstr(buff, "S=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			S	= value;
		}
		if (strstr(buff, "S2=")) {
			line 	= strstr(buff, "=");
			value	= atoi(&line[1]);
			S2	= value;
		}
		if (strstr(buff, "LOG_FILE=")) {
			line 	= strstr(buff, "=");
			value	= strlen(&line[1]);
			if ((LOG_FILE=malloc((value+1)*sizeof(char)))==NULL) {
				perror("malloc");
				exit(EXIT_FAILURE);
			}
			strcpy(LOG_FILE, &line[1]);
			strtok(LOG_FILE, "\n");
		}
	}
	fclose(fd_config);
}
/* utility functions to init cashier args array */
void setdatasm (cashierArgs_h* datasm, int i) {
	datasm->id		= i+1;
	datasm->n_customers	= 0;
	datasm->n_queue	= 0;
	datasm->n_products	= 0;
	datasm->n_close	= 0;
	datasm->t_service	= 0;
	datasm->t_product	= T_product;
	datasm->t_customers	= NULL;
	datasm->t_close	= NULL;
}
/* utility function for sending notifications to Manager */
void sendToManager (int nc, int fd_socket) {
	printf("Cashier_Handler[%d]: sending notification\n", nc);
	int qlen = length_queue(c_queues[nc-1]);
	char message[64];
	snprintf(message, sizeof(message), "Cashier:%d n_customers:%d", nc, qlen);
	int len= strlen(message)+1;
	int n;
	SYSCALL(n, write_h(fd_socket, &len, sizeof(int)), "write_len");
	SYSCALL(n, write_h(fd_socket, message, len), "write_info");
	printf("Sent: %s\n", message);
	notification_t answer;
	// read length
	SYSCALL(n, read_h(fd_socket, &answer.len, sizeof(int)), "read_len");
	answer.info = calloc((answer.len), sizeof(char));
	if (!answer.info) {
		fprintf(stderr, "ERROR: calloc\n");
		exit(EXIT_FAILURE);
	}
	// read string
	SYSCALL(n, read_h(fd_socket, answer.info, (answer.len)*sizeof(char)), "read_info");
	printf("Cashier_Handler[%d]: received %s\n", nc, answer.info);
	if (strcmp(answer.info, "Close Cashier")==0) {
			if (length_queue(c_queues[nc-1])<=1) { 
				MUTEX_LOCK(&cashiers_open, "cashiers open");
				if (n_cashiers_open>1) {
					MUTEX_LOCK(&spawn_cashiers, "sm close");
					closeCashier = 1;
					closeID   = nc;
					printf("N_CASHIERS: %d, closing %d\n", n_cashiers_open, closeID);
					pthread_cond_signal(&cond_spawn_cashiers);
					MUTEX_UNLOCK(&spawn_cashiers, "sm close");
				}
				else {
					printf("N_CASHIERS: %d, not closing\n", n_cashiers_open);
				}
				MUTEX_UNLOCK(&cashiers_open, "cashiers open");
			}	
	}
	else if (strcmp(answer.info, "Add Cashier")==0) {
		if (length_queue(c_queues[nc-1])>=S2) {
			MUTEX_LOCK(&spawn_cashiers, "sm add");
			addCashier = 1;
			pthread_cond_signal(&cond_spawn_cashiers);
			MUTEX_UNLOCK(&spawn_cashiers, "sm add");
		}
	}
	free(answer.info);
}
/* th_handlers func */
void* Notification_Handler (void* args) {
	int nc = *((int*)args);
	int res;
	struct timespec *ts_notification;
	if ((ts_notification=(struct timespec*)malloc(sizeof(struct timespec)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	ts_notification->tv_sec = I/1000;
	ts_notification->tv_nsec = (I%1000)*1000000;
	int fd_socket;
	
	struct sockaddr_un addr;
	memset(&addr, '0', sizeof(addr));
	addr.sun_family= AF_UNIX;
	strncpy(addr.sun_path, SOCKNAME_MANAGER, strlen(SOCKNAME_MANAGER)+1);
	SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	int notused;
	
	SYSCALL(notused, connect(fd_socket, (struct sockaddr*)&addr, sizeof(addr)), "connect");
	
	while (c_status[nc-1]==1 && !quit && !closing) {
	if (c_status[nc-1]==0) break;
		do {
			res = nanosleep(ts_notification, ts_notification);
		} while (res && errno==EINTR);
    		if (c_status[nc-1]==0) break;
		sendToManager(nc, fd_socket);
	}
	free(ts_notification);
	close(fd_socket);
	printf("HANDLER %d CLOSED\n", nc);
	pthread_exit((void*)EXIT_SUCCESS);
}
/* th_cashiers func */
void* Cashier (void* args) {
	/* Cashier info */
	int myid		= ((cashierArgs_h*)args)->id;
	long t_service		= ((cashierArgs_h*)args)->t_service;
	int t_product		= ((cashierArgs_h*)args)->t_product;
	do_join[myid-1] 	= 1;
	
	
	struct timeval open_start, open_end, open_result;
	struct timeval customer_start, customer_end, customer_result;
	
	long cashier_time, customer_time;
	
	struct timespec * ts_service;
	if ((ts_service=(struct timespec*)malloc(sizeof(struct timespec)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	
	
	int* id_handler;
	if ((id_handler=(int*)malloc(sizeof(int)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	*id_handler = myid;
	
	int open, empty;
	
	
	
	/* service time */
	unsigned int seed= myid * time(NULL);
	while ((t_service = rand_r(&seed) % 80)<20); // Random number [20-80]
	
	
	MUTEX_LOCK(&status_lock[myid-1], "cashier");
	c_status[myid-1] = 1;
	MUTEX_UNLOCK(&status_lock[myid-1], "cashier");
	open = 1;
	printf("Cashier[%d]: open\n", myid);
	
	if (pthread_create(&th_handlers[myid-1], NULL, Notification_Handler ,(void*)id_handler)!=0) {
		fprintf(stderr, "pthread_create failed\n");
		pthread_exit((void*)EXIT_FAILURE);
	}
	
	gettimeofday(&open_start, NULL);
	while (!quit && open) {
		MUTEX_LOCK(c_queues[myid-1].mutex, "cashier");
		while (c_queues[myid-1].first==NULL && !quit && open) {
			COND_WAIT(c_queues[myid-1].empty, c_queues[myid-1].mutex, "cashier");
			MUTEX_LOCK(&status_lock[myid-1], "cashier");
			open = c_status[myid-1];
			MUTEX_UNLOCK(&status_lock[myid-1], "cashier");
		}
		MUTEX_UNLOCK(c_queues[myid-1].mutex, "cashier");
		if (quit || !open) continue;
		
		gettimeofday(&customer_start, NULL);
		int curr_id		= getID(c_queues[myid-1]); 
		int curr_products	= getProducts(c_queues[myid-1]);
		printf("Cashier[%d]: executing customer[%d]\n", myid, curr_id);
		setCheckout(&c_queues[myid-1]);
		long time_tot = t_service + (t_product*curr_products);
		ts_service->tv_sec 	= time_tot/1000;
		ts_service->tv_nsec	= (time_tot%1000)*1000000;
		int res;
		do {
			res = nanosleep(ts_service, ts_service);
		} while (res && errno==EINTR);
		if (quit || !open) continue;
		setPaid(&c_queues[myid-1]);
		pop_queue(&c_queues[myid-1]);
		gettimeofday(&customer_end, NULL);
		MUTEX_LOCK(&tot_products, "tot products");
		n_tot_products+= curr_products;
		MUTEX_UNLOCK(&tot_products, "tot products");
		timersub(&customer_end, &customer_start, &customer_result);
		customer_time = (customer_result.tv_sec*1000000 + customer_result.tv_usec)/1000;
		printf("Cashier [%d]: customer[%d] served\n", myid, curr_id);
		
		
		put_list(&(datasm[myid-1].t_customers), curr_id, customer_time);
		datasm[myid-1].n_products+= curr_products;
		datasm[myid-1].n_customers++;
		MUTEX_LOCK(&tot_customers, "tot customers");
		n_tot_customers++;
		MUTEX_UNLOCK(&tot_customers, "tot customers");
		
		MUTEX_LOCK(&status_lock[myid-1], "sm satus");
		open = c_status[myid-1];
		MUTEX_UNLOCK(&status_lock[myid-1], "sm status");
	}
	
	gettimeofday(&open_end, NULL);
	timersub(&open_end, &open_start, &open_result);
	
	MUTEX_LOCK(&status_lock[myid-1], "sm satus");
	c_status[myid-1] = 0;
	open = c_status[myid-1];
	MUTEX_UNLOCK(&status_lock[myid-1], "sm status");
	TH_JOIN(th_handlers[myid-1], "th_handler");
	free(id_handler);
	free(ts_service);
	
	if (!quit) {
	empty = isEmpty_queue(c_queues[myid-1]);
	while (!empty && empty>=0) {
		setClosing(&c_queues[myid-1]);
		pop_queue(&c_queues[myid-1]);
		empty = isEmpty_queue(c_queues[myid-1]);
	}
	if (empty<0) {
		fprintf(stderr, "ERROR: checking queue if empty\n");
		pthread_exit((void*)EXIT_FAILURE);
	}
	}		
	
	datasm[myid-1].n_close++;
	cashier_time = (open_result.tv_sec*1000000 + open_result.tv_usec)/1000;
	put_list(&(datasm[myid-1].t_close), 0, cashier_time);
	printf("Cashier[%d]: close\n", myid);
	pthread_exit((void*)EXIT_SUCCESS);
}
/* th_customer func */
void* Customer (void* args) {
	int myid		= ((customerArgs_h*)args)->id;
	int my_products	= ((customerArgs_h*)args)->n_products;
	int n_qchange		= ((customerArgs_h*)args)->n_qchange; // n queue changes
	int paid		= ((customerArgs_h*)args)->paid;
	int checkout		= ((customerArgs_h*)args)->checkout;
	int closing		= ((customerArgs_h*)args)->closing;
	int t_queue		= ((customerArgs_h*)args)->t_queue;
	int t_buy		= ((customerArgs_h*)args)->t_buy;
	
	customerArgs_h* customer;
	if ((customer=(customerArgs_h*)malloc(sizeof(customerArgs_h)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	customer->id = myid;
	customer->n_qchange = n_qchange;
	customer->n_products = my_products;
	customer->paid = paid;
	customer->closing = closing;
	customer->checkout = checkout;
	customer->t_queue = t_queue;
	customer->t_buy = t_buy;
	

	struct timeval join_start, join_end, join_result;
	struct timeval queue_start, queue_end, queue_result;
	
	
	int q_new;
	long time_queue, time_sm;
	time_queue = 0;
	
	int gotserved = 0;
	int permission= 0;
	
	unsigned int seed = time(NULL) ^ (customer->id);

	/* variabili asuiliarie */
	int q_chosen; 		// coda scelta 
	int isOpen;		// check se coda valida
	int changequeue= 0;	// queue chiusa e deve cambiare
	int exitsm = 0;
	
	int res;
	struct timespec* q_interval;
	if ((q_interval=(struct timespec*)malloc(sizeof(struct timespec)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	q_interval->tv_sec	= S/1000;
	q_interval->tv_nsec	= (S%1000)*1000000;


	gettimeofday(&join_start, NULL);
	
	
	if (quit) {
		goto exit;
	}
	
	struct timespec *ts_purchase;
	if ((ts_purchase=(struct timespec*)malloc(sizeof(struct timespec)))==NULL) {
		perror("malloc");
		pthread_exit((void*)EXIT_FAILURE);
	}
	ts_purchase->tv_sec 	= (customer->t_buy)/1000;
	ts_purchase->tv_nsec 	= (customer->t_buy%1000)*1000000;

	do {
		res = nanosleep(ts_purchase, ts_purchase);
	} while (res && errno==EINTR);
	free(ts_purchase);
    	
    	if (quit) {
    		goto exit;
    	}
	/* ho dei prodotti */
	if (customer->n_products!=0) {
		if (quit) {
			goto exit;
		}
		do {
			decide_queue:
			isOpen= 0;
			changequeue= 0;
			do {
				q_chosen = rand_r(&seed) % (K);
				MUTEX_LOCK(&status_lock[q_chosen], "sm status");
				isOpen = c_status[q_chosen];
				MUTEX_UNLOCK(&status_lock[q_chosen], "sm status");
			} while (!isOpen);
			n_qchange++;
			if (quit) {
				goto exit;
			}
			gettimeofday(&queue_start, NULL);
			changed_queue:
			MUTEX_LOCK(&status_lock[q_chosen], "sm status");
			if (c_status[q_chosen]==1) {
			if (push_queue(&c_queues[q_chosen], &customer)!=0) {
				fprintf(stderr, "ERROR: push queue\n");
				pthread_exit((void*)EXIT_FAILURE);
			}
			}
			else {
				goto decide_queue;
			}
			MUTEX_UNLOCK(&status_lock[q_chosen], "sm status");
			printf("Customer[%d]: join queue %d\n", customer->id, (q_chosen+1));
			while (changequeue==0 && !customer->paid && !quit) {
				while (!customer->checkout && !quit) {
					do {
						res = nanosleep(q_interval, q_interval);
					} while (res && errno==EINTR);
					if (c_status[q_chosen]==0) changequeue=1;
					if (quit) {
						gettimeofday(&queue_end, NULL);
						timersub(&queue_end, &queue_start, &queue_result);
						time_queue	= (queue_result.tv_sec*1000000 + queue_result.tv_usec)/1000;
						goto exit;
					}
					if (customer->checkout) break;
					int found = 0;
					if (n_cashiers_open>1) {
					do {
					q_new = rand_r(&seed) % (K);
					MUTEX_LOCK(&status_lock[q_new], "sm status");
					found = c_status[q_new];
					MUTEX_UNLOCK(&status_lock[q_new], "sm status");
					} while (!found);
					
					if (nBeforeMe(c_queues[q_chosen], myid)>length_queue(c_queues[q_new])) {
						if (quit) {
							gettimeofday(&queue_end, NULL);
							timersub(&queue_end, &queue_start, &queue_result);
							time_queue	= (queue_result.tv_sec*1000000 + queue_result.tv_usec)/1000;
							goto exit;
						}
						if (customer->checkout) break;
						removeSpecific(&c_queues[q_chosen], myid);
						q_chosen = q_new;
						printf("Customer[%d]: change queue to %d\n", myid, q_chosen+1);
						n_qchange++;
						goto changed_queue;
					}
					}
				}
				if (c_status[q_chosen]==0) changequeue= 1;
				gettimeofday(&queue_end, NULL);
				timersub(&queue_end, &queue_start, &queue_result);
				time_queue	= (queue_result.tv_sec*1000000 + queue_result.tv_usec)/1000;
			}
			if (customer->paid) {
				exitsm = 1;
				gotserved = 1;
			}
		} while (!exitsm);
	}
	else {
		struct sockaddr_un sm_addr;
		memset(&sm_addr, '0', sizeof(sm_addr));
		sm_addr.sun_family= AF_UNIX;
		strncpy(sm_addr.sun_path, SOCKNAME_MANAGER, strlen(SOCKNAME_MANAGER)+1);
	
		int fd_socket;
		SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
		while (connect(fd_socket, (struct sockaddr*)&sm_addr, sizeof(sm_addr))==-1) {
			if (errno==ENOENT) sleep(1);
			if (errno==EADDRINUSE) sleep(1);
			else {
				fprintf(stderr, "ERROR: connect customer\n");
				pthread_exit((void*)EXIT_FAILURE);
			}
		}
	
		char message[64];
		snprintf(message, sizeof(message), "Customer[%d]: exit?", myid);
		int len= strlen(message)+1;
		int n;
		if ((n=write_h(fd_socket, &len, sizeof(int)))==-1) {
			fprintf(stderr, "write len failed\n");
			exit(EXIT_FAILURE);
		}
		if ((n=write_h(fd_socket, message, len))==-1) {
			fprintf(stderr, "write info failed\n");
			exit(EXIT_FAILURE);
		}
		notification_t answer;
		while (!permission) {
			// read length
			if ((n=read_h((int)fd_socket, &answer.len, sizeof(int)))==-1) {
				fprintf(stderr, "recv failed\n");
				exit(errno);
			}
			// read string
			answer.info = calloc((answer.len), sizeof(char));
			if (!answer.info) {
				fprintf(stderr, "ERROR: calloc\n");
				exit(EXIT_FAILURE);
			}
			if ((n=read_h((int)fd_socket, answer.info, (answer.len)*sizeof(char)))==-1) {
				fprintf(stderr, "read string failed\n");
				exit(errno);
			}
			if (n==0) break;
			if (strcmp(answer.info, "OK")==0) {
				printf("Customer[%d]: received permission to exit\n", myid);
				permission = 1;
			}
		}
		free(answer.info);
		close(fd_socket);
	}
	
	exit:
	gettimeofday(&join_end, NULL);
	timersub(&join_end, &join_start, &join_result);
	time_sm	= (join_result.tv_sec*1000000 + join_result.tv_usec)/1000;
	
	
	customerAnalytics_h my_analytics;
	my_analytics.id = myid;
	my_analytics.np = my_products;
	my_analytics.nq = n_qchange;
	my_analytics.tq = time_queue;
	my_analytics.tsm = time_sm;
	my_analytics.served = gotserved;
	
	MUTEX_LOCK(&customers_data, "customers data");
	put_customerData(&datacustomers, my_analytics);
	MUTEX_UNLOCK(&customers_data, "customers data");
	
	MUTEX_LOCK(&mutex_customers_in, "customers_in");
	n_customers_in--;
	pthread_cond_signal(&cond_customers_in);
	MUTEX_UNLOCK(&mutex_customers_in, "customers_in");
	free(q_interval);
	printf("Customer[%d]: exit after %ld milliseconds\n", customer->id, time_sm);
	free(args);
	free(customer);
	pthread_exit((void*)EXIT_SUCCESS);
}
/* th_doorman func */
void* Doorman (void* notused) {
	pthread_t th_customer;
	customerArgs_h* args;
	unsigned int seed= time(NULL);
	int id_c= 0;
	
	while (!quit && !closing) {
		MUTEX_LOCK(&mutex_customers_in, "customers_in");
		while (n_customers_in > (C-E) && !quit && !closing) {
			COND_WAIT(&cond_customers_in, &mutex_customers_in, "customers_in");	
		}
		MUTEX_UNLOCK(&mutex_customers_in, "customers_in");
		if (quit || closing) continue;
		
		MUTEX_LOCK(&mutex_customers_in, "customers_in");
		/* start th_customers */
		while(n_customers_in < C) {
			if ((args=(customerArgs_h*)malloc(sizeof(customerArgs_h)))==NULL) {
				perror("malloc");
				pthread_exit((void*)EXIT_FAILURE);
			}
			args->id 		= id_c;
			args->n_products	= rand_r(&seed) % (P+1);
			args->n_qchange		= 0;
			args->t_buy		= (rand_r(&seed) % (T-9))+10;
			args->t_queue		= 0;
			args->paid		= 0;
			args->checkout		= 0;
			args->closing		= 0;
			
			pthread_create(&th_customer, NULL, Customer, (void*)args);
			pthread_detach(th_customer);
			n_customers_in++;
			id_c++;
		}
		MUTEX_UNLOCK(&mutex_customers_in, "customers_in");
	}
	printf("DOORMAN CLOSED\n");
	pthread_exit((void*)EXIT_SUCCESS);
}
/* th_spawner func */
void* Cashier_spawner (void* args) {
	int isOpen = 1, newc;
	while (!quit) {
		MUTEX_LOCK(&spawn_cashiers, "sm spawn");
		COND_WAIT(&cond_spawn_cashiers, &spawn_cashiers, "sm spawn");
		if (addCashier) {
			if (n_cashiers_open < K) {
				do {
					unsigned int seed = time(NULL);
					newc = rand_r(&seed) % (K);
					MUTEX_LOCK(&status_lock[newc], "sm status");
					isOpen = c_status[newc];
					MUTEX_UNLOCK(&status_lock[newc], "sm satus");
				} while (isOpen);
				if (pthread_create(&th_cashiers[newc], NULL, Cashier, &datasm[newc])!=0) {
					fprintf(stderr, "ERROR: pthread_create\n");
					exit(EXIT_FAILURE);
				}
				MUTEX_LOCK(&cashiers_open, "cashiers open");
				n_cashiers_open++;
				MUTEX_UNLOCK(&cashiers_open, "cashiers open");
			}
			addCashier = 0;
		}
		if (closeCashier) {
			int index = (closeID-1);
			MUTEX_LOCK(&status_lock[index], "sm status");
			c_status[index]= 0;
			MUTEX_UNLOCK(&status_lock[index], "sm status");
			MUTEX_LOCK(c_queues[index].mutex, "sm queue");
			pthread_cond_signal(c_queues[index].empty);
			MUTEX_UNLOCK(c_queues[index].mutex, "sm queue");
			MUTEX_LOCK(&cashiers_open, "sm open");
			n_cashiers_open--;
			MUTEX_UNLOCK(&cashiers_open, "sm open");
			closeID= -1;
			closeCashier = 0;;
			do_join[index] = 0;
			TH_JOIN(th_cashiers[index], "th_cashier");
			printf("CLOSED CASHIER[%d]\n", index+1);
		}
		MUTEX_UNLOCK(&spawn_cashiers, "sm spawn");
	}
	for (int i=0;i<K;i++) {
		MUTEX_LOCK(c_queues[i].mutex, "sm queue");
		pthread_cond_signal(c_queues[i].empty);
		MUTEX_UNLOCK(c_queues[i].mutex, "sm queue");
	}
	printf("SPAWNER CLOSED\n");
	pthread_exit((void*)EXIT_SUCCESS);
}
/* utility function for writing analytics to log file */
void writeLogFile (void) {
	while(n_customers_in>0);
	printf("WRITING LOG FILE\n");
	char* string;
	if ((string=(char*)malloc(1024*sizeof(char)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	FILE* fd_log;
	fd_log = fopen(LOG_FILE, "w");
	
	fwrite ("|!| tot_customers tot_products\n", sizeof(char), 31, fd_log);
	
	fwrite("|!| customer_id n_products n_queue_change supermarket_time queue_time got_served\n", sizeof(char), 81, fd_log);
	
	fwrite("|!| cashier_id n_products n_customers n_closures {t_each_customer} {time_each_open}\n\n", sizeof(char), 85, fd_log);
	
	
	// writing tot_customers
	sprintf(string, "tot_customers: %d\n", n_tot_customers);
	fwrite(string, sizeof(char), strlen(string), fd_log);
	// writing tot_products
	sprintf(string, "tot_products: %d\n", n_tot_products);
	fwrite(string, sizeof(char), strlen(string), fd_log);
	
	int len = length_list2(datacustomers);
	customerAnalytics_h* array;
	if ((array=malloc(len*sizeof(customerAnalytics_h)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	listToArray(datacustomers, len, &(*array));
	qsort(&(*array), len, sizeof(customerAnalytics_h), compare_analytics);
	
	 
	// writing customers info
	for (int i=0;i<len;i++) {
		sprintf(string, "Customer: %d %d %d %d %d %d\n", array[i].id, array[i].np, array[i].nq, array[i].tsm, array[i].tq, array[i].served);
		fwrite(string, sizeof(char), strlen(string), fd_log);
	}
	free(array);
	
	// writing cashiers info
	long tmp;
	for (int i=0;i<K;i++) {
		if (datasm[i].n_close>0) {
		sprintf(string, "Cashier: %d %d %d %d ", i+1, datasm[i].n_products, datasm[i].n_customers, datasm[i].n_close);
		fwrite(string, sizeof(char), strlen(string), fd_log);
		
		// time_each_customer
		tmp = next (datasm[i].t_customers);
		if (tmp>=0) {
			sprintf(string, "%ld", tmp);
			fwrite(string, sizeof(char), strlen(string), fd_log);
		}
		else {
			fwrite("null", sizeof(char), 4, fd_log);
		}
		tmp = next(NULL);
		
		while (tmp>=0) {
			sprintf(string, ",%ld", tmp);
			fwrite(string, sizeof(char), strlen(string), fd_log);
			tmp = next(NULL);
		}
		fwrite(" ", sizeof(char), 1, fd_log);
		
		//time_each_close
		tmp = next(datasm[i].t_close);
		if (tmp>=0) {
			sprintf(string, "%ld", tmp);
			fwrite(string, sizeof(char), strlen(string), fd_log);
		}
		else {
			fwrite("null", sizeof(char), 4, fd_log);
		}
		tmp = next(NULL);
		
		while (tmp>=0) {
			sprintf(string, ",%ld", tmp);
			fwrite(string, sizeof(char), strlen(string), fd_log);
			tmp = next(NULL);
		}
		fwrite("\n", sizeof(char), 1, fd_log);
	}
	}
	
	free_customerData(&datacustomers);
	fclose(fd_log);
	free(string);
	free(LOG_FILE);
	/* some clears */
	printf("WRITE TO LOG FILE DONE\n");
}

int main (int argc, char* argv[]) {
	parse_config();	// load default values
	
	// input values (test1 & test2)
	if (argc!=1) {
		if (argc==7) {
			K= strtol(argv[1], NULL, 10);
			C= strtol(argv[2], NULL, 10);
			E= strtol(argv[3], NULL, 10);
			T= strtol(argv[4], NULL, 10);
			P= strtol(argv[5], NULL, 10);
			S= strtol(argv[6], NULL, 10);
		}
		else {
			fprintf(stderr, "Use: %s <K> <C> <E> <T> <P> <S>\n", argv[0]);
			return -1;
		}
	}
	
	datacustomers = NULL;
	datasm= malloc(K*sizeof(cashierArgs_h));
	if ((c_queues 		= (queue_h*)malloc(K*sizeof(queue_h)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	if ((c_status		= (int*)malloc(K*sizeof(int)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	if ((do_join		= (int*)malloc(K*sizeof(int)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	if ((status_lock		= malloc(K*sizeof(pthread_mutex_t)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	

	/* init struct info cashiers */
	for (int i=0;i<K;i++) {
		setdatasm(&datasm[i], i);
	}
	/* init queues cashiers & status */
	for (int i=0;i<K;i++) {
		init_queue(&c_queues[i]);
		c_status[i]= 0;
		do_join[i]= 0;
		pthread_mutex_init(&status_lock[i], NULL);
	}
	
	/* init th_cashiers */
	if ((th_handlers = malloc(K*sizeof(pthread_t)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	
	if ((th_cashiers = malloc(K*sizeof(pthread_t)))==NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	for (int i=0;i<K_start;i++) {
		if (pthread_create(&th_cashiers[i], NULL, Cashier, &datasm[i])!=0) {
			fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
		}
		MUTEX_LOCK(&cashiers_open, "cashiers open");
		n_cashiers_open++;
		MUTEX_UNLOCK(&cashiers_open, "cashiers open");	
	}
	pthread_t th_cSpawner;
	if (pthread_create (&th_cSpawner, NULL, Cashier_spawner, NULL)!=0) {
		fprintf(stderr, "ERROR: pthread_create\n");
		exit(EXIT_FAILURE);
	}
	/* init th_doorman */
	pthread_t th_doorman; // thread for handling customers entrance
	if (pthread_create(&th_doorman, NULL, Doorman, NULL)!=0) {
		fprintf(stderr, "pthread_create failed\n");
		exit(EXIT_FAILURE);
	}
	
	
	int n;
	char* answer_signal;
	int fd_listen;
	SYSCALL(fd_listen, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	
	struct sockaddr_un sa_signal;
	memset(&sa_signal, '0', sizeof(sa_signal));
	sa_signal.sun_family = AF_UNIX;
	strncpy(sa_signal.sun_path, SOCKNAME_SM, strlen(SOCKNAME_SM)+1);
	
	int notused;
	SYSCALL(notused, bind(fd_listen, (struct sockaddr*)&sa_signal, sizeof(sa_signal)), "bind");
	SYSCALL(notused, listen(fd_listen, 1), "listen");
	int fd_c;
	
	SYSCALL(fd_c, accept(fd_listen, (struct sockaddr*)NULL, NULL), "accept");
	printf("ACCEPTED SIGNAL SERVER\n");
	
	notification_t signal;
	while (1) {
		int n;
		SYSCALL(n, read_h(fd_c, &signal.len, sizeof(int)), "read_len");
		if (n==0) break;
		signal.info = calloc((signal.len), sizeof(char));
		if (!signal.info) {
			fprintf(stderr, "ERROR: calloc\n");
			break;
		}
		SYSCALL(n, read_h(fd_c, signal.info, (signal.len)*sizeof(char)), "read.info");
		if (n==0) break;
		printf("SIGNAL RECEIVED: %s\n", signal.info);
		if (strcmp(signal.info, "QUIT")==0) {
			answer_signal = "QUIT FINISHED";
			quit = 1;
			MUTEX_LOCK(&cashiers_open, "cashiers open");
			pthread_cond_signal(&cond_spawn_cashiers);
			MUTEX_UNLOCK(&cashiers_open, "cashiers open");
			break;
		}
		else if (strcmp(signal.info, "CLOSING")==0) {
			answer_signal = "CLOSING FINISHED";
			closing = 1;
			while (n_customers_in>0);
			printf("%d\n", n_customers_in);
			quit = 1;
			MUTEX_LOCK(&cashiers_open, "cashiers open");
			pthread_cond_signal(&cond_spawn_cashiers);
			MUTEX_UNLOCK(&cashiers_open, "cashiers open");
			break;
		}
	}
	free(signal.info);
	
	
	/* join th_doorman */
	if (pthread_join(th_doorman, NULL)==-1) {
		fprintf(stderr, "pthread_join failed\n");
		exit(EXIT_FAILURE);
	}
	/* join th_spawner */
	if (pthread_join(th_cSpawner, NULL)==-1) {
		fprintf(stderr, "ERROR: pthread_join \n");
		exit(EXIT_FAILURE);
	}
	/* join th_cashiers */
	for (int i=0;i<K;i++) {
		if (do_join[i]==1) {
		if (pthread_join(th_cashiers[i], NULL)==-1) {
			fprintf(stderr, "pthread_join failed\n");
			exit(EXIT_FAILURE);
		}
		}
	}
	
	writeLogFile();
	
	int len = strlen(answer_signal)+1;
	SYSCALL(n,write_h(fd_c, &len, sizeof(int)), "write_len");
	SYSCALL(n,write_h(fd_c, answer_signal, len), "write_anser");
	printf("Sent: %s\n", answer_signal);
	printf("SM QUIT\n");
	
	for (int i=0;i<K;i++) {
		if (datasm[i].t_customers!=NULL) {
			free_list(&datasm[i].t_customers);
			
		}
		if (datasm[i].t_close!=NULL) {
			free_list(&datasm[i].t_close);
		}
	}
	
	for (int i=0;i<K;i++) {
		free_queue(&c_queues[i]);
	}
	
	free(c_queues);
	free(datasm);
	free(do_join);
	free(c_status);
	free(status_lock);
	free(th_handlers);
	free(th_cashiers);
	close(fd_c);
	close(fd_listen);
	
	return 0;
}
