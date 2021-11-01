#ifndef MYLIST2_H
#define MYLIST2_H

#include <stdlib.h>

/* Linkedlist of customersdata for writing log file */

typedef struct customerAnalytics {
	int id;	// ID
	int np;	// products
	int nq;	// queue changes
	int tq;	// time queue
	int tsm;	// time supermarket
	int served;	// (0: not served, 1: served)
} customerAnalytics_h;

typedef struct element {
	customerAnalytics_h data;
	struct element* next;
} el_customerData;
typedef el_customerData* list_customerData;

/* add element */
void put_customerData (list_customerData* list, customerAnalytics_h customer) {
	list_customerData aux= *list;
	
	if (aux==NULL) {
		(*list)		= (list_customerData)malloc(sizeof(el_customerData));
		(*list)->data.id	= customer.id;
		(*list)->data.np	= customer.np;
		(*list)->data.nq	= customer.nq;
		(*list)->data.tq	= customer.tq;
		(*list)->data.tsm	= customer.tsm;
		(*list)->data.served	= customer.served;
		(*list)->next		= NULL;
	}
	else {
		while (aux->next!=NULL) {
			aux 	= aux->next;
		}
		aux->next		= (list_customerData)malloc(sizeof(el_customerData));
		aux			= aux->next;
		aux->data.id		= customer.id;
		aux->data.np		= customer.np;
		aux->data.nq		= customer.nq;
		aux->data.tq		= customer.tq;
		aux->data.tsm		= customer.tsm;
		aux->data.served 	= customer.served;
		aux->next		= NULL;
	}
}

/* return length */
int length_list2 (list_customerData list) {
	list_customerData aux = list;
	int len= 0;
	
	while (aux!=NULL) {
		len++;
		aux= aux->next;
	}
	return len;
}

/* list to array for qsort */
void listToArray (list_customerData list, int len, customerAnalytics_h* array) {
	int i= 0;
	while (list!=NULL) {
		(array[i]).id = list->data.id;
		(array[i]).np = list->data.np;
		(array[i]).nq = list->data.nq;
		(array[i]).tq = list->data.tq;
		(array[i]).tsm = list->data.tsm;
		(array[i]).served = list->data.served;
		i++;
		list = list->next;
	}
}

/* compare function for qsort */
int compare_analytics (const void * a, const void * b) {
	customerAnalytics_h* ca = (customerAnalytics_h*)a;
	customerAnalytics_h* cb = (customerAnalytics_h*)b;
	
	return (ca->id - cb->id);
}

/* free space */
void free_customerData (list_customerData* list) {
	list_customerData curr = *list;
	list_customerData next;
	
	while (curr!=NULL) {
		next = curr->next;
		free(curr);
		curr = next;
	}
	*list = NULL;
}

#endif /* MYLIST2_H */
