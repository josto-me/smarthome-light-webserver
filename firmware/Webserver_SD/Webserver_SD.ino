// SPDX-FileCopyrightText: Johannes Stockhammer
// SPDX-License-Identifier: Apache-2.0
/*
 * Webserver_SD.ino
 *
 *  Author: Johannes Stockhammer
 *
 * Version:     2.0
 * Hardware:    Arduino Uno (ATmega328P) + Ethernet shield (WIZnet W5100) with micro-SD slot
 * Software:    Arduino IDE, SPI + Ethernet (2.x) + SD library
 * Description: Remote light control with the page on the SD card. The sketch only
 *              switches the outputs ("?2=on", "?2=off", "?3=on", "?3=off",
 *              "?all=0") and sends index.htm from the card as the answer.
 *
 * Pins: outputs are 2 and 3, because pin 4 is the chip select of the SD card
 * on the shield (and pin 10 the chip select of the W5100).
 */

//Includes
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

//Defines
#define BUFFER_SIZE		100			//Size of the buffer for the request line (incl. closing 0)
#define SEND_BUFFER_SIZE	64			//Bytes per block when sending index.htm
#define HTTP_PORT		80			//TCP port of the web server
#define TIME_REQUEST_ms		2000			//Max. time to receive one request, then give up
#define PAGE_FILE		"index.htm"		//Page on the SD card (8.3 name, root folder)

#define PIN_OUTPUT_2		2			//Output 2 (relay / LED), HIGH = on
#define PIN_OUTPUT_3		3			//Output 3 (relay / LED), HIGH = on
#define PIN_SD_CS		4			//Chip select of the SD card on the shield (not free!)
#define PIN_ETH_CS		10			//Chip select of the W5100 on the shield (not free!)

	//States of Read_Request()
#define READ_LINE		0			//storing the request line ("GET /?3=on HTTP/1.1")
#define SKIP_HEADERS		1			//ignoring header lines until the empty line

//Variables
	//Network (placeholders, set your own values)
byte mac[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };	//placeholder, set your own (locally administered MAC)
IPAddress ip(192, 168, 1, 177);				//placeholder: IP address of the server
IPAddress gateway(192, 168, 1, 1);			//placeholder: router, same subnet as ip
IPAddress dns_server(192, 168, 1, 1);			//placeholder: DNS (not needed, but part of Ethernet.begin())
IPAddress subnet(255, 255, 255, 0);			//placeholder: subnet mask

EthernetServer server(HTTP_PORT);			//Server object, listens on HTTP_PORT

	//Request
char request_line[BUFFER_SIZE];				//Request line of the current request
uint8_t read_k=0;					//Number of chars in request_line
uint8_t clear_k=0;					//Counter for clearing request_line
uint8_t check_k=0;					//Counter for the parser

	//SD card
bool sd_ok=false;					//true = card and PAGE_FILE found in setup()
uint8_t send_buffer[SEND_BUFFER_SIZE];			//Block buffer file -> client

//Prototypes
bool Read_Request(EthernetClient &client);
bool Is_Page_Request(void);
void Parse_Request(void);
void All_Outputs_Low(void);
void Clear_Buffer(void);
void Send_Page(EthernetClient &client);
void Send_Status(EthernetClient &client, const __FlashStringHelper *status, const __FlashStringHelper *text);


void setup()
{
	//Outputs
	pinMode(PIN_OUTPUT_2, OUTPUT);				//Output 2 as output
	pinMode(PIN_OUTPUT_3, OUTPUT);				//Output 3 as output
	All_Outputs_Low();					//start with everything dark

	//Interfaces
	Serial.begin(9600);					//Status messages on the serial monitor

	pinMode(PIN_ETH_CS, OUTPUT);				//W5100 deselected while the SD card starts
	digitalWrite(PIN_ETH_CS, HIGH);				//(both share the SPI bus)
	sd_ok=SD.begin(PIN_SD_CS);				//Card present and readable?
	if(sd_ok) sd_ok=SD.exists(PAGE_FILE);			//Page file on the card?
	if(sd_ok) Serial.println(F("SD: " PAGE_FILE " ok"));
	else Serial.println(F("SD: card or " PAGE_FILE " missing"));

	Ethernet.begin(mac, ip, dns_server, gateway, subnet);	//Start W5100 with fixed IP
	server.begin();						//Start listening on HTTP_PORT

	Serial.print(F("Server at "));
	Serial.println(Ethernet.localIP());			//Show own IP address
}//end setup


void loop()
{
	EthernetClient client=server.available();		//Client with data waiting?

	if(client)
	{
		if(Read_Request(client))			//Request line + headers received
		{
			if(Is_Page_Request())			//"/" or "/?..." -> switch and answer with the page
			{
				Parse_Request();		//Switch outputs according to the request line
				Send_Page(client);		//Answer with index.htm from the card
			}
			else
			{
				Send_Status(client, F("404 Not Found"), F("not found"));	//e.g. /favicon.ico, nothing is switched
			}
		}
		Clear_Buffer();					//Ready for the next request
		client.stop();					//Close the connection
	}
}//end loop


