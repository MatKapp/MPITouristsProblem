#include "declarations.h"


bool checkGuideAvailability(int guideId, int requestLamportTime){
    // Return false if I asked for the same guide and i have the priviledge
    if(myRequestedGuideId == guideId &&  myRequestLamportTime < requestLamportTime)
    {
        return false;
    }
    if(guides[guideId].isBusy == true || guides[guideId].isBeaten == true)
    {
        return false;
    }
    return true;
}

bool checkGroupAvailability(int requestLamportTime, int source){
    if(requestLamportTime < myRequestLamportTime)
    {
        return true;
    }
    if(hasGroup == true)
    {
        return true;
    }
    if(requestLamportTime == myRequestLamportTime && source < processId)
    {
        return true;
    }
    return false;
}

void finishTrip(){
    printf("My trip has finished! Process: %d group: %d\n", processId, myGroupId);
    // Clear variables to be prepared fot new trip
    guides[myGuideId].isBusy = false;
    myGuideId = -1;
    myGroupId = -1;
    myRequestedGuideId = -1;
    actualGroupSize = 0;
    hasGroup = false;  
    myTripIsOn = false;
    receivedAcks = 0;
    receivedNacks = 0;
    myRequestLamportTime = INT_MAX;
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

void askForGuide(int guideId, int isWithNack){
    // Initialize request that will be sent
    int request[MESSAGE_SIZE];
    request[PROCESS_ID] = processId;
    request[REQUEST_TYPE] = FOR_GUIDE;
    request[REQUEST_ID] = myRequestId; 
    request[GUIDE_ID] = guideId;
    request[IS_WITH_NACK] = isWithNack;

    // Ask for guide
    myRequestedGuideId = guideId;
    myBcast(request, MPI_INT, REQUEST_TAG, MPI_COMM_WORLD);
}

int randomTripEndTime(){
    time_t now = time(NULL); 
    return now + 1 + rand() % TRIP_MAX_DURATION;
}

bool randomTouristBeating(){
    // Random a number in range 0..1mld
    int random = rand() % 1000000000;
    // Return true if prson has been beaten
    if (random < T_BEAT_PROBABIITY){
        return true;
    }
    return false;
}

bool randomGuideBeating(){
    // Random a number in range 0..1mld
    int random = rand() % 1000000000;
    // Return true if person has been beaten
    // Every tourist can observe guide beeing beatn, so we need to divide probability by numbr of tourists
    int beatProbability = G_BEAT_PROBABIITY / worldSize;
    if (random < beatProbability){
        return true;
    }
    return false;
}

// Leader starts the trip and informs the others
void startTrip(int guideId){
    guides[guideId].isBusy = true;
    myGuideId = guideId;
    myTripIsOn = true;
    printf("START TRIP! Process: %d group: %d with guide: %d ACKS: %d NACKS: %d\n", 
        processId, myGroupId, guideId, receivedAcks, receivedNacks);

    //Determine the trip's end time
    myTripEndTime = randomTripEndTime();

    // Initialize request that will be sent
    int request[MESSAGE_SIZE];
    request[PROCESS_ID] = processId; 
    request[GROUP_ID] = myGroupId;
    request[GUIDE_ID] = guideId;
    request[TRIP_END_TIME] = myTripEndTime;

    // Inform others about start of the trip
    myBcast(request, MPI_INT, TRIP_START_TAG, MPI_COMM_WORLD);
}

// If a tourist sees, that a guide is beaten, he informs others about it
void informAboutGuideBeaten(){
    // Initialize request that will be sent
    int message[MESSAGE_SIZE];
    message[GROUP_ID] = myGroupId;
    message[GUIDE_ID] = myGuideId;

    // Send message
    myBcast(message, MPI_INT, GUIDE_BEATEN_TAG, MPI_COMM_WORLD);
}

// Respond with ACK if tag == ACK_TAG
// Respond with NACK if tag == NACK_TAG
void respond(int source, int requestId, int tag){
    // Prepare the answer
    int answer[MESSAGE_SIZE];
    answer[LAMPORT_TIME] = lamportTime;
    answer[PROCESS_ID] = processId;
    answer[REQUEST_ID] = requestId;
    answer[GROUP_SIZE] = actualGroupSize;
    answer[GROUP_ID] = actualGroupId;              

    // Increment lamport time and send response
    lamportTime += 1;
    MPI_Send(&answer,
            MESSAGE_SIZE,              
            MPI_INT,                                        /* data item is an integer */
            source,                              /* destination process rank */
            tag,                                         /* user chosen message tag */
            MPI_COMM_WORLD);                                /* always use this */               
}

void takePendingFromQueue(int *i){               
    for(int j=0; j<MESSAGE_SIZE + ADDITIONAL_MESSAGE_SIZE; j++){             
        pendingRequests[*i][j] = pendingRequests[numberOfPendingRequests-1][j];
    }
    numberOfPendingRequests -= 1;
    *i -= 1;
}

void respondToPending(){
    int answer[MESSAGE_SIZE];

    for (int i = 0; i < numberOfPendingRequests; i++)
    {
        int requestId = pendingRequests[i][REQUEST_ID];
        int guideId = pendingRequests[i][GUIDE_ID];
        int requestLamportTime = pendingRequests[i][LAMPORT_TIME];
        int source = pendingRequests[i][PENDING_REQUEST_MPI_SOURCE];
        
        switch (pendingRequests[i][PENDING_REQUEST_REQUEST_TYPE])
        {                                   
            case FOR_GROUP:           
                if (true == checkGroupAvailability(requestLamportTime, source))
                {
                    // Respond with ACK
                    respond(source, requestId, GR_ACK_TAG);
                    takePendingFromQueue(&i);
                }
                break;
            
            case FOR_GUIDE:
                if(true == checkGuideAvailability(guideId, requestLamportTime)){
                    // Respond with ACK
                    respond(source, requestId, GU_ACK_TAG);
                    takePendingFromQueue(&i);
                }
                break;
        }
    }
}

void addRequestToPending(int *message, int source, int requestType){
    for (int i = 0; i < MESSAGE_SIZE; i++)
    {
        pendingRequests[numberOfPendingRequests][i] = message[i];
    }
    // Assign request type
    pendingRequests[numberOfPendingRequests][PENDING_REQUEST_REQUEST_TYPE] = requestType;
    pendingRequests[numberOfPendingRequests][PENDING_REQUEST_MPI_SOURCE] = source;
    numberOfPendingRequests += 1;
}

void handleMessages()
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
    int requestId = message[REQUEST_ID];
    int messageGuideId = message[GUIDE_ID];
    int messageGroupId = message[GROUP_ID];
    int messageLamportTime = message[LAMPORT_TIME];
    int source = status.MPI_SOURCE;
    
    // Update lamport time
    lamportTime = (lamportTime >= messageLamportTime) ? lamportTime : messageLamportTime;
    lamportTime += 1;

    // React to message
    switch (status.MPI_TAG)
    {
        case REQUEST_TAG:    

            switch (message[REQUEST_TYPE])
            {                    
                case FOR_GROUP:
                    if (true == checkGroupAvailability(messageLamportTime, source))
                    {
                        // Respond with ACK
                        respond(source, requestId, GR_ACK_TAG);
                    }
                    else
                    {
                        addRequestToPending(message, source, FOR_GROUP);
                    }
                    break;

                case FOR_GUIDE:
                    if(true == checkGuideAvailability(messageGuideId, messageLamportTime)){
                        // Respond with ACK
                        respond(source, requestId, GU_ACK_TAG);
                    }
                    else{
                        if(message[IS_WITH_NACK] == TRUE){
                            // Respond with NACK
                            respond(source, requestId, GU_NACK_TAG);
                        }
                        else{
                            addRequestToPending(message, source, FOR_GUIDE);                            
                        }
                    }
                    break;
            }
            break;

        case GR_ACK_TAG:
            // React to group acknowledge tag
            if (messageGroupId == actualGroupId)
            {
                actualGroupSize = (actualGroupSize >= message[GROUP_SIZE]) ? actualGroupSize : message[GROUP_SIZE];
            }                  
            else
            {
                if (messageGroupId > actualGroupId)
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
            if (requestId == myRequestId){
                receivedNacks += 1;
            }
            break;

        case TRIP_START_TAG:
            // Reaction when my trip starts
            if (messageGroupId == myGroupId){
                myGuideId = messageGuideId;
                myTripEndTime = message[TRIP_END_TIME];
                myTripIsOn = true;
            }
            break;

        case GUIDE_BEATEN_TAG:
            if (messageGroupId == myGroupId){
                finishTrip();
            }
            break;
    }
}

// This function is used for active waiting for ACKS, NACKS, beginning or finish of the trip
// It tries to handle all unhandled messages (received and not received)
void activeWaiting(){
    usleep(USLEEP);
    // Respond to pending first (as thy came earlier)
    respondToPending();  
    // Handle messages only if there is any message
    int flag;
    MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, NULL );
    if(flag == 1){
        handleMessages();
    }
}

