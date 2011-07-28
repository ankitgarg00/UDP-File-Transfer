/*
 * Server Application - Receives files over the network
 * Design -
 * 1. Server listens for File transfer requests on a well known port
 * 2. After receiving a request spawns a server child process to handle the data.
 * 3. Server child process creates a new socket and sends a response from a new port
 * 4. Server child process then receives any data from the client on this new socket
 * 	  and writes to the file
 * 5. Server child process receives a End of file request from client and then exits.
 * 6. Server Parent process continues to listen for more incoming client requests
 * Created by - Ankit Garg
 */


#include "utilities.h"
#define NEW_PORT    9900 //server uses this constant value + number of clients to generate a new port
//number of clients.
static int numClients = 0;

//header for DG
struct hdr {
  uint32_t      opcode;         //operation code
  uint32_t      seq;            //sequence #
  uint32_t      ts;             //timestamp when sent
};

/*
 *This function is called by the server child process.
 *It receives the file data and writes it into the file pointed by fp
 *fp - File which needs to be written
 *sockfd - Socket which the server is expecting client to send file data
 *pcliaddr - Client socket address
 *clilen - Client socket address length.
*/
void recvAndProcessClientData(FILE *fp, int sockfd,struct sockaddr *pcliaddr, socklen_t clilen)
{
	ssize_t                 n;                      //number of bytes received
	struct hdr              sendhdr,recvhdr;        //send and receive headers
	struct iovec            iovsend[2], iovrecv[2]; //send and receive iov structures
	char                    recvline[MAXLINE + 1];  //Buffer to hold file data received
	time_t curr_ts;

	//Keep looping until end of file indication is received from sending client
	for ( ; ; ) {

		//populate msg recv structures
		msgrecv.msg_name = pcliaddr;
		msgrecv.msg_namelen = clilen;
		msgrecv.msg_iov = iovrecv;
		msgrecv.msg_iovlen = 2;
		iovrecv[0].iov_base = &recvhdr;
		iovrecv[0].iov_len = sizeof(struct hdr);
		iovrecv[1].iov_base = recvline;
		iovrecv[1].iov_len = MAXLINE;

		n = Recvmsg(sockfd, &msgrecv, 0);

		//terminate the recline buffer with null character
		recvline[n-sizeof(struct hdr)] = 0;

#ifdef DEBUGTRACE
		//prints data received in packet and also the ip address received from
		printf("\ncharacters=%ld Mesg=%s",n,recvline);
		void                    *print_addr;
		char                    ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in *s4 = (struct sockaddr_in *)pcliaddr;
		print_addr = &s4->sin_addr;
		Inet_ntop(pcliaddr->sa_family, print_addr, ipstr, sizeof(ipstr));
		printf("Pkt received from:%s \n",ipstr);
		printf("Recieved Opcode:%d \n",recvhdr.opcode);
#endif

		switch(recvhdr.opcode)
		{
			case WRITEREQ:
			{
#ifdef DEBUGTRACE
				printf("Server received write req when expecting data\n");
#endif
				//we shouldn't be coming here as the server parent process handles the write req
				bail("Unexpected opcode: exit");
				break;
			}
			case DATA:
			{
#ifdef DEBUGTRACE
				printf("Put data in file\n");
#endif
				//File data received. Write to file
				Fputs(recvline,fp);
				fflush (fp);
#ifdef DEBUGTRACE
				printf("Flushed data in file\n");
#endif
				break;
			}
			case END:
			{
				//end of file data indication
				Fclose(fp); // close the file
				break;
			}
			default:
			{
				//terminate the server child process as unexpected opcode received
				bail("Unexpected opcode: exit");
				break;
			}
		}

#ifdef DEBUGTRACE
		printf("Out of switch statement\n");
#endif

		memset(&msgsend,0,sizeof(msgsend));

		//populate msg send structures to send ack
		msgsend.msg_name = pcliaddr;
		msgsend.msg_namelen = clilen;
		msgsend.msg_iov = iovsend;
		msgsend.msg_iovlen = 1;
		sendhdr.opcode = ACK;
		sendhdr.seq = recvhdr.seq; //echo sequence number received
		sendhdr.ts = recvhdr.ts;   //echo time stamp received
		iovsend[0].iov_base = &sendhdr;
		iovsend[0].iov_len = sizeof(struct hdr);

#ifdef DEBUGTRACE
		printf("Sending Message \n");
#endif
		Sendmsg(sockfd, &msgsend, 0); //send ack

		//if end of file indication received from client return to calling function
		//file transfer is complete
		if(recvhdr.opcode == END)
		{
			return;
		}

	}
}

