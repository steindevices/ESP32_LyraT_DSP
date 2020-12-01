
#include <ArduinoOTA.h>                 // OTA library
#include "dsp_process.h"

#define WIFI_SSID                     "SSID"
#define WIFI_PASSWORD                 "PASSWORD"

//------------------------------------------------------------------------------------ 
// Common data
//------------------------------------------------------------------------------------ 
char    message[256];
bool    dsp_init_OK = true;


//------------------------------------------------------------------------------------ 
// TelnetSpy setup
//------------------------------------------------------------------------------------ 

TelnetSpy       SerialAndTelnet;

void setupTelnetSpy() {

  // Welcome message
  SerialAndTelnet.setWelcomeMsg( "Successfully connected to ESP32 LyraT DSP Processor.\r\n" ); 

  // Set the buffer size to 2000 characters
  SerialAndTelnet.setBufferSize( 2000 );
}

//------------------------------------------------------------------------------------ 
// TelnetSpy loop
//------------------------------------------------------------------------------------

void loopTelnetSpy() {
  SerialAndTelnet.handle();
}


//------------------------------------------------------------------------------------ 
// SERIAL setup
//------------------------------------------------------------------------------------ 
void setupSerial() {
  SERIAL.begin( 115200 );
  delay( 100 ); 
  
  SERIAL.println( "Serial connection established" );
}

//------------------------------------------------------------------------------------ 
// SERIAL input
//------------------------------------------------------------------------------------ 
void loopSerialInput() {

  String input_text;
  
  if( SERIAL.available() > 0 ) {
    input_text = SERIAL.readStringUntil( '\n' );

    // Remove line feed character '\r'
    input_text.remove( input_text.length() - 1 );
    
    if( input_text.equals( "i" ) ) {        // Display filter information
      dsp_command( 'i' );
    } else if( input_text.equals( "e" ) ) { // Enable filter
      dsp_command( 'e' );
    } else if( input_text.equals( "d" ) ) { // Disable filter
      dsp_command( 'd' );
    } else if( input_text.equals( "s" ) ) { // Stop filter 
      dsp_command( 's' );
    } else if( input_text.equals( "r" ) ) { // Run filter        
      dsp_command( 'r' );
    } else if( input_text.equals( "p" ) ) { // Plot freqency response curve
      dsp_command( 'p' );         
    } else {
      SERIAL.println( "??? Unknown command" );
    }
  }
}

//------------------------------------------------------------------------------------ 
// WiFi setup
//------------------------------------------------------------------------------------ 

char    strIPAddress[16];

void connectWiFi() {
  SERIAL.print( "Connecting to " );
  SERIAL.print( WIFI_SSID );
  
  while( WiFi.status() != WL_CONNECTED ) {
    SERIAL.print( "." );
    delay( 500 );
  }
  
  SERIAL.println();
  SERIAL.print( "Connected to " );
  SERIAL.println( WIFI_SSID );
  SERIAL.print( "IP Address is: " );
  strcpy( strIPAddress, WiFi.localIP().toString().c_str() );  
  SERIAL.println( strIPAddress );
}

void setupWiFi() {
  WiFi.mode( WIFI_STA );
  WiFi.begin( WIFI_SSID, WIFI_PASSWORD );                              
  connectWiFi();
}


//------------------------------------------------------------------------------------ 
// WiFi loop
//------------------------------------------------------------------------------------
char    strConnectTimestamp[21];

void loopCheckWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    
    SERIAL.println( "Wi-Fi is disconnected. Reconnecting..." );
    connectWiFi();  
  
    strcat( message, "IP Address = " );
    strcat( message, strIPAddress );

    SERIAL.println( message );
  }
}


//------------------------------------------------------------------------------------ 
// OTA update setup
//------------------------------------------------------------------------------------ 
void setupOTA() {
 
  ArduinoOTA.setHostname( "ESP32_LyraT_DSP" );

  ArduinoOTA.onStart([]() {
    String      type;
    if( ArduinoOTA.getCommand() == U_FLASH ) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    SERIAL.println( "Start updating " + type );
  });
  
  ArduinoOTA.onEnd([]() {  
    SERIAL.println( "\nEnd" );
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    SERIAL.printf( "Progress: %u%%\r", ( progress/(total/100) ) );
  });
  
  ArduinoOTA.onError([]( ota_error_t error ) {
    SERIAL.printf( "Error[%u]: ", error );
    if( error == OTA_AUTH_ERROR ) {
      SERIAL.println( "Auth Failed" );
    } else if( error == OTA_BEGIN_ERROR ) {
      SERIAL.println( "Begin Failed" );
    } else if (error == OTA_CONNECT_ERROR) {
      SERIAL.println( "Connect Failed" );
    } else if (error == OTA_RECEIVE_ERROR) {
      SERIAL.println( "Receive Failed" );
    } else if (error == OTA_END_ERROR) {
      SERIAL.println( "End Failed" );
    }
  });
  
  ArduinoOTA.begin();
}


//------------------------------------------------------------------------------------ 
// OTA update loop
//------------------------------------------------------------------------------------

void loopArduinoOTA() {
  ArduinoOTA.handle();  
}


//------------------------------------------------------------------------------------ 
// Main setup
//------------------------------------------------------------------------------------ 
void setup() {
  setupTelnetSpy();    
  setupSerial();
  setupWiFi();  
  setupOTA();  
  if( dsp_init() != ESP_OK ) {
    dsp_init_OK = false;
  }  
}


//------------------------------------------------------------------------------------ 
// Main loop
//------------------------------------------------------------------------------------
void loop() {
  loopCheckWiFi();
  loopTelnetSpy(); 
  loopArduinoOTA();
  loopSerialInput();

  // DSP processing
  if( dsp_init_OK ) {
    dsp_loop();
  }
}
