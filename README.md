# Operating Systems and Laboratory Project 2020/2021 - University of Pisa

This is the source code of Supermarket application, project of **Operating Systems and Laboratory** course of the BSc in Computer Science, University of Pisa.

## Overview
The project's purpose was to realize a simulation of a system that models a supermarket with $K$ cashiers and a variable number of customers. The supermarket is administrated by a Manager, which decides when to open/close a cash register (at least one must be always open). The number of customers allowed to enter the supermarket is limited, no more than $C$ at the same time. At startup, $C$ customers enter automatically and then when the number of customers goes below $C-E$ ($0 < E < C), $E$ new custuomers enter.

### Customer
Each customer spends a variable time $T$ inside the supermarket, then enters a queue associated to a specific cashier and waits for its turn to pay for its purchase. Periodically, each customer checks whether it is convinient for to change queue or not (namely, if there is a shorter queue). A customer can buy up to $P$ products ($P >= 0$).
Notice that customers with 0 products don't enter cashiers' queues, instead they have to wait for the manager permission before exiting the supermarket.

### Cashier
Each active cash register has its own cashier that manages the customers in its queue (FIFO order) in a specific service time. The service depends depends on two things: a constant different for each cashier, and the number of products of the customer.

The parameters influencing the supermarket behaviour (some already mentioned):
```text
K		    // number of cashiers 
K_start	    // number of cashiers at startup (K_start <= K)
C		    // number of customers inside
E		    // (C-E) threshold for bringing in E new customers
P		    // maximum number of products purchasable
T		    // maximum time for purchasing (for a customer)
T_product	// variable processing time for each product at the cashier
I		    // time interval for notifications from cashiers to manager
S		    // time interval after which a customer tires to change queue at checkout
S1		    // n_cashiers with at most 1 customer, in order to close a cashier
S2		    // n_customers a cashiers must have, in order to open a new cashier
LOG_FILE	// outpot log file
```
The default settings for the above parameters can be found in the file `./config/config.txt`-

The supermarket.c process simulates cashiers and customers behaviour with a multithread implementation and communication between `supermarket.c` and `and manager.c` processes is implemented with an AF_UNIX socket.


## Project Report
The implementation consists of 2 main files,
* *manager.c*
* *supermarket.c*

and 6 headers files contained in the *include/* directory,
* *cashier.h*
* *customer.h*
* *utils.h*
* *myqueue.h*
* *mylist.h*
* *mylist2.h*

an in-depth description of such files will be given in the following sections.

## Headers
The headers file contain both the definition and the implementation of the functions, so that a corresponding *<header-name>.c* file, with subsequent linking phase to a library, is not needed. In order to avoid multiple declarations, since some headers are used in more than one section of the project, the directives `ifndef - define - endif` have been used.

### cashier.h
Header file used in *supermarket.c*. It defines the struct *cashierArgs_h* in which we have the parameters/information related to a single cashier.

### customer.h
Header file used in *supermarket.* and *myqueue.h*. It defines the struct *customerArgs_h* in which we have parameters/information related to a single customer. In particular, three boolean parameters are set to 1 from the cashier when there is the need to communicate to the customer a specific event:

* **closing:** cashier is closing (need to change queue)
* **checkout:** customer is being served
* **paid:** customer has been served and can exit

### utils.h
Header file containing some macros, structs and several utility functions for a better code readability inside the two main files:

* **notification_t:** struct used for receving messages
* **SYSCALL:** macro for managing errors from system calls
* **write_h:** function for writing on socket
* **read_h:** function for reading from socket
* **unlink:** function for cleaning the socket

The ones remaining (i.e. *MUTEX_LOCK*, *MUTEX_UNLOCK*, *COND_WAIT* and *TH_JOIN*) are trivial by taking a look at their naming, they're respectively used for execution and error checks of *pthread_mutex_lock*, *pthread_mutex_unlock*, *pthread_cond_wait* and *pthread_join*.

### myqueue.h
Header file defining a FIFO queue used for representing the cashiers' queues.

### mylist.h
Header file defining a linkedlist used for storing the time of serving each customer and the time of closing/opening of each cashier.

### mylist2.h
Header file defining a second linkedlist, this one is used for storing all data of the customers entering the supermarket for generating the log file at the end.