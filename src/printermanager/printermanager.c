#define _GNU_SOURCE
#define __USE_XOPEN_EXTENDED
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <dirent.h>

//#define ENABLE_B300_HACK 1 /* see: http://www.panix.com/~grante/files/arbitrary-baud.c for termios2 usage */
//#define DEFAULT_TO_115K2 1

#include <linux/serial.h>
#ifdef ENABLE_B300_HACK
#include <sys/ioctl.h>
#include <termios.h>
#else
#include <linux/termios.h>
#endif

#define BASE_PATH "/tmp/UltiFi"

int lineBufferPos = 0;
char lineBuffer[1024];
int serialfd;
int tempRecieveTimeout = 2; // how many times is a temperature timeout allowed (after temperature is received this should be more)
int temperatureCheckDelay = 50;
int tryAlternativeSpeed = 1;

char basePath[128];
char temperatureFilename[128];
char sdlistFilename[128];
char endstopFilename[128];
char printProgressFilename[128];
char printProgress2Filename[128];
char commandFilename[128];
char startLogFilename[128];
//char serverLogFilename[128];
FILE* commandFile = NULL;
FILE* sdlistFile = NULL;
FILE* startLog = NULL;

enum LOG_LEVELS {
	QUIET = -1,
	ERROR = 0,
	WARNING = 1,
	INFO = 2,
	VERBOSE = 3,
	BULK = 4
};

int serverLogLevel = WARNING; //default value, this is probably overridden in main()
FILE* serverLogFile = NULL;

/* GCode streaming code */
FILE* gcodeFile = NULL;
int gcodeLineNr;
int gcodeNumLines;

void logger(int level, const char* format, ...) {
	va_list args;
	time_t ltime;
	struct tm* now;

	if (!serverLogFile || level > serverLogLevel) return;

	ltime = time(NULL);
	now = localtime(&ltime);
	fprintf(serverLogFile, "%02i-%02i %02i:%02i:%02i  ",now->tm_mday, now->tm_mon + 1, now->tm_hour, now->tm_min, now->tm_sec);

	va_start(args, format);
	vfprintf(serverLogFile, format, args);
	va_end(args);
}

void sendSingleLine(const char* gcode) {
	logger(VERBOSE, "send single line: '%s'\n", gcode);
	write(serialfd, gcode, strlen(gcode));
}

void sendGCodeLineWithChecksum(const char* gcode)
{
	char buffer[128];
	unsigned char checksum = 0;
	int n = 0;
	sprintf(buffer, "N%d%s", gcodeLineNr, gcode);
	while(buffer[n])
		checksum ^= buffer[n++];
	sprintf(buffer, "N%d%s*%d\n", gcodeLineNr, gcode, checksum);
	logger(BULK, "GCODE: %s", buffer);
	write(serialfd, buffer, strlen(buffer));
	gcodeLineNr++;

	FILE* printProgress2File = fopen(printProgress2Filename, "w");
	fprintf(printProgress2File, "%d/%d\n", gcodeLineNr,gcodeNumLines);
	fclose(printProgress2File);
}

void sendNextGCodeLine()
{
	char lineBuffer[128];
	char* c;
	if (!fgets(lineBuffer, sizeof(lineBuffer), gcodeFile))
	{
		cancelCodeFile();
		return;
	}
	c = strchr(lineBuffer, ';');
	if (c)
		*c = '\0';
	c = lineBuffer + strlen(lineBuffer) - 1;
	while(c >= lineBuffer && c[0] <= ' ')
	{
		c[0] = '\0';
		c--;
	}
	//logger(BULK, "gcode %03ib: '%s'\n", strlen(lineBuffer), lineBuffer); //TEMP
	if (lineBuffer[0] == '\0')
		sendNextGCodeLine();
	else
		sendGCodeLineWithChecksum(lineBuffer);
}

void startCodeFile(const char* filename)
{
	//int i;
	gcodeFile = fopen(filename, "r");
	if (gcodeFile == NULL)
		return;
	gcodeLineNr = 0;

	gcodeNumLines = getNumLines(gcodeFile);
	printf("  gcodeNumLines: %d\n", gcodeNumLines);

	sendGCodeLineWithChecksum("M110");
	//for(i=0;i<6;i++)
		sendNextGCodeLine();
}

int getNumLines(fileName) {
	int ch, numLines = 0;
	do {
	    ch = fgetc(fileName);
	    if(ch == '\n')
	    	numLines++;
	} while (ch != EOF);
	// last line doesn't end with a new line!
	// but there has to be a line at least before the last line
	if(ch != '\n' && numLines != 0) 
	    numLines++;
	
	fseek(fileName, 0, SEEK_SET);
	return numLines;
}

