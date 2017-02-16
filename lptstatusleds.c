#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/lp.h>
#include <stdio.h>
#include <fcntl.h>              /* open() */
#include <sys/types.h>          /* open() */
#include <sys/stat.h>           /* open() */
#include <asm/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#define DEVICE "/dev/parport0"

#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#define PACKETSIZE	64
struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE-sizeof(struct icmphdr)];
};


void sig_term_handler(int signum, siginfo_t *info, void *ptr);

int pid=-1;
struct protoent *proto=NULL;




/*--------------------------------------------------------------------*/
/*--- checksum - standard 1s complement checksum                   ---*/
/*--------------------------------------------------------------------*/
unsigned short checksum(void *b, int len)
{	unsigned short *buf = b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

/*--------------------------------------------------------------------*/
/*--- ping - Create message send and wait for answer               ---*/
/*--------------------------------------------------------------------*/
int ping(struct sockaddr_in *addr)
{	const int val=255;
	int i, sd, cnt=1;
	struct packet pckt;
	struct sockaddr_in r_addr;

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if ( sd < 0 )
	{
		perror("socket");
		return -1;
	}
	if ( setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
		perror("Set TTL option");
	if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
		perror("Request nonblocking I/O");
	for (;;)
	{	int len=sizeof(r_addr);
		//printf("Msg #%d\n", cnt);
		bzero(&pckt, sizeof(pckt));
		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = pid;
		for ( i = 0; i < sizeof(pckt.msg)-1; i++ )
			pckt.msg[i] = i+'0';
		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
		if ( sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0 )
			return -1;
		for (int timeout = 0; timeout < 20; timeout++)
		{
        		if ( recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&r_addr, &len) > 0 ) return 1;
			usleep (100000);
                }
                return 0;
	}
}


double getload(void);
int getnetbytes(void);
int getnetbytessec(void);



#define BUFSIZE 8192
char gateway[255];

struct route_info {
    struct in_addr dstAddr;
    struct in_addr srcAddr;
    struct in_addr gateWay;
    char ifName[IF_NAMESIZE];
};

int readNlSock(int sockFd, char *bufPtr, int seqNum, int pId)
{
    struct nlmsghdr *nlHdr;
    int readLen = 0, msgLen = 0;

 do {
    /* Recieve response from the kernel */
        if ((readLen = recv(sockFd, bufPtr, BUFSIZE - msgLen, 0)) < 0) {
            perror("SOCK READ: ");
            return -1;
        }

        nlHdr = (struct nlmsghdr *) bufPtr;

    /* Check if the header is valid */
        if ((NLMSG_OK(nlHdr, readLen) == 0)
            || (nlHdr->nlmsg_type == NLMSG_ERROR)) {
            perror("Error in recieved packet");
            return -1;
        }

    /* Check if the its the last message */
        if (nlHdr->nlmsg_type == NLMSG_DONE) {
            break;
        } else {
    /* Else move the pointer to buffer appropriately */
            bufPtr += readLen;
            msgLen += readLen;
        }

    /* Check if its a multi part message */
        if ((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0) {
           /* return if its not */
            break;
        }
    } while ((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));
    return msgLen;
}
/* For printing the routes. */
void printRoute(struct route_info *rtInfo)
{
    char tempBuf[512];

/* Print Destination address */
    if (rtInfo->dstAddr.s_addr != 0)
        strcpy(tempBuf,  inet_ntoa(rtInfo->dstAddr));
    else
        sprintf(tempBuf, "*.*.*.*\t");
  //  fprintf(stdout, "%s\t", tempBuf);

/* Print Gateway address */
    if (rtInfo->gateWay.s_addr != 0)
        strcpy(tempBuf, (char *) inet_ntoa(rtInfo->gateWay));
    else
        sprintf(tempBuf, "*.*.*.*\t");
    //fprintf(stdout, "%s\t", tempBuf);

    /* Print Interface Name*/
    //fprintf(stdout, "%s\t", rtInfo->ifName);

    /* Print Source address */
    if (rtInfo->srcAddr.s_addr != 0)
        strcpy(tempBuf, inet_ntoa(rtInfo->srcAddr));
    else
        sprintf(tempBuf, "*.*.*.*\t");
    //fprintf(stdout, "%s\n", tempBuf);
}

void printGateway()
{
    printf("%s\n", gateway);
}

/* For parsing the route info returned */
void parseRoutes(struct nlmsghdr *nlHdr, struct route_info *rtInfo)
{
    struct rtmsg *rtMsg;
    struct rtattr *rtAttr;
    int rtLen;

    rtMsg = (struct rtmsg *) NLMSG_DATA(nlHdr);

/* If the route is not for AF_INET or does not belong to main routing table
then return. */
    if ((rtMsg->rtm_family != AF_INET) || (rtMsg->rtm_table != RT_TABLE_MAIN))
        return;

/* get the rtattr field */
    rtAttr = (struct rtattr *) RTM_RTA(rtMsg);
    rtLen = RTM_PAYLOAD(nlHdr);
    for (; RTA_OK(rtAttr, rtLen); rtAttr = RTA_NEXT(rtAttr, rtLen)) {
        switch (rtAttr->rta_type) {
        case RTA_OIF:
            if_indextoname(*(int *) RTA_DATA(rtAttr), rtInfo->ifName);
            break;
        case RTA_GATEWAY:
            rtInfo->gateWay.s_addr= *(u_int *) RTA_DATA(rtAttr);
            break;
        case RTA_PREFSRC:
            rtInfo->srcAddr.s_addr= *(u_int *) RTA_DATA(rtAttr);
            break;
        case RTA_DST:
            rtInfo->dstAddr .s_addr= *(u_int *) RTA_DATA(rtAttr);
            break;
        }
    }
    //printf("%s\n", inet_ntoa(rtInfo->dstAddr));

    if (rtInfo->dstAddr.s_addr == 0)
        sprintf(gateway, "%s", (char *) inet_ntoa(rtInfo->gateWay));
    //printRoute(rtInfo);

    return;
}


char *getgateway()
{
    struct nlmsghdr *nlMsg;
    struct rtmsg *rtMsg;
    struct route_info *rtInfo;
    char msgBuf[BUFSIZE];

    int sock, len, msgSeq = 0;

/* Create Socket */
    if ((sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0)
        perror("Socket Creation: ");

    memset(msgBuf, 0, BUFSIZE);

/* point the header and the msg structure pointers into the buffer */
    nlMsg = (struct nlmsghdr *) msgBuf;
    rtMsg = (struct rtmsg *) NLMSG_DATA(nlMsg);

/* Fill in the nlmsg header*/
    nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));  // Length of message.
    nlMsg->nlmsg_type = RTM_GETROUTE;   // Get the routes from kernel routing table .

    nlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;    // The message is a request for dump.
    nlMsg->nlmsg_seq = msgSeq++;    // Sequence of the message packet.
    nlMsg->nlmsg_pid = getpid();    // PID of process sending the request.

/* Send the request */
    if (send(sock, nlMsg, nlMsg->nlmsg_len, 0) < 0) {
      //  printf("Write To Socket Failed...\n");
        return "";
    }

/* Read the response */
    if ((len = readNlSock(sock, msgBuf, msgSeq, getpid())) < 0) {
     //   printf("Read From Socket Failed...\n");
    return "";
    }
/* Parse and print the response */
    rtInfo = (struct route_info *) malloc(sizeof(struct route_info));
//fprintf(stdout, "Destination\tGateway\tInterface\tSource\n");
    for (; NLMSG_OK(nlMsg, len); nlMsg = NLMSG_NEXT(nlMsg, len)) {
        memset(rtInfo, 0, sizeof(struct route_info));
        parseRoutes(nlMsg, rtInfo);
    }
    free(rtInfo);
    close(sock);

//    printGateway();
    return gateway;
}


int checkhost(char *hostname, int port, int seconds)
{
    struct sockaddr_in addr_s;
    char *addr;
    short int fd=-1;
    fd_set fdset;
    struct timeval tv;
    int rc;
    int so_error;
    socklen_t len;
    struct timespec tstart={0,0}, tend={0,0};

    int i;
    struct hostent *he;
    struct in_addr **addr_list;

    if ((he = gethostbyname(hostname)) == NULL) {  // get the host info
        herror("gethostbyname");
        return -1;
    }

    // print information about this host:
   // printf("Official name is: %s\n", he->h_name);
   // printf("    IP addresses: ");
    addr_list = (struct in_addr **)he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++) {
//        printf("%s ", inet_ntoa(*addr_list[i]));
    }
    printf("\n");
    
    
    addr = inet_ntoa(*addr_list[0]);

    //printf("addr=%s\n", addr);
    addr_s.sin_family = AF_INET; // utilizzo IPv4
    addr_s.sin_addr.s_addr = inet_addr(addr);
    addr_s.sin_port = htons(port);

    clock_gettime(CLOCK_MONOTONIC, &tstart);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK); // setup non blocking socket

    // make the connection
    rc = connect(fd, (struct sockaddr *)&addr_s, sizeof(addr_s));
    if ((rc == -1) && (errno != EINPROGRESS)) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        close(fd);
        return -2;
    }
    if (rc == 0) {
        // connection has succeeded immediately
        clock_gettime(CLOCK_MONOTONIC, &tend);
        //printf("socket %s:%d connected. It took %.5f seconds\n",
        //    addr, port, (((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec)));

        close(fd);
        return 1;
    } /*else {
        // connection attempt is in progress
    } */

    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    rc = select(fd + 1, NULL, &fdset, NULL, &tv);
    switch(rc) {
    case 1: // data to read
        len = sizeof(so_error);

        getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
            clock_gettime(CLOCK_MONOTONIC, &tend);
    //        printf("socket %s:%d connected. It took %.5f seconds\n",
//                addr, port, (((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec)));
            close(fd);
            return 1;
        } else { // error
//            printf("socket %s:%d NOT connected: %s\n", addr, port, strerror(so_error));
        }
        break;
    case 0: //timeout
  //      fprintf(stderr, "connection timeout trying to connect to %s:%d\n", addr, port);
        break;
    }

    close(fd);
    return 0;
}

