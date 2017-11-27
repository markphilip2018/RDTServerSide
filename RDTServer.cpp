#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "packet_struct.h"
#define MYPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 100

using namespace std;

/**
    this function to get the size of the requested file
    @file_name the name of the file
*/
int get_file_size(string file_name){

    FILE *source;
    source = fopen(file_name.c_str(), "r+b");
    fseek(source, 0, SEEK_END); // Set the new position at 10
    int sz = ftell(source);
    fclose(source);
    return sz;

}
/**
    this function to read the requested file
    @param values to put the data into it
    @file_name the name of the file
*/
int read_file(char *values,string file_name)
{
    FILE *source;

    source = fopen(file_name.c_str(), "r+b");

    fseek(source, 0, SEEK_END);
    int sz = ftell(source);
    cout <<"size: "<<sz;
    fseek(source, 0, SEEK_SET);
    if (source != NULL)
    {
        for (i = 0; i < sz; i++)
        {
            c = fgetc(source); // Get character
            values[i] = c; // Store characters in array
        }
    }
    else
    {
        printf("File not found.\n");
        return 0;
    }
    fclose(source);

    /* Add null to end string */
    values[i] = '\0';
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
    this function to send the packets to the client
    @file_name the name of the file
*/
void send_packets(string file_name)
{
    cout<<file_name<<endl;
    char values[get_file_size(file_name)] ;
    read_file(values,file_name);
    cout<<"values:"<<values<<endl;

    /// here create a packet
   // stop_and_wait(values);
}
int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
// loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");
    addr_len = sizeof their_addr;


    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }

     buf[numbytes] = '\0';

    char * ack_message = "ACK File" ;
    if ((numbytes = sendto(sockfd,ack_message, strlen(ack_message), 0,
                           (struct sockaddr *)&their_addr, addr_len)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }

    send_packets(buf);

    printf("listener: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));

    return 0;
}
