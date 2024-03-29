/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include <test.h>
#include "producerconsumer_driver.h"

/* Declare any variables you need here to keep track of and
   synchronise your bounded. A sample declaration of a buffer is shown
   below. You can change this if you choose another implementation. */

static struct pc_data buffer[BUFFER_SIZE];
int head;	// index for the head of the buffer.
int tail;	// index for the tail of the buffer.
static struct semaphore *buffer_empty;
static struct semaphore *buffer_full;
static struct semaphore *buffer_mutex;

/* consumer_receive() is called by a consumer to request more data. It
   should block on a sync primitive if no data is available in your
   buffer. */

struct pc_data consumer_receive(void)
{
        struct pc_data thedata;
	P(buffer_full);
	P(buffer_mutex);
	thedata = buffer[head];	
	head = (head+1) % BUFFER_SIZE;
	V(buffer_mutex);
	V(buffer_empty);
        return thedata;
}

/* procucer_send() is called by a producer to store data in your
   bounded buffer. */

void producer_send(struct pc_data item)
{
	P(buffer_empty);	
	P(buffer_mutex);
	tail = (tail+1) % BUFFER_SIZE;
	buffer[tail] = item;
	V(buffer_mutex);
	V(buffer_full);
}




/* Perform any initialisation (e.g. of global data) you need
   here. Note: You can panic if any allocation fails during setup */

void producerconsumer_startup(void)
{
	tail =0;
	head = 0;
	buffer_empty = sem_create("buffer_empty", BUFFER_SIZE);
	KASSERT(buffer_empty != 0); // panics when the buffer_empty is not initialized. 
	buffer_full = sem_create("buffer_full", 0);
	KASSERT(buffer_full != 0); // panics when the buffer_full is not initialized. 
	buffer_mutex = sem_create("buffer_mutex", 1);
	KASSERT(buffer_mutex != 0); // panics when the buffer_mutex is not initialized. 
}

/* Perform any clean-up you need here */
void producerconsumer_shutdown(void)
{
	sem_destroy(buffer_empty);
	sem_destroy(buffer_full);
	sem_destroy(buffer_mutex);
}