int pinghost(char *hostname)
{
        struct hostent *hname;
        struct sockaddr_in addr;


        pid = getpid();
        proto = getprotobyname("ICMP");
                hname = gethostbyname(hostname);
                bzero(&addr, sizeof(addr));
                addr.sin_family = hname->h_addrtype;
                addr.sin_port = 0;
                addr.sin_addr.s_addr = *(long*)hname->h_addr;
                int result = ping(&addr);
                if (result) printf ("Connection to host %s Ok\n", hostname);
                else printf ("Connection to host %s Failed\n", hostname);
                return result;
}


typedef struct threadArgs * ThreadArgs;

void *foo( void * argStruct );

struct threadArgs{
    int *pingGateway; // Now a pointer, not an int
    int *pingInternet; // Now a pointer, not an int
};


void *checkThread( void * argStruct ){

    ThreadArgs args = argStruct;
    
    int pingGateway = 0;
    int pingInternet = 0;

    pingGateway = pinghost (getgateway());
    pingInternet = pinghost ("www.google.com");

    *args->pingGateway = pingGateway;
    *args->pingInternet = pingInternet;
    return NULL;
}

int getnetbytessec()
{
    int bytessend, bytesrecv, bytessec;
    static long oldbytes = 0;
    static long oldtimestamp = 0;
    int connected = 0;
    FILE *fp;
    char *dump = NULL;
    size_t len = 0;
    char ifacename[50];

    // Check if ethernet device is connected
    fp = fopen("/sys/class/net/enp1s0/carrier","r");
    fscanf(fp,"%d",&connected);
    fclose(fp);
    if (connected == 0)
    {
        //printf ("Network is disconnected\n");
        return -1;
    }

    fp = fopen("/proc/net/dev","r");
    getline(&dump, &len, fp);
    
    getline(&dump, &len, fp);
    fscanf(fp,"%s %d %*d %*d %*d %*d %*d %*d %*d %d",ifacename, &bytesrecv,&bytessend);
    
    getline(&dump, &len, fp);
    
    if (strcmp (ifacename, "lo:") == 0) fscanf(fp,"%s %d %*d %*d %*d %*d %*d %*d %*d %d",ifacename, &bytesrecv,&bytessend);
    free(dump);
    fclose(fp);

    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
    long timestamp  = (1000 * tp.tv_sec) + (tp.tv_nsec/1000000);


    bytessec = ((bytesrecv + bytessend - oldbytes)*1000) / ((timestamp - oldtimestamp));
//    printf("The current network troughput on %s is : %d bytes/sec (measured in %ld ms)\n",ifacename, bytessec, (timestamp - oldtimestamp));
    oldbytes = bytesrecv + bytessend;
    oldtimestamp = timestamp;
    if (strcmp (ifacename, "lo") == 0) return -1;
    return(bytessec);
}

