#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

// read status macro
#define READ_NONE  1
#define READ_INCOMPLETE  2
#define READ_FINISHED  3

struct node {
  int socket;
  int read_status;
  struct sockaddr_in client_addr;
  char *payload;
  unsigned int data_transmitted;
  int data_received;
  unsigned int data_total;
  FILE *fd;
  struct node *next;
};


void send_exception_msg(struct node *current, char *msg);
void handle_server_request(char* payload, struct node* current, 
		char *directory_path);

/**
 * Delete a node from the linked list 
 */
void
dump(struct node *head, int socket) {
  struct node *current, *temp;
  current = head;
  while (current->next) {
    if (current->next->socket == socket) {
      temp = current->next;
      current->next = temp->next;
      free(temp); 
      return;
    } else {
      current = current->next;
    }
  }
}

/**
 * Add a node to the linked list 
 */
void
add(struct node *head, int socket, struct sockaddr_in addr) {
  struct node *new_node;
  new_node = (struct node *)malloc(sizeof(struct node));
  new_node->read_status = READ_NONE;
  new_node->socket = socket;
  new_node->client_addr = addr;
  new_node->payload = NULL;
  new_node->fd = NULL;
  new_node->next = head->next;
  head->next = new_node;
}

/**
 * parse the http request header from the broser, return 
 * the file path 
 */
char *
headParse(char *p) {
  char *ptmp = p;
  if(strncmp(ptmp, "GET /", 5) == 0) { 
    ptmp += 5; 
    while(*ptmp != ' ') {
			if (*ptmp == '/') {
				if (strncmp(ptmp, "/../", 4) == 0) {
					return NULL;
				}
			}
      ptmp++; 
    }
    *ptmp++ = '\0'; 
    if (strncmp(ptmp, "HTTP/", 5) == 0) {
      return p + 5;  
		} else {
      return NULL; 
		}
  } else {
    return NULL;     
	}
}


struct node head;

