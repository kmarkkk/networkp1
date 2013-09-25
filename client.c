#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* simple client, takes two parameters, the server domain name,
   and the server port number */

int main(int argc, char** argv) {

  /* our client socket */
  int sock;
    unsigned short data_size=atoi(argv[3]);
  /* address structure for identifying the server */
  struct sockaddr_in sin;

  /* convert server domain name to IP address */
  struct hostent *host = gethostbyname(argv[1]);
  unsigned int server_addr = *(unsigned int *) host->h_addr_list[0];

  /* server port number */
  unsigned short server_port = atoi (argv[2]);

  char *sendbuffer;
  int counter=atoi(argv[4]);
  int count;
    if(counter>10000) {
        printf("count is too large");
        abort();
    }
        
    
  /* allocate a memory buffer in the heap */
  /* putting a buffer on the stack like:

         char buffer[500];

     leaves the potential for
     buffer overflow vulnerability */
  

  sendbuffer = (char *) malloc(data_size);
  if (!sendbuffer)
    {
      perror("failed to allocated sendbuffer");
      abort();
    }


  /* create a socket */
  if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
      perror ("opening TCP socket");
      abort ();
    }

  /* fill in the server's address */
  memset (&sin, 0, sizeof (sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = server_addr;
  printf(server_addr);
 // sin.sin_addr.s_addr="10.115.194.177";
  sin.sin_port = htons(server_port);

  /* connect to the server */
  if (connect(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0)
    {
      perror("connect to server failed");
      abort();
    }

  /* everything looks good, since we are expecting a
     message from the server in this example, let's try receiving a
     message from the socket. this call will block until some data
     has been received */
     
  if (count < 0)
    {
      perror("receive failure");
      abort();
    }

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
    int loop_count=0;
  while (loop_count<counter){ 
      loop_count++;
     // sleep(1);

    /* first byte of the sendbuffer is used to describe the number of
       bytes used to encode a number, the number value follows the first 
       byte */
    //printf("Enter the value of the number to send: ");
    //fgets(buffer, size, stdin);
     
      struct timeval tvStart;
      gettimeofday (&tvStart,NULL);
      int num1=tvStart.tv_usec;
      int num2=tvStart.tv_sec;
      /* for 32 bit integer type, byte ordering matters */
      *(int *) (sendbuffer+6) = (int) htonl(num1);
      *(int *) (sendbuffer+2) = (int) htonl(num2);
      *(unsigned short *) (sendbuffer) = ( unsigned short) htons(data_size);   
      
      send(sock, sendbuffer, data_size, 0);
      count = recv(sock, sendbuffer, data_size, 0);
      int temp_counter=count;
      while(temp_counter!=data_size){
          count=recv(sock, sendbuffer+temp_counter, data_size-temp_counter, 0);
          temp_counter+=count;
      }
      
      int recv_sec = (int) ntohl(*(int *)(sendbuffer+2));
      int recv_usec= (int) ntohl(*(int *)(sendbuffer+6));
      gettimeofday (&tvStart,NULL);
      int res_sec=tvStart.tv_sec-recv_sec;
      int res_usec=tvStart.tv_usec-recv_usec;
      printf("received %d bytes seq=%d time=%.3f\n",temp_counter, loop_count,res_usec/1000.0+res_sec*1000);
  }
  //  close(sock);
   // dump(&head, sock);
   // free(sendbuffer);
  return 0;
}
