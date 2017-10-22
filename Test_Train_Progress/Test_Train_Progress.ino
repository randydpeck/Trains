// TEST TRAIN PROGRESS Rev: 09/26/17.
// Test Train Progress contains all of the key routines to initialize, enqueue, dequeue, and search the Train Progress table.
// Rev: 09/06/17.

const byte MAX_TRAINS           =   8;  // Maximum number of trains supported at once, use to dimension arrays
const byte MAX_BLOCKS_PER_TRAIN =  12;  // Maximum number of occupied blocks FOR ANY ONE TRAIN in the Train Progress table, to dimension array.
const byte TOTAL_SENSORS        =  52;  // Added sensor 53 in block 1
const byte TOTAL_BLOCKS         =  26;
const byte TOTAL_TURNOUTS       =  32;
const byte TRAIN_STATIC         =  99;  // This train number is a static train.
const byte TRAIN_DONE           = 100;  // Used when selecting "done" on rotary encoder
// true = any non-zero number
// false = 0

// DESCRIPTION OF CIRCULAR BUFFERS Rev: 9/5/17.
// We use CLOCKWISE circular buffers, and tracks HEAD, TAIL, and COUNT.
// HEAD always points to the UNUSED cell where the next new element will be inserted (enqueued), UNLESS the queue is full,
// in which case head points to tail and new data cannot be added until the tail data is dequeued.
// TAIL always points to the last active element, UNLESS the queue is empty, in which case it points to garbage.
// COUNT is the total number of active elements, which can range from zero to the size of the buffer.
// Initialize head, tail, and count to zero.
// To enqueue an element, if array is not full, insert data at HEAD, then increment both HEAD and COUNT.
// To dequeue an element, if array is not empty, read data at TAIL, then increment TAIL and decrement COUNT.
// Always incrment HEAD and TAIL pointers using MODULO addition, based on the size of the array.
// Note that HEAD == TAIL *both* when the buffer is empty and when full, so we use COUNT as the test for full/empty status.

// TRAIN PROGRESS STRUCTURE Rev: 9/5/17.
// An array of a structures.  Each structure represents the active route of a single train, and contains a circular buffer head, tail,
// and count (number of active records), as well as an array of about a dozen blocks, which includes Block Number, Entry Sensor, and
// Exit Sensor numbers.  There is one element of the structure array for each train -- so about 8 structure array elements.
// Each record represents a reserved block that is part of the train's current reserved route.
// New records are added when a train is first registered (just a single block) and when new routes are added (multiple blocks.)
// Old records are "cleared" as the train progresses along its route, based on exit sensors clearing.
// The blocks in the Block Reservation table are closely tied to these records -- when new records are added to Train Progress, the
// corresponding blocks are reserved in the Block Reservation table; and when Train Progress exit sensor records are cleared (i.e. when
// a block is released/dequeued), the corresponding blocks are "un-reserved" in the Block Reservation table.
// Similarly, every active record in the Train Progress table must have exactly one "reserved" record in the Block Reservation table.
// We might want to run a periodic function that confirms that the number of active Train Progress records (sum of the "length" for all trains)
// is equal to the number of records flagged as "Reserved" in the Block Reservation table.  Fatal error if they are ever not the same.
// When the table is initialized (with no train), head == tail == 0, and length == 0.
// When a new train is first established, it will have just a single record where head == tail == 0, and length = 1.
// As a route is added, records will be added at the head and then head and length will be incremented.
// As the train clears blocks, they will be "un-reserved" and the tail will be incremented and the length will be decrimented.
// trainProgress does not keep track of the actual location of the train; only that the train has those blocks reserved, and
// the train is guaranteed to be somewhere within that route -- not necessarily at the head or the tail.
// The code will watch the sensors being tripped and cleared, and infer the location of each train to be at, or just beyond (if the train
// is moving), the tripped sensor which is closest to the head.
// When we receive a sensor status change, we must be CERTAIN that we find it in the Train Progress talbe, otherwise it's a fatal error!

// DEBUG NOTE: It will be easy to provide debug code to the serial monitor that shows a "picture" of an entire array -- each reserved
// block, including entry and exit sensor, and the status (tripped or cleared), updated each time something happens.
// Perhaps we can show a list of sensos tripped and cleared separately from the linear "map" of the route.
// (In this context, "route" does not necessarily correspond exactly with a specific "route table" route, but rather all or a portion of
// one or two specific routes (the portion(s) reserved for this train,) or even just a single block when a train is first registered.

