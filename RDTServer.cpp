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
#include <algorithm>
#include "packet_struct.h"
#define MYPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 100
#define PACKET_SIZE 500

using namespace std;

/**
    this function to get the size of the requested file
    @file_name the name of the file
*/
int get_file_size(string file_name)
{

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
/*int read_file(char *values,string file_name,int sz, int start, int end)
{
    FILE *source;
    int i;
    char c;
    source = fopen(file_name.c_str(), "r+b");

    fseek(source, 0, SEEK_SET);

    if (source != NULL)
    {
        int counter = 0;
        for(int j = 0 ; j < start ; j++)
            c = fgetc(source);

        for ( i = start; i < min(end,sz); i++)
        {
            c = fgetc(source); // Get character
            values[counter++] = c; // Store characters in array
        }
    }
    else
    {
        printf("File not found.\n");
        return 0;
    }
    fclose(source);
}*/

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int send_and_wait(string file_name,int file_sz, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{

    struct packet p;
    int counter = 0;
    int numbytes;
    bool first_time = true ;
    FILE *source;
    int i;
    char c;
    source = fopen(file_name.c_str(), "r+b");

    fseek(source, 0, SEEK_SET);

    if (source != NULL)
    {

        for( i = 0 ; i < file_sz; i++)
        {
            p.data[i%500] = fgetc(source);

            if((i+1)%500 == 0 || i == (file_sz-1))
            {
                p.len = ( (i+1)%500 ==0 )? 500 : (i+1)%500;
                p.seqno = counter++;

                while(1)
                {


                    if ((numbytes = sendto(sockfd,(struct packet*)&p, sizeof(p), 0,
                                           (struct sockaddr *)&their_addr, addr_len)) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }


                    struct ack_packet acknowledgement;

                    timeval timeout = { 3, 0 };
                    fd_set in_set;

                    FD_ZERO(&in_set);
                    FD_SET(sockfd, &in_set);

                    // select the set
                    int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);

                    //cout << "cnt: "<<cnt<<endl;
                    if (FD_ISSET(sockfd, &in_set))
                    {
                        if ((numbytes = recvfrom(sockfd,(struct ack_packet*)&acknowledgement, sizeof(acknowledgement), 0,
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
                        {
                            perror("recvfrom");
                            exit(1);
                        }

                        cout<< "receive ack num: "<<acknowledgement.ackno<<endl;
                        break;
                    }
                    cout<< "timeout ack num: "<<acknowledgement.ackno<<endl;
                }
            }




        }

    }
    else
    {
        printf("File not found.\n");
        return 0;
    }
    fclose(source);

}
/**
    this function to send the packets to the client
    @file_name the name of the file
*/
void send_packets(string file_name, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    cout<<file_name<<endl;
    send_and_wait(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);
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
    // (char values[], int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
    send_packets(buf, sockfd, their_addr, addr_len);



    return 0;
}
