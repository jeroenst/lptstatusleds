#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <pthread.h> 
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <ifaddrs.h>
#include <resolv.h>

#include <asm/ioctl.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>

#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>

#include <linux/lp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/parport.h>
#include <linux/ppdev.h>

#define DEVICE "/dev/parport0"
#define PACKETSIZE 9

struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE-sizeof(struct icmphdr)];
};

void sig_term_handler(int signum, siginfo_t *info, void *ptr);
double getload(void);
int getnetbytes(void);

int pid=-1;
struct protoent *proto=NULL;

int debug = 0;

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

    if (rtInfo->dstAddr.s_addr == 0)
        sprintf(gateway, "%s", (char *) inet_ntoa(rtInfo->gateWay));

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
        return "";
    }

    /* Read the response */
    if ((len = readNlSock(sock, msgBuf, msgSeq, getpid())) < 0) {
    return "";
    }
    
    /* Parse and print the response */
    rtInfo = (struct route_info *) malloc(sizeof(struct route_info));
    for (; NLMSG_OK(nlMsg, len); nlMsg = NLMSG_NEXT(nlMsg, len)) {
        memset(rtInfo, 0, sizeof(struct route_info));
        parseRoutes(nlMsg, rtInfo);
    }
    free(rtInfo);
    close(sock);

    return gateway;
}

int pingip(char *ipaddress)
{
        struct hostent *hname;
        struct sockaddr_in addr;
        proto = getprotobyname("ICMP");
                        
                bzero(&addr, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = 0;
                addr.sin_addr.s_addr = inet_addr(ipaddress);
                int result = ping(&addr);
                if (result) {if (debug) printf ("Ping to ip %s Ok\n", ipaddress);}
                else printf ("Ping to ip %s Failed\n", ipaddress);
                return result;
}

typedef struct pingthreadArgs * pingThreadArgs;

struct pingthreadArgs{
    int *pingGateway; // Now a pointer, not an int
    int *pingInternet; // Now a pointer, not an int
};

void *pingThread( void * argStruct ){
    if (debug) printf("PingThread Start...\n");
    pingThreadArgs args = argStruct;
    
    int pingGateway = 0;
    int pingInternet = 0;

    pingGateway = pingip (getgateway());
    pingInternet = pingip ("8.8.8.8");
    if (!pingInternet) pingInternet = pingip ("4.2.2.1");
    if (!pingInternet) pingInternet = pingip ("151.197.0.38");
    if (!pingInternet) pingInternet = pingip ("81.218.119.11");
    
    // If internet is online but gateway offline retry gateway, maybe it was online later
    if ((pingInternet) && (!pingGateway)) pingGateway = pingip (getgateway());
    
    *args->pingGateway = pingGateway;
    *args->pingInternet = pingInternet;
    if (debug) printf("PingThread End...\n");
    pthread_exit(0);
}

long long getnetbytessec()
{
    long long bytessend, bytesrecv, bytessec;
    static long long oldbytes = 0;
    static long long oldtimestamp = 0;
    int connected = 0;
    FILE *fp;
    char *dump = NULL;
    size_t len = 0;
    char ifacename[50];
    int ret;

    // Check if ethernet device is connected
    fp = fopen("/sys/class/net/enp1s0/carrier","r");
    ret = fscanf(fp,"%d",&connected);
    fclose(fp);
    if (connected == 0)
    {
        // Network is disconnected
        return -1;
    }

    fp = fopen("/proc/net/dev","r");
    ret = getline(&dump, &len, fp);
    ifacename[0]='\0';
    
    while ((!feof(fp)) && (strcmp (ifacename, "enp1s0:") != 0))
    {
         ret = getline(&dump, &len, fp);
         ret = fscanf(fp,"%s %lld %*d %*d %*d %*d %*d %*d %*d %lld",ifacename, &bytesrecv,&bytessend);
    }
    fclose(fp);
    free(dump);
    
    if (ret)
    {
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
        long timestamp  = (1000 * tp.tv_sec) + (tp.tv_nsec/1000000);


        bytessec = ((bytesrecv + bytessend - oldbytes)*1000) / ((timestamp - oldtimestamp));
        oldbytes = bytesrecv + bytessend;
        oldtimestamp = timestamp;
        if (strcmp (ifacename, "lo") == 0) return -1;
        return(bytessec);
    }
    else
    {
        printf ("Error while reading bytes of network interface\n");
        return 0;
    }
}

double getcpuload(void)
{
    long double a[4], b[4];
    int cpuusage;
    FILE *fp;
    int ret;

    fp = fopen("/proc/stat","r");
    ret = fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
    fclose(fp);
    usleep(200000);

    fp = fopen("/proc/stat","r");
    ret = fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
    fclose(fp);

    cpuusage = (((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])))*100;

    return(cpuusage);
}

