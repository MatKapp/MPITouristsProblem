#include <mpi.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

// typedef enum {FORGROUP} REQUESTTYPE;

typedef enum { false, true } bool;

#define FORGROUP 0
#define LAMPORTTIME 0
#define REQUESTTYPE 1
#define PROCESSID 2
#define REQUESTID 3
#define ACKTAG 0
#define NACKTAG 1
#define REQUESTTAG 2
#define GROUPSIZE 2
#define MESSAGESIZE 4
#define PENDINGREQUESTSSIZE 100
#define PENDINGREQUESTREQUESTTYPE 1
#define PENDINGREQUESTMPISOURCE 2
#define ADDITIONALMESSAGESIZE 2


void MyBcast(void* data, int count, MPI_Datatype datatype, int tag, MPI_Comm communicator, int *lamportTime) 
{
    int worldRank, worldSize;
    MPI_Comm_rank(communicator, &worldRank);
    MPI_Comm_size(communicator, &worldSize);

    for (int i = 0; i < worldSize; i++) 
    {
        // *lamportTime += 1;

        if (i != worldRank) 
        {        
            MPI_Send(data, count, datatype, i, tag, communicator);
        }    
    }
    // printf("%d Zapytałem %d procesow\n", worldRank, i);                    
    
}


void HandleMessages(bool    hasGroup,
                    int     *myLamportTime,
                    int     *receivedAcks,
                    int     *actualGroupSize, 
                    int     **pendingRequests, 
                    int     *numberOfPendingRequests)
{
    int message[MESSAGESIZE];
    int answer[MESSAGESIZE];
    int flag, processId; 
    MPI_Request request;
    MPI_Status status;
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    memset(message, -1, sizeof message);
    memset(answer, -1, sizeof answer);
    MPI_Recv(&message,       
        MESSAGESIZE,              
        MPI_INT,     
        MPI_ANY_SOURCE, 
        MPI_ANY_TAG,    
        MPI_COMM_WORLD, 
        &status);       

    switch (status.MPI_TAG)
    {
        case REQUESTTAG:

            switch (message[REQUESTTYPE])
            {                    
                case FORGROUP:
                    
                    if (message[LAMPORTTIME] < *myLamportTime || hasGroup == true)
                    {
                        //updating lamport time
                        // *myLamportTime = (*myLamportTime >= message[LAMPORTTIME]) ? *myLamportTime : message[LAMPORTTIME];
                        // *myLamportTime += 1;

                        answer[REQUESTID] = message[REQUESTID];
                        answer[GROUPSIZE] = *actualGroupSize;                  
                        printf("Wysylam ACKS %d do %d--------------\n", processId, status.MPI_SOURCE);
                        // *myLamportTime += 1;
                        MPI_Send(&answer,
                                MESSAGESIZE,              
                                MPI_INT,                                        /* data item is an integer */
                                status.MPI_SOURCE,                              /* destination process rank */
                                ACKTAG,                                         /* user chosen message tag */
                                MPI_COMM_WORLD);                                /* always use this */
                    }
                    else
                    {
                        //adding request to pending request
                        for (int i = 0; i < MESSAGESIZE; i++)
                        {
                            pendingRequests[*numberOfPendingRequests][i] = message[i];
                        }
                        //request type
                        pendingRequests[*numberOfPendingRequests][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTREQUESTTYPE] = FORGROUP;
                        pendingRequests[*numberOfPendingRequests][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTMPISOURCE] = status.MPI_SOURCE;
                        *numberOfPendingRequests += 1;
                    }
                    break;
            }
            break;

        case ACKTAG:
            printf("+++++++++++++++Dostałem ACK %d od %d z rozmiarem grupy %d\n", processId, status.MPI_SOURCE, message[GROUPSIZE]);                    
            *actualGroupSize = (*actualGroupSize >= message[GROUPSIZE]) ? *actualGroupSize : message[GROUPSIZE];
            *receivedAcks += 1;
            break;
    }
}