//----------------------------------------------------------//
//Read_Request
//Stores the request line in request_line and reads over the header lines.
//Returns true when the empty line after the headers arrived,
//false when the client closed the connection or took too long.
//----------------------------------------------------------//
bool Read_Request(EthernetClient &client)
{
	uint8_t state=READ_LINE;				//start with the request line
	bool line_empty=true;					//current header line has no chars yet
	uint32_t start_time=millis();				//for the timeout
	char c;

	while(client.connected())
	{
		if((millis()-start_time)>=TIME_REQUEST_ms) return false;	//Client too slow -> give up

		if(client.available())				//At least one byte waiting
		{
			c=client.read();			//Read one byte

			switch(state)
			{
				case READ_LINE:		if(c=='\n')			//End of request line
							{
								state=SKIP_HEADERS;	//Headers follow
								line_empty=true;
							}
							else if((c!='\r')&&(read_k<(BUFFER_SIZE-1)))	//keep last place for the 0
							{
								request_line[read_k]=c;	//Store char
								read_k++;
							}
							break;

				case SKIP_HEADERS:	if(c=='\n')			//End of a header line
							{
								if(line_empty) return true;	//Empty line -> request complete
								line_empty=true;	//Next header line starts
							}
							else if(c!='\r')
							{
								line_empty=false;	//Line has content
							}
							break;
			}//end switch
		}//end if client.available
	}//end while client.connected

	return false;						//Connection closed before the empty line
}//end Read_Request


//----------------------------------------------------------//
//Is_Page_Request
//true for "GET / ..." and "GET /?..." (the page itself), false for
//everything else, e.g. "GET /favicon.ico".
//----------------------------------------------------------//
bool Is_Page_Request(void)
{
	if(strncmp_P(request_line, PSTR("GET / "), 6)==0) return true;	//plain page
	if(strncmp_P(request_line, PSTR("GET /?"), 6)==0) return true;	//page with parameters
	return false;
}//end Is_Page_Request


//----------------------------------------------------------//
//Parse_Request (char array instead of String)
//Looks backwards from check_k: output number at check_k-3, 'o' at
//check_k-1, 'n' or 'f' at check_k.
//"?2=on" -> '2' '=' 'o' 'n' -> output 2 on
//"?2=off" -> '2' '=' 'o' 'f' -> output 2 off
//check_k starts at 3, so check_k-3 never goes below index 0.
//"all=0" is checked separately (All_Outputs_Low).
//----------------------------------------------------------//
void Parse_Request(void)
{
	for(check_k=3; check_k<read_k; check_k++)
	{
		if((request_line[check_k-3]=='3')&&(request_line[check_k-1]=='o')&&(request_line[check_k]=='n'))	//Pattern "3=on" -> output 3 on
		{
			digitalWrite(PIN_OUTPUT_3, HIGH);
			Serial.println(F("Pin 3 switched on!"));
		}

		if((request_line[check_k-3]=='3')&&(request_line[check_k-1]=='o')&&(request_line[check_k]=='f'))	//Pattern "3=of" -> output 3 off
		{
			digitalWrite(PIN_OUTPUT_3, LOW);
			Serial.println(F("Pin 3 switched off!"));
		}

		if((request_line[check_k-3]=='2')&&(request_line[check_k-1]=='o')&&(request_line[check_k]=='n'))	//Pattern "2=on" -> output 2 on
		{
			digitalWrite(PIN_OUTPUT_2, HIGH);
			Serial.println(F("Pin 2 switched on!"));
		}

		if((request_line[check_k-3]=='2')&&(request_line[check_k-1]=='o')&&(request_line[check_k]=='f'))	//Pattern "2=of" -> output 2 off
		{
			digitalWrite(PIN_OUTPUT_2, LOW);
			Serial.println(F("Pin 2 switched off!"));
		}
	}//end for

	if(strstr_P(request_line, PSTR("all=0"))!=NULL)		//Parameter "all=0" -> everything dark
	{
		All_Outputs_Low();
		Serial.println(F("All outputs dark!"));
	}
}//end Parse_Request


//----------------------------------------------------------//
//All_Outputs_Low
//Sets every used output LOW (dark).
//----------------------------------------------------------//
void All_Outputs_Low(void)
{
	digitalWrite(PIN_OUTPUT_2, LOW);			//Output 2 dark
	digitalWrite(PIN_OUTPUT_3, LOW);			//Output 3 dark
}//end All_Outputs_Low


//----------------------------------------------------------//
//Clear_Buffer
//Clears the whole buffer, so no old request is parsed again.
//----------------------------------------------------------//
void Clear_Buffer(void)
{
	for(clear_k=0; clear_k<BUFFER_SIZE; clear_k++)
	{
		request_line[clear_k]=0;
	}
	read_k=0;
}//end Clear_Buffer


//----------------------------------------------------------//
//Send_Page
//HTTP header + content of PAGE_FILE, sent in blocks of SEND_BUFFER_SIZE.
//Without card/file: 503 with a short text instead.
//----------------------------------------------------------//
void Send_Page(EthernetClient &client)
{
	File page;
	int block_length;

	if(sd_ok) page=SD.open(PAGE_FILE, FILE_READ);		//Open page for reading
	if(!sd_ok || !page)					//No card or file cannot be opened
	{
		Send_Status(client, F("503 Service Unavailable"), F(PAGE_FILE " missing on SD card"));
		return;
	}

	//HTTP header
	client.println(F("HTTP/1.1 200 OK"));
	client.println(F("Content-Type: text/html; charset=utf-8"));
	client.println(F("Connection: close"));
	client.println();					//Empty line -> body follows

	//File content
	while(page.available())					//Bytes left in the file
	{
		block_length=page.read(send_buffer, SEND_BUFFER_SIZE);	//Read up to one block
		if(block_length<=0) break;			//Read error -> stop
		client.write(send_buffer, block_length);	//Block to the browser
	}
	page.close();
}//end Send_Page


//----------------------------------------------------------//
//Send_Status
//Short plain text answer with the given status line (404, 503).
//----------------------------------------------------------//
void Send_Status(EthernetClient &client, const __FlashStringHelper *status, const __FlashStringHelper *text)
{
	client.print(F("HTTP/1.1 "));
	client.println(status);
	client.println(F("Content-Type: text/plain"));
	client.println(F("Connection: close"));
	client.println();
	client.println(text);
}//end Send_Status
