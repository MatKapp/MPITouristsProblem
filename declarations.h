#ifndef DECLARATIONS_H
#define DECLARATIONS_H

#include <mpi.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

typedef enum { false, true } bool;

#define FOR_GROUP 0
#define LAMPORT_TIME 0
#define REQUEST_TYPE 1
#define PROCESS_ID 2
#define REQUEST_ID 3
#define ACK_TAG 0
#define NACK_TAG 1
#define REQUEST_TAG 2
#define GROUP_SIZE 2
#define GROUP_ID 4
#define MESSAGE_SIZE 5
#define PENDING_REQUESTS_SIZE 100
#define PENDING_REQUEST_REQUEST_TYPE 1
#define PENDING_REQUEST_MPI_SOURCE 2
#define ADDITIONAL_MESSAGE_SIZE 2


int myRequestId = 0;
int numberOfGroups = 2;
int lamportTime = INT_MAX;
int requestLamportTime;
bool hasGroup = false;
int processId, worldSize;
int numberOfElementsToSend = 0;
int receivedAcks = 0;
int actualGroupSize = 0;
int **pendingRequests;
int numberOfPendingRequests = 0;
int actualGroupId = 0;

#endif