void RespondToPendingGroup(int **pendingRequests,
                        int *numberOfPendingRequests,
                        int *myLamportTime, 
                        bool hasGroup, 
                        int *actualGroupSize){

    int answer[MESSAGESIZE];
    int processId;
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    
    for (int i = 0; i < *numberOfPendingRequests; i++)
    {
        // printf("%d\n",pendingRequests[i][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTREQUESTTYPE]);
        switch (pendingRequests[i][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTREQUESTTYPE])
            {
                                   
                case FORGROUP:
                    
                    if (pendingRequests[i][LAMPORTTIME] <= *myLamportTime || hasGroup == true)
                    {
                        answer[REQUESTID] = pendingRequests[i][REQUESTID];
                        answer[GROUPSIZE] = *actualGroupSize;                  
                        printf("Wysylam ACKS %d do %d-------------- rozmiar grupy %d\n", processId,
                                                                                     pendingRequests[i][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTMPISOURCE],
                                                                                     *actualGroupSize);
                        // *myLamportTime += 1;
                        MPI_Send(&answer,
                                MESSAGESIZE,              
                                MPI_INT,                                        
                                pendingRequests[i][MESSAGESIZE + ADDITIONALMESSAGESIZE - PENDINGREQUESTMPISOURCE],    
                                ACKTAG,                                         
                                MPI_COMM_WORLD);                             
                        pendingRequests[i] = pendingRequests[*numberOfPendingRequests-1];
                        *numberOfPendingRequests -= 1;
                        i -= 1;
                    }
                    break;
            }
    }
}


int main(int argc, char **argv)
{
    if (argc < 3){
        printf("Not enough arguments\n");
        exit(1);
    }
    int myRequestId = 0;
    int numberOfTourists = atoi(argv[1]);
    int numberOfGroups = 2;
    int lamportTime = INT_MAX;
    bool hasGroup = false;
    int processId, size;
    int request[MESSAGESIZE];
    int numberOfElementsToSend = 0;
    int receivedAcks = 0;
    int actualGroupSize = 0;
    int maxGroupSize = 0;
    int groupSize = atoi(argv[2]);
    int **pendingRequests;
    int numberOfPendingRequests = 0;

    // allocate an "array of arrays" of int
    pendingRequests = (int**)malloc( PENDINGREQUESTSSIZE * sizeof(int*) ) ;
    // each entry in the array of arrays of int
    // isn't allocated yet, so allocate it

    for( int row = 0 ; row < PENDINGREQUESTSSIZE ; row++ )
    {
        pendingRequests[row] = (int*)malloc( (MESSAGESIZE + ADDITIONALMESSAGESIZE)*sizeof(int) ) ;
    }
    // initializing values to send
    request[PROCESSID] = processId;
    request[REQUESTTYPE] = FORGROUP;
    request[REQUESTID] = myRequestId;
    MPI_Init(&argc, &argv);
    MPI_Status status;
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    lamportTime = processId; 
    request[LAMPORTTIME] = lamportTime;
    MyBcast(&request, MESSAGESIZE, MPI_INT, REQUESTTAG, MPI_COMM_WORLD, &lamportTime);

    while (receivedAcks < numberOfTourists - 1)
    {
        HandleMessages(hasGroup, &lamportTime, &receivedAcks, &actualGroupSize, pendingRequests, &numberOfPendingRequests);
    }
    hasGroup = true;
    actualGroupSize += 1;
    printf("Mam grupe ziomek __________________________________________%d, actual group size: %d\n", processId, actualGroupSize);

    while (1==1){
        RespondToPendingGroup(pendingRequests, &numberOfPendingRequests, &lamportTime, hasGroup, &actualGroupSize);
        HandleMessages(hasGroup, &lamportTime, &receivedAcks, &actualGroupSize, pendingRequests, &numberOfPendingRequests);
    }
    MPI_Finalize();
}