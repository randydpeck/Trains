// Tested by RDP on 8/4/19 to work fine.
// Returns the index into the array of the key being searched for.
// binarySearch() works for UNIQUE KEY arrays only.
// binarySearchFirst and binarySearchLast work with DUPLICATE KEY arrays.
// My intention is to use binarySearchFirst to find the starting point in the Route table to start looking
//   for appropriate routes for a given origin siding.  The table must be sorted by Origin Siding.

// Returns -1 if not found.
// *array points to the array.
// n is the number of elements in the array.
// target is the value to search for.

// Binary Search of UNIQUE KEYS returns the array offset, assuming NO DUPLICATES.
// If there are duplicates, this will simply return whichever it hits first.
// No guarantees if it will be the first, last, or some middle occurence.
int binarySearch(int *array, int n, int target)
{
    int low = 0;
    int high = n - 1;

    while (low <= high)
    {
        int middle = (low + high) / 2;
        if (array[middle] == target)
            return middle;
        else if (array[middle] < target)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return -1;
}

// Binary Search of possibly DUPLICATE KEYS, returns the *first* occurence in array.
// Returns -1 if not found.
int binarySearchFirst(int *array, int n, int target)
{
    int low = 0;
    int high = n - 1;
    int first = -1;

    while (low <= high)
    {
        int middle = (low + high) / 2;
        if (array[middle] == target)
        {
            first = middle;
            high = middle - 1;
        }
        else if (array[middle] < target)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return first;
}

// Binary Search of possibly DUPLICATE KEYS, returns the *last* occurence in array.
// Returns -1 if not found.
int binarySearchLast(int *array, int n, int target)
{
    int low = 0;
    int high = n - 1;
    int last = -1;

    while (low <= high)
    {
        int middle = (low + high) / 2;
        if (array[middle] == target)
        {
            last = middle;
            low = middle + 1;
        }
        else if (array[middle] < target)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return last;
}


void setup() {
    Serial.begin(115200);                 // PC serial monitor window
    /* Binary Search */
    /* Sorted array containing duplicate values */
    int array[] = {1, 5, 12, 48, 49, 49, 49, 50, 50, 65, 89};
    int n = sizeof(array)/sizeof(array[0]);

    /* Regular Binary Search */
    Serial.print("Value found at Index: ");
    Serial.println(binarySearch(array, n, 51));  // returns -1

    /* Binary Search finding first occurence of duplicate values */
    Serial.print("Value found at first Index: ");
    Serial.println(binarySearchFirst(array, n, 51));  // returns -1

    /* Binary Search finding last occurence of duplicate values */
    Serial.print("Value found at last Index: ");
    Serial.println(binarySearchLast(array, n, 51));  // returns -1

    /* Regular Binary Search */
    Serial.print("Value found at Index: ");
    Serial.println(binarySearch(array, n, 49));  // returns 5

    /* Binary Search finding first occurence of duplicate values */
    Serial.print("Value found at first Index: ");
    Serial.println(binarySearchFirst(array, n, 49));  // returns 4

    /* Binary Search finding last occurence of duplicate values */
    Serial.print("Value found at last Index: ");
    Serial.println(binarySearchLast(array, n, 49));  // returns 6
}

void loop() { }
