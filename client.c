#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <utime.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <time.h>

#define PORT 8000
#define MAX_ARGUMENTS 10
#define BUFFER_SIZE 1024
#define MIRROR_IP "192.168.2.33"

int flag = 0;

void remove_linebreak(char **tokens, int num_tokens) // to remove line breaks
{
	for (int i = 0; i < num_tokens; i++)
	{
		char *token = tokens[i];
		int length = strcspn(token, "\n");
		char *new_token = (char *)malloc(length + 1);
		strncpy(new_token, token, length);
		new_token[length] = '\0';
		tokens[i] = new_token;
	}
}
time_t convertStringToDate(char *date) //function to convert string to date
{
	struct tm tm = {0};
	if (strptime(date, "%Y-%m-%d", &tm) == NULL)
	{
		return -1;
	}
	tm.tm_isdst = -1;
	time_t temp;
	temp = mktime(&tm);
	return temp;
}

int validate_input(char *rawCommand) // for validating the commands that is entered by the client
{
	int isUnzip = 0;
	char *ptr = strtok(rawCommand, " "); //first token is stored in ptr and other in local
	char *local[50];
	int cnt = 0;
	int i = 0;

	while (1)
	{
		char *ptr1 = strtok(NULL, " "); 
		if (ptr1 == NULL)
		{
			break;
		}
		if (strcmp(ptr1, "\n") == 0)
		{
			continue;
		}
		local[i] = ptr1;
		i++;
		cnt++;
	}
	if (cnt > 0 && strcmp(local[cnt - 1], "-u\n") == 0)
	{
		isUnzip = 1;
	}
	if (strcmp(ptr, "findfile") == 0)
	{
		if (cnt != 1)
		{
			fprintf(stderr, "Command Invalid - findfile `filename`\n");
			return -1;
		}
		return 1;
	}

	if (strcmp(ptr, "sgetfiles") == 0)
	{
		if (cnt < 2 || cnt > 3)
		{
			fprintf(stderr, "Command Invalid - sgetfiles size1 size2 <-u>\n");
			return -1;
		}

		int size1 = atoi(local[0]);
		int size2 = atoi(local[1]);
		if (size1 < 0 || size2 < 0)
		{
			fprintf(stderr, "Command Invalid - sgetfiles size1 size2 <-u>: [Size1, Size2] >= 0\n");
			return -1;
		}

		if (size2 < size1)
		{
			fprintf(stderr,
					"Command Invalid - sgetfiles size1 size2 <-u>: Size 1 should be less than equal to size 2\n");
			return -1;
		}

		return 2;
	}

	if (strcmp(ptr, "dgetfiles") == 0)
	{
		if (cnt < 2 || cnt > 3)
		{
			fprintf(stderr, "Command Invalid - dgetfiles date1 date2 <-u>\n");
			return -1;
		}

		time_t date1, date2;
		date1 = convertStringToDate(local[0]);
		date2 = convertStringToDate(local[1]);
		if (date1 == -1 || date2 == -1)
		{
			fprintf(stderr, "Invalid date format should YYYY-MM-DD\n");
			return -1;
		}

		if (date2 < date1)
		{
			fprintf(stderr, "date2 should be greater than equal to date1\n");
			return -1;
		}
		return 2;
	}

	if (strcmp(ptr, "getfiles") == 0)
	{

		if (isUnzip == 0 && cnt > 6)
		{
			fprintf(stderr,
					"Command Invalid - getfiles file1 file2 file3 file4 file5 file6(file 1 ..up to file6) <-u>\n");
			return -1;
		}

		if (cnt < 1 || cnt > 7)
		{
			fprintf(stderr,
					"Command Invalid - getfiles file1 file2 file3 file4 file5 file6(file 1 ..up to file6) <-u>\n");
			return -1;
		}

		return 4;
	}

	if (strcmp(ptr, "gettargz") == 0)
	{
		if (isUnzip == 0 && cnt > 6)
		{
			fprintf(stderr, "Command Invalid - gettargz <extension list> <-u> //up to 6 different file types\n");
			return -1;
		}

		if (cnt < 1 || cnt > 7)
		{
			fprintf(stderr, "Command Invalid - gettargz <extension list> <-u> //up to 6 different file types\n");
			return -1;
		}
		return 5;
	}

	if (strcmp(ptr, "quit\n") == 0)
	{
		if (cnt)
		{
			fprintf(stderr, "Command Invalid - quit\n");
			return -1;
		}
		return 6;
		flag = 1;
	}

	fprintf(stderr, "Command not supported!\n");
	return -1;
}

