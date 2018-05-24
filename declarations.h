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

struct Guide{
    bool isBusy;
    bool isBeaten;
};

#define FOR_GROUP 0
#define FOR_GUIDE 1

#define LAMPORT_TIME 0
#define REQUEST_TYPE 1
#define PROCESS_ID 2
#define REQUEST_ID 3
#define GUIDE_ID 4
#define GROUP_ID 5
#define TRIP_END_TIME 6
#define IS_WITH_NACK 7

#define GR_ACK_TAG 0
#define GU_ACK_TAG 1
#define GU_NACK_TAG 2
#define REQUEST_TAG 3
#define TRIP_START_TAG 4

#define GROUP_SIZE 2

#define MESSAGE_SIZE 8
#define PENDING_REQUESTS_SIZE 100
#define PENDING_REQUEST_REQUEST_TYPE 1
#define PENDING_REQUEST_MPI_SOURCE 2
#define ADDITIONAL_MESSAGE_SIZE 2

#define TRIP_MAX_DURATION 4      // Seconds

#define TRUE 1
#define FALSE 0


int myRequestId = 0;
int myRequestedGuideId = -1;
int numberOfGroups = 2;
int lamportTime = INT_MAX;
int myRequestLamportTime;
bool hasGroup = false;
bool myTripIsOn = false;
int myGroupId = -1; // No real group will have id=-1
int myGuideId = -1; // No real guide will have id=-1
int processId, worldSize;
int numberOfElementsToSend = 0;
int receivedAcks = 0;
int receivedNacks = 0;
int actualGroupSize = 0;
int numberOfPendingRequests = 0;
int actualGroupId = 0;
int numberOfTourists;
int maxGroupSize;
int guidesCount;
int myTripEndTime = -1;
int **pendingRequests;
struct Guide *guides;

#endif