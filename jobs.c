// ------------
// This code is provided solely for the personal and private use of
// students taking the CSC369H5 course at the University of Toronto.
// Copying for purposes other than this use is expressly prohibited.
// All forms of distribution of this code, whether as given or with
// any changes, are expressly prohibited.
//
// Authors: Bogdan Simion
//
// All of the files in this directory and all subdirectories are:
// Copyright (c) 2019 Bogdan Simion
// -------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "executor.h"

extern struct executor tassadar;

/**
 * Populate the job lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <type> <num_resources> <resource_id_0> <resource_id_1> ...
 *
 * Each job is added to the queue that corresponds with its job type.
 */
void parse_jobs(char *file_name)
{
    int id;
    struct job *cur_job;
    struct admission_queue *cur_queue;
    enum job_type jtype;
    int num_resources, i;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int *)&jtype, (int *)&num_resources) == 3)
    {

        /* construct job */
        cur_job = malloc(sizeof(struct job));
        cur_job->id = id;
        cur_job->type = jtype;
        cur_job->num_resources = num_resources;
        cur_job->resources = malloc(num_resources * sizeof(int));

        int resource_id;
        for (i = 0; i < num_resources; i++)
        {
            fscanf(f, "%d ", &resource_id);
            cur_job->resources[i] = resource_id;
            tassadar.resource_utilization_check[resource_id]++;
        }

        assign_processor(cur_job);

        /* append new job to head of corresponding list */
        cur_queue = &tassadar.admission_queues[jtype];
        cur_job->next = cur_queue->pending_jobs;
        cur_queue->pending_jobs = cur_job;
        cur_queue->pending_admission++;
    }

    fclose(f);
}

/*
 * Magic algorithm to assign a processor to a job.
 */
void assign_processor(struct job *job)
{
    int i, proc = job->resources[0];
    for (i = 1; i < job->num_resources; i++)
    {
        if (proc < job->resources[i])
        {
            proc = job->resources[i];
        }
    }
    job->processor = proc % NUM_PROCESSORS;
}

