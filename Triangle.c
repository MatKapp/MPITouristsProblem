#include "declarations.h"


bool checkGuideAvailability(int guideId, int requestLamportTime){
    if(myRequestedGuideId == guideId &&  myRequestLamportTime < requestLamportTime)
    {
        return false;
    }
    if(guides[guideId].isBusy && guides[guideId].isBeaten)
    {
        return false;
    }
    return true;
}


void myBcast(int *request, MPI_Datatype datatype, int tag, MPI_Comm communicator) 
{
    // Increment lamport time and set request lamport time
    lamportTime += 1;
    myRequestLamportTime = lamportTime;
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

void askForGroup(){
    // Initialize request that will be sent
    int request[MESSAGE_SIZE];
    request[PROCESS_ID] = processId;
    request[REQUEST_TYPE] = FOR_GROUP;
    request[REQUEST_ID] = myRequestId; 

    // Ask for group
    myBcast(request, MPI_INT, REQUEST_TAG, MPI_COMM_WORLD);
}

void askForGuide(int guideId){
    // Initialize request that will be sent
    int request[MESSAGE_SIZE];
    request[PROCESS_ID] = processId;
    request[REQUEST_TYPE] = FOR_GUIDE;
    request[REQUEST_ID] = myRequestId; 
    request[GUIDE_ID] = guideId;

    // Ask for guide
    myRequestedGuideId = guideId;
    myBcast(request, MPI_INT, REQUEST_TAG, MPI_COMM_WORLD);
}

// Respond with ACK if tag == ACK_TAG
// Respond with NACK if tag == NACK_TAG
void respond(MPI_Status status, int requestId, int tag){
    int answer[MESSAGE_SIZE];
    answer[REQUEST_ID] = requestId;
    answer[GROUP_SIZE] = actualGroupSize;
    answer[GROUP_ID] = actualGroupId;              

    // Increment lamport time and send response
    lamportTime += 1;
    MPI_Send(&answer,
            MESSAGE_SIZE,              
            MPI_INT,                                        /* data item is an integer */
            status.MPI_SOURCE,                              /* destination process rank */
            tag,                                         /* user chosen message tag */
            MPI_COMM_WORLD);                                /* always use this */               
}

void handleMessages()
{
    int message[MESSAGE_SIZE];
    int answer[MESSAGE_SIZE];
    int flag; 
    int guideId;
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
    int requestId = message[REQUEST_ID];
    
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
                    if (message[LAMPORT_TIME] < myRequestLamportTime || hasGroup == true)
                    {
                        // Respond with ACK
                        respond(status, requestId, GR_ACK_TAG);
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

                case FOR_GUIDE:
                    guideId = message[GUIDE_ID];
                    if(true == checkGuideAvailability(guideId, message[LAMPORT_TIME])){
                        // Respond with ACK
                        respond(status, requestId, GU_ACK_TAG);
                    }
                    else{
                        // Respond with NACK
                        respond(status, requestId, GU_NACK_TAG);
                    }
                    break;
            }
            break;

        case GR_ACK_TAG:     
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

        case GU_ACK_TAG:
            if(requestId == myRequestId){
                receivedAcks += 1;
            }
            break;

        case GU_NACK_TAG:
            if(requestId == myRequestId){
                printf("--NACK group %d guide %d\n", myGroupId, myRequestedGuideId);
                receivedNacks += 1;
            }
            break;
    }
}

void respondToPendingGroup(){

    int answer[MESSAGE_SIZE];
    
    for (int i = 0; i < numberOfPendingRequests; i++)
    {
        switch (pendingRequests[i][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_REQUEST_TYPE])
        {                                   
            case FOR_GROUP:
                
                if (pendingRequests[i][LAMPORT_TIME] <= myRequestLamportTime || hasGroup == true)
                {
                    answer[REQUEST_ID] = pendingRequests[i][REQUEST_ID];
                    answer[GROUP_SIZE] = actualGroupSize;                
                    answer[GROUP_ID] = actualGroupId;  
         
                    lamportTime += 1;
                    MPI_Send(&answer,
                            MESSAGE_SIZE,              
                            MPI_INT,                                        
                            pendingRequests[i][MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE - PENDING_REQUEST_MPI_SOURCE],    
                            GR_ACK_TAG,                                         
                            MPI_COMM_WORLD);                             
                    pendingRequests[i] = pendingRequests[numberOfPendingRequests-1];
                    numberOfPendingRequests -= 1;
                    i -= 1;
                }
                break;
        }
    }
}

void startTrip(int guideId){
    guides[guideId].isBusy = true;
    printf("START group %d with guide %d ACKS: %d NACKS: %d\n", myGroupId, guideId, receivedAcks, receivedNacks);
}

void searchForGuide(){
    for( int guideId = 0; guideId < guidesCount; guideId++){
        // Prepare for new guide request
        myRequestId += 1;
        receivedAcks = 0;
        receivedNacks = 0;

        // Request specified guide
        askForGuide(guideId);

        // Wait for acceptance of all the tourists
        while(receivedAcks < numberOfTourists - 1
                    && receivedNacks == 0){
            // Receive ACKs / NACKs
            handleMessages(pendingRequests);
            if(receivedAcks == numberOfTourists - 1){
                // Guide has been found
                startTrip(guideId);
                myRequestId += 1;
                return;
            }
        }
    }
}

void init(int argc, char **argv){
    numberOfTourists = atoi(argv[1]);
    maxGroupSize = atoi(argv[2]);
    guidesCount = atoi(argv[3]);

    // Allocate array of guides
    guides = (struct Guide*)malloc(guidesCount * sizeof(struct Guide)) ;
    for( int row = 0; row < guidesCount; row++){
        guides[row].isBusy = false;
    }

    // allocate an "array of arrays" of int
    pendingRequests = (int**)malloc( PENDING_REQUESTS_SIZE * sizeof(int*) ) ;
    for( int row = 0 ; row < PENDING_REQUESTS_SIZE ; row++ )
    {
        pendingRequests[row] = (int*)malloc( (MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE)*sizeof(int) ) ;
    }

    //Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Status status;
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    // Initialize lamport time
    lamportTime = processId;
}

int main(int argc, char **argv)
{
    // Check arguments
    if (argc < 4){
        printf("Not enough arguments\n");
        exit(1);
    }

    // Initialize importand data structures
    init(argc, argv);

    askForGroup();

    // Wait for all processes / tourists to accept the request
    while (receivedAcks < numberOfTourists - 1)
    {
        handleMessages();
    }

    // Enter the group
    hasGroup = true;
    actualGroupSize += 1;
    receivedAcks = 0;
    myGroupId = actualGroupId;
    printf("----I have a group: %d, actual group id: %d, group size: %d\n", processId, actualGroupId, actualGroupSize);
    // If a process is the last to enter this group, it becomes a leader
    bool isLeader = actualGroupSize == maxGroupSize ? true : false;

    // Leder has to inform others about finished group and search for a guid
    if (isLeader == true)
    {
        actualGroupId += 1;
        actualGroupSize = 0;
        respondToPendingGroup();

        searchForGuide();
    }
    else{
        respondToPendingGroup();

        // Wait for start of the trip
        while (myTripIsOn == false){
            handleMessages();
        }
    }
    
    while (1 == 1){
        handleMessages();
    }
    MPI_Finalize();
}