void cancelCodeFile() {
	if(gcodeFile != NULL) fclose(gcodeFile);
	gcodeFile = NULL;
	printf("finished GCode file");

	FILE* printProgress2File = fopen(printProgress2Filename, "w");
	fprintf(printProgress2File, "");
	fclose(printProgress2File);
}

void parseLine(const char* line)
{
	logger(INFO, "|%s|\n", line);
	if (strstr(line, "Resend:"))
	{
		gcodeLineNr = atoi(strstr(line, "Resend:") + 7);
		return;
	}
	if (strcmp(line, "ok") == 0)
	{
		if (gcodeFile != NULL)
		{
			sendNextGCodeLine();
		}
	}
	if (startLog != NULL)
	{
		if (strstr(line, "echo:"))
		{
			fprintf(startLog, "%s\n", line);
		}else{
			fclose(startLog);
			startLog = NULL;
		}
	}
	if (strcmp(line, "start") == 0)
	{
		startLog = fopen(startLogFilename, "w");
	}
	if (strstr(line, "ok T:") || (strstr(line, "T:") == line))
	{
		FILE* f = fopen(temperatureFilename, "w");
		fprintf(f, "%s\n", line);
		fclose(f);
		tempRecieveTimeout = 5;

		// keep checking the temperature while printing, but less frequently
		if (gcodeFile == NULL) temperatureCheckDelay = 20;
		else temperatureCheckDelay = 100;
		return;
	}
	if (strstr(line, "SD printing byte "))
	{
		//SD printing byte xxx/xxx
		//Can report 0/0 when there is no file open.
		FILE* f = fopen(printProgressFilename, "w");
		fprintf(f, "%s\n", line);
		fclose(f);
		return;
	}
	if (strstr(line, "Not SD printing"))
	{
		//Only happens when there is no SD card inserted.
		FILE* f = fopen(printProgressFilename, "w");
		fclose(f);
		return;
	}
	if (strstr(line, "echo:endstops hit:"))
	{
		FILE* f = fopen(endstopFilename, "w");
		fprintf(f, "%s", line);
		fclose(f);
		return;
	}
	if (strcmp(line, "echo:SD card ok") == 0)
	{
		sendSingleLine("M20\n");
		return;
	}
	if (strcmp(line, "Begin file list") == 0)
	{
		sdlistFile = fopen(sdlistFilename, "w");
		return;
	}
	if (strcmp(line, "End file list") == 0)
	{
		fclose(sdlistFile);
		sdlistFile = NULL;
		return;
	}
	if (sdlistFile)
	{
		fprintf(sdlistFile, "%s\n", line);
		return;
	}
}

void checkActionDirectory()
{
	if (commandFile == NULL)
		commandFile = fopen(commandFilename, "rb");
}

void setSerialSpeed(int speed)
{
#ifdef ENABLE_B300_HACK
	struct termios options;
#else
	struct termios2 options;
#endif
	int modemBits;

#ifdef ENABLE_B300_HACK
	tcgetattr(serialfd, &options);
#else
	ioctl(serialfd, TCGETS2, &options);
#endif

	cfmakeraw(&options);

	// Enable the receiver
	options.c_cflag |= CREAD;
	// Clear handshake, parity, stopbits and size
	options.c_cflag &= ~CLOCAL;
	options.c_cflag &= ~CRTSCTS;
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;

	switch(speed)
	{
	case 38400:
		cfsetispeed(&options, B38400);
		cfsetospeed(&options, B38400);
		break;
	case 57600:
		cfsetispeed(&options, B57600);
		cfsetospeed(&options, B57600);
		break;
	case 115200:
		cfsetispeed(&options, B115200);
		cfsetospeed(&options, B115200);
		break;
	case 250000:
#ifdef ENABLE_B300_HACK
		//Hacked the kernel to see B300 as 250000
		cfsetispeed(&options, B300);
		cfsetospeed(&options, B300);
#else
		options.c_ospeed = options.c_ispeed = 250000;
		options.c_cflag &= ~CBAUD;
		options.c_cflag |= BOTHER;
#endif
		break;
	}
	options.c_cflag |= CS8;
	options.c_cflag |= CLOCAL;

#ifdef ENABLE_B300_HACK
	tcsetattr(serialfd, TCSANOW, &options);
#else
	ioctl(serialfd, TCSETS2, &options);
#endif

	ioctl(serialfd, TIOCMGET, &modemBits);
	modemBits |= TIOCM_DTR;
	ioctl(serialfd, TIOCMSET, &modemBits);
	usleep(100 * 1000);
	modemBits &=~TIOCM_DTR;
	ioctl(serialfd, TIOCMSET, &modemBits);
}

