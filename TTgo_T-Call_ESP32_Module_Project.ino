//$ last work 21/July/23
// # version 1.0.0
//! Not fully tested so not merging in main branch
//% remove delay before calling getResponse() by setting up timeout and look if
//% recived OK/ERROR then quit loop.

const char simPIN[] = "";
// Stable till now 17/July/23
/*
Usefull AT commands:
AT+CMGF=1 - Set SMS text mode
AT+CMGL="ALL" - List all SMS messages
AT+CMGR=1 - Read SMS message at index 1
AT+CMGD=1 - Delete SMS message at index 1
AT+CLIP=1 - Enable missed call notification
AT+DDET=1 - Enable DTMF detection
AT+CLCC - List current calls
ATD+923354888420; - Make a call to +923354888420
AT+CHUP - Hang up current call
AT+CREG? - Check if registered to the network
AT+CSQ - Check signal quality
AT+CBC - Check battery voltage
AT+CMEE=2 - Enable verbose error messages
AT+CMEE=0 - Disable verbose error messages
AT+CMEE? - Check if verbose error messages are enabled
AT+CPIN? - Check if SIM card is inserted
*/
String MOBILE_No = "+923354888420";

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Define the serial console for debug prints, if needed
// #define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

String receivedMessage = ""; // Global variable to store received SMS message

String updateBatteryStatus();
double batteryVoltage = 0;
int batteryPercentage = 0;

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart
  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3) {
    modem.simUnlock(simPIN);
  }
  modem.sendAT(GF("+CLIP=1"));
  // Enable SMS text mode
  modem.sendAT(GF("+CMGF=1"));
  delay(500);
  // giveMissedCall();
}

void loop() {
  // read full string of SerialMon not char by char and send it to SerialAT
  if (SerialMon.available()) {
    String command = SerialMon.readString();
    if (command.indexOf("call") != -1) {
      SerialMon.println("Calling " + String(MOBILE_No));
      giveMissedCall();
    } else if (command.indexOf("sms") != -1) {
      // fetch sms from input string, sample-> sms : msg here
      String sms = command.substring(command.indexOf("sms") + 6);
      SerialMon.println("Sending SMS : " + sms + " to : " + String(MOBILE_No));
      sendSMS(sms);
    } else if (command.indexOf("all") != -1) {
      SerialMon.println("Reading all messages");
      moduleManager();
    } else if (command.indexOf("battery") != -1) {
      SerialMon.println(updateBatteryStatus());
    } else if (command.indexOf("t1") != -1) {
      Serial.println("Msg number : " +
                     String(getNewMessageNumber("+CMTI: \"SM\",2")));
    } else if (command.indexOf("read") != -1) {
      // say i recived read 2, so it will read message of index 2
      Serial.println(
          readMessage(command.substring(command.indexOf("read") + 5).toInt()));
    } else if (command.indexOf("delete") != -1) { // to delete message
      Serial.println("Deleting message number : " +
                     String(command.substring(command.indexOf("delete") + 7)));
      deleteMessage(command.substring(command.indexOf("delete") + 7).toInt());
    } else {
      SerialMon.println("Executing: " + command);
      SerialAT.println(command);
    }
  }
  if (SerialAT.available())
    Serial.println(getResponse());
}

void giveMissedCall() {
  SerialAT.println("ATD+ " + MOBILE_No + ";");
  updateSerial();
  // delay(20000);            // wait for 20 seconds...
  // SerialAT.println("ATH"); // hang up
  // updateSerial();
}

void sendSMS(String sms) {
  if (modem.sendSMS(MOBILE_No, sms)) {
    SerialMon.println(sms);
  } else {
    SerialMon.println("SMS failed to send");
  }
  delay(500);
}

void updateSerial() {
  delay(500);
  while (SerialMon.available()) {
    SerialAT.write(SerialMon.read());
  }
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
}

void moduleManager() {
  // this function will check if there is any unread message or not
  // store response of AT+CMGL="ALL" in a string 1st
  SerialAT.println("AT+CMGL=\"ALL\"");
  delay(3000);
  String response = getResponse();
  SerialMon.println("********************\n" + response +
                    "\n********************");
  String lastMessage;
  int messageNumber;
  getLastMessageAndIndex(response, lastMessage, messageNumber);

  SerialMon.println("Last CMGL number: " + String(messageNumber));
  SerialMon.println("Last message: " + lastMessage);
}

void getLastMessageAndIndex(String response, String &lastMessage,
                            int &messageNumber) {
  // Find the last occurrence of "+CMGL:" in the response
  int lastCmglIndex = response.lastIndexOf("CMGL:");

  // Find the next occurrence of "+CMGL:" after the last occurrence
  int nextCmglIndex = response.indexOf("+CMGL:", lastCmglIndex + 1);
  // Extract the last message and its CMGL number
  String lastCmgl = response.substring(lastCmglIndex, nextCmglIndex);
  int commaIndex = response.indexOf(",", lastCmglIndex);

  messageNumber =
      response
          .substring(lastCmglIndex + 6, response.indexOf(",", lastCmglIndex))
          .toInt();

  // Extract the message content
  int messageStartIndex = lastCmgl.lastIndexOf("\"") + 3;
  lastMessage = lastCmgl.substring(messageStartIndex);
}

String updateBatteryStatus() {
  SerialAT.println("AT+CBC");
  delay(2000);
  return getResponse();
}

String getResponse() {
  String response = "";
  while (SerialAT.available()) {
    response += SerialAT.readString();
  }
  if (response.indexOf("+CMTI:") != -1) {
    int newMessageNumber = getNewMessageNumber(response);
    String temp_str = executeCommand(removeOk(readMessage(newMessageNumber)));
    SerialMon.println("New message [ " + temp_str + "]");
    if (temp_str.indexOf("<not executed>") != -1)
      deleteMessage(newMessageNumber);
  }
  return response;
}

int getNewMessageNumber(String response) {
  return response.substring(response.lastIndexOf(",") + 1, -1).toInt();
}

String readMessage(int index) { // read the message of given index
  SerialAT.println("AT+CMGR=" + String(index));
  delay(3000);
  String tempStr = getResponse();
  return tempStr.substring(tempStr.lastIndexOf("\"") + 2);
}
void deleteMessage(int index) {
  SerialAT.println("AT+CMGD=" + String(index));
  delay(2000);
  SerialMon.println(getResponse());
}

String removeOk(String str) {
  return str.substring(0, str.lastIndexOf("OK") - 2);
}

String executeCommand(String str) {
  //~ additional commands will be executed here so define new sms commands here
  if (str.indexOf("#call") != -1) {
    giveMissedCall();
    str += " <executed>";
  } else if (str.indexOf("#battery") != -1) {
    updateBatteryParameters(updateBatteryStatus());
    sendSMS("Battery percentage : " + String(batteryPercentage) +
            "\nBattery voltage : " + String(batteryVoltage));
    str += " <executed>";
  } else {
    SerialMon.println("-> Module is not trained to execute this command ! <-");
    str += " <not executed>";
  }
  return str;
}

void updateBatteryParameters(String response) {
  // get +CBC: 0,81,4049 in response
  batteryPercentage =
      response.substring(response.indexOf(",") + 1, response.lastIndexOf(","))
          .toInt();
  int milliBatteryVoltage =
      response.substring(response.lastIndexOf(",") + 1, -1).toInt();
  batteryVoltage =
      milliBatteryVoltage / pow(10, (String(milliBatteryVoltage).length() - 1));
}