int main(int argc, char *argv[])
{
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	char valbuf[1024];
	char server_ip[16];
	char *filename = "out.tar.gz";

    // take two arguments when running client 
	if (argc < 2)
	{
		printf("Usage: %s <server_ip>\n", argv[0]);
		return 1;
	}
	
	strcpy(server_ip, argv[1]);

		//create client socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		if (flag==1) {
			close(sock);
		}
		return 1;
	}

	//configure server address 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 1;
	}

	//connect to the server
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Connection failed\n");
		return 1;
	}

	// to read message from the server
	read(sock, buffer, 1024);
	printf("Message from server: %s \n\n", buffer);

	//to connect to mirror 
	if (strcmp(buffer, "success") != 0) // if the buffer is not equal to success, it will connect to mirror.
	{
		printf("Connecting to Mirror\n\n");
		close(sock);
		
		//creating a socket
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("Socket creation error\n");
			return 1;
		}

		//configure server address
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(8001);

		if (inet_pton(AF_INET, MIRROR_IP, &serv_addr.sin_addr) <= 0)
		{
			printf("Invalid address or Address not supported\n");
			return 1;
		}
		
		//connection
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			printf("Connection failed\n");
			return 1;
		}
	}
	else
		printf("Connected to the server! \n");
	
	//runs in infinite loop and waits for user input
	while (1)
	{
		printf("C$ ");
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, stdin);
		strcpy(valbuf, buffer);
		if (validate_input(valbuf) == -1)
		{
			continue;
		}

		// send command to server
		send(sock, buffer, strlen(buffer), 0);
		
		
		char *arguments[MAX_ARGUMENTS];
		int num_arguments = 0;

		// Parse the command received from client
		char *token = strtok(buffer, " "); // Tokenize command using space as delimiter

		while (token != NULL)
		{
			arguments[num_arguments++] = token; // Store the token in the array
			token = strtok(NULL, " ");			// Get the next token
		}
		arguments[num_arguments] = NULL; // Set the last element of the array to NULL

		// Remove line breaks from tokens
		remove_linebreak(arguments, num_arguments);

		char *buffcmd = arguments[0]; // Extract first token as the command
		memset(buffer, 0, sizeof(buffer));
		// Compare buffer with pattern
		if (strcmp(buffcmd, "gettargz") == 0 || strcmp(buffcmd, "getfiles") == 0 || strcmp(buffcmd, "dgetfiles") == 0 || strcmp(buffcmd, "sgetfiles") == 0)
		{
			//printf("Buffer matches the pattern\n");

			// send(sock, filename, strlen(filename), 0);

			// Receive file from server
			FILE *fp = fopen(filename, "wb");
			if (fp == NULL)
			{
				printf("Error opening file");
				return -1;
			}

			// Get file size from server
			long file_size;
			// memset(file_size, 0, sizeof(file_size));
			recv(sock, &file_size, sizeof(file_size), 0);

			printf(" File Size %ld\n", file_size);

			// Receive file data from server
			long total_bytes_received = 0;
			while (total_bytes_received < file_size)
			{
				int bytes_to_receive = BUFFER_SIZE;
				if (total_bytes_received + BUFFER_SIZE > file_size)
				{
					bytes_to_receive = file_size - total_bytes_received;
				}
				int bytes_received = recv(sock, buffer, bytes_to_receive, 0);
				if (bytes_received < 0)
				{
					printf("Error receiving file data");
					return -1;
				}
				fwrite(buffer, sizeof(char), bytes_received, fp);
				total_bytes_received += bytes_received;
				if (total_bytes_received >= file_size)
				{
					break;
				}
			}

			fclose(fp);

			printf("File received successfully\n");

			// Check if the last argument is "-u" to unzip the file
			if (strcmp(arguments[num_arguments - 1], "-u") == 0)
			{
				char cmd[BUFFER_SIZE];
				snprintf(cmd, BUFFER_SIZE, "tar -xzvf %s", filename);
				system(cmd);
				printf("File unzipped successfully\n");
			}

			memset(buffer, 0, sizeof(buffer));
			continue;
		}
		else
		{
			//printf("Buffer does not match the file trasfer or unzip pattern so no need for file transfer\n");
		}

		

		// receive response from server
		memset(buffer, 0, sizeof(buffer));
		valread = read(sock, buffer, 1024);

		if (valread > 0)
		{
			printf("Bytes Received: %d\n", valread);
			printf("%s\n", buffer);
		}
		else
		{
			printf("Server disconnected\n");
			break;
		}
		fflush(stdout);
	}

	close(sock);
	return 0;
}
