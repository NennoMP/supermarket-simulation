#ifndef MYQUEUE_H
#define MYQUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "customer.h"

/* FIFO queue for queues of cashiers */

typedef struct q_element {
	customerArgs_h* customer;
	struct q_element* next;
} q_element_h;

typedef struct queue {
	int len;
	q_element_h* first;
	q_element_h* last;
	pthread_mutex_t* mutex;
	pthread_cond_t*  empty;
} queue_h;



/* init queue */
int init_queue (queue_h* q) {
	q->len		= 0;
	q->first	= NULL;
	q->last	= NULL;
	
	q->mutex= (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(q->mutex, NULL)!=0) {
		fprintf(stderr, "ERROR: queue mutex\n");
		return -1;
	}
	q->empty= (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	if (pthread_cond_init(q->empty, NULL)!=0) {
		fprintf(stderr, "ERROR: queue cond full\n");
		return -1;
	}
	return 0;
}

/* is empty */
int isEmpty_queue (queue_h q) {
	int r = 0;
	if (pthread_mutex_lock(q.mutex) != 0) {
	        fprintf(stderr, "ERROR: queue mutex lock\n");
	        return -1;
    	}
    	if (q.first==NULL) r= 1;
    	if (pthread_mutex_unlock(q.mutex) != 0) {
        	fprintf(stderr, "ERROR: queue mutex unlock\n");
        	return -1;
    	}
    	return r;
}
/* add element */
int push_queue (queue_h* q, customerArgs_h** c) {
	if (pthread_mutex_lock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex lock\n");
		return -1;
	}
	if ((*q).first==NULL) { // empty
		q->first		= malloc(sizeof(queue_h));
		q->first->customer	= (*c);
		q->first->next		= NULL;
		q->last		= q->first;	
	}
	else { // not empty
		q->last->next		= malloc(sizeof(queue_h));
		q->last		= q->last->next;
		q->last->customer	= (*c);
		q->last->next		= NULL;
	}
	q->len++;
	if (pthread_cond_signal(q->empty)!=0) {
		fprintf(stderr, "ERROR: queue cond signal\n");
		return -1;
	}
	if (pthread_mutex_unlock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex unlock\n");
		return -1;
	}
	return 0;
}
/* take first customer */
customerArgs_h* takeCustomer (queue_h* q) {
	if (isEmpty_queue(*q)) {
		return NULL;
	}
	pthread_mutex_lock(q->mutex);
	q_element_h* el = q->first;
	customerArgs_h* customer = el->customer;
	pthread_mutex_unlock(q->mutex);
	return customer;
}

/* remove element */
int pop_queue (queue_h* q) {
	q_element_h* tmp;
	
	if (isEmpty_queue(*q)) {
		return -1;
	}
	if (pthread_mutex_lock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex lock\n");
		exit(EXIT_FAILURE);
	}
	tmp = q->first;
	q->first = q->first->next;
	q->len--;
	if (q->first==NULL) {
		q->last= NULL;
	}
	free(tmp);
	if (pthread_mutex_unlock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex unlock\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}


/* return length */
int length_queue (queue_h q) {
	if (q.len==0) return 0;
	if (pthread_mutex_lock(q.mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex lock\n");
		return -1;
	}
	int length = q.len;
	if (pthread_mutex_unlock(q.mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex unlock\n");
		return -1;
	}
	return length;
}

/* remove customer with myid */
int removeSpecific (queue_h* q, int myid) {
	q_element_h* tmp;
	
	if (isEmpty_queue(*q)) {
		return -1;
	} 
	
	if (pthread_mutex_lock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex lock\n");
		exit(EXIT_FAILURE);
	}
	
	q_element_h* prec = NULL;
	tmp = q->first;
	while (tmp->customer->id!=myid) {
		prec = tmp;
		tmp = tmp->next;
	}
		if (tmp->next==NULL) {
			q->last = prec;
			q->last->next= NULL;
			tmp->next = NULL;
			free(tmp);
			q->len--;
		}
		else if (prec==NULL) {
			q->first= q->first->next;
			tmp->next = NULL;
			free(tmp);
			q->len--;
		}
		else {
			prec->next = tmp->next;
			tmp->next = NULL;
			free(tmp);
			q->len--;
		}
	if (pthread_mutex_unlock(q->mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex unlock\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}
/* return number of customers before me */
int nBeforeMe (queue_h q, int myid) {
	if (pthread_mutex_lock(q.mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex lock\n");
		return -1;
	}
	int n_before = 0;
	q_element_h* tmp = q.first;
	while (tmp->customer->id!=myid) {
		tmp = tmp->next;
		n_before++;
	}
	if (pthread_mutex_unlock(q.mutex)!=0) {
		fprintf(stderr, "ERROR: queue mutex unlock\n");
		return -1;
	}
	return n_before;
}

/* get ID */
int getID (queue_h q) {
	return q.first->customer->id;
}
/* get number of products */
int getProducts (queue_h q) {
	return q.first->customer->n_products;
}
/* get paid flag */
int getPaid (queue_h q) {
	return q.first->customer->paid;
}
/* set paid flag to 1 */
void setPaid (queue_h* q) {
	if (q->first->customer!=NULL) q->first->customer->paid= 1;
}
/* set checkout flag to 1 */
void setCheckout (queue_h* q) {
	if (q->first->customer!=NULL) q->first->customer->checkout= 1;
}
/* set closing flag to 1 */
void setClosing (queue_h* q) {
	if (q->first->customer!=NULL) q->first->customer->closing= 1;
}
/* free space */
int free_queue (queue_h* q) {

	while (q->first!=NULL) {
		pop_queue(q);
	}

	free(q->mutex);
	free(q->empty);
	return 0;
}

#endif /* MYQUEUE_H */