int fd;
int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    static struct sigaction _sigact;

    int opt = 0;
    while ((opt = getopt(argc, argv, "dh")) != -1) 
    {
        switch(opt) 
        {
            case 'd':
            debug = 1;
            printf("\nDebug mode enabled\n");
            break;
            case 'h':
            printf("\n-d = debug\n-h = help\n\n");
            exit(0);
            break;
        }
    }    

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
    sigaction(SIGINT, &_sigact, NULL);
    sigaction(15, &_sigact, NULL);
    
    pthread_t t1 = 0;
    int pingGateway = 0;
    int pingInternet = 0;
    int ch;
    pingThreadArgs args = (pingThreadArgs)malloc(sizeof(struct pingthreadArgs));
    args->pingInternet = &pingInternet; // pass in a pointer to the variable, rather than the variable itself
    args->pingGateway = &pingGateway; // pass in a pointer to the variable, rather than the variable itself

    struct ppdev_frob_struct frob;
    int mode;

    if (debug) printf ("Starting LPTSTATUSLEDS....\n");

    if((fd=open(DEVICE, O_RDWR)) < 0) 
    {
           fprintf(stderr, "can not open %s\n", DEVICE);
           exit(1);
    }
    
    if(ioctl(fd, PPCLAIM)) 
    {
           perror("PPCLAIM");
           close(fd);
           exit(1);
    }
    if (debug) printf ("lpt port initialized....\n");
    
    int lptdata = 0;
    int netbytes = 0;
    int slowblinkloadcounter = 0;
    int pinghostcounter = 50;
    int prevnet = -1;
    while (1)
    {
                double cpuload = getcpuload();
                long long net = getnetbytessec();
                
                // When network connection changes from online to offline or visa versa display message
                if (net != prevnet)
                {
                    if ((prevnet < 0) && (net >= 0))
                    {
                        if (debug) printf ("Ethernet connection is up\n");
                        pinghostcounter = 5;
                    }
                    if ((net < 0) && (prevnet >= 0)) printf ("Ethernet connection is down\n");
                    prevnet = net; 
                }
                
                // Check hosts with ping to determine if default gateway is online and if internet is reachable
                if (pinghostcounter-- < 0)
                {
                        if (t1)
                        {
                            pthread_cancel(t1);
                            void *res;
                            pthread_join(t1, NULL);
                        }
                        if (debug) printf ("Current network troughput in bytes/sec: %lld\n", net); 
                        int ret = pthread_create(&t1, NULL, pingThread, args);
                        if (ret) printf("Creation of pingthread failed errno:%d\n",ret);
                        pinghostcounter = 50;
                }
                
                // If previous internet state was offline, do checks faster
                if ((pinghostcounter > 5) && (pingInternet == 0)) pinghostcounter = 5;

                if (net > 0) lptdata &= 0b11101111; // When traffic is measured blink 1st led
                if (net > 104857)   lptdata &= 0b11011111; // When more than 1mbit traffic is generated blink 2nd led
                if (net > 10485760) lptdata &= 0b10111111; // Above 100 mbit blink 3rd led
                ioctl(fd, PPWDATA,&lptdata); // Write leds to lpt port

                usleep (200000);

                
                if (cpuload >= 50) lptdata |= 0b1000; // If load is above 50% enable alarm led
                if (slowblinkloadcounter++ > 6) slowblinkloadcounter = 0;
                if ((slowblinkloadcounter > 3) && (cpuload < 90)) lptdata &= 0b11110111; // If load is below 90% blink alarm led

                if (net >= 0) lptdata |= 0b10000; // When connection is ok show 1st led
                else // When connection is not ok turn all network leds off
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

    pthread_cancel(t1);
    pthread_join(t1, NULL);
    printf("whole program terminating\n");
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

