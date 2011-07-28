#include "utilities.h"

//maximum number of times a packet is re-trasmitted
#define MAXRETRANS  3

//header for DG
static struct hdr {
	uint32_t      opcode;         //operation code
	uint32_t      seq;    /* sequence # */
	uint32_t      ts;             /* timestamp when sent */
} sendhdr, recvhdr;

static void     sig_alrm(int signo);
static sigjmp_buf       jmpbuf;

/*dg_send_recv -
 * This function is responsible for sending and receiving datagrams
 * opcode - operation code can be WriteReq, DATA, ACK, END
 * fd - socket on which the requests to the destination are sent
 * outbuff - Buffer which holds the data to be sent
 * outbytes - length of data to be sent form the Buffer.
 * destaddr - destination address
 * destlen - destination address length
 * recvaddr - the address from which the response from the destination comes
 * recvaddrlen - recvaddr length
 */
ssize_t dg_send_recv(int opcode, int fd, void *outbuff, size_t outbytes,
		struct sockaddr *destaddr, socklen_t destlen,
		struct sockaddr *recvaddr, socklen_t recvaddrlen)
{
	ssize_t                 n; //number of bytes received from received
	struct iovec    iovsend[2], iovrecv[2];
	time_t curr_ts;  // current timestamp
	int retrans = 0; //retransmit counter

	//populate sending data structures
	sendhdr.seq++;
	sendhdr.opcode = opcode;
	msgsend.msg_name = destaddr;
	msgsend.msg_namelen = destlen;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = outbytes;

#ifdef DEBUGTRACE
	printf("Size of header=%ld Size of data=%ld\n",sizeof sendhdr, outbytes);
#endif

	//populating receving data strcutures
	msgrecv.msg_name = recvaddr;
	msgrecv.msg_namelen = recvaddrlen;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 1;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);

	//Alarm is handled here
	signal(SIGALRM, sig_alrm);

	sendagain: //label which is used to re-transmit datagram after timeout
	curr_ts = time(NULL); //current system time
	sendhdr.ts = curr_ts;
	Sendmsg(fd, &msgsend, 0);
	alarm(3); //set alarm to 3 seconds


	if (sigsetjmp(jmpbuf, 1) != 0) {
		//if MAXRETRANS number of retransmissions have happened then exit.
		if (++retrans >= MAXRETRANS) {
#ifdef DEBUGTRACE
			printf("\ndg_send_recv: no response from server, giving up");
#endif
			retrans = 0;    /* reinit in case we're called again */
			errno = ETIMEDOUT;
			return(-1);
		}
#ifdef DEBUGTRACE
		printf("\n dg_send_recv: timeout, retransmitting");
#endif
		//retransmit datagram as ack was not received in time
		goto sendagain;
	}

	//Keep receiving data grams until the ACK for the sent datagram is received.
	do {
		n = Recvmsg(fd, &msgrecv, 0);
#ifdef DEBUGTRACE
		printf("recv %4d\n", recvhdr.seq);
#endif
	} while (n < sizeof(struct hdr) || recvhdr.seq != sendhdr.seq);

	alarm(0);                       /* stop SIGALRM timer */

	return(n - sizeof(struct hdr)); /* return size of received data datagram */
}

/*
 * sig_alrm -
 * Function handles the alarm signal.
 */
static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

/*sendAndRecvData -
 * This function is responsible for sending and receiving data from the server
 * opcode - operation code can be WriteReq, DATA, ACK, END
 * fd - socket on which the requests to the destination are sent
 * outbuff - Buffer which holds the data to be sent
 * outbytes - length of data to be sent form the Buffer.
 * destaddr - destination address
 * destlen - destination address length
 * recvaddr - the address from which the response from the destination comes
 * recvaddrlen - recvaddr length
 */
ssize_t sendAndRecvData(int opcode, int fd, void *outbuff, size_t outbytes,
		struct sockaddr *destaddr, socklen_t destlen,
		struct sockaddr *recvaddr, socklen_t recvaddrlen)
{
	ssize_t n; //number of bytes of data received from server

