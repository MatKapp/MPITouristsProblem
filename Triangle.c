#include "declarations.h"
    

void myBcast(MPI_Datatype datatype, int tag, MPI_Comm communicator) 
{
    // Increment lamport time and set request lamport time
    lamportTime += 1;
    requestLamportTime = lamportTime;

    // Initialize request that will be sent
    int request[MESSAGE_SIZE];
    request[PROCESS_ID] = processId;
    request[REQUEST_TYPE] = FOR_GROUP;
    request[REQUEST_ID] = myRequestId; 
    request[LAMPORT_TIME] = lamportTime;

    // Send request for all other processes / tourists
    for (int i = 0; i < worldSize; i++) 
    {
        if (i != processId) 
        {        
            MPI_Send(request, MESSAGE_SIZE, datatype, i, tag, communicator);
        }    
    }
}


void handleMessages(int **pendingRequests)
{
    int message[MESSAGE_SIZE];
    int answer[MESSAGE_SIZE];
    int flag; 
    MPI_Status status;

    // Receive message
    memset(message, -1, sizeof message);
    memset(answer, -1, sizeof answer);
    MPI_Recv(&message,       
        MESSAGE_SIZE,              
        MPI_INT,     
        MPI_ANY_SOURCE, 
        MPI_ANY_TAG,    
        MPI_COMM_WORLD, 
        &status);
    
    // Increment lamport time
    lamportTime = (lamportTime >= message[LAMPORT_TIME]) ? lamportTime : message[LAMPORT_TIME];
    lamportTime += 1;

    // React to message
    switch (status.MPI_TAG)
    {
        case REQUEST_TAG:    

            switch (message[REQUEST_TYPE])
            {                    
                case FOR_GROUP:

                    if (message[LAMPORT_TIME] < requestLamportTime || hasGroup == true)
                    {
                        answer[REQUEST_ID] = message[REQUEST_ID];
                        answer[GROUP_SIZE] = actualGroupSize;
                        answer[GROUP_ID] = actualGroupId;              

                        // Increment lamport time and send response
                        lamportTime += 1;
                        MPI_Send(&answer,
                                MESSAGE_SIZE,              
                                MPI_INT,                                        /* data item is an integer */
                                status.MPI_SOURCE,                              /* destination process rank */
                                ACK_TAG,                                         /* user chosen message tag */
                                MPI_COMM_WORLD);                                /* always use this */
                    }
                    else
                    {
                        // Add request to pending requests
                        for (int i = 0; i < MESSAGE_SIZE; i++)
                        {
                            pendingRequests[numberOfPendingRequests][i] = message[i];
                        }
                        // Assign request type
                        pendingRequests[numberOfPendingRequests][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_REQUEST_TYPE] = FOR_GROUP;
                        pendingRequests[numberOfPendingRequests][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_MPI_SOURCE] = status.MPI_SOURCE;
                        numberOfPendingRequests += 1;
                    }
                    break;
            }
            break;

        case ACK_TAG:     

            if (message[GROUP_ID] == actualGroupId)
            {
                actualGroupSize = (actualGroupSize >= message[GROUP_SIZE]) ? actualGroupSize : message[GROUP_SIZE];
            }                  
            else
            {
                if (message[GROUP_ID] > actualGroupId)
                {
                    actualGroupSize = message[GROUP_SIZE];
                    actualGroupId = message[GROUP_ID];
                }
            }
            receivedAcks += 1;
            break;
    }
}

void respondToPendingGroup(int **pendingRequests){

    int answer[MESSAGE_SIZE];
    
    for (int i = 0; i < numberOfPendingRequests; i++)
    {
        switch (pendingRequests[i][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_REQUEST_TYPE])
        {                                   
            case FOR_GROUP:
                
                if (pendingRequests[i][LAMPORT_TIME] <= requestLamportTime || hasGroup == true)
                {
                    answer[REQUEST_ID] = pendingRequests[i][REQUEST_ID];
                    answer[GROUP_SIZE] = actualGroupSize;                
                    answer[GROUP_ID] = actualGroupId;  
         
                    lamportTime += 1;
                    MPI_Send(&answer,
                            MESSAGE_SIZE,              
                            MPI_INT,                                        
                            pendingRequests[i][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_MPI_SOURCE],    
                            ACK_TAG,                                         
                            MPI_COMM_WORLD);                             
                    pendingRequests[i] = pendingRequests[numberOfPendingRequests-1];
                    numberOfPendingRequests -= 1;
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
    int numberOfTourists = atoi(argv[1]);
    int maxGroupSize = atoi(argv[2]);


    // allocate an "array of arrays" of int
    pendingRequests = (int**)malloc( PENDING_REQUESTS_SIZE * sizeof(int*) ) ;
    // each entry in the array of arrays of int
    // isn't allocated yet, so allocate it

    for( int row = 0 ; row < PENDING_REQUESTS_SIZE ; row++ )
    {
        pendingRequests[row] = (int*)malloc( (MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE)*sizeof(int) ) ;
    }
    // initializing values to send
    MPI_Init(&argc, &argv);
    MPI_Status status;
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    lamportTime = processId;

    myBcast(MPI_INT, REQUEST_TAG, MPI_COMM_WORLD);

    // Wait for all processes / tourists to accept the request
    while (receivedAcks < numberOfTourists - 1)
    {
        handleMessages(pendingRequests);
    }

    // Enter the group
    hasGroup = true;
    actualGroupSize += 1;

    printf("Mam grupe __________________________________________%d, actual group id: %d, group size: %d\n", processId, actualGroupId, actualGroupSize);
    
    // If a process is the last to enter this group, become a leader
    if (actualGroupSize >= maxGroupSize)
    {
        actualGroupId += 1;
        actualGroupSize = 0;
    }

    while (1==1){
        respondToPendingGroup(pendingRequests);
        handleMessages(pendingRequests);
    }
    MPI_Finalize();
}