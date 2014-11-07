/*************************************************
 *	rcvfile.c
 *	utility to receive files over a TCP socket
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

static int port = 45639;
static int verbose_flag = 0;
static int force_flag = 0;
static int quiet_flag = 0;

void displayhelptext(char* script_name)
{
	printf("usage: %s [options] file_name\n", script_name);
	printf("receives files from remote machines over a TCP connection.\n\n");
	printf("mandatory arguments to long options are mandatory for short options too.\n");
	printf("  -p, --port <port>\tconnect to <port> on the remote machine\n");
	printf("  -q, --quiet\t\tsuppress the progress bar\n");
	printf("  -v, --verbose\t\texplain what is being done\n");
	printf("  -f, --force\t\toverwrite files and accept connections without prompts\n");
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
		{"quiet", no_argument, NULL, 'q'},
		{"force", no_argument, NULL, 'f'},
		{NULL, 0, NULL, 0}
	};

	while((val = getopt_long(argc, argv, "fp:qv", long_options, NULL)) > 0)
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

			case 'f':
				force_flag = 1;
			break;

			case 'q':
				quiet_flag = 1;
			break;

			case '?':
				exit(1);
			break;
		}
	}


	if(argc - optind != 1)
	{
		fprintf(stderr, "usage: %s [options] file_name.\n", argv[0]);
		fprintf(stderr, "Try --help for more options.\n");
		exit(1);
	}

	return optind;
}

int main(int argc, char** argv)
{
	int startindex = parseoptions(argc, argv);

	char* file_name = argv[startindex];

	FILE* fp;
	int lisnfd, sockfd;
	struct sockaddr_in local_addr, remote_addr;

	char* buf;
	int bytes_read;

	if(port <= 0)
	{
		fprintf(stderr, "error: invalid port number.\n");
		exit(1);
	}

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(port);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	if((fp = fopen(file_name, "r")) != NULL)
	{
		fclose(fp);

		if(!force_flag)
			printf("%s already exists. overwrite? (y/n) ", file_name);

		pt1:
		switch(getchar())
		{
			case 'Y':
			case 'y':
				//do nothing
			break;

			case 'N':
			case 'n':
				exit(0);
			break;

			default:
				goto pt1;
			break;
		}

		if((fp = fopen(file_name, "wb")) == NULL)
		{
			fprintf(stderr, "error: failed to overwrite file or file is locked.\n");
			exit(1);
		}
	}
	else if((fp = fopen(file_name, "wb")) == NULL)
	{
		fprintf(stderr, "error: failed to create file.\n");
		exit(0);
	}

	if((lisnfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "error: failed to obtain socket descriptor.\n");
		fclose(fp);
		exit(1);
	}

	if(bind(lisnfd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)) < 0)
	{
		fprintf(stderr, "error: failed to bind port %d.\n", port);
		fclose(fp);
		close(lisnfd);
		exit(1);
	}

	if(listen(lisnfd, 10) < 0)
	{
		fprintf(stderr, "error: failed to listen for connections on port %d.\n", port);
		fclose(fp);
		close(lisnfd);
		exit(0);
	}

	buf = (char*) malloc(sizeof(char) * 1024);

	int sin_size = sizeof(struct sockaddr_in);

	pt2:
	if((sockfd = accept(lisnfd, (struct sockaddr*)&remote_addr, &sin_size)) < 0)
	{
		goto pt2;
	}

	if(!force_flag)
	{
		printf("received a connection from %s. proceed? (y/n) ", inet_ntoa(remote_addr.sin_addr));

		while(1)
		{
			switch(getchar())
			{
				case 'Y':
				case 'y':
					send(sockfd, "PROCEED", 7, 0);
					goto pt3;
				break;

				case 'N':
				case 'n':
					send(sockfd, "REFUSED", 7, 0);
					goto pt2;
				break;
			}
		}
	}
	else
	{
		send(sockfd, "PROCEED", 7, 0);
	}

	ssize_t file_size, bytes_rcvd;
	int i;

	pt3:
	recv(sockfd, &file_size, sizeof(ssize_t), 0);
	bytes_rcvd = 0;

	buf = (char*) malloc(sizeof(char) * 1024);

	if(!quiet_flag)
		printf("\n");
	while(bytes_read = recv(sockfd, buf, sizeof(char) * 1024, 0))
	{
		if(bytes_read < 0)
		{
			fprintf(stderr, "error: lost connection to remote machine.\n");
			fclose(fp);
			close(lisnfd);
			close(sockfd);
			free(buf);
			exit(0);
		}
		if(fwrite(buf, sizeof(char), bytes_read, fp) < bytes_read)
		{
			fprintf(stderr, "error: failed to write file.\n");
			fclose(fp);
			close(lisnfd);
			close(sockfd);
			free(buf);
			exit(0);
		}

		bytes_rcvd += bytes_read;

		if(!quiet_flag)
		{
			printf("\r[");
			for(i = 0 ; i < (bytes_rcvd * 100) / file_size ; i++)
				printf("#");
			for(i ; i < 100 ; i++)
				printf("-");
			printf("] %dB (%d%%)", bytes_rcvd, (bytes_rcvd * 100) / file_size);
			fflush(stdout);
		}
	}
	printf("\n");

	if(bytes_rcvd != file_size)
	{
		fprintf(stderr, "error: lost connection to remote machine.\n");
	}

	send(sockfd, "ACK", 3, 0);

	fclose(fp);
	close(sockfd);
	close(lisnfd);
	free(buf);
}
