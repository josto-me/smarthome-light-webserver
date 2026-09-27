// SPDX-FileCopyrightText: Johannes Stockhammer
// SPDX-License-Identifier: Apache-2.0
/*
 * Webserver_Basic.ino
 *
 *  Author: Johannes Stockhammer
 *
 * Version:     2.0
 * Hardware:    Arduino Uno (ATmega328P) + Ethernet shield (WIZnet W5100)
 * Software:    Arduino IDE, SPI + Ethernet library (2.x)
 * Description: Remote light control. Small HTTP server on port 80 that switches
 *              output 3 and output 4 with the GET parameters "?3=on", "?3=off",
 *              "?4=on", "?4=off" and answers with a page that shows the state
 *              of both outputs. Basic variant: page text as normal strings.
 */

//Includes
#include <SPI.h>
#include <Ethernet.h>

//Defines
#define BUFFER_SIZE		100			//Size of the buffer for the request line (incl. closing 0)
#define HTTP_PORT		80			//TCP port of the web server
#define TIME_REQUEST_ms		2000			//Max. time to receive one request, then give up

#define PIN_OUTPUT_3		3			//Output 3 (relay / LED), HIGH = on
#define PIN_OUTPUT_4		4			//Output 4 (relay / LED), HIGH = on

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

	//Outputs
bool output_3_on=false;					//State output 3 (for the page)
bool output_4_on=false;					//State output 4 (for the page)

//Prototypes
bool Read_Request(EthernetClient &client);
bool Is_Page_Request(void);
void Parse_Request(void);
void Clear_Buffer(void);
void Send_Page(EthernetClient &client);
void Send_Output(EthernetClient &client, uint8_t output_nr, bool output_on);
void Send_Not_Found(EthernetClient &client);


void setup()
{
	//Outputs
	pinMode(PIN_OUTPUT_3, OUTPUT);				//Output 3 as output
	pinMode(PIN_OUTPUT_4, OUTPUT);				//Output 4 as output
	digitalWrite(PIN_OUTPUT_3, LOW);			//start with output 3 off
	digitalWrite(PIN_OUTPUT_4, LOW);			//start with output 4 off

	//Interfaces
	Serial.begin(9600);					//Status messages on the serial monitor
	Ethernet.begin(mac, ip, dns_server, gateway, subnet);	//Start W5100 with fixed IP
	server.begin();						//Start listening on HTTP_PORT

	Serial.print("Server at ");
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
				Send_Page(client);		//Answer with the page (current state)
			}
			else
			{
				Send_Not_Found(client);		//e.g. /favicon.ico -> 404, nothing is switched
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
	if(strncmp(request_line, "GET / ", 6)==0) return true;	//plain page
	if(strncmp(request_line, "GET /?", 6)==0) return true;	//page with parameters
	return false;
}//end Is_Page_Request


//----------------------------------------------------------//
//Parse_Request (char array instead of String)
//Looks for the pattern: output number, 2 chars later 'o', then 'n' or 'f'.
//"?3=on" -> '3' '=' 'o' 'n' -> output 3 on
//"?3=off" -> '3' '=' 'o' 'f' -> output 3 off
//check_k+3<read_k: the access to check_k+3 stays inside the received chars.
//----------------------------------------------------------//
void Parse_Request(void)
{
	for(check_k=0; check_k+3<read_k; check_k++)
	{
		if((request_line[check_k]=='3')&&(request_line[check_k+2]=='o')&&(request_line[check_k+3]=='n'))	//Pattern "3=on" -> output 3 on
		{
			digitalWrite(PIN_OUTPUT_3, HIGH);
			Serial.println("Pin 3 switched on!");
			output_3_on=true;
		}

		if((request_line[check_k]=='3')&&(request_line[check_k+2]=='o')&&(request_line[check_k+3]=='f'))	//Pattern "3=of" -> output 3 off
		{
			digitalWrite(PIN_OUTPUT_3, LOW);
			Serial.println("Pin 3 switched off!");
			output_3_on=false;
		}

		if((request_line[check_k]=='4')&&(request_line[check_k+2]=='o')&&(request_line[check_k+3]=='n'))	//Pattern "4=on" -> output 4 on
		{
			digitalWrite(PIN_OUTPUT_4, HIGH);
			Serial.println("Pin 4 switched on!");
			output_4_on=true;
		}

		if((request_line[check_k]=='4')&&(request_line[check_k+2]=='o')&&(request_line[check_k+3]=='f'))	//Pattern "4=of" -> output 4 off
		{
			digitalWrite(PIN_OUTPUT_4, LOW);
			Serial.println("Pin 4 switched off!");
			output_4_on=false;
		}
	}//end for
}//end Parse_Request


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
//HTTP header + small page: one line per output with state and two links.
//----------------------------------------------------------//
void Send_Page(EthernetClient &client)
{
	//HTTP header
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html; charset=utf-8");
	client.println("Connection: close");
	client.println();					//Empty line -> body follows

	//Page
	client.println("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">");
	client.println("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
	client.println("<title>Light Switch</title></head>");
	client.println("<body style=\"font-family:system-ui,sans-serif;margin:1em\">");
	client.println("<h2>Light Switch</h2>");

	Send_Output(client, 3, output_3_on);			//Line for output 3
	Send_Output(client, 4, output_4_on);			//Line for output 4

	client.println("<p><a href=\"/\">refresh</a></p>");
	client.println("</body></html>");
}//end Send_Page


//----------------------------------------------------------//
//Send_Output
//One line: "Output n: lit/dark  [on] [off]" with links "?n=on" / "?n=off".
//----------------------------------------------------------//
void Send_Output(EthernetClient &client, uint8_t output_nr, bool output_on)
{
	client.print("<p>Output ");
	client.print(output_nr);
	client.print(": <b>");
	if(output_on) client.print("lit");			//State text
	else client.print("dark");
	client.print("</b> &nbsp; <a href=\"/?");
	client.print(output_nr);
	client.print("=on\">[on]</a> <a href=\"/?");
	client.print(output_nr);
	client.println("=off\">[off]</a></p>");
}//end Send_Output


//----------------------------------------------------------//
//Send_Not_Found
//Answer for every other path (e.g. /favicon.ico).
//----------------------------------------------------------//
void Send_Not_Found(EthernetClient &client)
{
	client.println("HTTP/1.1 404 Not Found");
	client.println("Content-Type: text/plain");
	client.println("Connection: close");
	client.println();
	client.println("not found");
}//end Send_Not_Found
