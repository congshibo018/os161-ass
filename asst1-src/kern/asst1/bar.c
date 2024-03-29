#include <types.h>
#include <lib.h>
#include <synch.h>
#include <test.h>
#include <thread.h>

#include "bar.h"
#include "bar_driver.h"



/*
 * **********************************************************************
 * YOU ARE FREE TO CHANGE THIS FILE BELOW THIS POINT AS YOU SEE FIT
 *
 */
static struct barorder* order_queue[NCUSTOMERS];

int head, tail;
static struct semaphore *order_queue_empty;
static struct semaphore *order_queue_full;
static struct semaphore *queue_mutex;
static struct lock *bottle_lock;
/* Declare any globals you need here (e.g. locks, etc...) */


/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY CUSTOMER THREADS
 * **********************************************************************
 */

/*
 * order_drink()
 *
 * Takes one argument referring to the order to be filled. The
 * function makes the order available to staff threads and then blocks
 * until a bartender has filled the glass with the appropriate drinks.
 */

void order_drink(struct barorder *order)
{
        P(order_queue_empty);
	P(queue_mutex);
	tail = (tail+1) % NCUSTOMERS;
	order_queue[tail] = order;
	order->wait = sem_create("order_wait", 0);
	V(queue_mutex);	
	V(order_queue_full);
	if(order->go_home_flag != 1){
                P(order->wait);
	}
	sem_destroy(order->wait);
}



/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY BARTENDER THREADS
 * **********************************************************************
 */

/*
 * take_order()
 *
 * This function waits for a new order to be submitted by
 * customers. When submitted, it returns a pointer to the order.
 *
 */

struct barorder *take_order(void)
{
        P(order_queue_full);
	P(queue_mutex);
	struct barorder *ret = order_queue[head];
	head = (head+1) % NCUSTOMERS;
	V(queue_mutex);
	V(order_queue_empty);
        return ret;
}


/*
 * fill_order()
 *
 * This function takes an order provided by take_order and fills the
 * order using the mix() function to mix the drink.
 *
 * NOTE: IT NEEDS TO ENSURE THAT MIX HAS EXCLUSIVE ACCESS TO THE
 * REQUIRED BOTTLES (AND, IDEALLY, ONLY THE BOTTLES) IT NEEDS TO USE TO
 * FILL THE ORDER.
 */

void fill_order(struct barorder *order)
{

        /* add any sync primitives you need to ensure mutual exclusion
           holds as described */
	lock_acquire(bottle_lock);
        /* the call to mix must remain */
        mix(order);
	lock_release(bottle_lock);
}


/*
 * serve_order()
 *
 * Takes a filled order and makes it available to (unblocks) the
 * waiting customer.
 */

void serve_order(struct barorder *order)
{	
  if(order->go_home_flag == 1){
    return;
  }
  V(order->wait);
}



/*
 * **********************************************************************
 * INITIALISATION AND CLEANUP FUNCTIONS
 * **********************************************************************
 */


/*
 * bar_open()
 *
 * Perform any initialisation you need prior to opening the bar to
 * bartenders and customers. Typically, allocation and initialisation of
 * synch primitive and variable.
 */

void bar_open(void)
{
	head = 0; tail = -1;
	// create and Initialize semaphores
	order_queue_full = sem_create("order_queue_full", 0);
	KASSERT(order_queue_full != 0); // panics when the order_queue_full is not initialized.

	order_queue_empty = sem_create("order_queue_empty", NCUSTOMERS);
	KASSERT(order_queue_empty != 0); // panics when the order_queue_empty is not initialized.	

	queue_mutex = sem_create("queue_mutex", 1);
	KASSERT(queue_mutex != 0); // panics when the queue_mutex is not initialized.

	//create locks
	bottle_lock = lock_create("bottle lock");
        if (bottle_lock == NULL) {
                panic("no memory");
        }        
}

/*
 * bar_close()
 *
 * Perform any cleanup after the bar has closed and everybody
 * has gone home.
 */

void bar_close(void)
{
        sem_destroy(order_queue_full);
	sem_destroy(order_queue_empty);
	sem_destroy(queue_mutex);
	lock_destroy(bottle_lock);
}

