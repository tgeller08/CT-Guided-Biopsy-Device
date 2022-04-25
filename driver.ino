/*********************************************************************
 This code is based off of the Adafruit nRF52 based Bluefruit LE module example "blueart". 

 MIT license, check LICENSE for more information
*********************************************************************/
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <stdlib.h>

// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery


// (c) Michael Schoeffler 2017, http://www.mschoeffler.de

#include "Wire.h" // This library allows you to communicate with I2C devices.

const int MPU_ADDR = 0x68; // I2C address of the MPU-6050. If AD0 pin is set to HIGH, the I2C address will be 0x69.

int16_t accelerometer_x, accelerometer_y, accelerometer_z; // variables for accelerometer raw data
int16_t gyro_x, gyro_y, gyro_z; // variables for gyro raw data
int16_t temperature; // variables for temperature data
float adjusted_temp;

char newline[2] = {'\n'};

/*
Converts int16_t to string. 
Input: int16_t i storing the variable to be converted and char* sout which will eventually be used to hold the output string. 
char* sout must be declared before call to function. 
Output: char* sout, the converted string. 
*/
char* convert_int16_to_str(int16_t i, char* sout) { 
  sprintf(sout, "%6d", i);
  return sout;
}

/*
Converts double to string. 
Input: double i storing the variable to be converted and char* sout which will eventually be used to hold the output string. 
char* sout must be declared before call to function. 
Output: char* sout, the converted string. 
*/
char *dtostrf (double i, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, i);
  return sout;
}


void setup()
{
  Serial.begin(115200);

#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif
  
  Serial.println("Bluefruit52 BLEUART Example");
  Serial.println("---------------------------\n");

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();

  Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode");
  Serial.println("Once connected, enter character(s) that you wish to send");
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR); // Begins a transmission to the I2C slave (GY-521 board)
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H) [MPU-6000 and MPU-6050 Register Map and Descriptions Revision 4.2, p.40]
  Wire.endTransmission(false); // the parameter indicates that the Arduino will send a restart. As a result, the connection is kept active.
  Wire.requestFrom(MPU_ADDR, 7*2, true); // request a total of 7*2=14 registers
  
  // "Wire.read()<<8 | Wire.read();" means two registers are read and stored in the same variable
  accelerometer_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x3B (ACCEL_XOUT_H) and 0x3C (ACCEL_XOUT_L)
  accelerometer_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x3D (ACCEL_YOUT_H) and 0x3E (ACCEL_YOUT_L)
  accelerometer_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x3F (ACCEL_ZOUT_H) and 0x40 (ACCEL_ZOUT_L)
  
  temperature = Wire.read()<<8 | Wire.read(); // reading registers: 0x41 (TEMP_OUT_H) and 0x42 (TEMP_OUT_L)
  adjusted_temp = temperature/340.00+36.53;
  
  gyro_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x43 (GYRO_XOUT_H) and 0x44 (GYRO_XOUT_L)
  gyro_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x45 (GYRO_YOUT_H) and 0x46 (GYRO_YOUT_L)
  gyro_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x47 (GYRO_ZOUT_H) and 0x48 (GYRO_ZOUT_L)

  char ACCEL_XOUT_LABEL[14] = {'A','C','C','E','L','_','X','O','U','T',':',' '};
  char ACCEL_XOUT[20];
  convert_int16_to_str(accelerometer_x, ACCEL_XOUT);
  bleuart.write(ACCEL_XOUT_LABEL);
  bleuart.write(ACCEL_XOUT);
  bleuart.write(newline);


  char ACCEL_YOUT_LABEL[14] = {'A','C','C','E','L','_','Y','O','U','T',':',' '};
  char ACCEL_YOUT[20];
  convert_int16_to_str(accelerometer_y, ACCEL_YOUT);
  bleuart.write(ACCEL_YOUT_LABEL);
  bleuart.write(ACCEL_YOUT);
  bleuart.write(newline);


  char ACCEL_ZOUT_LABEL[14] = {'A','C','C','E','L','_','Z','O','U','T',':',' '};
  char ACCEL_ZOUT[20];
  convert_int16_to_str(accelerometer_z, ACCEL_ZOUT);
  bleuart.write(ACCEL_ZOUT_LABEL);
  bleuart.write(ACCEL_ZOUT);
  bleuart.write(newline);


  char GYRO_XOUT_LABEL[14] = {'G','Y','R','O','_','X','O','U','T',':',' '};
  char GYRO_XOUT[20];
  convert_int16_to_str(gyro_x, GYRO_XOUT);
  bleuart.write(GYRO_XOUT_LABEL);
  bleuart.write(GYRO_XOUT);
  bleuart.write(newline);


  char GYRO_YOUT_LABEL[14] = {'G','Y','R','O','_','Y','O','U','T',':',' '};
  char GYRO_YOUT[20];
  convert_int16_to_str(gyro_y, GYRO_YOUT);
  bleuart.write(GYRO_YOUT_LABEL);
  bleuart.write(GYRO_YOUT);
  bleuart.write(newline);


  char GYRO_ZOUT_LABEL[14] = {'G','Y','R','O','_','Z','O','U','T',':',' '};
  char GYRO_ZOUT[20];
  convert_int16_to_str(gyro_z, GYRO_ZOUT);
  bleuart.write(GYRO_ZOUT_LABEL);
  bleuart.write(GYRO_ZOUT);
  bleuart.write(newline);

  char TEMP_LABEL[14] = {'T','E','M','P','(','C',')',':',' '};
  char charBuf[20];
  dtostrf(adjusted_temp, 4, 6, charBuf);
  bleuart.write(TEMP_LABEL);
  bleuart.write(charBuf);
  bleuart.write(newline);
  bleuart.write(newline);
  delay(1000);
  
   //Forward data from HW Serial to BLEUART
  while (Serial.available())
  {
    // Delay to wait for enough input, since we have a limited transmission buffer
    delay(2);

    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf)); //input to serial
    bleuart.write( buf, count );
  }

  // Forward from BLEUART to HW Serial
  while ( bleuart.available() )
  {
    uint8_t ch;
    ch = (uint8_t) bleuart.read();
    Serial.write(ch);
  }
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