/*This function receives a file transfer request and spawns a child to handle that request.
 * sockfd - socket on which the server is listening for client connections
 * pcliaddr - this structure is populated with the sending client's address
 * clilen - length of the pcliaddr structure.
 */
void recvClientRequest (int sockfd, struct sockaddr *pcliaddr, socklen_t clilen)
{
	struct hdr           sendhdr,recvhdr;         //send and receive headers
	struct iovec         iovrecv[2],iovsend[2];   //send and receive iov structures
	char                 recvline[MAXLINE + 1];   //Buffer to hold filename from the incoming file transfer request
	ssize_t              n;                       //number of received bytes from client
	pid_t                childpid;                //holds server child pid
	FILE                *fp = NULL;               //Pointer to the file opened for writing.


	//Keep looping in server parent process to receive File transfer requests from client.
	for ( ; ; ) {
			//populate msg recv structures
			msgrecv.msg_name = pcliaddr;
			msgrecv.msg_namelen = clilen;
			msgrecv.msg_iov = iovrecv;
			msgrecv.msg_iovlen = 2;
			iovrecv[0].iov_base = &recvhdr;
			iovrecv[0].iov_len = sizeof(struct hdr);
			iovrecv[1].iov_base = recvline;
			iovrecv[1].iov_len = MAXLINE;

			n = Recvmsg(sockfd, &msgrecv, 0);
			//null terminate the recvline buffer which holds the filename
			recvline[n-sizeof(struct hdr)] = 0;

#ifdef DEBUGTRACE
			//prints data received in packet and also the ip address received from
			printf("\ncharacters=%ld Mesg=%s",n,recvline);
			void                    *print_addr;
			char                    ipstr[INET_ADDRSTRLEN];
			struct sockaddr_in *s4 = (struct sockaddr_in *)pcliaddr;
			print_addr = &s4->sin_addr;
			Inet_ntop(pcliaddr->sa_family, print_addr, ipstr, sizeof(ipstr));
			printf("Pkt received from:%s \n",ipstr);
#endif
			//received File transfer request
			//Client wants to write a file to the server
			if(recvhdr.opcode == WRITEREQ)
			{
				//increase the number of clients currently transferring data to server
				numClients++;
				childpid = fork();

				if(childpid == 0)
				{ //child begin

					//close sever listening socket as child doesn't use it
					close(sockfd);

					//create new server socket
					int childSocket;
					childSocket = Socket(AF_INET, SOCK_DGRAM, 0);

					//bind the interface to a new port
					bindInterface(childSocket, NEW_PORT+numClients);

					//open file to be written
					fp = Fopen(recvline, "w");

					//send acknowledgement
					msgsend.msg_name = pcliaddr;
					msgsend.msg_namelen = clilen;
					msgsend.msg_iov = iovsend;
					msgsend.msg_iovlen = 1;
					sendhdr.opcode = ACK;
					sendhdr.seq = recvhdr.seq;
					sendhdr.ts = recvhdr.ts;
					iovsend[0].iov_base = &sendhdr;
					iovsend[0].iov_len = sizeof(struct hdr);

					Sendmsg(childSocket, &msgsend, 0);

					//Start receiving file data on new socket
					recvAndProcessClientData(fp, childSocket, (struct sockaddr *) &pcliaddr, sizeof(pcliaddr));

					//File transfer complete. Close child socket
					close(childSocket);

					//decrease the number of clients currently the server is talking to as file transfer is complete
					numClients--;

					//exit child after file transfer is complete
					exit(0);
				}

			}
			else
			{
#ifdef DEBUGTRACE
				printf("Dropping this packet as Parent process handles only write requests");
#endif
				//ignore this request
			}

	}


}


//Server main function
//Doesn't need any command line arguments
int main(int argc, char **argv)
{
	int                     sockfd;
	struct sockaddr_in      cliaddr;

	//create a socket on which the server will listen for client requests
	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	//bind the socket to the server AF_INET interface and
	//well known port on which the server will listen for connections
	bindInterface(sockfd, SERV_PORT);

	//process incoming requests and data from clients.
	recvClientRequest(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr));

	close(sockfd);

	return 0;
}