int waitForRandomGuide(){
    // Reset necessary variables
    myRequestId += 1;
    receivedAcks = 0;
    receivedNacks = 0;

    // Get random guide id
    int guideId = rand() % guidesCount;

    // Request specified guide without NACKS
    askForGuide(guideId, FALSE);
    printf("Insist on guide! Guide: %d process: %d\n", guideId, processId);

    // Wait for acceptance of all the tourists
    while(receivedAcks < numberOfTourists - 1){
        // Receive ACKs
        activeWaiting();
    }

    // Guide has been found. Increment requestId return guide id
    myRequestId += 1;
    return guideId;
}

int searchForGuide(){
    for( int guideId = 0; guideId < guidesCount; guideId++){
        // Prepare for new guide request
        myRequestId += 1;
        receivedAcks = 0;
        receivedNacks = 0;

        // Request specified guide
        askForGuide(guideId, TRUE);

        // Wait for acceptance of all the tourists
        while(receivedAcks < numberOfTourists - 1
                    && receivedNacks == 0){
            // Receive ACKs / NACKs
            activeWaiting();
            if(receivedAcks == numberOfTourists - 1
                    && receivedNacks == 0){
                // Guide has been found
                myRequestId += 1;
                return guideId;
            }
        }
    }
    return waitForRandomGuide();
}

