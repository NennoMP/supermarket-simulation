#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/un.h>
#include <errno.h>

#ifndef BUFSIZE
#define BUFSIZE 64
#endif

#define UNIX_PATH_MAX 108
#define SOCKNAME_MANAGER "./toM_socket"
#define SOCKNAME_SM "./toSM_socket"

#define SYSCALL(r, sc, str) 	\
	if ((r=sc) == -1) { 	\
		perror(str); 	\
		exit(errno);	\
	}

#define ec_minus1(sc, str)	\
	if (sc != 0) {		\
		perror(str);	\
		exit(errno);	\
	}
	

typedef struct notification {
	int len;	// length
	char* info;	// information string
} notification_t;


/* mutex lock with error check */
void MUTEX_LOCK (pthread_mutex_t* mutex, char* str) {
	int error;
	if ((error=pthread_mutex_lock(mutex))!=0) {
		errno = error;
		fprintf(stderr, "ERROR: %s lock\n", str);
		pthread_exit((void*)EXIT_FAILURE);
	}
}
/* mutex unlock with error check */
void MUTEX_UNLOCK (pthread_mutex_t* mutex, char* str) {
	int error;
	if ((error=pthread_mutex_unlock(mutex))!=0) {
		errno = error;
		fprintf(stderr, "ERROR: %s unlock\n", str);
		pthread_exit((void*)EXIT_FAILURE);
	}
}
/* cond wait with error check */
void COND_WAIT (pthread_cond_t* condition, pthread_mutex_t* mutex, char* str) {
	int error;
	if ((error=pthread_cond_wait(condition,mutex))!=0) {
		errno = error;
		fprintf(stderr, "ERROR: %s cond wait\n", str);
		pthread_exit((void*)EXIT_FAILURE);
	}
}
/* thread join with error check */
void TH_JOIN (pthread_t thread, char* str) {
	int error;
	if ((error=pthread_join(thread, NULL))!=0) {
		errno= error;
		fprintf(stderr, "ERROR: %s join\n", str);
		pthread_exit((void*)EXIT_FAILURE);
	}
}
/* return time difference end-start */
long usec_diff (struct timeval start, struct timeval end) {
	long usec_cust_diff = ((end.tv_sec - start.tv_sec)*1000000) + (end.tv_usec - start.tv_usec);
	return usec_cust_diff;
}
/* read from socket */
static inline int read_h (long fd, void* buffer, size_t size) {
	int res;
	size_t left = size;
	char *bufptr = (char*)buffer;
	while(left>0) {
	if ((res=read((int)fd ,bufptr,left))==-1) {
		if (errno == EINTR) continue;
		return -1;
	}
	if (res==0) return 0;
	left    -= res;
	bufptr  += res;
    }
    return size;
}
/* write on socket */
static inline int write_h (long fd, void* buffer, size_t size) {
	int res;
	size_t left = size;
	char *bufptr = (char*)buffer;
	while(left>0) {
	if ((res=write((int)fd ,bufptr,left))==-1) {
		if (errno == EINTR) continue;
		return -1;
	}
	if (res==0) return 0;  
	left    -= res;
	bufptr  += res;
	}
	return 1;
}
/* clear sockets */
void cleanup (void) {
	unlink(SOCKNAME_MANAGER);
	unlink(SOCKNAME_SM);
}
#endif /* UTILS_H */	
