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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <sys/time.h>
#include "packet_struct.h"
#define MYPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 1000
#define PACKET_SIZE 500
#include <map>


int seed = 5 ;

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
    this function to calculate the probability of receiving an acknowledgment
*/
bool probability_recieve()
{
    int p= (rand() % 100) + 1;
    cout<<"probability of receive "<<p<<endl;
    return p > seed ;
}

/**
    this function to read the requested file
    @param values to put the data into it
    @file_name the name of the file
*/
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/**
    this function to send the file using send and wait approach
    @param file_name the name of requested file
    @param file_sz the size of the file
    @param sockfd the socket number of the client
    @param their_addr the address information of client
    @param addr_len the address length of the client
*/
int send_and_wait(string file_name,int file_sz, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{

    struct packet p;
    int counter = 0;
    int numbytes;
    bool first_time = true ;
    FILE *source;
    char c;

    // open the file
    source = fopen(file_name.c_str(), "r+b");
    fseek(source, 0, SEEK_SET);

    // check the existence of the file
    if (source != NULL)
    {

        for( int i = 0 ; i < file_sz; i++)
        {
            // put the data in packet
            p.data[i%PACKET_SIZE] = fgetc(source);

            // check if reach the packet size or it is the last packet
            if((i+1)%PACKET_SIZE == 0 || i == (file_sz-1))
            {
                p.len = ( (i+1)%PACKET_SIZE ==0 )? PACKET_SIZE : (i+1)%PACKET_SIZE;
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

                    // set the time out = 1 s
                    timeval timeout = { 1, 0 };
                    fd_set in_set;
                    FD_ZERO(&in_set);
                    FD_SET(sockfd, &in_set);
                    int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);

                    if (FD_ISSET(sockfd, &in_set))
                    {
                        if ((numbytes = recvfrom(sockfd,(struct ack_packet*)&acknowledgement, sizeof(acknowledgement), 0,
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
                        {
                            perror("recvfrom");
                            exit(1);
                        }


                        if(probability_recieve())
                        {
                            break;
                        }
                    }
                    cout<< "timeout ack num: "<<acknowledgement.ackno<<endl;
                }
            }

        }
        fclose(source);
    }
    else
    {
        printf("File not found.\n");
        return 0;
    }


}


/**
*/

/**
*/

void selective_repeat(string file_name,int file_sz, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    map<uint32_t,packet> buffer ;
    vector<uint32_t> order_list ;
    int window_size = 1 ;
    int max_window_size = 10 ;

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
            p.data[i%PACKET_SIZE] = fgetc(source);

            if((i+1)%PACKET_SIZE == 0 || i == (file_sz-1))
            {

                while(order_list.size() == 0 || !(buffer.size() < max_window_size && (order_list[order_list.size()-1]-order_list[0] +1) < max_window_size))
                {
                    if(order_list.size() == 0 || (buffer.size() < max_window_size && (order_list[order_list.size()-1]-order_list[0] +1) < max_window_size))
                    {
                        p.len = ( (i+1)%PACKET_SIZE ==0 )? PACKET_SIZE : (i+1)%PACKET_SIZE;
                        p.seqno = counter++;


                        if ((numbytes = sendto(sockfd,(struct packet*)&p, sizeof(p), 0,
                                               (struct sockaddr *)&their_addr, addr_len)) == -1)
                        {
                            perror("talker: sendto");
                            exit(1);
                        }
                        p.timer = time(NULL);

                        buffer.insert(std::pair<uint32_t,packet> (p.seqno,p));
                        order_list.push_back(p.seqno);
                        window_size++;
                    }


                    // rec ack from client

                    struct ack_packet acknowledgement;

                    timeval timeout = { 1, 0 };

                    fd_set in_set;

                    FD_ZERO(&in_set);
                    FD_SET(sockfd, &in_set);

                    // select the set
                    int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);

                    if (FD_ISSET(sockfd, &in_set))
                    {

                        if ((numbytes = recvfrom(sockfd,(struct ack_packet*)&acknowledgement, sizeof(acknowledgement), 0,
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
                        {
                            perror("recvfrom");
                            exit(1);
                        }


                        if(probability_recieve())
                        {
                            //cout<< "receive ack num: "<<acknowledgement.ackno<<endl;
                            break;
                        }
                        else
                        {

                            buffer.erase (acknowledgement.ackno);
                            order_list.erase(std::remove(order_list.begin(), order_list.end(), acknowledgement.ackno), order_list.end());

                        }



                    }


                    // after rcv check time out
                    for (std::map<uint32_t,packet>::iterator it=buffer.begin(); it!=buffer.end(); ++it)
                    {
                        struct packet  datagram= it->second ;
                        double duration = time(NULL)-datagram.timer;
                        if(duration > 5)
                        {
                            if ((numbytes = sendto(sockfd,(struct packet*)&datagram, sizeof(datagram), 0,
                                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
                            {
                                perror("talker: sendto");
                                exit(1);
                            }
                            datagram.timer = time(NULL);
                            buffer.erase(datagram.seqno);
                            buffer.insert(std::pair<uint32_t,packet> (datagram.seqno,datagram));
                        }

                    }


                }
            }

        }

        fclose(source);
    }

    else
    {
        printf("File not found.\n");
    }




}
/**
    this function to send the packets to the client
    @file_name the name of the file
*/
void send_packets(string file_name, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    cout<<file_name<<endl;
   // send_and_wait(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);

    selective_repeat(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);



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