void init(int argc, char **argv){
    numberOfTourists = atoi(argv[1]);
    maxGroupSize = atoi(argv[2]);
    guidesCount = atoi(argv[3]);

    // Allocate array of guides
    guides = (struct Guide*)malloc(guidesCount * sizeof(struct Guide)) ;
    for( int row = 0; row < guidesCount; row++){
        guides[row].isBusy = false;
        guides[row].isBeaten = false;
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

    // Seed for random generator. Pass processId to make it different for different threads
    srand(processId);
}

void makeOneTrip(){
    // First ask other tourists to access the group
    askForGroup();

    // Wait for all processes / tourists to accept the request
    while (receivedAcks < numberOfTourists - 1)
    {
        activeWaiting();
    }

    // Enter the group
    hasGroup = true;
    actualGroupSize += 1;
    receivedAcks = 0;
    myRequestLamportTime = INT_MAX;
    myGroupId = actualGroupId;
    printf("----I have a group! Process: %d, group: %d, group size: %d\n", processId, actualGroupId, actualGroupSize);

    // If a process is the last to enter this group, it becomes a leader
    bool isLeader = actualGroupSize == maxGroupSize ? true : false; 

    // Leder has to inform others about finished group and search for a guid
    if (isLeader == true)
    {
        actualGroupId += 1;
        actualGroupSize = 0;
        respondToPending();

        int guideId = searchForGuide();
        startTrip(guideId);
    }
    else{ 
        respondToPending();

        // Wait for start of the trip
        while (myTripIsOn == false){
            activeWaiting();
        }
    }
    
    // Trip has been started
    // Enjoy it until it lasts
    while (myTripIsOn == true){     
        activeWaiting();

        // End the trip if tourist has been beaten
        if (true == randomTouristBeating()){
            printf("I've got beaten! Process: %d\n", processId);
            finishTrip();
        }

        // End the trip and inform others if guide has been beaten
        if (true == randomGuideBeating()){
            printf("Guide got beaten! Guide: %d %d\n", myGuideId, myGroupId);
            informAboutGuideBeaten();
            finishTrip();
        }

        // Exit the trip if it has already finished
        if(time(NULL) > myTripEndTime){
            finishTrip();
            break;
        }
    }
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

    // Make trips forever
    while(1 == 1){ 
        makeOneTrip();
    }

    MPI_Finalize();
}