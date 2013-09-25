#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define HEADER_SIZE 10

/* simple client, takes two parameters, the server domain name,
   and the server port number */
int 
main(int argc, char** argv) {

	if (argc != 5) {
		fprintf(stderr, "Usage: ./client <hostname> <port> <size> <count>\n");
		exit(0);
	}
  /* our client socket */
  int sock;
 	unsigned short data_size = atoi(argv[3]);
	if (data_size < 10) {
		fprintf(stderr, "Date size should fall in range [10, 65535]\n");
		abort();
	}
  /* address structure for identifying the server */
  struct sockaddr_in sin;
  /* convert server domain name to IP address */
  struct hostent *host = gethostbyname(argv[1]);
	/* Save the ip address as bytes in an unsigned int */
	/* Check if the address requested exist */
	if (!host) {
		fprintf(stderr, "Get host by name: Host IP Not Found\n");
		abort();
	}
	unsigned int server_addr = *(unsigned int *)host->h_addr_list[0];

  /* server port number */
  unsigned short server_port = atoi(argv[2]);
	/* Check the times message send */
  int counter = atoi(argv[4]);
	if (counter < 1 || counter > 10000) {
			fprintf(stderr, "count should fall in range [10, 10000]\n");
			abort();
	}
  /* putting a buffer on the stack like:

         char buffer[500];

     leaves the potential for
     buffer overflow vulnerability 
	*/
  /* Allocate a memory buffer in the heap */
  char *sendbuffer = (char *)malloc(data_size + HEADER_SIZE);
  if (!sendbuffer) {
      perror("failed to allocated sendbuffer");
      abort();
  }

  /* create a socket */
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
     	perror("opening TCP socket");
      abort();
   }

  /* fill in the server's address */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = server_addr;
  //printf(server_addr);
  sin.sin_port = htons(server_port);

  /* connect to the server */
  if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      perror("connect to server failed");
      abort();
   }

  /* everything looks good, since we are expecting a
     message from the server in this example, let's try receiving a
     message from the socket. this call will block until some data
     has been received */
	/*
  int count;
  if (count < 0) {
      perror("receive failure");
      abort();
   }
	 */

  /* in this simple example, the message is a string, 
     we expect the last byte of the string to be 0, i.e. end of string 
  	if (buffer[count-1] != 0)
    {
       In general, TCP recv can return any number of bytes, not
	 			necessarily forming a complete message, so you need to
	 			parse the input to see if a complete message has been received.
         if not, more calls to recv is needed to get a complete message.
      
      printf("Message incomplete, something is still being transmitted\n");
    } 
  	else
    {
      printf("Here is what we got: %s", buffer);
    }
   
   */
	int loop_count = 0;
	int temper_byte_count;
	int total_byte_count;
	int fail;
	int total_time = 0;
	/* The size of the data field in network order */
	unsigned short net_data_size = htons(data_size);
	while (loop_count < counter) {
		sleep(1);
		loop_count++;
		/* first byte of the sendbuffer is used to describe the number of
		 bytes used to encode a number, the number value follows the first 
		 byte */
		struct timeval tvStart;
		gettimeofday(&tvStart, NULL);
		/*
		int num1 = tvStart.tv_usec;
		int num2 = tvStart.tv_sec;
		*(unsigned short *)sendbuffer = (unsigned short)htons(data_size);
		*(int *)(sendbuffer + 6) = (int)htonl(num1);
		*(int *)(sendbuffer + 2) = (int)htonl(num2);
		*/
		
		/* for 32 bit integer type, byte ordering matters */
		/* Author : drunk Fred */
		int tv_sec = htonl(tvStart.tv_sec);
		int tv_usec = htonl(tvStart.tv_usec);
		memset(sendbuffer, 0, data_size);
		memcpy(sendbuffer, &net_data_size, 2);
		memcpy(sendbuffer + 2, &tv_sec, 4);
		memcpy(sendbuffer + 6, &tv_usec, 4);
		/* Padding the data field */
		memset(sendbuffer + 10, 0, data_size - HEADER_SIZE);
		
		/* Send and handle partial send here */
		temper_byte_count = 0;
		total_byte_count = 0;
		fail = 0;
		while (total_byte_count != data_size) {
			temper_byte_count = send(sock, sendbuffer + total_byte_count,
					data_size - total_byte_count, 0);
			/* Network error may occur, stop this round of sending */
			if (temper_byte_count < 0) {
				fail = 1;
				break;
			}
			total_byte_count += temper_byte_count;
		}
		/* Package loss, forgo this send, go to next ping */
		if (fail) {
			printf("Sending failure: send returns -1\n");
			continue;
		}

		/* Receive and handle partial receive here */
		temper_byte_count = 0;
		total_byte_count = 0;
		fail = 0;
		/* Clear buffer */
		memset(sendbuffer, 0, data_size);
		while (total_byte_count != data_size) {
				temper_byte_count = recv(sock, sendbuffer + total_byte_count,
						data_size - total_byte_count, 0);
				// temp_counter += count;
				if (temper_byte_count < 0) {
					fail = 1;
					break;
				}
				total_byte_count += temper_byte_count;
		}
		/* Package loss, forgo this recv, go to next ping */
		if (fail) {
			printf("Recving failure on %d seq: recv returns -1\n", loop_count);
			continue;
		}
		
		int recv_sec = (int)ntohl(*(int *)(sendbuffer + 2));
		int recv_usec = (int)ntohl(*(int *)(sendbuffer + 6));
		gettimeofday(&tvStart, NULL);
		int res_sec = tvStart.tv_sec - recv_sec;
		int res_usec = tvStart.tv_usec - recv_usec;
		total_time += res_usec / 1000.0 + res_sec * 1000.0;
		printf("received %d bytes seq=%d time=%.3f\n",
				total_byte_count, loop_count, res_usec / 1000.0 + res_sec * 1000);
	}
	if (close(sock) < 0) {
		perror("Close socket");
		abort();
	}
  return (0);
}
