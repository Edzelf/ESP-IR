// IR_WiFi_esp -- Remote control for TV, DVD, etc.
//
// Webserver receiving commands that will be converted to IR signals to control TV/DVD/Amplifier.
// It will drive the IR LEDs to simulate the remote controls.
// The webserver receives commands from the javascript (supplied in the index.html file on the server)
// running on a tablet or similar.
// The remote control can also simulate a PT2261-like remote control (433 MHz) for on/off swiching of lights.
//
// The original version of this program used an ESP8266 and an Arduino for this project.  At a later stage
// the functions are combined and only an ESP8266 is used.
//
// 15-04-2015, ES: First set-up
// 14-10-2015, ES: Find strongest WiFi signal
// 15-10-2015, ES: Show hostname on netwerk
// 09-03-2016, ES: Selective choice of WiFi netwerk
// 31-05-2016, ES: Combine ESP8266 and Arduino functions
//
// Wiring:
// NodeMCU  GPIO    Pin to program  Connect to
// -------  ------  --------------  --------------
// D1       GPIO0    0              433 MHz module
// D2       GPIO4    4              IR leds driver
//
#include <IRremoteESP8266.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <stdio.h>
#include <string.h>
#include <FS.h>
#include <ArduinoOTA.h>
extern "C"
{
  #include "user_interface.h"
}


//******************************************************************************************
// Some definitions                                                                        *
//******************************************************************************************
// Debug buffer size
#define DEBUG_BUFFER_SIZE 100
// Version number
#define VERSION "31-may-2016"
// Output pins
#define IR_PIN 4
#define RF_PIN 0

//******************************************************************************************
// Forward declaration of methods                                                          *
//******************************************************************************************
char* dbgprint( const char* format, ... ) ;


//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
bool            myDEBUG = true ;          // Enable or disable debug output
String          ssid ;                    // SSID of selected WiFi network
WiFiServer      server(80) ;              // A server on port 80
AsyncWebServer  cmdserver ( 80 ) ;        // Instance of embedded webserver
IRsend          irsend ( IR_PIN ) ;       // Object for IR sender
bool            cmdreq = false ;          // Command received by server
String          argument[5] ;             // Arguments in command received by server


//******************************************************************************************
// End of lobal data section.                                                              *
//******************************************************************************************


//******************************************************************************************
//                                  D B G P R I N T                                        *
//******************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the BEDUg flag.*
// Print only if myDEBUG flag is true.  Always returns the the formatted string.           *
//******************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list     varArgs ;                                // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( myDEBUG )                                       // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}


//******************************************************************************************
//                                   H E X T O L O N G                                     *
//******************************************************************************************
// Convert string to unsigned long.                                                        *
//******************************************************************************************
unsigned long hextolong ( const char* hexstr )
{
  unsigned long res = 0 ;

  while ( *hexstr )
  {
    if ( *hexstr >= '0' && *hexstr <= '9' )
    {
      res = ( res << 4 ) + *hexstr - '0' ;
    }
    else if ( *hexstr >= 'A' && *hexstr <= 'F' )
    {
      res = ( res << 4 ) + *hexstr - 'A' + 10 ;
    }
    hexstr++ ;
  }
  return res ;
}


//******************************************************************************************
//                                    S N D I T V                                          *
//******************************************************************************************
// Send code to KPN itv.                                                                   *
// Command is coded like "mSMSmsMSMSmsmsmsmsmsmsmsmsmsmsmsmsMSm".                          *
//******************************************************************************************
void snditv ( const char* str )
{
  unsigned int sndbuf[50] ;
  int          i = 0 ;

  while ( *str )                          // Convert all characters
  {
    switch ( *str )
    {
      case 's' :                          // Short space
        sndbuf[i] = 317 ;
        break ;
      case 'S' :                          // Long space
        sndbuf[i] = 634 ;
        break ;
      case 'm' :                          // Short mark
        sndbuf[i] = 317 ;
        break ;
      case 'M' :                          // Long mark
        sndbuf[i] = 634 ;
        break ;
    }
    i++ ;
    str++ ;
  }
  irsend.sendRaw ( sndbuf, i, 56 ) ;      // Send code 56 kHz
}



