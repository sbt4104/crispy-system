#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include<signal.h>
#include <algorithm> 
#include <chrono> 
#include <iostream>
#include <unistd.h> 
#include <vector>
using namespace std; 
using namespace std::chrono;

//note, to allow root to use icmp sockets, run:
//sysctl -w net.ipv4.ping_group_range="0 0"

int total_time = 0;
float packet_loss = 0;
int sequence = 0;
float failed =0;

void handler(int code){

    cout<<"\npackets transmitted: "<<sequence<<", packets received: "<<sequence - failed
        <<" loss percentage : "<< failed/sequence*100<<"%, total_time: "<<total_time
        <<" milliseconds\n";
    exit(code);
}

void ping_it(struct in_addr *dst, float ttl_val, bool flag_ttl = false)
{
    struct icmphdr icmp_hdr;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr); 

    int sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_ICMP);
    if (sock < 0) {
        perror("socket");
        return ;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr = *dst;
    memset(&icmp_hdr, 0, sizeof icmp_hdr);
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = 1234;//arbitrary id

    float loss_percentage;
    char *message = "echo shubham 12345123 hello world ";
    
    for (;;) {
        unsigned char data[2048];
        int rc;

        socklen_t slen;
        struct icmphdr rcv_hdr;

        icmp_hdr.un.echo.sequence = sequence++;

        memcpy(data, &icmp_hdr, sizeof icmp_hdr);
        memcpy(data + sizeof icmp_hdr, message, strlen(message)); //icmp payload
                
        auto start = high_resolution_clock::now();   //clock start
        rc = sendto(sock, data, sizeof icmp_hdr + strlen(message),
                        0, (struct sockaddr*)&addr, sizeof addr);

        if (rc <= 0) {
            perror("Sendto");
            break;
        }        
        if (rc == 0) {
            puts("Got no reply\n");
            continue;
        } else if (rc < 0) {
            perror("Select");
            break;
        }

        //we don't care about the sender address for receiving reply
        slen = 0;

        unsigned char buff[2048];
        memset(buff, 0, strlen(message)); 

        rc = recvfrom(sock, buff,sizeof icmp_hdr + strlen(message), 0, NULL,
             &slen);
        auto stop = high_resolution_clock::now();  // clock stop

        auto duration = duration_cast<milliseconds>(stop - start); 
        total_time += duration.count();
        
        //cout<<endl<<(float)duration.count()/1000<<" "<<ttl_val<<" "<<flag_ttl<<endl;
        if(((float)duration.count()/1000) > ttl_val && flag_ttl==true){
            failed++;
            cout<<"failed due to time out\n";
            continue;
        }        

        data[10]=0;
        memcpy(&rcv_hdr, buff, sizeof rcv_hdr);
        if (rcv_hdr.type == ICMP_ECHOREPLY) {
            /* 
            calculating accuracy
            The echo reply is an ICMP message generated in response to 
            an echo request; it is mandatory for all hosts, and must 
            include the exact payload received in the request.
            */
            packet_loss = 0;
            for(int test=sizeof(icmp_hdr);test<sizeof(icmp_hdr) + 
                strlen(message);test++){
                    if(buff[test]!=message[test-8]){
                        packet_loss++;
                    }
            }

            loss_percentage = (packet_loss/strlen(message))*100;

            cout<<endl;
            printf("ICMP Reply, id=0x%x, sequence =  0x%x\n",
                            icmp_hdr.un.echo.id, icmp_hdr.un.echo.sequence);
        } else {
            printf("Got ICMP packet with type 0x%x ?!?\n", rcv_hdr.type);
        }
    
        printf("message_size: %d packet_loss: %f% RTT: %d miliseconds",strlen(message),
            loss_percentage, duration.count());    
    }

    usleep(300000);
}

int main(int argc, char *argv[])
{
    vector<string> cmdLineArgs(argv, argv+argc);
    int i = 0;
    bool flag_ttl = false;
    float ttl_val = 0; 
    for(auto& arg : cmdLineArgs)
    {
        if(arg == "--help" || arg == "-help")
        {
            std::cout << "Helpful stuff here\n";
        }
        if(arg == "-t")
        {
            if(i+1<cmdLineArgs.size()){
                ttl_val = stof(cmdLineArgs[i+1]);
                flag_ttl = true;
            }
        }
        i++;
    }

    struct sigaction newact;

    // Specify that we want the handler function to handle the
    // signal:
    newact.sa_handler = handler;

    // Use default flags:
    newact.sa_flags = 0;

    // Specify that we don't want any signals to be blocked during
    // execution of handler:
    sigemptyset(&newact.sa_mask);

    // Modify the signal table so that handler is called when
    // signal SIGINT is received:
    sigaction(SIGINT, &newact, NULL);


    if (argc < 2) {
        printf("usage: %s destination_ip\n", argv[0]);
        return 1;
    }
    struct in_addr dst;

    
    if (inet_aton(argv[1], &dst) == 0) {

        perror("inet_aton");
        printf("%s isn't a valid IP address\n", argv[1]);
        return 1;
    }
    
    ping_it(&dst, ttl_val = ttl_val, flag_ttl = flag_ttl );
    cout<<"\ntotal_time: "<<total_time<<" milliseconds\n";
    return 0;
}