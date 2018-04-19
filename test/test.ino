void setup()
{
}

void loop()
{
   byte aMsg[8] = { 8, 7, 6, 5, 4, 3, 2, 1 };
   function1(aMsg);
}

void function1(byte tMsg[])
{
   Serial2.write(tMsg, tMsg[0]);
}
