//////////////////////////////////////////////////////////////////////////////////////////
//
// ftcSoundBar Library
//
// Communicate via ROBOPro from fischertechnik TXT Controller with ftcSoundBar
//
// Version 1.31
//
// (C) 2020/21 Oliver Schmiel, Christian Bergschneider & Stefan Fuss
//
// compile to libftcSoundBar.so and copy it as User ROBOPRO to /opt/knobloch/libs
//
/////////////////////////////////////////////////////////////////////////////////////////

// Version 1.31
//
// - API Update firmware 1.31
// - refactoring json handling without jsmn.h due to stability problems 
// - serialzeREST API requests dur to stability problems

// Version 1.20:
//
// get* functions are fully reentrant now
// if DNS resulution of ftcSoundBar fails, use 192.168.8.100 instead
// getLastError eleminated

using namespace std;

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <semaphore.h>

// Library version
const double MyVersion = 1.31;

// Error codes
#define COM_OK                      0
#define COM_ERR_OpenSocket         -1
#define COM_ERR_ResolveHostname    -2
#define COM_ERR_Connect            -3
#define COM_ERR_Send               -4
#define COM_ERR_ContentTypeMissing -5
#define COM_ERR_ContentUncomplete  -6

// FT Codes
#define FISH_OK  0
#define FISH_ERR 1

// simple semaphore handling to serialze communication

sem_t *mutex = NULL;

// request mutex and initialize mutix on first call
void request_mutex( void ) {
	
	sem_t x;
	
	// initialize mutex if needed
	if (mutex == NULL) {
		mutex=(sem_t*)malloc(sizeof(x));
		sem_init(mutex, 0, 1);
	}
	
	// get semaphore
	sem_wait(mutex); 
	
}

// release mutex
void release_mutex( void ) {
	
	if (mutex != NULL ) {
		sem_post(mutex);
	}
	
}

// define a ip address union

union in_addr2 {
  unsigned long s_addr;
  uint8_t       octed[4];
};

// simplyfied JSON handling, simple data types only

#define MAXJSON 2000

class JSON {
	public:
	    char jsonData[MAXJSON];
		JSON();
		int GetParam( char *tag, char *value );
		int GetParam( char *tag, short *value );
		
};

JSON::JSON() {
	
	bzero( (char *) jsonData, MAXJSON );

}

int JSON::GetParam( char *tag, char *value ) {
	// search for tag and return it's value
	// if value is a string, strip trainling and ending "'s\n
	// return 0 if tag is found, -1 on error
	
	FILE *f = fopen( "/tmp/ftcSoundBar.log", "wa" );
	
	char delimiters[] = "\t\n {}:,";
	char *ptr;
	
	// complete search string to "tag"
	char needle[200];
	
	needle[0] = '"';
	strcpy( &needle[1], tag );
	needle[strlen(tag)+1] = '"';
	needle[strlen(tag)+2] = '\0';
	
	// split header into lines
    ptr = strtok( jsonData, delimiters);
	
	// analyze tags and values
    while ( ptr != NULL ) {
		
		fprintf( f, "tag=%s\n", ptr ); fflush(f);
		
		fprintf( f, "tag=%s needle=%s\n", ptr, needle ); fflush(f);
		
		if ( strcmp( ptr, needle ) == 0) {
			// tag found, next field is value
			
			fprintf( f, "%s found", ptr ); fflush(f);
			
			ptr = strtok(NULL, delimiters);
			
			fprintf( f, "next object: %s", ptr ); fflush(f);
			
			if ( ptr == NULL ) {
				// no data found
				
				value[0] = '\0';
				fprintf( f, "no data found.\n" ); fclose( f );
				return 0;
				
			} else {
				
				// data found
				fprintf( f, "value: %s", ptr ); fclose( f );
				
				// copy data
				strcpy( value, ptr );
				
				return 0;
				
			}
		}
		
		// get next line
		ptr = strtok(NULL, delimiters);
	}
	
	fclose(f);
	return -1;
	
}

