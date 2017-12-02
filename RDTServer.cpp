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
#define MAXBUFLEN 1000
#define PACKET_SIZE 500
#define DURATION_INTERVAL 1
#include <map>

using namespace std;

int seed = 5 ; // default probability of loss = 5%
int max_window_size = 20 ; // default window size is 20
int algo_type = 1; // algo type
string MYPORT = "4950" ; // the port users will be connecting to


/**
    this function to get the size of the requested file
    @file_name the name of the file
*/
int get_file_size(string file_name)
{

    FILE *source;
    source = fopen(file_name.c_str(), "r+b");
    fseek(source, 0, SEEK_END);
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
    if(p <= seed )
        cout<<"probability of receive "<<p<<endl;
    return p > seed ;
}

/**
    this function to calculate the probability of receiving an acknowledgment
*/
bool probability_checksum()
{
    int p= (rand() % 100) + 1;
    if(p <= seed )
        cout<<"probability of receive "<<p<<endl;
    return p > 1 ;
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

    p.cksum = 0;

    // check the existence of the file
    if (source != NULL)
    {

        for( int i = 0 ; i < file_sz; i++)
        {
            // put the data in packet
            c = fgetc(source);
            p.data[i%PACKET_SIZE] = c;
            p.cksum+=c;

            // check if reach the packet size or it is the last packet
            if((i+1)%PACKET_SIZE == 0 || i == (file_sz-1))
            {
                p.len = ( (i+1)%PACKET_SIZE ==0 )? PACKET_SIZE : (i+1)%PACKET_SIZE;
                p.seqno = counter++;
                p.cksum = ~p.cksum;
                uint16_t correct = p.cksum;
                if(!probability_checksum())
                {
                    p.cksum+=1;
                }
                while(1)
                {

                    if ((numbytes = sendto(sockfd,(struct packet*)&p, sizeof(p), 0,
                                           (struct sockaddr *)&their_addr, addr_len)) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }
                    p.cksum = correct;



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
                            p.cksum = 0;
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
    this is selective repeat function send the file according to selective repeat approach
    @param file_name the name of requested file
    @param file_sz the size of the file
    @param sockfd the socket number of the client
    @param their_addr the address information of client
    @param addr_len the address length of the client
*/

void selective_repeat(string file_name,int file_sz, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    map<uint32_t,packet> buffer ;
    vector<uint32_t> order_list ;
    struct packet p;
    int counter = 0, three_ack = 0;
    int numbytes;
    FILE *source;
    int i;
    char c;
    int last_packet = -1;
    bool enter = true;

    // start read the file
    source = fopen(file_name.c_str(), "r+b");
    fseek(source, 0, SEEK_SET);

    if (source != NULL)
    {
        for( i = 0 ; i < file_sz; i++)
        {

            p.data[i%PACKET_SIZE] = fgetc(source);

            enter  = true;

            if((i+1)%PACKET_SIZE == 0 || i == (file_sz-1))
            {

                while((i== file_sz-1 && buffer.size()!=0)||enter || !( (buffer.size() < max_window_size) && (order_list.size() == 0 ||((counter-order_list[0] ) < max_window_size))))
                {

                    enter = false;
                    if((last_packet != i)&&(order_list.size() == 0 || ((order_list[order_list.size()-1]-order_list[0] +1) < max_window_size)))
                    {
                        cout<<"Create a packet num : "<<counter+1<<endl;

                        p.len = ( (i+1)%PACKET_SIZE ==0 )? PACKET_SIZE : (i+1)%PACKET_SIZE;
                        p.seqno = counter++;
                        last_packet = i;

                        if ((numbytes = sendto(sockfd,(struct packet*)&p, sizeof(p), 0,
                                               (struct sockaddr *)&their_addr, addr_len)) == -1)
                        {
                            perror("talker: sendto");
                            exit(1);
                        }
                        p.timer = time(NULL);

                        // put the packet in the buffer map and the vector list
                        buffer.insert(std::pair<uint32_t,packet> (p.seqno,p));
                        order_list.push_back(p.seqno);

                    }


                    // rec ack from client
                    struct ack_packet acknowledgement;

                    timeval timeout = { 1/100, 0 };
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
                            if(std::find(order_list.begin(), order_list.end(), acknowledgement.ackno) != order_list.end())
                            {
                                max_window_size++;
                                if( acknowledgement.ackno > order_list[0] )
                                {
                                    three_ack++;

                                    if(three_ack == 3)
                                    {
                                        cout<<"Three acknowlegde happen"<<endl;
                                        struct packet old_packet = buffer[order_list[0]];
                                        if ((numbytes = sendto(sockfd,(struct packet*)&old_packet, sizeof(old_packet), 0,
                                                               (struct sockaddr *)&their_addr, addr_len)) == -1)
                                        {
                                            perror("talker: sendto");
                                            exit(1);
                                        }

                                        max_window_size = max_window_size/2 + 1 ;
                                        three_ack = 0;
                                    }
                                }
                                buffer.erase (acknowledgement.ackno);
                                order_list.erase(std::remove(order_list.begin(), order_list.end(), acknowledgement.ackno), order_list.end());
                            }

                        }




                    }

                    // check the buffer list of not acknowledged packets
                    for(auto const& value: order_list)
                    {


                        struct packet  datagram= buffer[value] ;
                        double duration = time(NULL)-datagram.timer;

                        if(duration >  DURATION_INTERVAL)
                        {
                            cout<<"resend packet number : "<<datagram.seqno<<endl;
                            max_window_size = 1;
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

/**********************************************************************************************/
/**
    this is go back n function send the file according to go back n approach
    @param file_name the name of requested file
    @param file_sz the size of the file
    @param sockfd the socket number of the client
    @param their_addr the address information of client
    @param addr_len the address length of the client
*/

void go_back_n(string file_name,int file_sz, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    map<uint32_t,packet> buffer ;
    vector<uint32_t> order_list ;
    struct packet p;
    int counter = 0, three_ack = 0;
    int numbytes;
    FILE *source;
    int i;
    char c;
    int last_packet = -1;
    bool enter = true;

    // start read the file
    source = fopen(file_name.c_str(), "r+b");
    fseek(source, 0, SEEK_SET);

    if (source != NULL)
    {
        for( i = 0 ; i < file_sz; i++)
        {

            p.data[i%PACKET_SIZE] = fgetc(source);

            enter  = true;

            if((i+1)%PACKET_SIZE == 0 || i == (file_sz-1))
            {

                while((i== file_sz-1 && buffer.size()!=0)||enter || !( (buffer.size() < max_window_size) && (order_list.size() == 0 ||((counter-order_list[0] ) < max_window_size))))
                {

                    enter = false;
                    if((last_packet != i)&&(order_list.size() == 0 || ((order_list[order_list.size()-1]-order_list[0] +1) < max_window_size)))
                    {

                        p.len = ( (i+1)%PACKET_SIZE ==0 )? PACKET_SIZE : (i+1)%PACKET_SIZE;
                        p.seqno = counter++;
                        last_packet = i;

                        if ((numbytes = sendto(sockfd,(struct packet*)&p, sizeof(p), 0,
                                               (struct sockaddr *)&their_addr, addr_len)) == -1)
                        {
                            perror("talker: sendto");
                            exit(1);
                        }
                        p.timer = time(NULL);

                        // put the packet in the buffer map and the vector list
                        buffer.insert(std::pair<uint32_t,packet> (p.seqno,p));
                        order_list.push_back(p.seqno);

                    }


                    // rec ack from client
                    struct ack_packet acknowledgement;

                    timeval timeout = { 1/100, 0 };
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
                            //cout<<"ack num : ///////////////////////////////"<<acknowledgement.ackno<<endl;
                           // if(std::find(order_list.begin(), order_list.end(), acknowledgement.ackno) != order_list.end())
                           // {
                               // cout<<"ack num : "<<acknowledgement.ackno<<endl;
                                for(auto const& value: order_list)
                                {
                                    if(value <= acknowledgement.ackno)
                                    {
                                        //cout<<"remove : "<<value<<endl;
                                        buffer.erase (value);
                                        order_list.erase(std::remove(order_list.begin(), order_list.end(), value), order_list.end());
                                    }
                                    else
                                    {
                                        break;
                                    }
                                //}


                            }

                        }




                    }

                    if(order_list.size() != 0)
                    {
                        double duration = time(NULL)-buffer[order_list[0]].timer;

                        if(duration >  DURATION_INTERVAL)
                        {
                            cout<<"timeout occurs for the first packet"<<endl;
                            for(auto const& value: order_list)
                            {
                               // cout<<"resend packet num: "<<value<<endl;
                                struct packet  datagram= buffer[value] ;
                                // max_window_size = 1;
                                if ((numbytes = sendto(sockfd,(struct packet*)&datagram, sizeof(datagram), 0,
                                                       (struct sockaddr *)&their_addr, addr_len)) == -1)
                                {
                                    perror("talker: sendto");
                                    exit(1);
                                }
                                //datagram.timer = time(NULL);
                             //   buffer.erase(datagram.seqno);
                               // buffer.insert(std::pair<uint32_t,packet> (datagram.seqno,datagram));
                            }
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
void send_packets(int wait_repeat_goback,string file_name, int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    if(wait_repeat_goback == 1)
        send_and_wait(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);
    else if(wait_repeat_goback == 2)
        selective_repeat(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);
    else
        go_back_n(file_name,get_file_size(file_name), sockfd, their_addr, addr_len);
}

/**
    @param void
    creates new socket file without binding it to the port number
    @return the brand new created socket
*/

int get_new_socket_fd()
{

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
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
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return -1;
    }
    freeaddrinfo(servinfo);

    return sockfd ;
}

void parse_args()
{

    string line;
    ifstream infile("server.in");


    getline(infile, line);
    if(line.compare("null") != 0)
        MYPORT =  line.c_str();

    getline(infile, line);
    if(line.compare("null") != 0)
        max_window_size= atoi(line.c_str());

    getline(infile, line);
    if(line.compare("null") != 0)
        seed= (int)(atof(line.c_str())*100);

    getline(infile, line);
    if(line.compare("null") != 0)
        algo_type = atoi(line.c_str());


    cout<<"Port : "<<MYPORT<<endl;
    cout<<"Max window size : "<<max_window_size<<endl;
    cout<<"Seed : "<<seed<<endl ;
    cout<<"Algo : "<<algo_type<<endl ;

}

int main(void)
{

    parse_args();
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

    if ((rv = getaddrinfo(NULL, MYPORT.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
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
        return -1;
    }
    freeaddrinfo(servinfo);
    addr_len = sizeof their_addr;

    while(1)
    {
        // wait for a new connection request
        printf("listener: waiting to recvfrom...\n");

        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        // adding ull to terminal file name string
        buf[numbytes] = '\0';

        // creates new child to handle the client
        if(!fork())
        {
            cout<<"new connection"<<endl;
            // create new socket fd for the client
            int new_sockfd = get_new_socket_fd();

            char * ack_message = "ACK File" ;

            // send the ACK on the new socket fd
            if ((numbytes = sendto(new_sockfd,ack_message, strlen(ack_message), 0,
                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }

            // send the remaining packets on the new socket fd
            send_packets(algo_type,buf, new_sockfd, their_addr, addr_len);
        }

    }

    return 0;
}
