#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <assert.h>
#include <signal.h>
#include <semaphore.h>

#include "./include/utils.h"

/* config values */
int S1;
int S2;

/* auxiliary values */
static volatile sig_atomic_t quit	= 0;	// SIGQUIT
static volatile sig_atomic_t closing	= 0;	// SIGHUP
int counterS1 = 0;				// counter for S1 (closing cashier)
sem_t interrupt_sem;				// semaphore for starting th_signal

/* threads */
pthread_t th_listener;

/* mutex */
static pthread_mutex_t mutex_counter = PTHREAD_MUTEX_INITIALIZER;	// mutex for counterS1

/* handler SIGQUIT */
void quit_handler(int sig) {
    quit = 1;
    write(1, "SIGQUIT catched\n", 16);
    sem_post(&interrupt_sem);
}
/* handler SIGHUP */
void hup_handler(int sig) {
    closing = 1;
    write(1, "SIGHUP catched\n", 15);
    sem_post(&interrupt_sem);
}
/* utility function for setting handlers */
void set_sigHandler(void) {
    sigset_t set;
    struct sigaction sa;

    if (sigfillset(&set) != 0) {
        perror("error during filling set");
        exit(EXIT_FAILURE);
    }

    pthread_sigmask(SIG_SETMASK, &set, NULL);

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = quit_handler;
    if(sigaction(SIGQUIT, &sa, NULL) != 0) {
        perror("error during set sigaction");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = hup_handler;
    if(sigaction(SIGHUP, &sa, NULL) != 0) {
        perror("error during set sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigfillset(&set) != 0) {
        perror("error during filling set");
        exit(EXIT_FAILURE);
    }

    if (sigdelset(&set, SIGQUIT) != 0) {
        perror("error during unmasking SIGQUIT");
        exit(EXIT_FAILURE);
    }

    if (sigdelset(&set, SIGHUP) != 0) {
        perror("error during unmasking HUP");
        exit(EXIT_FAILURE);
    }

    pthread_sigmask(SIG_SETMASK, &set, NULL);
}
/* utility function for loading default values from config.txt */
void parse_config (void) {
	FILE* fd;
	if ((fd=fopen("./config/config.txt", "r"))==NULL) {
		perror("fopen");
		fprintf(stderr, "cannot open config.txt\n");
		exit(EXIT_FAILURE);
	}
	int value;
	char buff[BUFSIZE];
	char* line;
	while (fgets(buff, sizeof(buff), fd)!=NULL) {
		if (strstr(buff, "S1=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			S1	= value;
		}
		if (strstr(buff, "S2=")) {
			line	= strstr(buff, "=");
			value	= atoi(&line[1]);
			S2 = value;
		}
	}
	fclose(fd);
}

/* th func for handling notifications */
void* Handler (void* args) {
	assert(args);
	long fd_c = (long)args;
	notification_t message;
	int n;
	
	while (!quit) {
		SYSCALL(n, read_h(fd_c, &message.len, sizeof(int)), "read_len");
		message.info = calloc((message.len), sizeof(char));
		if (!message.info) {
			fprintf(stderr, "ERROR: calloc\n");
			pthread_exit((void*)EXIT_FAILURE);
		}
		SYSCALL(n, read_h(fd_c, message.info, (message.len)*sizeof(char)), "read_info");
		if (strstr(message.info, "Customer")!=NULL) { // customer message
			printf("Received: %s\n", message.info);
			char* answer= "OK";
			int len= strlen(answer)+1;
			SYSCALL(n, write_h(fd_c, &len, sizeof(int)), "write_len");
			SYSCALL(n, write_h(fd_c, answer, len), "write_answer");
			close(fd_c);
			break;
		}
		else if (strstr(message.info, "Cashier")) { // cashier message
			printf("Received: %s\n", message.info);
			int qlen;
			char delim[] = " ";
			char* res = strtok(message.info, delim);
			while (res!=NULL) {
				char* line = strstr(res, ":");
				res = strtok(NULL, delim);
				if (res==NULL) {
					qlen = atoi(&line[1]);
				}
			}
			char* answer;
			if (qlen>=S2) {
				answer = "Add Cashier";
				int len = strlen(answer)+1;	
				SYSCALL(n,write_h(fd_c, &len, sizeof(int)), "write_len");
				SYSCALL(n,write_h(fd_c, answer, len), "write_anser");
				printf("Sent: %s\n", answer);
			}
			else if (qlen<=1) {
				MUTEX_LOCK(&mutex_counter, "manager signal");
				counterS1++;
				if (counterS1>=S1) {
					counterS1--;
					answer = "Close Cashier";
					int len = strlen(answer)+1;
					SYSCALL(n,write_h(fd_c, &len, sizeof(int)), "write_len");
					SYSCALL(n,write_h(fd_c, answer, len), "write_anser");
					printf("Sent: %s\n", answer);
				}
				MUTEX_UNLOCK(&mutex_counter, "manager signal");
			}
			else {
				answer = "OK";
				int len = strlen(answer)+1;	
				SYSCALL(n,write_h(fd_c, &len, sizeof(int)), "write_len");
				SYSCALL(n,write_h(fd_c, answer, len), "write_anser");
			}
		}
	}
	close(fd_c);
	pthread_exit((void*)EXIT_SUCCESS);
}
/* utility function for spawning thread each connection */
void spawn_handler (long fd_c) {
	pthread_attr_t thattr;
	pthread_t th;
	
	if (pthread_attr_init(&thattr)!=0) {
		fprintf(stderr, "ERROR: pthread_attr_init\n");
		close(fd_c);
		return;
	}
	if (pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_DETACHED)!=0) {
		fprintf(stderr, "ERROR: th detached\n");
		pthread_attr_destroy(&thattr);
		close(fd_c);
		return;
	}
	if (pthread_create(&th, &thattr, Handler, (void*)fd_c)!=0) {
		fprintf(stderr, "ERROR: pthread_create\n");
		pthread_attr_destroy(&thattr);
		close(fd_c);
		return;
	}
}
/* th_listener func */
void* Listener (void* args) {
	int fd_listen;
	
	SYSCALL(fd_listen, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	
	struct sockaddr_un sa;
	memset(&sa, '0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, SOCKNAME_MANAGER, strlen(SOCKNAME_MANAGER)+1);
	
	int notused;
	SYSCALL(notused, bind(fd_listen, (struct sockaddr*)&sa, sizeof(sa)), "bind");
	SYSCALL(notused, listen(fd_listen, SOMAXCONN), "listen");
	
	while (!quit) {
		long fd_c;
		SYSCALL(fd_c, accept(fd_listen, (struct sockaddr*)NULL, NULL), "accept");
		spawn_handler(fd_c);
	}
	printf("LISTENER CLOSED\n");
	pthread_exit((void*)EXIT_SUCCESS);
}
/* th_signal func */
void* Signal (void* args) {
	int fd_signal, n;
	struct sockaddr_un addr_signal;
	memset(&addr_signal, '0', sizeof(addr_signal));
	addr_signal.sun_family = AF_UNIX;
	strncpy(addr_signal.sun_path, SOCKNAME_SM, strlen(SOCKNAME_SM)+1);

	SYSCALL(fd_signal, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	
	while (connect(fd_signal, (struct sockaddr*)&addr_signal, sizeof(addr_signal))!=0) {
		if (errno!=ENOENT && errno!=ECONNREFUSED) {
			perror("connect signal");
			fprintf(stderr, "ERROR: connect th_signal\n");
			pthread_exit((void*)EXIT_FAILURE);
		}
	}
	printf("Th_signal CONNECTED\n");
	
	char* signal;
	if (quit) {
		signal = "QUIT";
	}
	if (closing) {
		signal = "CLOSING";
	}
	int len = strlen(signal)+1;
	SYSCALL(n,write_h(fd_signal, &len, sizeof(int)), "write_len");
	SYSCALL(n,write_h(fd_signal, signal, len), "write_anser");
	notification_t answer;
	
	printf("SIGNAL SENT: %s\n", signal);
	
	
	while (1) {
	SYSCALL(n, read_h(fd_signal, &answer.len, sizeof(int)), "read_len");
	if (n==0) break;
	answer.info = calloc((answer.len), sizeof(char));
	if (!answer.info) {
		fprintf(stderr, "ERROR: calloc\n");
		break;
	}
	SYSCALL(n, read_h(fd_signal, answer.info, (answer.len)*sizeof(char)), "read_info");
	if (n==0) break;
	}
	printf("MANAGER SIGNAL RECEIVED: %s\n", answer.info);
	quit = 1;
	free(answer.info);
	close(fd_signal);
	pthread_cancel(th_listener);
	printf("SIGNAL CLOSED\n");
	pthread_exit((void*)EXIT_SUCCESS);
}

int main (int argc, char* argv[]) {
	// delete socket if exists
	cleanup();
	atexit(cleanup);
	parse_config();
	
	set_sigHandler();
	
	if (argc!=2) {
		fprintf(stderr, "Use: %s <test_case>\n", argv[0]);
		return -1;
	}
	int test = strtol(argv[1], NULL, 10);
	
	printf("MANAGER STARTED...\n");
	
	/* threads for handling customers and cashiers */
	if (pthread_create(&th_listener, NULL, Listener, NULL)!=0) {
		fprintf(stderr, "ERROR: pthread_create\n");
		exit(EXIT_FAILURE);
	}
	printf("LISTENER STARTED...\n");
	
	if (test==2) {
		int pid= fork();
		if (pid==0) { // child
			static char* argv[]={"supermarket", "6", "50", "3", "200", "100", "20", NULL};
			execv("./supermarket",argv);
			exit(EXIT_FAILURE); // only if execv fails
		}
		else { // father
			waitpid(pid, 0, 0);
		}
	}
	
	/* wait interrupt before starting th_signal */
	sem_wait(&interrupt_sem);
	pthread_t th_signal;
	if (pthread_create(&th_signal, NULL, Signal, NULL)!=0) {
		fprintf(stderr, "ERROR: pthread_create\n");
		exit(EXIT_FAILURE);
	}
	printf("TH_SIGNAL STARTED...\n");
	

	if (pthread_join(th_listener, NULL)!=0) {
		fprintf(stderr, "ERROR: pthread_join (th_Listener)\n");
		exit(EXIT_FAILURE);
    	}
    	if (pthread_join(th_signal, NULL)!=0) {
    		fprintf(stderr, "ERROR: pthread_join (th_Signal)\n");
    		exit(EXIT_FAILURE);
    	}
	
	printf("MANAGER CLOSED\n");
	
	return 0;
}