int JSON::GetParam( char *tag, short *value ){
	// search for tag and return it's value
	// if value is NAN, the function returns 0
	// return 0 if tag is found, -1 on error

	char temp[100];
	int  err;

	err = GetParam( tag, temp);
	if ( err == 0) {
		*value = atoi( temp );
	}
  
	return err;
  
}

// Rest server to communicate with ftcSoundBar

class RESTServer {
	private:
	    in_addr2 ip;
		char  hostname[20];
		short port;
		void  updateHostname();
		int   tcp_send_receive( char *request, char *responseHeader, int s_responseHeader, char *responseData, int s_responseData );
	public:
	    RESTServer();
		void  setIP( unsigned long newIP );
		short getOcted( short octed);
		void  setOcted( short octed, short value  );
		void  setPort( short newPort );
		short getPort( void );
		char  *getHostname();
		int   http_get( char serviceMethod[], JSON *jsonData );
		int   http_post( char serviceMethod[], char jsonData[] );
};

RESTServer ftcSoundBar;

/**
 * @brief      constructor, tries to get a dns reolution for ftcSoundBar
 *
 * @param      no parameters
 *
 * @return     no result
 */
RESTServer::RESTServer() {
	// initialize structure
	
	// set default port to 80
	port = 80;
	
	// scan IP
	struct hostent *ftcSoundBar;
	  
	// ask DNS for ftcSoundBar's IP
	ftcSoundBar = gethostbyname("ftcSoundBar");
	
	ip.s_addr = 0;
	  
	if ( ( ftcSoundBar != NULL ) && ( ftcSoundBar->h_addrtype == AF_INET ) && ( ftcSoundBar->h_addr_list[0] != 0 ) ) { 
		// DNS ok, IPv4
		ip.s_addr = *((unsigned long*) ftcSoundBar->h_addr_list[0]);
    } else {
		// no DNS, assume TXT in AP Mode. ftcSoundBar has now 192.168.8.100
		ip.octed[0] = 192;
		ip.octed[1] = 168;
		ip.octed[2] = 8;
		ip.octed[3] = 100;
		
	}	
	
	// update hostname string
	updateHostname();
	
	port = 80;

}

/**
 * @brief      get an octed [0..3] of the ftcSoundBar's IP address
 *
 * @param[in]  octed	number of the octed
 *
 * @return
 *     - octed
 */
short RESTServer::getOcted( short octed ) {
	
	return ip.octed[octed];
	
}

/**
 * @brief      set an octed [0..3] of the ftcSoundBar's IP address
 *
 * @param[in]  octed	number of the octed
 *             value    value of the octed
 *
 * @return	   noting
 */
void RESTServer::setOcted( short octed, short value ) {
	
	ip.octed[octed] = value;
	
    // update hostname string
	updateHostname();
	
}
	
/**
 * @brief      set the ftcSoundBar's IP address
 *
 * @param[in]  newIP	IP Address
 *
 * @return	   noting
 */
void RESTServer::setIP( unsigned long newIP ) {

	ip.s_addr = newIP;
	
	// update hostname string
	updateHostname();
	
}

/**
 * @brief      set the port of the ftcSoundBar
 *
 * @param[in]  port		port number
 *
 * @return	   noting
 */
void RESTServer::setPort( short newPort ) {
	port = newPort;
}

/**
 * @brief      get the port of the ftcSoundBar
 *
 * @param[in]  noting
 *
 * @return	   port number
 */
short RESTServer::getPort( void ) {
	return port;
}

/**
 * @brief      get the hostname based on an IP address in format xxx.xxx.xxx.xxx
 *
 * @param[in]  noting
 *
 * @return	   hostname
 */
char *RESTServer::getHostname() {
	return hostname;
}

