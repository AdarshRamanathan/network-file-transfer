/*************************************************
 *	sndfile.c
 *	utility to send files over a TCP socket
 *	Adarsh Ramanathan
 *	2014/12/19
 ************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

static int port = 45639;
static int verbose_flag = 0;
static int quiet_flag = 0;

void displayhelptext(char* script_name)
{
	printf("usage: %s [options] destination_address file_name\n", script_name);
	printf("sends files to remote machines over a TCP connection.\n\n");
	printf("mandatory arguments to long options are mandatory for short options too.\n");
	printf("  -p, --port <port>\tconnect to <port> on the remote machine\n");
	printf("  -q, --quiet\t\tsuppress the progress bar\n");
	printf("  -v, --verbose\t\texplain what is being done\n");
	printf("      --help\t\tprint this help text and exit\n");
	printf("\n");
}

int parseoptions(int argc, char** argv)
{
	int val;
	int opterr = 0;

	static struct option long_options[] =
	{
		{"verbose", no_argument, NULL, 'v'},
		{"port", required_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	while((val = getopt_long(argc, argv, "p:qv", long_options, NULL)) > 0)
	{
		switch(val)
		{
			case 'v':
				verbose_flag = 1;
			break;

			case 'p':
				port = atoi(optarg);
			break;

			case 'h':
				displayhelptext(argv[0]);
				exit(0);
			break;

			case 'q':
				quiet_flag = 1;
			break;

			case '?':
				exit(1);
			break;
		}
	}


	if(argc - optind != 2)
	{
		fprintf(stderr, "usage: %s [options] destination_address file_name.\n", argv[0]);
		fprintf(stderr, "Try --help for more options.\n");
		exit(1);
	}

	return optind;
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	int startindex = parseoptions(argc, argv);

	char* destination_address = argv[startindex];
	char* file_name = argv[startindex + 1];

	FILE* fp;
	int sockfd;
	struct sockaddr_in remote_addr;

	char* buf;
	int bytes_read;

	if(port <= 0)
	{
		fprintf(stderr, "error: invalid port number.\n");
		exit(1);
	}

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(port);
	if(!inet_pton(AF_INET, destination_address, &remote_addr.sin_addr))
	{
		fprintf(stderr, "error: invalid network address.\n");
		exit(1);
	}

	if((fp = fopen(file_name, "rb")) == NULL)
	{
		fprintf(stderr, "error: failed to read file or file not found\n");
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "error: failed to obtain socket descriptor.\n");
		fclose(fp);
		exit(1);
	}

	if(connect(sockfd, (struct sockaddr*)&remote_addr, sizeof(struct sockaddr)) < 0)
	{
		fprintf(stderr, "error: failed to establish connection to remote machine.\n");
		fclose(fp);
		close(sockfd);
		exit(1);
	}

	buf = (char*) malloc(sizeof(char) * 1024);

	recv(sockfd, buf, 7, 0);
	if(strcmp(buf, "PROCEED") == 0)
	{
		ssize_t file_size, bytes_sent = 0;
		int i;

		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		rewind(fp);

		send(sockfd, &file_size, sizeof(ssize_t), 0);

		if(!quiet_flag)
			printf("\n");
		while(bytes_read = fread(buf, sizeof(char), 1024, fp))
		{
			sleep(1);
			if(send(sockfd, buf, bytes_read, 0) < bytes_read)
			{
				printf("\n");
				fprintf(stderr, "error: lost connection to remote machine.\n");
				fclose(fp);
				close(sockfd);
				free(buf);
				exit(0);
			}

			bytes_sent += bytes_read;

			if(!quiet_flag)
			{
				printf("\r[");
				for(i = 0 ; i < (bytes_sent * 100) / file_size ; i++)
					printf("#");
				for(i ; i < 100 ; i++)
					printf("-");
				printf("] %dB (%d%%)", bytes_sent, (bytes_sent * 100) / file_size);
				fflush(stdout);
			}
		}
		printf("\nasdasdasd\n");

		if(bytes_sent != file_size || recv(sockfd, buf, 3, 0) <= 0 || strcmp(buf, "ACK"))
		{
			fprintf(stderr, "error: lost connection to remote machine.\n");
		}

		printf("\n---> %s\n", buf);
	}
	else if(strcmp(buf, "REFUSED") == 0)
	{
		fprintf(stderr, "error: connection refused by remote machine.\n");
	}
	else
	{
		fprintf(stderr, "error: protocol violation.\n");
	}

	fclose(fp);
	close(sockfd);
	free(buf);
}
