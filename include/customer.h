#ifndef CUSTOMER_H
#define CUSTOMER_H

/* customer arguments */
typedef struct customerArgs {
	int id;		// ID
	int n_products;	// n products purchased
	int n_qchange;		// n queue change
	
	int closing;		// (0: cashier open, 1: cashier closing)
	int checkout;		// (0: in queue, 1: checkout)
	int paid;		// (0: not paid, 1: paid)
	
	int t_buy;		// t for purchasing
	int t_queue;		// t spent in queue
	int t_change;		// interval for changin queue
} customerArgs_h;

#endif /* CUSTOMER_H */