/**
 * @brief      internal function to update the internal hostname-string after a change on the IP Address
 *
 * @param[in]  noting
 *
 * @return	   nothing
 */
void RESTServer::updateHostname() {
	
	sprintf( hostname, "%u.%u.%u.%u", ip.octed[0], ip.octed[1], ip.octed[2], ip.octed[3] );

}

/**
 * @brief      sends a request to the RESTServer and returns the response (header + data)
 *
 * @param[in]  request		request
 *			   responseHeader	header of the response
 *			   s_responseHeader size of responseHeader
 *			   responseData     data of the response
 *			   s_responseData   size of responseData
 *
 * @return
 *		- COM_OK
 *		- COM_ERR_Connect
 *	    - COM_ERR_Send
 */
int RESTServer::tcp_send_receive( char *request, char *responseHeader, int s_responseHeader, char *responseData, int s_responseData ) {
	
	struct sockaddr_in serveraddr;
	
	int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
	
	// initialize serveraddr
	bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
 
    // set IP
    serveraddr.sin_addr.s_addr = ip.s_addr;
    
	// set port
    serveraddr.sin_port = htons(port);
    
	// connect
    if (connect(tcpSocket, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
		return COM_ERR_Connect;
	}
	
	// send
    if (send(tcpSocket, request, strlen(request), 0) < 0) {
		return COM_ERR_Send;
	}
	
    // clear response
    bzero(responseHeader, s_responseHeader);
    bzero(responseData,   s_responseData);
	
	// copy value, expect 2 packets
    #ifdef LOGFILE
	int header = recv(tcpSocket, responseHeader, s_responseHeader-1, 0);
	int data   = recv(tcpSocket, responseData,   s_responseData-1,   0);
	
	# else
	recv(tcpSocket, responseHeader, s_responseHeader-1, 0);
	recv(tcpSocket, responseData,   s_responseData-1,   0);
	
	#endif
	
	close(tcpSocket);
	
	#ifdef LOGFILE
	FILE *f = fopen( "/tmp/ftcSoundBar.log", "aw");
    
	fprintf( f, "******\n" ); fflush(f);
	fprintf( f, "header: %d\n", header ); fflush(f);
	fprintf( f, "data: %d\n", data ); fflush(f);
	
	char x[1010];
	char y[1010];
	
	if ( (header > 0 ) && ( header < 1000 ) ) {
	
		strncpy( x, responseHeader, 1000 );
		x[999] = '\0';
		fprintf( f, "sheader: %s\n", x );
		fflush(f);
	}
	
	if ( (data > 0 ) && ( data < 1000 ) ) {
	
		strncpy( y, responseData, 1000 );
		y[999] = '\0';
		fprintf( f, "sdata: %s\n", y );
		fflush(f);
	}
	
	fclose(f);
	#endif
	
	return COM_OK;

}

/**
 * @brief      sends a get-request to the RESTServer and stores the result internally
 *
 * @param[in]  serviceMethod	method to call
 *
 * @return
 *		- COM_OK
 *		- COM_ERR_Connect
 *	    - COM_ERR_Send
 *		- COM_ERR_ContentTypeMissing
 *      - COM_ERR_ContentUncomplete
 */
int RESTServer::http_get( char serviceMethod[], JSON *jsonData )
{
	// needed buffers
	char request[1000];
	char responseHeader[1000];
	int  com_status;
	
	// build command
    sprintf(request, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", serviceMethod, hostname);
	
	// send command
	com_status = tcp_send_receive( request, responseHeader, 1000, jsonData->jsonData, MAXJSON );
	if ( com_status != COM_OK ) {
		return com_status;
	}
	
	// start checking response header
	char delimiter[] = "\r\n";
    char *ptr;
	int  status = 0;
    bool contentLength = false;
    bool contentType = false;
	char temp[20];
	
	// split header into lines
    ptr = strtok(responseHeader, delimiter);
	
	// analyze lines in header
    while(ptr != NULL) {
		
		if ( strncmp( ptr, "HTTP/1.1 ", 9 ) == 0) {
			// line starts with HTTP... check for HTTP status
			strncpy( temp, &(ptr[9]), 3 );
			status = atoi(temp);
		}

		if ( strcmp( ptr, "Content-Type: application/json" ) == 0) {
			// Content-Type json
			contentType = true;
		} 

		if ( strncmp( ptr, "Content-Length: ", 16 ) == 0) {
			// check Content-Length. Attention: ContentLength starting with {
			contentLength = ( (unsigned int)atoi( &(ptr[16]) ) == strlen( strstr( jsonData->jsonData, "{" ) ) );			
		}

		// get next line
		ptr = strtok(NULL, delimiter);
	}

	if ( !contentType ) {
		return COM_ERR_ContentTypeMissing;
		
	} else if ( !contentLength ) {
		return COM_ERR_ContentUncomplete;
		
	} 

	return status;
	
}



/**
 * @brief      post a request to the RESTServer
 *
 * @param[in]  serviceMethod	method to call
 *			   jsonData			json-string to pass
 *
 * @return
 *		- COM_OK
 *		- COM_ERR_Connect
 *	    - COM_ERR_Send
 */
int RESTServer::http_post( char serviceMethod[], char jsonData[] )
{
	char request[1000];
	char responseHeader[250];
	char responseData[250];
	
	sprintf(request, "POST /%s HTTP/1.1\r\nHOST: %s:%d\r\nContent-Type:application/json\r\nAccept:*/*\r\nContent-Length:%d\r\n\r\n%s", 
						serviceMethod, 
						hostname, 
						port,
						strlen(jsonData), 
						jsonData);
	
	return tcp_send_receive( request, responseHeader, 250, responseData, 250 );
	
}

extern "C" {

  /**
   * @brief      get the internal library version
   *
   * @param[in]  Version	Version number
   *
   * @return
   *		- FISH_OK
   */	
  int getVersion(double *Version)
  // gets the libs Version
  {
	  
	  *Version = MyVersion;
	  
	  return FISH_OK;
  }

  
   /**
   * @brief      set ftcSoundBar's port
   *
   * @param[in]  port	port number
   *
   * @return
   *		- FISH_OK
   */	
  int setPort(short port) {
	  // set servers port
	  request_mutex();
	  ftcSoundBar.setPort( port );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      get ftcSoundBar's port
   *
   * @param[in]  port	port number
   *
   * @return
   *		- FISH_OK
   */	
  int getPort(short *port) {
	  request_mutex();
	  *port = ftcSoundBar.getPort( );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      get ftcSoundBar's IP address/1st octed
   *
   * @param[in]  octed	value of the octed
   *
   * @return
   *		- FISH_OK
   */	
  int getIP0(short *octed) {
	  request_mutex();
	  *octed = ftcSoundBar.getOcted( 0 );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      get ftcSoundBar's IP address/2nd octed
   *
   * @param[in]  octed	value of the octed
   *
   * @return
   *		- FISH_OK
   */  
  int getIP1(short *octed) {
	  request_mutex();
	  *octed = ftcSoundBar.getOcted( 1 );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      get ftcSoundBar's IP address/3rd octed
   *
   * @param[in]  octed	value of the octed
   *
   * @return
   *		- FISH_OK
   */  
    int getIP2(short *octed) {
	  request_mutex();
	  *octed = ftcSoundBar.getOcted( 2 );
	  release_mutex();
	  return FISH_OK;
  }
 
  /**
   * @brief      get ftcSoundBar's IP address/4th octed
   *
   * @param[in]  octed	value of the octed
   *
   * @return
   *		- FISH_OK
   */ 
    int getIP3(short *octed) {
	  request_mutex();
	  *octed = ftcSoundBar.getOcted( 3 );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      set ftcSoundBar's IP address/1st octed
   *
   * @param[in]  ip		value of the octed
   *
   * @return
   *		- FISH_OK
   */  
  int setIP0(short ip) {
	  // set 1st octed of server IP
	  request_mutex();
	  ftcSoundBar.setOcted( 0, ip );
	  release_mutex();
	  return FISH_OK;
  }
 
  /**
   * @brief      set ftcSoundBar's IP address/2nd octed
   *
   * @param[in]  ip		value of the octed
   *
   * @return
   *		- FISH_OK
   */   
  int setIP1(short ip) {
	  // set 2nd octed of server IP
	  request_mutex();
	  ftcSoundBar.setOcted( 1, ip );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      set ftcSoundBar's IP address/3rd octed
   *
   * @param[in]  ip		value of the octed
   *
   * @return
   *		- FISH_OK
   */  
  int setIP2(short ip) {
	  // set 3rd octed of server IP
	  request_mutex();
	  ftcSoundBar.setOcted( 2, ip );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      set ftcSoundBar's IP address/4th octed
   *
   * @param[in]  ip		value of the octed
   *
   * @return
   *		- FISH_OK
   */  
  int setIP3(short ip) {
	  // set 4th octed of server IP
	  request_mutex();
	  ftcSoundBar.setOcted( 3, ip );
	  release_mutex();
	  return FISH_OK;
  }

  /**
   * @brief      play track
   *
   * @param[in]  track	track number [1..n]
   *
   * @return
   *		- FISH_OK
   */ 
  int play(short track) {
	  // play track #track
	  
	  request_mutex();
	  
	  char jsonData[100];
	  
	  sprintf( jsonData, "{\"track\": %hi}", track );
	  
	  ftcSoundBar.http_post( (char *) "api/track/play", jsonData );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      set volume
   *
   * @param[in]  volumne
   *
   * @return
   *		- FISH_OK
   */ 
  int setVolume(short volume) {
	  // set volumne
	  
	  request_mutex();
	  
	  char jsonData[100];
	  
	  sprintf( jsonData, "{\"volume\": %hi}", volume );
	  
	  ftcSoundBar.http_post( (char *) "api/volume", jsonData );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      stop the active track
   *
   * @param[in]  none
   *
   * @return
   *		- FISH_OK
   */ 
  int stopTrack(short dummy) {
	  // stops the actual track
	  
	  request_mutex();
	  
	  ftcSoundBar.http_post( (char *) "api/track/stop", (char *)"" );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      pauses the active track
   *
   * @param[in]  none
   *
   * @return
   *		- FISH_OK
   */ 
  int pauseTrack(short dummy) {
	  // pauses the actual track
	  
	  request_mutex();
	  
	  ftcSoundBar.http_post( (char *) "api/track/pause", (char *)"" );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      resumes the active track
   *
   * @param[in]  none
   *
   * @return
   *		- FISH_OK
   */ 
  int resumeTrack(short dummy) {
	  // continues the actual track
	  
	  request_mutex();
	  
	  ftcSoundBar.http_post( (char *) "api/track/resume", (char *)"" );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }
 
  /**
   * @brief      play previous track
   *
   * @param[in]  none
   *
   * @return
   *		- FISH_OK
   */  
  int previous(short dummy) {
	  // play previous track
	  
	  request_mutex();
	  
	  ftcSoundBar.http_post( (char *) "api/track/previous", (char *)"" );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      play next track
   *
   * @param[in]  none
   *
   * @return
   *		- FISH_OK
   */   
  int next(short dummy) {
	  // play next track
	  
	  request_mutex();
	  
	  ftcSoundBar.http_post( (char *) "api/track/next", (char *)"" );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }

  /**
   * @brief      set mode (NORMAL/SHUFFLE/REPEAT)
   *
   * @param[in]  mode
   *
   * @return
   *		- FISH_OK
   */   
  int setMode(short mode) {
	  
	  // set mode: 0 - normal, 1 - shuffle, 2 - repeat
	  request_mutex();
	  
	  char jsonData[100];
	  
	  sprintf( jsonData, "{\"mode\": %hi}", mode );
	  
	  ftcSoundBar.http_post( (char *) "api/mode", jsonData );
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }
 
  /**
   * @brief      get mode 
   *
   * @param[in]  mode	NORMAL/SHUFFLE/REPEAT
   *
   * @return
   *		- FISH_OK
   */   
  int getMode(short *mode)  {

	request_mutex();
	
	int status;
	JSON jsonData;
	  
	// http_get mode
	status = ftcSoundBar.http_get( (char *) "api/mode", &jsonData );
	  
	if ( status != 200 ) {
		*mode = -1;
		release_mutex();
		return FISH_ERR;
	}
	  
	// get return value mode and test on error
	if ( 0 != jsonData.GetParam( (char *) "mode", mode ) ) {
		release_mutex();
		return FISH_ERR;
	}
	
	release_mutex();
	return FISH_OK;
	  
  }
 
  /**
   * @brief      get number of tracks
   *
   * @param[in]  tracks		number of tracks
   *
   * @return
   *		- FISH_OK
   */   
  int getTracks(short *tracks)  {
	  
	request_mutex();
	  
	int status;
	JSON jsonData;

	// http_get tracks
	status = ftcSoundBar.http_get( (char *) "api/tracks", &jsonData );

	if ( status != 200 ) {
		*tracks = -1;
		release_mutex();
		return FISH_ERR;
	}
	  
	// get return value mode and test on error
	if ( 0 != jsonData.GetParam( (char *) "tracks", tracks ) ) {
		release_mutex();
		 return FISH_ERR;
	}
	  
	release_mutex();

	return FISH_OK;
	  
  }  

  /**
   * @brief      get active track
   *
   * @param[in]  track		active track
   *
   * @return
   *		- FISH_OK
   */  
  int getActiveTrack(short *active_track)  {

	  request_mutex();
	  
	  int status;
	  JSON jsonData;
	  
	  // http_get tracks
	  status = ftcSoundBar.http_get( (char *) "api/activeTrack", &jsonData );
	  
	  if ( status != 200 ) {
		  *active_track = -1;
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  // get return value mode and test on error
	  if ( 0 != jsonData.GetParam( (char *) "activeTrack", active_track ) ) {
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }  

  /**
   * @brief      get audio pipeline's state
   *
   * @param[in]  state		RUNNING/PAUSED/STOPPED/FINISHED...
   *
   * @return
   *		- FISH_OK
   */
  int getTrackState(short *state)  {
	  
	  request_mutex();
	  
	  int status;
	  JSON jsonData;
	  
	  // http_get tracks
	  status = ftcSoundBar.http_get( (char *) "api/activeTrack", &jsonData );

	  if ( status != 200 ) {
		  *state = -1;
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  // get return value mode and test on error
	  if ( 0 != jsonData.GetParam( (char *) "state", state ) ) {
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  release_mutex();
	  
	  return FISH_OK;
	  
  }  

  /**
   * @brief      get volumne
   *
   * @param[in]  volumne	volume
   *
   * @return
   *		- FISH_OK
   */
  int getVolume(short *volume)  {
	 
	  request_mutex();

	  int status;
	  JSON jsonData;
	  
	  // http_get volume
	  status = ftcSoundBar.http_get( (char *) "api/volume", &jsonData );

	  if ( status != 200 ) {
		  *volume = -1;
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  // get return value mode and test on error
	  if ( 0 != jsonData.GetParam( (char *) "volume", volume ) ) {
		  release_mutex();
		  return FISH_ERR;
	  }
	  
	  release_mutex();
	  return FISH_OK;
	  
  }
  
} // extern "C"