int
main(int argc, char **argv) {

  int sock, new_sock, max;
  int optval = 1;

  struct sockaddr_in sin, addr;
  unsigned short server_port = atoi(argv[1]);
  int server_flag = 0;
  char *directory_path;
  if (argc > 2) {
		if (strcmp(argv[2], "www") == 0) {
    	server_flag = 1;
			if (argc > 3) {
				directory_path = argv[3];
			} else {
				perror("www model misses root directory");
				printf("Usage: ./serve <port> www <root_directory>\n");
				return (1);
			}
		}
  }
	/* Set up the server socket bind and listen */
  socklen_t addr_len = sizeof(struct sockaddr_in);
  int BACKLOG = 5;
  fd_set read_set, write_set;
  struct timeval time_out;
  int select_retval;
  int count;
  struct node *current;
  char *buf;
  buf = (char *)malloc(65590);
  head.socket = -1;
  head.next = 0;
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    perror("opening TCP socket");
    abort();
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval,
				sizeof(optval)) < 0) {
    perror("setting TCP socket option");
    abort();
  }
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(server_port);
  if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("binding socket to address");
    abort();
  }
  if (listen(sock, BACKLOG) < 0) {
    perror ("listen on socket failed");
    abort();
  }
  while (1) {

    /* set up the file descriptor bit map that select should be watching */
    FD_ZERO(&read_set); /* clear everything */
    FD_ZERO(&write_set); /* clear everything */

    FD_SET(sock, &read_set); /* put the listening socket in */
    max = sock; /* initialize max */

    for (current = head.next; current; current = current->next) {
      if(current->read_status == READ_NONE || current->read_status == READ_INCOMPLETE) {
      FD_SET(current->socket, &read_set);
      }

      if (current->fd != NULL || current->payload != NULL) {
				FD_SET(current->socket, &write_set);
      }
      if (current->socket > max) {
				max = current->socket;
      }
    }

    time_out.tv_usec = 100000; /* 1-tenth of a second timeout */
    time_out.tv_sec = 0;
    select_retval = select(max + 1, &read_set, &write_set, NULL, &time_out);
    if (select_retval < 0) {
      perror("select failed");
      abort();
    }

    if (select_retval == 0) {
      continue;
    }
		/* at least one file descriptor is ready */
    if (select_retval > 0) {
			/* check the server socket */
			if (FD_ISSET(sock, &read_set)) {
				/* there is an incoming connection, try to accept it */
				new_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);
				if (new_sock < 0) {
					perror("error accepting connection");
					abort();
				}
				if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0) {
					perror("making socket non-blocking");
					abort();
				}
				printf("Accepted connection. Client IP address is: %s\n",
						inet_ntoa(addr.sin_addr));
				add(&head, new_sock, addr);
			}
		}
		for (current = head.next; current; current = current->next) {
			/* see if we can now do some previously unsuccessful writes */
			if (FD_ISSET(current->socket, &write_set)) {
				/* in this case there is unfinished outbound writes */
				if (current->payload != NULL && current->data_transmitted != 0 &&
						current->read_status == READ_FINISHED) {
					int amount = send(current->socket,
							current->payload + current->data_transmitted,
							current->data_total - current->data_transmitted, 0);
					current->data_transmitted += amount;
					if (current->data_total == current->data_transmitted) {
						if (current->fd == NULL) {
							if (server_flag) {
								free(current->payload);
								close(current->socket);
								dump(&head, current->socket);
							} else {
								current->read_status = READ_NONE;
							}
						} else {
							current->payload = NULL;
						}
					}
					/** 
						* in this case the msg header finishes transmission but the file 
					  *  transmission is not finished   
					  */
				} else if (current->fd != NULL &&
					current->read_status == READ_FINISHED) {
					printf("sending file \n");
					char *read_buf = (char*)malloc(10000);
					if (!read_buf) {
						perror("Error malloc read_buf");
						abort();
					}
					size_t amount = fread(read_buf, 1, 10000, current->fd);
					size_t amount_send = send(current->socket, read_buf, amount, 0);
					fseek(current->fd, -(long)(amount-amount_send), SEEK_CUR);
					char c;
					if ((c = fgetc(current->fd)) == EOF) {
						printf("finishing file transfer \n");
						fclose(current->fd);
						close(current->socket);
						dump(&head, current->socket);
					} else {
						fseek(current->fd, -1, SEEK_CUR);
					}
					free(read_buf);
				}
			}
            
      /* socket read is ready */
			if (FD_ISSET(current->socket, &read_set)) {
				if (!server_flag) {
					/* The starting of the read procedure 
					   recv the msg at best effort*/
					if (current->read_status == READ_NONE) {
						char *buffer = (char *)malloc(20000);
						if (!buffer) {
							perror("Error malloc buffer");
							abort();
						}
						current->payload = buffer;
						count = recv(current->socket, buf, 2, 0);
						current->data_received = count;
						current->read_status = READ_INCOMPLETE;
						if (count < 0) {
							close(current->socket);
							dump(&head, current->socket);
						} 
						if(count >= 2) {
							int size = (unsigned short)ntohs(*(unsigned short *)(buf));
							current->data_total = size;
						}
					} else if (current->read_status == READ_INCOMPLETE) {
						if (current->data_received < 2) {
							int count = recv(current->socket, buf, 2, 0);
							current->data_received += count;
							if (count >= 2) {
								int size = (unsigned short)ntohs(*(unsigned short *)(buf));
								current->data_total = size;
							}
							if (count < 0) {
								close(current->socket);
								dump(&head, current->socket);
							} 
							continue;
						}
						int count = recv(current->socket,
								current->payload + current->data_received,
								current->data_total - current->data_received, 0);   
						if (count < 0) {
							close(current->socket);
							dump(&head, current->socket);
						} 
						current->data_received += count; 
						if (current->data_total == current->data_received) {
							current->read_status = READ_FINISHED;
							count = send(current->socket, current->payload,
									current->data_total, 0);
							current->data_transmitted = count; 
						}
					}
				} else { 
					/* run as the server  */
					if (current->read_status == READ_NONE) {
						char *buffer = malloc(10000);
						if (!buffer) {
							perror("Error malloc buffer");
							abort();
						}
						int amount1 = recv(current->socket, buffer, 500 ,0);
						current->payload = buffer;
						current->data_received = amount1;
						printf("data rec is %d\n", current->data_received);
						current->read_status = READ_INCOMPLETE;
						if (0 == strncmp("\r\n\r\n",
									current->payload + current->data_received - 4, 4)){
							current->read_status = READ_FINISHED;
							handle_server_request(current->payload,
									current, directory_path);
						}
					} else if (current->read_status == READ_INCOMPLETE) {
						int amount2 = recv(current->socket,
								current->payload + current->data_received, 500 , 0);
						if (amount2 > 0) {
							current->data_received += amount2;
						}
						if (0 == strncmp("\r\n\r\n",
									current->payload + current->data_received - 4, 4)) {
							current->read_status = READ_FINISHED;
							handle_server_request(current->payload, current,directory_path
								);
						}
					}
				}
			}
		}
	}
}

void
handle_server_request(char* payload, struct node* current,
		char *directory_path) {
	printf("handle server request \n");
	char* path = headParse(payload);
	free(payload); 
	if (path == NULL) {
		char *msg = 
			"HTTP/1.1 400 Bad Request\r\nContent-Type:text/html\r\n\r\n<h>ERROR 400 </h><img src =\"error.jpg\">";
		send_exception_msg(current, msg);
		return;
	}
	char final_path[100];
	strcpy(final_path, directory_path);
	strcat(final_path, path);
	printf(final_path);
	FILE *fd = fopen(final_path, "r");
	if (fd == NULL) {
		char *msg = "HTTP/1.1 404 Not Found\r\nContent-Type:text/html\r\n\r\n<h>ERROR 404 </h>>";
		send_exception_msg(current, msg);
		return;
	} else {
		char *msg = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n";
		current->payload = msg;
		current->fd = fd;
		current->data_total = (unsigned)strlen(msg) * sizeof(char);
		int transmitted = send(current->socket, msg, current->data_total, 0);
		printf("data total is %d transmitted is %d", current->data_total,
				transmitted);
		if (transmitted == strlen(msg)) {
			printf("header finished \n");
			current->payload = NULL;
		} else {
			current->data_transmitted = transmitted;
		}
	}
}

void
send_exception_msg(struct node *current, char *msg) {
	printf(msg);
	current->payload = msg;
	current->data_total = (unsigned)strlen(msg) * sizeof(char);
	int transmitted = send(current->socket, current->payload,
			current->data_total, 0);
	printf("data total is %d transmitted is %d", current->data_total,
			transmitted);
	if (transmitted == current->data_total){
		printf("finishing transmission, closing socket \n");
		close(current->socket);
		dump(&head, current->socket);
	} else {
		current->data_transmitted = transmitted;
	}
}
