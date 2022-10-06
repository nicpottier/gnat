#include <Arduino.h>

// The build version comes from an environment variable. Use the VERSION
// define wherever the version is needed.
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define VERSION STR_VALUE(BUILD_VERSION)

void setup()
{
  delay(5000);
  Serial.begin(115200);
  Serial.print("Version: ");
  Serial.println(VERSION);
}

void loop()
{
  // put your main code here, to run repeatedly
}