int main(int argc, char** argv)
{
	const char* portName = NULL;

	if (argc > 1)
		portName = argv[1];

	if (portName == NULL)
		portName = "/dev/ttyACM0";

	sprintf(basePath, "%s/%s", BASE_PATH, strrchr(portName, '/') + 1);
	mkdir(BASE_PATH, 0777);
	mkdir(basePath, 0777);
	sprintf(temperatureFilename, "%s/temp.out", basePath);
	sprintf(sdlistFilename, "%s/sdlist.out", basePath);
	sprintf(endstopFilename, "%s/endstop.out", basePath);
	sprintf(printProgressFilename, "%s/progress.out", basePath);
	sprintf(printProgress2Filename, "%s/progress2.out", basePath);
	sprintf(commandFilename, "%s/command.in", basePath);
	sprintf(startLogFilename, "%s/startup.out", basePath);
	//sprintf(serverLogFilename, "%s/server.log", basePath);

	//sprintf(lineBuffer, "stty -F %s raw 115200 -echo -hupcl", portName);
	//system(lineBuffer);

	//setup logger
	serverLogLevel = BULK;
	serverLogFile = stderr;

	serialfd = open(portName, O_RDWR);
#ifndef DEFAULT_TO_115K2
	logger(INFO, "Setting port speed to 250000\n");
	setSerialSpeed(250000);
#else
	logger(INFO, "Setting port speed to 115200\n");
	setSerialSpeed(115200);
#endif

	logger(INFO, "Start\n");
	while(1)
	{
		fd_set fdset;
		struct timeval timeout;

		FD_ZERO(&fdset);
		FD_SET(serialfd, &fdset);

		timeout.tv_sec = 0;
		timeout.tv_usec = 100 * 1000;//100ms timeout
		select(serialfd + 1, &fdset, NULL, NULL, &timeout);
		if (FD_ISSET(serialfd, &fdset))
		{
			//logger(BULK,"FD_ISSET\n");
			int len = read(serialfd, lineBuffer + lineBufferPos, sizeof(lineBuffer) - lineBufferPos - 1);
			if (len < 1)
			{
				logger(INFO, "Connection closed.\n");
				break;
			}
			lineBufferPos += len;
			lineBuffer[lineBufferPos] = '\0';
			while(strchr(lineBuffer, '\n'))
			{
				char* c = strchr(lineBuffer, '\n');
				*c++ = '\0';
				parseLine(lineBuffer);
				lineBufferPos -= strlen(lineBuffer) + 1;
				memmove(lineBuffer, c, strlen(c) + 1);
			}
			//On buffer overflow clear the buffer (this usually happens on the wrong baudrate)
			if (lineBufferPos == sizeof(lineBuffer) - 1)
				lineBufferPos = 0;
		}
		else
		{
			//logger(BULK,"no FD_ISSET\n");
			//if(gcodeFile == NULL) {
				if (temperatureCheckDelay > 0)
				{
					temperatureCheckDelay--;
				}
				else
				{
					temperatureCheckDelay = 100;
					sendSingleLine("M105\n");
					logger(BULK," tempRecieveTimeout: %d\n",tempRecieveTimeout);
					if (tempRecieveTimeout)
					{
						tempRecieveTimeout--;
					}
					else
					{
						if (tryAlternativeSpeed)
						{
	#ifndef DEFAULT_TO_115K2
							logger(INFO, "Trying 115200\n");
							setSerialSpeed(115200);
	#else
							logger(INFO, "Trying 250000\n");
							setSerialSpeed(250000);
	#endif

							lineBufferPos = 0;
							tryAlternativeSpeed = 0;
							tempRecieveTimeout = 5;
						}
						else
						{
							logger(WARNING, "Failed to connect\n");
							break;
						}
					}
				}
			//}
		}
		if (commandFile != NULL)
		{
			logger(BULK, "commandFile found\n");
			char sendLine[1024];
			if (fgets(sendLine, sizeof(sendLine), commandFile) != NULL)
			{
				if (strstr(sendLine, "(SENDFILE=") == sendLine) 
				{ //ptr==.ptr
					if (sendLine[strlen(sendLine)-1]==10) sendLine[strlen(sendLine)-1] = 0; //recplace LF by \0
					logger(INFO, "sending '%s'\n",sendLine+sizeof("SENDFILE="));
					startCodeFile(sendLine+sizeof("SENDFILE=")); //remaining characters after (SENDFILE=
				} 
				else 
				{
					sendSingleLine(sendLine);
				}

				if (strstr(sendLine, "(CANCELFILE") == sendLine) 
				{ //ptr==.ptr
					fclose(gcodeFile);
					gcodeFile = NULL;
					logger(INFO, "cancelled GCode file\n");
				}
			}
			else
			{
				logger(BULK, "  commandFile empty\n");
				//sendSingleLine("\n"); // why would we send a empty line?
				fclose(commandFile);
				unlink(commandFilename);
				commandFile = NULL;
			}
		}
		checkActionDirectory();
	}
	close(serialfd);
	return 0;
}