double getload(void)
{
    long double a[4], b[4];
    int cpuusage;
    FILE *fp;

        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
        fclose(fp);
        usleep(200000);

        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
        fclose(fp);

        cpuusage = (((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])))*100;
  //      printf("The current CPU utilization is : %d%%\n",cpuusage);

    return(cpuusage);
}
int fd;
int main(int argc, char **argv)
{
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
    sigaction(SIGINT, &_sigact, NULL);
    sigaction(15, &_sigact, NULL);
    
    pthread_t t1;
    int pingGateway = 0;
    int pingInternet = 0;
    int ch;
    ThreadArgs args = (ThreadArgs)malloc(sizeof(struct threadArgs));
    args->pingInternet = &pingInternet; // pass in a pointer to the variable, rather than the variable itself
    args->pingGateway = &pingGateway; // pass in a pointer to the variable, rather than the variable itself


        struct ppdev_frob_struct frob;
       // int fd;
        int mode;

        if((fd=open(DEVICE, O_RDWR)) < 0) {
                fprintf(stderr, "can not open %s\n", DEVICE);
                exit(1);
        }
        if(ioctl(fd, PPCLAIM)) {
                perror("PPCLAIM");
                close(fd);
                exit(1);
        }
        int lptdata = 0;
        int netbytes = 0;
        int slowblinkloadcounter = 0;
        int pinghostcounter = 50;
        int prevnet = -1;
        while (1)
        {
                double load = getload();
                int net = getnetbytessec();
                
                if (net != prevnet)
                {
                    if ((prevnet < 0) && (net >= 0))
                    {
                        printf ("Ethernet connection is up\n");
                        pinghostcounter = 5;
                    }
                    if ((net < 0) && (prevnet >= 0)) printf ("Ethernet connection is down\n");
                    prevnet = net; 
                }
                
                if (pinghostcounter-- < 0)
                {
                        pthread_create(&t1, NULL, checkThread, args);
                        pinghostcounter = 25;
                }

                if (net > 0) // When traffic is measured blink 1st led
                {
                    lptdata &= 0b11101111; // When little traffic is generated blink 1st led
                }
                
                if (net > 1000000) lptdata &= 0b11011111; // When more than 1mbit traffic is generated blink 2nd led
                if (net > 100000000) lptdata &= 0b10111111; // Above 100 mbit blink 3rd led
                ioctl(fd, PPWDATA,&lptdata); // Write leds to lpt port

                usleep (200000);

                if (slowblinkloadcounter++ > 6) slowblinkloadcounter = 0;
                
                if (load >= 50) lptdata |= 0b1000; // If load is above 50% enable alarm led
                
                if (slowblinkloadcounter > 3)
                {
                    if (load < 90) lptdata &= 0b11110111; // If load is below 90% blink alarm led
                }

                if (net >= 0) lptdata |= 0b10000; // When connection is ok show 1st led
                else
                {
                    lptdata &= 0b11101111;
                    pingGateway = 0;
                    pingInternet = 0;
                }
                    

                if (pingGateway) lptdata |= 0b100000; // If gateway is reachable show 2nd led
                else lptdata &= 0b11011111;
                
                if (pingInternet)  lptdata |= 0b1000000; // If internet is reachable (google) show 3rd led
                else lptdata &= 0b10111111;
                ioctl(fd, PPWDATA,&lptdata); // write leds to lpt port
        }

        pthread_join(t1, NULL);
        printf("whole program terminating\n");
        /* put example code here ... */
        ioctl(fd, PPRELEASE);
        close(fd);
        return 0;
}

void sig_term_handler(int signum, siginfo_t *info, void *ptr)
{
        int lptdata = 0;
        ioctl(fd, PPWDATA,&lptdata);
        close(fd);
        abort();
}