//******************************************************************************************
//                               S N D 4 3 3 C O D E                                       *
//******************************************************************************************
// Send a frame to the 433 MHz module.                                                     *
// pw   is the (shortest) pulsewidth, for example 150 microseconds.                        *
// cmd  is a string containing one character for every bit to send: '0', '1' or 'f', for   *
//         example: "111110fffff0" for "A on".                                             *
// rept is the number of times to repeat, for exaple 8.                                    *
//******************************************************************************************
void send433code ( int pw, const char *cmd, int rept )
{
  int pw3 = 3 * pw ;    // Timing for long pulse/pause
  int i, j ;

  for ( i = 0 ; i < rept ; i++ )
  {
    for ( j = 0 ; cmd[j] ; j++ )  // Stops at end of string
    { 
      switch ( cmd[j] )
      {
        case '0' :                // Short pulse, long pause, short puls, long pause
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw3 ) ;
    digitalWrite( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw3 ) ;
    break;
  case '1' :                // Long pulse, short pause, long pulse, short pause
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw3 ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw ) ;
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw3 ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw ) ;
    break;
  case 'f' :                // short pulse, long pause, long pulse, short pause
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw3 ) ;
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw3 ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw ) ;
    break;
      }
    }
    // Send termination/synchronization-signal. Total length: 32 periods
    digitalWrite ( RF_PIN, HIGH ) ;
    delayMicroseconds ( pw ) ;
    digitalWrite ( RF_PIN, LOW ) ;
    delayMicroseconds ( pw * 31 ) ;
  }
}


//******************************************************************************************
//                             G E T E N C R Y P T I O N T Y P E                           *
//******************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                     *
//*********************4********************************************************************
const char* getEncryptionType ( int thisType )
{
  switch (thisType) 
  {
    case ENC_TYPE_WEP:
      return "WEP " ;
    case ENC_TYPE_TKIP:
      return "WPA " ;
    case ENC_TYPE_CCMP:
      return "WPA2" ;
    case ENC_TYPE_NONE:
      return "None" ;
    case ENC_TYPE_AUTO:
      return "Auto" ;
  }
  return "????" ;
}


//******************************************************************************************
//                                L I S T N E T W O R K S                                  *
//******************************************************************************************
// List the available networks and select the strongest.                                   *
// Acceptable networks are those who have a "SSID.pw" file in the SPIFFS.                  *
//******************************************************************************************
void listNetworks()
{
  int         maxsig = -1000 ;   // Used for searching strongest WiFi signal
  int         newstrength ;
  byte        encryption ;       // TKIP(WPA)=2, WEP=5, CCMP(WPA)=4, NONE=7, AUTO=8 
  const char* acceptable ;       // Netwerk is acceptable for connection
  int         i, j ;
  bool        found ;            // True if acceptable network found
  String      path ;             // Full filespec to see if SSID is an acceptable one
  
  // scan for nearby networks:
  dbgprint ( "* Scan Networks *" ) ;
  int numSsid = WiFi.scanNetworks() ;
  if ( numSsid == -1 )
  {
    dbgprint ( "Couldn't get a wifi connection" ) ;
    return ;
  }
  // print the list of networks seen:
  dbgprint ( "Number of available networks: %d",
            numSsid ) ;
  // Print the network number and name for each network found and
  // find the strongest acceptable network
  for ( i = 0 ; i < numSsid ; i++ )
  {
    acceptable = "" ;                                    // Assume not acceptable
    path = String ( "/" ) + WiFi.SSID ( i ) + String ( ".pw" ) ;
    newstrength = WiFi.RSSI ( i ) ;
    if ( found = SPIFFS.exists ( path ) )                // Is this SSID acceptable?
    {
      acceptable = "Acceptable" ;
      if ( newstrength > maxsig )                        // This is a better Wifi
      {
        maxsig = newstrength ;
        ssid = WiFi.SSID ( i ) ;                         // Remember SSID name
      }
    }
    encryption = WiFi.encryptionType ( i ) ;
    dbgprint ( "%2d - %-25s Signal: %3d dBm Encryption %4s  %s",
               i + 1, WiFi.SSID ( i ).c_str(), WiFi.RSSI ( i ),
               getEncryptionType ( encryption ),
               acceptable ) ;
  }
  dbgprint ( "------------------------------------------------------------------" ) ;
}


