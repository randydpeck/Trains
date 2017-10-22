// Rev: 08-17-17 by RDP.  Proof of concept for passing structures to functions.
// Passing a STRUCT item (not part of an array) to a function.  We must pass functions a pointer to the struct item.
//   This will permit the function to modify the contents of the struct item.
// Passing an array of STRUCT to a function.  Since the name of the array is synonomous with the address, we can pass just the name.

const int NUMBODIES = 5;

struct body {
    double p[3];//position
    double radius;
};

void setup() {
  Serial.begin(115200);            // PC serial monitor window
  Serial.println("Press Enter to begin...");
  while (Serial.available() == 0) {}
  
  struct body singleBody;          // Single instance of the structure item of type "body"
  struct body bodies[NUMBODIES];   // Array of structure items of type "body"

  // Put some data in singleBody...
  singleBody.p[0] = 234.567;
  singleBody.p[1] = 345.678;
  singleBody.p[2] = 456.789;
  
  // Put some data in bodies[]...
  for(int a = 0; a < NUMBODIES; a++) {
    for(int b = 0; b < 3; b++) {
      bodies[a].p[b] = a * b;
    }
    bodies[a].radius = 123.456;
  }

  // To pass a single STRUCT variable (not part of an array), we need to pass the address...
  printBody(&singleBody);

  // Now print again and we should see 888 to prove that the function modified part of the struct
  printBody(&singleBody);

  // Now pass the entire STRUCT array.  This is equivalent to passing the address/pointer.
  printBodies(bodies, NUMBODIES);

  // Now print again and we should see 999 to prove that the function modified part of the struct.
  // Note that this call is functionally identical to the previous call, because the name of the array is
  // equivalent to the address of the first element.  I.e. bodies = &bodies[0].
  printBodies(&bodies[0], NUMBODIES);

}

void loop() {
}

int printBody(struct body * testBody) {
  // We want to pass a single STRUCT variable (not part of an array), so we'll pass a pointer...
  // It's also possible to pass the struct itself, just like any other variable, but that would
  // make a copy of the struct so less efficient than a pointer.
  // See "C Primer Plus" page 622.
  // testBody is our local name for singleBody
  // Because testBody is a pointer (the address of the original struct) and not a structure, we use
  // the "->" operator instead of the dot operator.  i.e. testBody->p[0] rather than testBody.p[0].
  // Note that testBody−>p[2] is exactly equivalent to (*testBody).p[2]
  // object.member
  // pointer->member
  Serial.println("The discrete structure element singleBody:");
  Serial.println(testBody->p[0]);
  Serial.println(testBody->p[1]);
  Serial.println(testBody->p[2]);
  Serial.println((*testBody).p[0]);
  Serial.println((*testBody).p[1]);
  Serial.println((*testBody).p[2]);
  Serial.println("==============");
  (*testBody).p[1] = 777;   // Two identical ways to assign a value to singleBody.p[1]
  testBody->p[2] = 888;
  return true;
}

// int printBodies(struct body tbodies[], int NUMBODIES) {    // Identical to the following
int printBodies(struct body * tbodies, int NUMBODIES) {       // Identical to the above
  // Because an array name (without brackets) is the same as the address of the first element of the array, and this is an
  // array of struct body, you can use "struct body tbodies[]" and "struct body * tbodies" interchangably.
  // See "C Primer Plus" page 402 and 637.
  // tbodies[] is our local name for bodies[]
  // Because tbodies[] is a structure (not a pointer) we can use the dot operator i.e. tbodies[1].p[2] rather than the "->" operator.
  // NUMBODIES is a global constant so not really needed to pass.
  // "Just as you can use pointer notation with array names, you can use array notation with a pointer."  I think this is why
  // we don't need to use the "->" operator.
  // The array is an array of structures, not an array of pointers, so once you select a particular structure
  // with [n] there is no need to further dereference it.
  // NOTE 8/19/17: Despite the above descriptions (taken from book and websites) I still don't really understand why you need
  // to use the de-reference "->" when you pass a pointer to an individual struct, but you can use the dot operator if you're
  // passing (a pointer to) an ARRAY of struct.
  // NOTE: If we did not want the function to modify any part of the structure, we could use the "const" keyword:
  // int printBodies(const struct body tbodies[], int NUMBODIES) {
  // object.member
  // pointer->member
  Serial.println("The array bodies[]:");
  for (int a = 0; a < NUMBODIES; a++) {
    for (int b = 0; b < 3; b++) {
      Serial.println(tbodies[a].p[b]);
    }
  Serial.println(tbodies[a].radius);
  }
  Serial.println(".................");
  tbodies[4].p[2] = 999;
  return true;
}