// WITH REGARDS TO A-OCC: Painting the red/blue and white occupancy sensor LEDs:
// If we update the train progress table every time a sensor status changes, we will have enough information to paint the control panel
// red/blue and white LEDs.
// We will pass a parm to the paint routine "train number selected" and paint that red, if any.

// Maybe use typedef struct so you don't need to use the word "struct" each time you reference it i.e. in the function header.
// HOWEVER you can't use typedef if you want to pass a structure to a function, because the function needs to know about
//   struct I think.  Not an issue since structs are global.
struct trainProgressStruct {
  byte head;                               //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Next array element to be written
  byte tail;                               //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Next array element to be removed
  byte count;                              //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Number of active elements (reserved blocks) for this train.
  byte blockNum[MAX_BLOCKS_PER_TRAIN];     // [0..11] = 1..26
  byte entrySensor[MAX_BLOCKS_PER_TRAIN];  // [0..11] = 1..52 (or 53)
  byte exitSensor[MAX_BLOCKS_PER_TRAIN];   // [0..11] = 1..52 (or 53)
};
trainProgressStruct trainProgress[MAX_TRAINS];  // Create trainProgress[0..MAX_TRAINS - 1]; thus, trainNum 1..MAX_TRAINS is always 1 more than the index.

// FUNCTION DEFINITIONS:


void trainProgressInit(const byte tTrainNum) {
  // Rev: 09/05/17
  // When we first start registration mode, we need to initialize all possible trains to empty; no blocks reserved/occupied.
  // This routine will initialize a SINGLE TRAIN, so we must call it once for each possible train.
  // Input: train number to initialize (1..MAX_TRAINS)
  trainProgress[tTrainNum - 1].head = 0;
  trainProgress[tTrainNum - 1].tail = 0;
  trainProgress[tTrainNum - 1].count = 0;
  for (byte i = 0; i < MAX_BLOCKS_PER_TRAIN; i++) {
    trainProgress[tTrainNum - 1].blockNum[i] = 0;
    trainProgress[tTrainNum - 1].entrySensor[i] = 0;
    trainProgress[tTrainNum - 1].exitSensor[i] = 0;
  }
  return;
}

bool trainProgressIsEmpty(const byte tTrainNum) {
  // Rev: 09/06/17
  // Returns true if tTrainNum's train progress table has zero valid records; else false.
  if ((tTrainNum > 0) && (tTrainNum <= MAX_TRAINS)) {  // Looking at actual train numbers 1..MAX_TRAINS
    return (trainProgress[tTrainNum - 1].count == 0);
  } else {
    Serial.println("FATAL ERROR!  Train number out of range in trainProgressIsEmpty."); // *********************** CHANGE TO STANDARD ALL-STOP FATAL ERROR IN REGULAR CODE
    while (true) {}
  }
}

bool trainProgressIsFull(const byte tTrainNum) {
  // Rev: 09/06/17
  // Returns true if tTrainNum's train progress table has the maximum possible records; else false.
  if ((tTrainNum > 0) && (tTrainNum <= MAX_TRAINS)) {  // Looking at actual train numbers 1..MAX_TRAINS
    return (trainProgress[tTrainNum - 1].count == MAX_BLOCKS_PER_TRAIN);
  } else {
    Serial.println("FATAL ERROR!  Train number out of range in trainProgressIsFull."); // *********************** CHANGE TO STANDARD ALL-STOP FATAL ERROR IN REGULAR CODE
    while (true) {}
  }
}