void do_stuff(struct job *job)
{
    /* Job prints its id, its type, and its assigned processor */
    printf("%d %d %d\n", job->id, job->type, job->processor);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the executor
 * before any jobs start coming
 * 
 */
void init_executor()
{

    for (int i = 0; i < NUM_QUEUES; i++)
    {
        struct admission_queue aq;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        aq.lock = lock;
        pthread_cond_t a_cv = PTHREAD_COND_INITIALIZER;
        pthread_cond_t e_cv = PTHREAD_COND_INITIALIZER;
        aq.admission_cv = a_cv;
        aq.execution_cv = e_cv;
        aq.pending_admission = 0;
        aq.num_admitted = 0;
        aq.pending_jobs = NULL;
        aq.admitted_jobs = malloc(sizeof(struct job *) * QUEUE_LENGTH);
        aq.head = 0;
        aq.tail = 0;
        aq.capacity = QUEUE_LENGTH;
        tassadar.admission_queues[i] = aq;
    }

    for (int i = 0; i < NUM_RESOURCES; i++)
    {
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        tassadar.resource_locks[i] = lock;
        tassadar.resource_utilization_check[i] = 0;
    }

    for (int i = 0; i < NUM_PROCESSORS; i++)
    {
        struct processor_record pr;
        pr.completed_jobs = NULL;
        pr.num_completed = 0;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pr.lock = lock;
        tassadar.processor_records[i] = pr;
    }
}

/**
 * TODO: Fill in this function
 *
 * Handles an admission queue passed in through the arg (see the executor.c file). 
 * Bring jobs into this admission queue as room becomes available in it. 
 * As new jobs are added to this admission queue (and are therefore ready to be taken
 * for execution), the corresponding execute thread must become aware of this.
 * 
 */
void *admit_jobs(void *arg)
{
    struct admission_queue *q = arg;
    pthread_mutex_lock(&(q->lock)); // we use the lock in order to protect the admission_queue values

    // We use a check to see if its the first time in the while loop to protect the while loop argument
    int first = 1;

    while (q->pending_admission != 0)
    {
        if (first)
        { // Only lock if its not already locked from before
            first = 0;
        }
        else
        {
            pthread_mutex_lock(&(q->lock)); // We use the lock in order to protect the admission_queue values
        }

        if (q->num_admitted == q->capacity -1)
        {                //if the number of admitted jobs is at capacity, then wait until there is room
            q->tail = 0; // if the list of jobs is at capacity, then circle back around to the front
            pthread_cond_wait(&q->admission_cv, &(q->lock));
        }

        q->admitted_jobs[q->tail] = q->pending_jobs; // Queue the next pending job
        q->pending_jobs = q->pending_jobs->next;     // Move to the next pending job

        q->tail++;              // Increment index counter since length of list increased by 1
        q->num_admitted++;      // Since a job was admitted, increment counter
        q->pending_admission--; // and decrement the number of jobs pending

        pthread_cond_signal(&(q->execution_cv)); // signals to the corresponding execution thread that there is a job admitted into the queue

        pthread_mutex_unlock(&(q->lock)); //Unlock as we are done using this admission queue for now
    }

    return NULL;
}

/**
 * 
 * This function handles the processor information that will be used for testing
 * 
*/
void proc_Info(struct job *job)
{
    //  processor_record lock to protect values in the processor 
    pthread_mutex_lock(&(tassadar.processor_records[job->processor].lock));

    // The idea is to make the new completed job the head of the completed jobs list instead of traversing through the whole list
    // So the next value of this job will be the other completed jobs
    job->next = tassadar.processor_records[job->processor].completed_jobs;

    // Make the head of the list this new completed job and then increment the number of completed jobs
    tassadar.processor_records[job->processor].completed_jobs = job;
    tassadar.processor_records[job->processor].num_completed++;

    pthread_mutex_unlock(&(tassadar.processor_records[job->processor].lock));
}
/**
 * TODO: Fill in this function
 *
 * Moves jobs from a single admission queue of the executor. 
 * Jobs must acquire the required resource locks before being able to execute. 
 *
 * Note: You do not need to spawn any new threads in here to simulate the processors.
 * When a job acquires all its required resources, it will execute do_stuff.
 * When do_stuff is finished, the job is considered to have completed.
 *
 * Once a job has completed, the admission thread must be notified since room
 * just became available in the queue. Be careful to record the job's completion
 * on its assigned processor and keep track of resources utilized. 
 *
 * Note: No printf statements are allowed in your final jobs.c code, 
 * other than the one from do_stuff!
 */
void *execute_jobs(void *arg)
{
    struct admission_queue *q = arg;

    while (q->pending_admission != 0 || q->num_admitted > 0)
    { // only activate if there is a pending job or there is a job in the queue

        pthread_mutex_lock(&(q->lock)); // We use the lock in order to protect the admission_queue values

        if (q->num_admitted == 0)
        { //if the number of admitted jobs is zero, then wait until there is a job to execute
            pthread_cond_wait(&q->execution_cv, &(q->lock));
        }

        for (int i = 0; i < q->admitted_jobs[q->head]->num_resources; i++)
        { // Lock for every resource that this job requires which will allow this job to execute safely
            pthread_mutex_lock(&(tassadar.resource_locks[q->admitted_jobs[q->head]->resources[i]]));
        }

        do_stuff(q->admitted_jobs[q->head]);  // Excute the job
        proc_Info(q->admitted_jobs[q->head]); // Add the processor information

        for (int i = 0; i < q->admitted_jobs[q->head]->num_resources; i++)
        {
            // Decrement the used resources from the check
            tassadar.resource_utilization_check[q->admitted_jobs[q->head]->resources[i]]--;

            //Unock every resource that this job used to allow other jobs to execute
            pthread_mutex_unlock(&(tassadar.resource_locks[q->admitted_jobs[q->head]->resources[i]]));
        }

        if (q->head == q->capacity - 1)
        { // if the list of jobs is at capacity, then circle back around to the front
            q->head = 0;
        }
        else
        {
            q->head++; // Increment the head for the next admitted job
        }

        q->num_admitted--; // Since a job was executed, decrement counter

        pthread_cond_signal(&(q->admission_cv)); // signals to the corresponding execution thread that there is a job admitted into the queue
        pthread_mutex_unlock(&(q->lock));
    }
    return NULL;
}
