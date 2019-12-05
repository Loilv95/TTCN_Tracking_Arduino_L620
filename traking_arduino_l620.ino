#include <TinyGPS.h>
#include <avr/wdt.h>
#include <SoftwareSerial.h>
//Call class gps
TinyGPS gps;

//Setup serial Software
SoftwareSerial SerialSim(10, 11); // RX, TX
SoftwareSerial SerialGPS(5, 6); // RX, TX

//define power key module sim
#define PWR_KEY     9
#define RESET_KEY   8

/*AT*/
char* AT = "AT\r\n";
char* NETWORK_STATUS =  "AT+CEREG?\r\n";

//char* OPEN_MQTT = "AT+EMQNEW=\"m14.cloudmqtt.com\",\"11342\",60000,100\r\n";
char* OPEN_MQTT = "AT+EMQNEW=\"scpbroker.thingxyz.net\",\"1993\",60000,100\r\n";

//char* LOGIN_MQTT = "AT+EMQCON=0,3,\"blink\",1000,1,0,0,\"pammvrnv\",\"VFKR7oxo2qgb\"\r\n";
char* LOGIN_MQTT = "AT+EMQCON=0,3,\"/SCPCloud/DEVICE/SW002\",1000,1,0\r\n";

void turn_on_L620(void)
{
  digitalWrite (PWR_KEY, LOW);
  delay (500);
  digitalWrite (PWR_KEY, HIGH);
  delay (300);
}

void reset_L620(void)
{
  digitalWrite (RESET_KEY, LOW);
  delay (100);
  digitalWrite(RESET_KEY, HIGH);
  delay (500);
}
//Define

bool newData = false;
char* data_gps = "{\"geometry\":{\"type\":\"Point\",\"coordinates\":[%s,%s]}}";
char* data_header = "AT+EMQPUB=0,\"/SCPCloud/DEVICE/A4F4C020D6D4D\",1,0,0,%d,\"%s\"\r\n";
char buf_str[128] = "";
char buf_hex[200] = "";
char data_pub[200] = "";

void setup()
{
  //Set up  baud rate serial
  Serial.begin(115200);
  SerialSim.begin(115200);
  SerialGPS.begin(9600);

  pinMode (PWR_KEY, OUTPUT);
  pinMode (RESET_KEY, OUTPUT);
  //Reset and Turn on module sim L620
RST_L620:
  init_BC66();
  //Close MQTT
  sendATcommand("AT+EMQDISCON=0", "OK", 2000);
  //Open MQTT
  if (sendATcommand(OPEN_MQTT, "OK", 2000)) {
    Serial.println("Open MQTT sucess");
  }
  else {
    Serial.println ("Open MQTT false");
    goto RST_L620;
  }
  //Login MQTT
  delay(100);
  if (sendATcommand(LOGIN_MQTT, "OK", 2000)) {
    Serial.println("Login MQTT sucess");
  }
  else {
    Serial.println ("Login MQTT flase");
    goto RST_L620;
  }
}

void loop()
{
  while (SerialGPS.available()) {
    char c = SerialGPS.read();
    // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
    if (gps.encode(c)) // Did a new valid sentence come in?
      newData = true;
  }

  if (newData) {
    float flat, flon;
    char flat_str[15], flon_str[15];
    unsigned long age;
    gps.f_get_position(&flat, &flon, &age);

    dtostrf(flat, 7, 6, flat_str);
    dtostrf(flon, 7, 6, flon_str);

    Serial.println("abd");
    sprintf (buf_str, data_gps, flon_str, flat_str);

    string2hexString(buf_str, buf_hex);

    sprintf (data_pub, data_header, strlen(buf_hex), buf_hex);

    MQTT_PUB(data_pub);

    newData = false;
  }
  else
    Serial.println("ERROR_READ_GPS");

  delay(2000);
}

void MQTT_PUB(char *pub)
{
  if (sendATcommand(pub, "OK", 5000)) {
    Serial.println("Push data to sever MQTT sucess");
  }
  else {
    Serial.println ("Push data to sever MQTT false");
  }
}


//function to convert ascii char[] to hex-string (char[])
void string2hexString(char* input, char* output)
{
  int loop = 0;
  int i = 0;
  while (input[loop] != '\0')
  {
    sprintf((char*)(output + i), "%02X", input[loop]);
    loop += 1;
    i += 2;
  }
  //insert NULL at the end of the output string
  output[i++] = '\0';
}

int8_t sendATcommand(char* ATcommand, char* expected_answer, unsigned int timeout) {

  uint8_t x = 0,  answer = 0;
  char response[100];
  unsigned long previous;

  memset(response, '\0', 100); // Clean response buffer

  delay(100); // Delay to be sure no passed commands interfere

  while ( SerialSim.available() > 0) SerialSim.read();   // Wait for clean input buffer

  SerialSim.println(ATcommand);    // Send the AT command

  previous = millis();

  // this loop waits for the answer
  do {
    // if there are data in the UART input buffer, reads it and checks for the asnwer
    if (SerialSim.available() != 0) {
      response[x] = SerialSim.read();
      x++;
      // check if the desired answer is in the response of the module
      if (strstr(response, expected_answer) != NULL) {
        answer = 1;
      }
    }
    // Waits for the answer with time out
  } while ((answer == 0) && ((millis() - previous) < timeout));

  return answer;
}

void init_BC66()
{
  uint8_t i = 0;
  reset_L620();
  turn_on_L620();
  //Sent AT
  if (sendATcommand(AT, "OK", 1000)) {
    Serial.println("Send AT Oke");
  }
  else
    Serial.println ("Send AT false");

  //Turn off echo
  sendATcommand("ATE0", "OK", 1000);

  //Check Network status
  //Wait 1,5s
  Serial.println ("Wait network ...");
  //Check network in three minutes
  for (i = 0; i < 18; i++) {
    if (sendATcommand(NETWORK_STATUS, "+CEREG: 0,1", 1000)) {
      Serial.println ("Nbiot network connect sucess...");
      break;
    }
    delay (5000);
  }
  if (i == 18) {
    Serial.println ("Reset module L620");
    init_BC66();
  }
  return;
}