trainProgressEnqueue(const byte tTrainNum, const byte tBlockNum, const byte tEntrySensor, const byte tExitSensor) {
  // Rev: 09/29/17.
  // Adds a single record (block number, entry sensor, exit sensor) for a given train number to the circular buffer.
  // Inputs are: train number, block number, entry sensor number, and exit sensor number.
  // Returns true if successful; false if buffer already full.
  // If not full, insert data at head, then increment head (modulo size), and increments count.
  if (!trainProgressIsFull(tTrainNum)) {    // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    byte tHead = trainProgress[tTrainNum - 1].head;
    // byte tCount = trainProgress[tTrainNum - 1].count;    MAYBE NOT NEEDED?????????????
    trainProgress[tTrainNum - 1].blockNum[tHead] = tBlockNum;
    trainProgress[tTrainNum - 1].entrySensor[tHead] = tEntrySensor;
    trainProgress[tTrainNum - 1].exitSensor[tHead] = tExitSensor;
    trainProgress[tTrainNum - 1].head = (tHead + 1) % MAX_BLOCKS_PER_TRAIN;
    trainProgress[tTrainNum - 1].count++;
  } else {  // Train Progress table is full; how could this happen?
    sprintf(lcdString, "%.20s", "Train progress full!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  return;
}

bool trainProgressDequeue(const byte tTrainNum, byte * tBlockNum, byte * tEntrySensor, byte * tExitSensor) {
  // Rev: 09/05/17.
  // Retrieves and clears a single record for a given train number, from the circular buffer.
  // Because this function returns values via the passed parameters, CALLS MUST SEND THE ADDRESS OF THE RETURN VARIABLES, i.e.:
  //   status = trainProgressDequeue(trainNum, &blockNum, &entrySensor, &exitSensor);
  // And because we are passing addresses, our code inside this function must use the "*" dereference operator.
  // Input is: train number.
  // Returns true if successful; false if buffer already empty.
  // If not empty, returns block number, entry sensor, and exit sensor AS PASSED PARAMETERS.
  // Data will be at tail, then tail will be incremented (modulo size), and count will be decremented.
  if (!trainProgressIsEmpty(tTrainNum)) {    // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    byte tTail = trainProgress[tTrainNum - 1].tail;
    // byte tCount = trainProgress[tTrainNum - 1].count;     MAYBE NOT NEEDED??????????????????
    *tBlockNum = trainProgress[tTrainNum - 1].blockNum[tTail];
    *tEntrySensor = trainProgress[tTrainNum - 1].entrySensor[tTail];
    *tExitSensor = trainProgress[tTrainNum - 1].exitSensor[tTail];
    trainProgress[tTrainNum - 1].tail = (tTail + 1) % MAX_BLOCKS_PER_TRAIN;
    trainProgress[tTrainNum - 1].count--;
    return true;
  } else {    // Train Progress table is empty
    return false;
  }
}

bool trainProgressFindSensor(const byte tTrainNum, const byte tSensorNum, byte * tBlockNum, byte * tEntrySensor, byte * tExitSensor, bool * tLocHead, bool * tLocTail, bool * tLocPenultimate) {
  // Rev: 09/06/17.
  // Search every active block of a given train, until given sensor is found or not part of this train's route.
  // Must be called for every train, until desired sensor is found as either and entry or exit sensor.
  // Because this function returns values via the passed parameters, CALLS MUST SEND THE ADDRESS OF THE RETURN VARIABLES, i.e.:
  //   status = trainProgressFindSensor(trainNum, sensorNum, &blockNum, &entrySensor, &exitSensor, &locHead, &locTail, &locPenultimate);
  // And because we are passing addresses, our code inside this function must use the "*" dereference operator.
  // Inputs are: train number, sensor number to find.
  // Returns true if the sensor was found as part of this train's route; false if the sensor was not part of the train's route.
  // If not found, then the values for all other returned parameters will be undefined/garbage.  Can't think of a reason to
  // initialize them i.e. to zero and false, but we easily could.
  // If found, returns the block number, entry sensor, and exit sensor AS PASSED PARAMETERS.
  // If found, also returns if the block is the head, tail, or penultimate block in the train's route.  We use three separate bool
  // variables for this, rather than an enumeration, because the sensor could be up to two of these (but not all three,) such as
  // both the head and the tail if the route only had one block, or both the tail and the penultimate sensor if the route only had
  // two blocks.
  // We won't specify if a found sensor is the entry or exit sensor of the block because the calling routine can easily determine that.
  // IMPORTANT RE: HEAD!!!  The "head" that we track internally is actually one in front of the "highest" active element (unless zero elements.)
  // When we return the bool tLocHead, we mean is the sensor found in the highest active element, one less than the head pointer (unless zero elements.)
  // Let's set all of the return values to zero and false just for good measure...
  * tBlockNum = 0;
  * tEntrySensor = 0;
  * tExitSensor = 0;
  * tLocHead = false;
  * tLocTail = false;
  * tLocPenultimate = false;
  // The sensor certainly won't be found if this train's progress table is empty, but perhaps we want to check that before even
  // calling this routine, in which case the following block can be removed as redundant.
  if (!trainProgressIsEmpty(tTrainNum)) {     // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    // Start at tail and increment using modulo addition, traversing/searching each active block
    // tElement starts pointing at tail and will be incremented (modulo size) until it points at head
    byte tElement = trainProgress[tTrainNum - 1].tail;
    // i is the number of iterations; the number of elements in this train's (non-empty) table 1..MAX_BLOCKS_PER_TRAIN
    for (byte i = 0; i < trainProgress[tTrainNum - 1].count; i++) {
      if ((trainProgress[tTrainNum - 1].entrySensor[tElement] == tSensorNum) || (trainProgress[tTrainNum - 1].exitSensor[tElement] == tSensorNum)) {
        // Found it at this block's entry or exit sensor!
        * tBlockNum = trainProgress[tTrainNum - 1].blockNum[tElement];
        * tEntrySensor = trainProgress[tTrainNum - 1].entrySensor[tElement];
        * tExitSensor = trainProgress[tTrainNum - 1].exitSensor[tElement];
        if (tElement == trainProgress[tTrainNum - 1].tail) {
          *tLocTail = true;
        }
        if (((tElement + 1) % MAX_BLOCKS_PER_TRAIN) == trainProgress[tTrainNum - 1].head) {  // Head in terms of highest active element, one less than head pointer
          * tLocHead = true;
        }
        if (((tElement + 2) % MAX_BLOCKS_PER_TRAIN) == trainProgress[tTrainNum - 1].head) {  // Head in terms of highest active element, one less than head pointer
          * tLocPenultimate = true;
        }
        return true;   // We found the sensor in the current block!
      }
      tElement = (tElement + 1) % MAX_BLOCKS_PER_TRAIN;  // We didn't find the sensor here, so point to the next element (if not past head) and try again
    }
    return false;
  } else {  // Train Progress table is empty
    return false;
  }
}

void trainProgressDisplay(const byte tTrainNum) {
  // Rev: 09/06/17.
  // Display (to the serial monitor) the entire Train Progress table for a given train, starting at tail and working to head.
  // This is just used for debugging and can be removed from final code.
  // Iterates from tail, and prints out count number of elements, wraps around if necessary.
  if (!trainProgressIsEmpty(tTrainNum)) {   // In this case, actual train number tTrainNum, not tTrainNum - 1
    Serial.print("Train number: "); Serial.println(tTrainNum);
    Serial.print("Is Empty? "); Serial.println(trainProgressIsEmpty(tTrainNum));
    Serial.print("Is Full?  "); Serial.println(trainProgressIsFull(tTrainNum));
    Serial.print("Count:    "); Serial.println(trainProgress[tTrainNum - 1].count);
    Serial.print("Head:     "); Serial.println(trainProgress[tTrainNum - 1].head);
    Serial.print("Tail:     "); Serial.println(trainProgress[tTrainNum - 1].tail);
    Serial.print("Data:     "); 
    // Start at tail and increment using modulo addition, traversing/searching each active block
    // tElement starts pointing at tail and will be incremented (modulo size) until it points at head
    byte tElement = trainProgress[tTrainNum - 1].tail;
    // i is the number of iterations; the number of elements in this train's (non-empty) table 1..MAX_BLOCKS_PER_TRAIN
    for (byte i = 0; i < trainProgress[tTrainNum - 1].count; i++) {   
      Serial.print(trainProgress[tTrainNum - 1].blockNum[tElement]); Serial.print(", ");
      Serial.print(trainProgress[tTrainNum - 1].entrySensor[tElement]); Serial.print(", ");
      Serial.print(trainProgress[tTrainNum - 1].exitSensor[tElement]); Serial.print(";   ");
      tElement = (tElement + 1) % MAX_BLOCKS_PER_TRAIN;
    }
    Serial.print("End of train "); Serial.println(tTrainNum);
  }
  else {
    Serial.print("Train number "); Serial.print(tTrainNum); Serial.println("'s Train Progress table is empty.");
  }
}

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                // PC serial monitor window
  for (byte i = 0; i < MAX_TRAINS; i++) {
    trainProgressInit(i + 1);  // First train is Train #1 (not zero) because functions accept actual train numbers, not zero-offset numbers.
  }

  byte blockNum = 0;
  byte entrySensor = 0;
  byte exitSensor = 0;
  bool status = false;
  bool locHead = false;
  bool locTail = false;
  bool locPenultimate = false;
  bool foundSensor = false;
  byte findTrain = 0;
  byte findSensor = 0;


  trainProgressEnqueue(1, 5, 9, 10);  // Train 1, block 5, entry 9, exit 10
  trainProgressEnqueue(1, 7, 32, 31);
  trainProgressEnqueue(1, 2, 4, 3);

  trainProgressEnqueue(2,2,3,4);  // Train 2, block 2, entry 3, exit 4
  trainProgressEnqueue(2,21,51,52);
  trainProgressEnqueue(2,16,44,43);
  trainProgressEnqueue(2, 16, 44, 43);
  trainProgressEnqueue(2,15,16,15);
  trainProgressEnqueue(2,12,42,41);
  trainProgressEnqueue(2,8,34,33);
  trainProgressEnqueue(2, 8, 34, 33);
  trainProgressEnqueue(2,1,1,53);

  trainProgressEnqueue(4, 2, 3, 4);  // Train 2, block 2, entry 3, exit 4
  trainProgressEnqueue(4, 21, 51, 52);
  trainProgressEnqueue(4, 16, 44, 43);
  trainProgressEnqueue(4, 16, 44, 43);
  trainProgressEnqueue(4, 15, 16, 15);
  trainProgressEnqueue(4, 12, 42, 41);
  trainProgressEnqueue(4, 8, 34, 33);
  trainProgressEnqueue(4, 8, 34, 33);
  trainProgressEnqueue(4, 1, 1, 53);

  trainProgressEnqueue(5, 2, 3, 4);  // Train 2, block 2, entry 3, exit 4
  trainProgressEnqueue(5, 21, 51, 52);
  trainProgressEnqueue(5, 16, 44, 43);
  trainProgressEnqueue(5, 16, 44, 43);
  trainProgressEnqueue(5, 15, 16, 15);
  trainProgressEnqueue(5, 12, 42, 41);
  trainProgressEnqueue(5, 8, 34, 33);
  trainProgressEnqueue(5, 8, 34, 33);
  trainProgressEnqueue(5, 1, 1, 53);

  trainProgressEnqueue(7, 2, 3, 4);  // Train 2, block 2, entry 3, exit 4
  trainProgressEnqueue(7, 21, 51, 52);
  trainProgressEnqueue(7, 16, 44, 43);
  trainProgressEnqueue(7, 16, 44, 43);
  trainProgressEnqueue(7, 15, 16, 15);
  trainProgressEnqueue(7, 12, 42, 41);
  trainProgressEnqueue(7, 8, 34, 33);
  trainProgressEnqueue(7, 8, 34, 33);
  trainProgressEnqueue(7, 1, 1, 53);

  trainProgressEnqueue(8, 2, 3, 4);  // Train 2, block 2, entry 3, exit 4
  trainProgressEnqueue(8, 21, 51, 52);
  trainProgressEnqueue(8, 16, 44, 43);
  trainProgressEnqueue(8, 16, 44, 43);
  trainProgressEnqueue(8, 15, 16, 15);
  trainProgressEnqueue(8, 12, 42, 41);
  trainProgressEnqueue(8, 8, 34, 33);
  trainProgressEnqueue(8, 8, 34, 33);
  trainProgressEnqueue(8, 1, 1, 53);




  trainProgressDisplay(2);
/*

  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);
  trainProgressDequeue(2, &blockNum, &entrySensor, &exitSensor);
  Serial.print("Dequeued: "); Serial.print(blockNum); Serial.print(", "); Serial.print(entrySensor); Serial.print(", "); Serial.println(exitSensor);
  trainProgressDisplay(2);

  trainProgressEnqueue(2,6,11,12);
  trainProgressDisplay(2);
  trainProgressEnqueue(2,5,9,10);
  trainProgressDisplay(2);
  trainProgressEnqueue(2,11,40,39);
  trainProgressDisplay(2);
  */

  Serial.println(millis());

  findTrain = 2;
  findSensor = 53;
  foundSensor = trainProgressFindSensor(findTrain, findSensor, &blockNum, &entrySensor, &exitSensor, &locHead, &locTail, &locPenultimate);
  Serial.println(millis());
  for (findTrain = 1; findTrain < MAX_TRAINS + 1; findTrain++) {
    foundSensor = trainProgressFindSensor(findTrain, findSensor, &blockNum, &entrySensor, &exitSensor, &locHead, &locTail, &locPenultimate);
    if (foundSensor) {
//      Serial.print("Found one...");
    }
  }
  Serial.println(millis());
  Serial.println("Done...");


  Serial.print("Finding sensor "); Serial.println(findSensor);
  Serial.print("Returned: "); Serial.println(foundSensor);
  if (foundSensor) {
    Serial.print("Block "); Serial.println(blockNum);
    Serial.print("Entry "); Serial.println(entrySensor);
    Serial.print("Exit "); Serial.println(exitSensor);
    Serial.print("Tail? "); Serial.println(locTail);
    Serial.print("Penultimate? "); Serial.println(locPenultimate);
    Serial.print("Head? "); Serial.println(locHead);
  }


  Serial.println("All done!");

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

}  // end of main loop()

// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************




