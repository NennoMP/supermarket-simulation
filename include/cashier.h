#ifndef CASHIER_H
#define CASHIER_H

#include "mylist.h"

/* cashier arguments */
typedef struct cashierArgs {
	int id;			// ID
	int n_customers;		// n customers served
	int n_queue;			// n customers in queue
	int n_products;		// n products handled
	int n_close;			// n open-close
	
	long t_service;		// time (random) service
	int t_product;			// time (unchanged) each product
	ListaDiElementi t_close;	// list time each open-close
	ListaDiElementi t_customers;	// list time each customer
} cashierArgs_h;
#endif /* CASHIER_H */
