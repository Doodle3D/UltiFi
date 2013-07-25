#define _GNU_SOURCE
#define __USE_XOPEN_EXTENDED
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <dirent.h>

#include <linux/serial.h>
#include <sys/ioctl.h>
#include <termios.h>

#define BASE_PATH "/tmp/UltiFi"

int lineBufferPos = 0;
char lineBuffer[1024];
int serialfd;
int tempRecieveTimeout = 20;
int temperatureCheckDelay = 100;
int tryAlternativeSpeed = 1;

char basePath[128];
char temperatureFilename[128];
char sdlistFilename[128];
char endstopFilename[128];
char printProgressFilename[128];
char commandFilename[128];
char gcodeFilename[128];
char startLogFilename[128];
FILE* commandFile = NULL;
FILE* sdlistFile = NULL;
FILE* startLog = NULL;

/* GCode streaming code */
FILE* gcodeFile = NULL;
int gcodeLineNr;

void sendGCodeLineWithChecksum(const char* gcode)
{
	char buffer[128];
	unsigned char checksum = 0;
	int n = 0;
	sprintf(buffer, "N%d%s", gcodeLineNr, gcode);
	while(buffer[n])
		checksum ^= buffer[n++];
	sprintf(buffer, "N%d%s*%d\n", gcodeLineNr, gcode, checksum);
	printf("GCODE: %s", buffer);
	write(serialfd, buffer, strlen(buffer));
	gcodeLineNr++;
}

void sendNextGCodeLine()
{
	char lineBuffer[128];
	char* c;
	if (!fgets(lineBuffer, sizeof(lineBuffer), gcodeFile))
	{
		fclose(gcodeFile);
		gcodeFile = NULL;
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
	if (lineBuffer[0] == '\0')
		sendNextGCodeLine();
	else
		sendGCodeLineWithChecksum(lineBuffer);
}

void startCodeFile(const char* filename)
{
	int i;
	gcodeFile = fopen(filename, "r");
	if (gcodeFile == NULL)
		return;
	gcodeLineNr = 0;
	sendGCodeLineWithChecksum("M110");
	for(i=0;i<6;i++)
		sendNextGCodeLine();
}

void parseLine(const char* line)
{
	printf("|%s|\n", line);
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
		tempRecieveTimeout = 60;
		temperatureCheckDelay = 20;
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
		write(serialfd, "M20\n", 5);
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
	if (gcodeFile == NULL)
		startCodeFile(gcodeFilename);
}

void setSerialSpeed(int speed)
{
	struct termios options;
	int modemBits;

	tcgetattr(serialfd, &options);
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
		//Hacked the kernel to see B300 as 250000
		cfsetispeed(&options, B300);
		cfsetospeed(&options, B300);
		break;
	}
	options.c_cflag |= CS8;
	options.c_cflag |= CLOCAL;

	tcsetattr(serialfd, TCSANOW, &options);

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
	sprintf(commandFilename, "%s/command.in", basePath);
	sprintf(startLogFilename, "%s/startup.out", basePath);
	sprintf(gcodeFilename, "%s/gcode.in", basePath);

	//sprintf(lineBuffer, "stty -F %s raw 115200 -echo -hupcl", portName);
	//system(lineBuffer);

	serialfd = open(portName, O_RDWR);
	setSerialSpeed(250000);

	printf("Start\n");
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
			int len = read(serialfd, lineBuffer + lineBufferPos, sizeof(lineBuffer) - lineBufferPos - 1);
			if (len < 1)
			{
				printf("Connection closed.\n");
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
		}else{
			if (gcodeFile != NULL)
				temperatureCheckDelay = 50;
			if (temperatureCheckDelay)
			{
				temperatureCheckDelay--;
			}else{
				temperatureCheckDelay = 100;
				write(serialfd, "M105\nM27\n", 9);
				if (tempRecieveTimeout)
				{
					tempRecieveTimeout--;
				}else{
					if (tryAlternativeSpeed)
					{
						printf("Trying 115200\n");
						setSerialSpeed(115200);
						lineBufferPos = 0;
						tryAlternativeSpeed = 0;
						tempRecieveTimeout = 20;
					}else{
						printf("Failed to conect\n");
						break;
					}
				}
			}
		}
		if (commandFile != NULL)
		{
			char sendLine[1024];
			if (fgets(sendLine, sizeof(sendLine), commandFile) != NULL)
			{
				write(serialfd, sendLine, strlen(sendLine));
			}else{
				write(serialfd, "\n", 1);
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
