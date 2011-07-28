/*
 * utilities.h
 *
 *  Created on: Jul 20, 2011
 *      Author: ankigarg
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <setjmp.h>
#include <signal.h>

#define	MAXLINE		512  //Maximum size of data received in DG
#define	SERV_PORT	9877 //port on which server is listening
//#define DEBUGTRACE //uncomment to print debug traces

//operation codes exchanged between server and client
enum OPCODE
{
	WRITEREQ =0,
	DATA,
	ACK,
	END
};

struct msghdr        msgsend, msgrecv;

//function which prints error's
static void
bail(const char *on_what) {
	fputs(strerror(errno),stderr);
	fputs(": ",stderr);
	fputs(on_what,stderr);
	fputc('\n',stderr);
	exit(1);
}

//Creates a socket and checks to see if any error happens
int Socket(int family, int type, int protocol)
{
	int             n;

	if ( (n = socket(family, type, protocol)) < 0)
		bail("socket error");
	return(n);
}

//Reads datagram from the socket and returns the number of bytes read
ssize_t Recvmsg(int fd, struct msghdr *msg, int flags)
{
        ssize_t         n;

        if ( (n = recvmsg(fd, msg, flags)) < 0)
                bail("recvmsg error");
        return(n);
}


//Sends datagram to the socket
void Sendmsg(int fd, const struct msghdr *msg, int flags)
{
	unsigned int    i;
	ssize_t                 nbytes;

	nbytes = 0;     /* must first figure out what return value should be */
	// Sum the lengths of all buffers
	for (i = 0; i < msg->msg_iovlen; i++)
		nbytes += msg->msg_iov[i].iov_len;

	if (sendmsg(fd, msg, flags) != nbytes)
		bail("sendmsg error");
}

//Connects the socket to the server address
void Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
	if (connect(fd, sa, salen) < 0)
		bail("connect error");
}

//Write the nbytes from the buffer pointed by ptr
//to fd descriptor.
void Write(int fd, void *ptr, size_t nbytes)
{
	if (write(fd, ptr, nbytes) != nbytes)
		bail("write error");
}

//Read nbytes into the buffer pointed by ptr
//from fd descriptor.
ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t         n;

	if ( (n = read(fd, ptr, nbytes)) == -1)
		bail("read error");
	return(n);
}

//Read n characters from the File stream.
char * Fgets(char *ptr, int n, FILE *stream)
{
	char    *rptr;

	if ( (rptr = fgets(ptr, n, stream)) == NULL && ferror(stream))
		bail("fgets error");

	return (rptr);
}

//Put the character array pointed by ptr into File stream.
void Fputs(const char *ptr, FILE *stream)
{
	if (fputs(ptr, stream) == EOF)
		bail("fputs error");
}

//Open a file name in the mode specified by the caller.
FILE * Fopen(const char *filename, const char *mode)
{
        FILE    *fp;

        if ( (fp = fopen(filename, mode)) == NULL)
                bail("fopen error");

        return(fp);
}

//Close a file
void Fclose(FILE *fp)
{
        if (fclose(fp) != 0)
                bail("fclose error");
}

//Binds the socket to an ip address
void Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
	if (bind(fd, sa, salen) < 0)
		bail("bind error");
}

//Convert network address to human readable form
const char * Inet_ntop(int family, const void *addrptr, char *strptr, size_t len)
{
	const char      *ptr;

	if (strptr == NULL)             /* check for old code */
		bail("NULL 3rd argument to inet_ntop");
	if ( (ptr = inet_ntop(family, addrptr, strptr, len)) == NULL)
		bail("inet_ntop error");             /* sets errno */
	return(ptr);
}

//Convert human readable ip address to machine form
void Inet_pton(int family, const char *strptr, void *addrptr)
{
	int             n;

	if ( (n = inet_pton(family, strptr, addrptr)) < 0)
		bail("inet_pton error");      /* errno set */
	else if (n == 0)
	{
		printf("inet_pton error for %s", strptr);     /* errno not set */
		exit(1);
	}

	/* nothing to return */
}

//Bind the INET interface on the server to the socket
void bindInterface(int sfd, unsigned int port)
{
	//stores the interface on which the server is listening for connections.
	static struct sockaddr_in      servaddr;
	struct ifaddrs          *myaddrs, *ifa;
	void                    *print_addr;
	char                    ipstr[INET_ADDRSTRLEN];

	//get all interfaces on this machine
	if(getifaddrs(&myaddrs) != 0)
		bail("getifaddrs");


	//loop thru the interfaces to find an interface with IPv4 address to use
	for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		//can't use any interface which doesn't have an address
		if (ifa->ifa_addr == NULL)
			continue;
		//can't use any interface which is not up
		if (!(ifa->ifa_flags & IFF_UP))
			continue;
		//can't use any interfaces which are loopback
		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			// Create address from which we want to send/recv, and bind it
			bzero(&servaddr, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			servaddr.sin_port = htons(port);

			Bind(sfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

			//Print server listening only when the binding is done with ther server listening port
			if(port == SERV_PORT)
			{
				struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
				print_addr = &s4->sin_addr;
				Inet_ntop(ifa->ifa_addr->sa_family, print_addr, ipstr, sizeof(ipstr));
				printf("Server listening on -> %s:%d\n",ipstr,SERV_PORT);
			}
			break;

		}
	}

	freeifaddrs(myaddrs); // no longer needed
}

#endif /* UTILITIES_H_ */
