//Wentao Guo (wgtc2015, wguo) and Colin Ferris (clf02014, cferris)

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 10

//Global declarations for pthread synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t has_value = PTHREAD_COND_INITIALIZER;
pthread_cond_t has_space = PTHREAD_COND_INITIALIZER;

//Struct for messages
typedef struct message
{
  int value;          /* value to be passed to consumer */
  int consumer_sleep; /* time (in ms) for consumer to sleep */
  int line;           /* line number in input file */
  int print_code;     /* output code */
  int quit;           /* non-zero if consumer should exit */
} message_t;

//Ring buffer variables; ring buffer implemented using array
message_t buffer[BUFFER_SIZE];
int rear, front;

//Enqueues a message on the ring buffer
void enqueue(message_t *msg)
{
  //Mutex lock
  pthread_mutex_lock(&mutex);

  //Wait if ring buffer is full
  while ((rear == front -1) || (rear == BUFFER_SIZE -1 && front == 0))
  {
    pthread_cond_wait(&has_space, &mutex);
  }

  //Update rear and front indices of ring buffer if empty
  if(front == -1)
  {
    front = 0;
    rear = 0;
  }
  //Update rear index of ring buffer if not empty
  else
  {
    rear += 1;
    if(rear >= BUFFER_SIZE)
    {
      rear = 0;
    }
  }

  //Enqueue message in ring buffer
  buffer[rear] = *msg;

  //Mutex unlock and signal that the ring buffer has a value in it
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&has_value);

  return;
}

//Dequeues a message from the ring buffer
message_t dequeue()
{
  //Mutex lock
  pthread_mutex_lock(&mutex);

  //Wait if ring buffer is empty
  while (front == -1)
  {
    pthread_cond_wait(&has_value, &mutex);
  }

  //Hold value at front of ring buffer
  message_t msg = buffer[front];

  //Update front and rear indices of ring buffer if it is about to be empty
  if(front == rear)
  {
    front = -1;
    rear = -1;
  }
  //Update front index of ring buffer if it still contains at least one message
  else
  {
    front += 1;
    if(front >= BUFFER_SIZE)
    {
        front = 0;
    }
  }

  //Mutex unlock and signal that the ring buffer is not full
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&has_space);

  //Return dequeued message
  return msg;
}


//Sleeps for sleep_time milliseconds, using the nanosleep function
void sleep_wrapper(int sleep_time)
{
  //Create a timespec struct from sleep_time
  struct timespec tim;
  tim.tv_sec = sleep_time / 1000;
  tim.tv_nsec = (sleep_time % 1000) * 1000000;

  //Sleep and check for error
  if(nanosleep(&tim, NULL) < 0)
  {
    printf("Error: sleep call failed to execute properly.\n");
    exit(1);
  }

  return;
}



//Producer thread will read in values and enqueue messages on the ring buffer
void *producer_thread(void *vargp)
{
  //Variables to hold values from input
  int consumer_val, producer_sleep, consumer_sleep, print_code;

  //Begin counting lines at 1
  int line = 1;

  //Loop to process values from input and enqueue messages on the ring buffer
  while (scanf("%d %d %d %d", &consumer_val, &producer_sleep, &consumer_sleep, &print_code) == 4)
  {
    //Sleep if necessary
    if(producer_sleep != 0)
    {
      sleep_wrapper(producer_sleep);
    }

    //Generate message and enqueue
    message_t msg = {.value  = consumer_val, .consumer_sleep = consumer_sleep, .line = line, .print_code = print_code, .quit = 0};
    enqueue(&msg);

    //Print status report if input instructs to do so
    if(print_code == 1 || print_code == 3)
    {
      printf("Producer: value %d from input line %d\n", consumer_val, line);
    }

    line++;
  }

  //Check if end of input file has been reached
  if(scanf("%d %d %d %d", &consumer_val, &producer_sleep, &consumer_sleep, &print_code) == EOF)
  {
    //Generate message instructing consumer to quit, and enqueue once for each consumer
    message_t msg = {.value  =0, .consumer_sleep = 0, .line = 0, .print_code = 0, .quit = 1};
    enqueue(&msg);
    enqueue(&msg);
  }
  //Print error and exit if problem reading input
  else
  {
    printf("Error: problem reading input.\n");
    exit(1);
  }

  return NULL;
}

//Consumer thread will read return sum
void *consumer_thread(void *vargp)
{
  //Local variable for accumulating sum
  int sum = 0;
  //Argument that first contains index of consumer and later will contain sum
  int *consumer_sum = ((int *)vargp);

  //Variable to hold messages as consumer dequeues from the ring buffer
  message_t msg;

  //Loop to process messages from the ring buffer
  do
  {
    //Dequeue a message from the ring buffer
    msg = dequeue();

    //Quit if message instructs to do so
    if(msg.quit != 0)
    {
      printf("Consumer %d: final sum is %d\n", *consumer_sum, sum);
      *consumer_sum = sum;
      return NULL;
    }

    //Sleep if necessary
    if(msg.consumer_sleep != 0)
    {
      sleep_wrapper(msg.consumer_sleep);
    }

    //Add the message's value to the sum
    sum += msg.value;

    //Print a status report if message instructs to do so
    if(msg.print_code == 2 || msg.print_code == 3)
    {
      printf("Consumer %d: %d from input line %d; sum = %d\n", *consumer_sum, msg.value, msg.line, sum);
    }
  } while(msg.quit == 0);

  return NULL;
}

int main(int argc, char* argv[])
{
  setlinebuf(stdout);

  //Variables that will first contain indices of consumers and eventually
  //contain sums of consumers
  int consumer_zero_sum, consumer_one_sum;
  consumer_zero_sum = 0;
  consumer_one_sum = 1;

  //Front and rear values of ring buffer (initialized to -1 for empty buffer)
  front = -1;
  rear = -1;

  //Initialize two consumer threads and one producer thread
  pthread_t consumer_zero, consumer_one, producer;
  if(pthread_create(&consumer_zero, NULL, consumer_thread, &consumer_zero_sum) != 0 ||
  pthread_create(&consumer_one, NULL, consumer_thread, &consumer_one_sum) != 0 ||
  pthread_create(&producer, NULL, producer_thread, NULL) != 0)
  {
    printf("Error: failure to create thread.\n");
    exit(1);
  }

  //Wait for all three threads to terminate
  if(pthread_join(consumer_zero, NULL) != 0 ||
  pthread_join(consumer_one, NULL) != 0 ||
  pthread_join(producer, NULL) != 0)
  {
    printf("Error: failure to join thread.\n");
    exit(1);
  }

  //Print total sum logged by consumer_one and consumer_zero
  printf("Main: total sum is %d\n", (consumer_one_sum + consumer_zero_sum));

  exit(0);
}