	//send and receive data grams
	n = dg_send_recv(opcode, fd, outbuff, outbytes,
			destaddr, destlen, recvaddr, recvaddrlen);
	if (n < 0)
		bail("sendAndRecvData error");

	return(n);
}

/*readAndSendFileData -
 * Reads a file line by line and sends that data to the server
 * fp - File which needs to be read and sent to the server
 * sockfd - socket on which we are sending data to server
 * pservaddr - server address
 * servlen - server address length
 */

void readAndSendFileData(FILE *fp, int sockfd, struct sockaddr *pservaddr, socklen_t servlen)
{
	ssize_t n; //number of bytes of data received from server
	char    sendline[MAXLINE]; //Buffer to hold data read from the file

	//Keep reading the file until end of file
	while (Fgets(sendline, MAXLINE, fp) != NULL) {

#ifdef DEBUGTRACE
		printf("Size of sendline=%ld\n",strlen(sendline));
#endif
		//send data read from the file to ther server
		n = sendAndRecvData(DATA, sockfd, sendline, strlen(sendline),
				pservaddr, servlen, NULL, 0);
	}
}

/*sendFileOperationReq -
 * Sends the file operation request namely Write Req or End req to the server
 * opcode - operation code is set by the caller
 * fileName - filename on which the operation is requested
 * sockfd - socket on which the requests to the server are sent
 * pservaddr - server address
 * servlen - server address length
 * recvaddr - the address from which the response from server is received
 * recvaddrlen - recvaddr length
 */
void sendFileOperationReq(int opcode, char *fileName, int sockfd,
		struct sockaddr *pservaddr, socklen_t servlen,
		struct sockaddr *recvaddr, socklen_t recvaddrlen)
{
	ssize_t n; //number of bytes of data received from server

#ifdef DEBUGTRACE
	printf("Size of Filename=%ld\n",strlen(fileName));
#endif
	n = sendAndRecvData(opcode, sockfd, fileName, strlen(fileName),
			pservaddr, servlen, recvaddr, recvaddrlen);
}

/*
 * main Client function
 * This function is responsible to receiving user arguments server address, filename
 * and starts the file transfer process.
 */
int main(int argc, char **argv)
{
	int                     sockfd; //socket to send data
	//socket address structures for server address and address
	//from which response from server is received
	struct sockaddr_in      servaddr, recvaddr;
	//File pointer which will hold the handle to the file being transferred
	FILE *fp = NULL;

	//Check to see if we received the required arguments from user
	if (argc != 3)
	{
		printf("\nusage -> <ip>:<port> <data-file>");
		exit(1);
	}

	/* Addr on cmdline: */
	const char delimiters[] = ":";
	char *srvr_addr_port = strdup(argv[1]);
	char *srvr_addr = strtok(srvr_addr_port,delimiters);
	char *srvr_port = strtok(NULL,delimiters); //server address
	int srvrport = atoi(srvr_port); // server port
	char *fileName = argv[2];

#ifdef DEBUGTRACE
	//print data which the user entered
	printf("Address::%s\n",srvr_addr);
	printf("Port::%d\n",srvrport);
	printf("Filename::%s\n",fileName);
#endif

	//populate server address structure with ip address and port
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(srvrport);
	Inet_pton(AF_INET, srvr_addr, &servaddr.sin_addr);

	//create a socket to send requests to the server
	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	//Open file to be transferred to the server
	fp = Fopen(fileName, "r");

	//send file transfer request to the server with the filename
	sendFileOperationReq(WRITEREQ, fileName,sockfd,
			(struct sockaddr *) &servaddr, sizeof(servaddr),
			(struct sockaddr *) &recvaddr, sizeof(recvaddr));

	//Use the the address received in the previous sendFileOperationReq to
	//send following packets to the server
	//Send file data to the Server
	readAndSendFileData(fp, sockfd, (struct sockaddr *) &recvaddr, sizeof(recvaddr));

	//send file end request to the server
	sendFileOperationReq(END, fileName,sockfd, (struct sockaddr *) &recvaddr, sizeof(recvaddr),
			NULL, 0);

	//close the file
	Fclose(fp);
	//close socket conencted to the server
	close(sockfd);
	exit(0);
}