//******************************************************************************************
//                               C O N N E C T W I F I                                     *
//******************************************************************************************
// Connect to WiFi using passwords available in the SPIFFS.                                *
//******************************************************************************************
void connectwifi()
{
  String path ;                                        // Full file spec
  String pw ;                                          // Password from file
  File   pwfile ;                                      // File containing password for WiFi
  char*  pfs ;                                         // Poiter to formatted string
  
  path = String ( "/" )  + ssid + String ( ".pw" ) ;   // Form full path
  pwfile = SPIFFS.open ( path, "r" ) ;                 // File name equal to SSID
  pw = pwfile.readStringUntil ( '\n' ) ;               // Read password as a string
  pw.trim() ;                                          // Remove CR                              
  WiFi.begin ( ssid.c_str(), pw.c_str() ) ;            // Connect to selected SSID
  dbgprint ( "Try WiFi %s", ssid.c_str() ) ;           // Message to show during WiFi connect
  if (  WiFi.waitForConnectResult() != WL_CONNECTED )  // Try to connect
  {
    dbgprint ( "WiFi Failed!" ) ;
    return;
  }
  pfs = dbgprint ( "IP = %d.%d.%d.%d",
                   WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
  #if defined ( USETFT )
  tft.println ( pfs ) ;
  #endif
}


//******************************************************************************************
//                                   O T A S T A R T                                       *
//******************************************************************************************
// Update via WiFi has been started by Arduino IDE.                                        *
//******************************************************************************************
void otastart()
{
  dbgprint ( "OTA Started" ) ;
}


//******************************************************************************************
//                             G E T C O N T E N T T Y P E                                 *
//******************************************************************************************
// Returns the contenttype of a file to send.                                              *
//******************************************************************************************
String getContentType ( String filename )
{
  if      ( filename.endsWith ( ".html" ) ) return "text/html" ;
  else if ( filename.endsWith ( ".png"  ) ) return "image/png" ;
  else if ( filename.endsWith ( ".gif"  ) ) return "image/gif" ;
  else if ( filename.endsWith ( ".jpg"  ) ) return "image/jpeg" ;
  else if ( filename.endsWith ( ".ico"  ) ) return "image/x-icon" ;
  else if ( filename.endsWith ( ".zip"  ) ) return "application/x-zip" ;
  else if ( filename.endsWith ( ".gz"   ) ) return "application/x-gzip" ;
  else if ( filename.endsWith ( ".pw"   ) ) return "" ;              // Passwords are secret
  return "text/plain" ;
}


//******************************************************************************************
//                                H A N D L E F S                                          *
//******************************************************************************************
// Handling of requesting files from the SPIFFS. Example: /favicon.ico                     *
//******************************************************************************************
void handleFS ( AsyncWebServerRequest *request )
{
  String fnam ;                                         // Requested file
  String ct ;                                           // Content type

  fnam = request->url() ;
  dbgprint ( "onFileRequest received %s", fnam.c_str() ) ;
  ct = getContentType ( fnam ) ;                        // Get content type
  if ( ct == "" )                                       // Empty is illegal
  {
    request->send ( 404, "text/plain", "File not found" ) ;  
  }
  else
  {
    request->send ( SPIFFS, fnam, ct ) ;                // Okay, send the file
  }
}


//******************************************************************************************
//                             H A N D L E I R C M D                                       *
//******************************************************************************************
// Handling of the various IR / RF433 commands available in array arguments.               *
//******************************************************************************************
void handleIRCmd()
{
  unsigned long   li ;                                 // Code as unsigned long int
  
  if ( argument[0] == "samsung" )                      // Samsung DVD request?
  {
    li = hextolong ( argument[1].c_str() ) ;           // Convert code to long int
    irsend.sendSAMSUNG ( li, 32 ) ;                    // Send 32 bits SAMSUNG format
  }
  else if ( argument[0] == "kpnitv" )                  // KPN itv request?
  {
    snditv ( argument[1].c_str() ) ;                   // Send KPN itv format
  }
  else if ( argument[0] == "lgtv" )                    // LG tv request?
  {
    li = hextolong ( argument[1].c_str() ) ;           // Convert code to long int
    irsend.sendNEC ( li, 32 ) ;                        // Send 32 bits NEC format
  }
  else if ( argument[0] == "rf433" )                   // RF433 request?
  {
    send433code ( argument[1].toInt(), argument[2].c_str(), argument[3].toInt() ) ;
  }
}


//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// A command is received by the server.  IR commands are not executed directly; they are   *
// handled in the main loop.  All commands contain at least one argument like "/?xxxx" or  *
// "/?xxxx&aaaa&bbbb".                                                                     *
// The maximal number of parameters is 5. They will be stored in array "arguments" and the *
// cmdreq flag is set to signal the main loop that a command is ready to be executed.      *
// The startpage will be returned if no arguments are given.                               *
// Non-IR commands are "debugon", "debugoff" and "test".                                   *
// An extra argument may be "version=<random number>", which will be ignored in order to   *
// prevent browsers like Edge and IE to use their cache.                                   *
// IR commands, examples:                                                                  *
//    "/?samsung&C2CA649B&power&version=0.9775479450590543" or                             *
//    "/?kpnitv&mSMSmsMSMSmsmsmsmsmsmsmsmsmsmsmsmsMSm&one&version=0.12"                    *
// The 1st argument tells us with protocol to use, the 2nd is the code to be transmitted.  *
// The 3rd argument is the command, like "power", "one" or "return". The command is not    *
// used in this sketch.                                                                    *
//******************************************************************************************
void handleCmd ( AsyncWebServerRequest *request )
{
  AsyncWebParameter* p ;                              // Points to parameter structure
  static char*       reply ;                          // Reply to client
  int                numargs ;                        // Number of arguments
  int                i ;                              // For loop through arguments 
  
  numargs = request->params() ;                       // Get number of arguments
  if ( numargs >  5 )                                 // Max. 5 arguments
  {
    numargs = 5 ;
  }
  if ( numargs == 0 )                                 // Any arguments?
  {
    request->send ( SPIFFS, "/index.html",            // No parameters, send the startpage
                    "text/html" ) ;
    return ;
  }
  if ( cmdreq )                                       // Check completion of previous command
  {
    dbgprint ( "Last IR command not finished!" ) ;    // Command coming in too fast...
  }
  for ( i = 0 ; i < numargs ; i++ )                   // Put parameters in array
  {
    p = request->getParam ( i ) ;                     // Get pointer to parameter structure
    argument[i] = p->name() ;                         // Get the argument
    dbgprint ( "Command[%d] is %s", i, argument[i].c_str() ) ;
  }
  argument[0].toLowerCase() ;                         // Force lowercase for first argument
  // Default reply
  reply = dbgprint ( "Command %s accepted", argument[0].c_str()  ) ;
  if ( argument[0] == "reset" )                       // Reset request?
  {
    ESP.restart() ;                                   // Reset all
    // No continuation here......
  }
  else if ( argument[0] == "test" )                   // Test command
  {
    reply = dbgprint ( "Free memory is %d", system_get_free_heap_size() ) ;
  }
  else if ( argument[0] =="debugon" )                 // debug on request?
  {
    myDEBUG = true ;                                  // Yes, set flag
  }
  else if ( argument[0] == "debugoff" )               // debug off request?
  {
    myDEBUG = false ;                                 // Yes, clear flag
  }
  else
  {
    // other commands (IR) will be handled in the main loop
    cmdreq = true ;
  }
  request->send ( 200, "text/plain", reply ) ;        // Send the reply
}


//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{
  FSInfo      fs_info ;                              // Info about SPIFFS
  Dir         dir ;                                  // Directory struct for SPIFFS
  File        f ;                                    // Filehandle
  String      filename ;                             // Name of file found in SPIFFS
  String      potSSID ;                              // Potential SSID if only 1 one password file
  int         numpwf = 0 ;                           // Number of password files 
  int         i ;                                    // Loop control

  irsend.begin() ;                                   // Set output pin for IR
  Serial.begin ( 115200 ) ;                          // For debugging
  Serial.println() ;
  pinMode ( RF_PIN, OUTPUT ) ;                       // Init I/O pin
  digitalWrite ( RF_PIN, LOW ) ;
  system_update_cpu_freq ( 80 ) ;                    // Set to 80/160 MHz
  SPIFFS.begin() ;                                   // Enable file system
  SPIFFS.info ( fs_info ) ;                          // Load info
  dbgprint ( "FS Total %d, used %d", fs_info.totalBytes, fs_info.usedBytes ) ;
  if ( fs_info.totalBytes == 0 )
  {
    dbgprint ( "No SPIFFS found!  See documentation." ) ;
  }
  dir = SPIFFS.openDir("/") ;                        // Show files in FS
  while ( dir.next() )                               // All files
  {
    f = dir.openFile ( "r" ) ;
    filename = dir.fileName() ;
    dbgprint ( "%-32s - %6d",                        // Show name and size
               filename.c_str(), f.size() ) ;
    if ( filename.endsWith ( ".pw" ) )               // If this a password file?
    {
      numpwf++ ;                                     // Yes, count number password files
      potSSID = filename.substring ( 1 ) ;           // Save filename (without starting "/") of potential SSID 
      potSSID.replace ( ".pw", "" ) ;                // Convert into SSID 
    }
  }
  WiFi.mode ( WIFI_STA ) ;                           // This ESP is a station
  wifi_station_set_hostname ( (char*)"ESP-IR" ) ;    // Set hostname
  // Print some memory and sketch info
  dbgprint ( "Starting ESP Version " VERSION "...  Free memory %d",
             system_get_free_heap_size() ) ;
  dbgprint ( "Sketch size %d, free size %d",
              ESP.getSketchSize(),
              ESP.getFreeSketchSpace() ) ;
  listNetworks() ;                                   // Search for strongest WiFi network  if ( numpwf == 1 )                                 // If there's only one pw-file...
  if ( numpwf == 1 )                                 // If there's only one pw-file...
  {
    dbgprint ( "Single (hidden) SSID found" ) ;
    ssid = potSSID ;                                 // Use this SSID (it may be hidden)
  }
  dbgprint ( "Selected network: %-25s", ssid.c_str() ) ;
  connectwifi() ;                                    // Connect to WiFi network
  dbgprint ( "Start server for commands" ) ;
  cmdserver.on ( "/", handleCmd ) ;                  // Handle startpage
  cmdserver.onNotFound ( handleFS ) ;                // Handle file from FS
  cmdserver.begin() ;
  ArduinoOTA.setHostname ( "ESP-IR" ) ;              // Set the hostname
  ArduinoOTA.onStart ( otastart ) ;
  ArduinoOTA.begin() ;                               // Allow update over the air
}


//******************************************************************************************
//                                   L O O P                                               *
//******************************************************************************************
// Main loop of the program.                                                               *
//******************************************************************************************
void loop()
{
  String req ;
  int    pos ;

  if ( cmdreq )                                      // New command waiting?
  {
    handleIRCmd() ;                                  // Yes, handle it
    cmdreq = false ;                                 // Clear flag
  }
  // Check de WiFi status.  Als die wegvalt zorgt de watchdog voor reset
  if ( WiFi.status() != WL_CONNECTED )
  {
    dbgprint ( "WiFi disconnected, going to restart..." ) ;
    ESP.restart() ;                                  // Reset the CPU, probably no return
  }
  ArduinoOTA.handle() ;                              // Check for OTA
}

