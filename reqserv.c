#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


char* udp_message(int fd, char *message, struct sockaddr_in addr, char *buffer_rec)
{
	int n;
	socklen_t addrlen;

	n=sendto(fd,message,strlen(message) + 1,0,(struct sockaddr*)&addr,(socklen_t)sizeof(addr));
	if(n==-1)
	{
		fprintf(stderr, "Couldn't send UDP message: \n'%s'\n\n", message);
		printf("ERROR: %s\n", strerror(errno));	
		exit(1);//error
	}
	
	addrlen=sizeof(addr);
	
	n=recvfrom(fd,buffer_rec,128,0,(struct sockaddr*)&addr,&addrlen);
	if(n==-1)
	{
		fprintf(stderr, "Couldn't receive UDP message.\n");
		exit(1);//error
	}
	
	return buffer_rec;
}

int check_valid_server(char *buffer_rec)
{
	int  id,  aip1, aip2, aip3, aip4, upt;
	char ip2[32];
	sscanf(buffer_rec, "OK %d;%d.%d.%d.%d;%d", &id, &aip1, &aip2, &aip3, &aip4, &upt);
	sprintf(ip2, "%d.%d.%d.%d", aip1, aip2, aip3, aip4);

	if(id > 0) 
	{
		fprintf(stderr, "\n-----------------------------------------------");
		fprintf(stderr, "\nFound Valid Server, Server's ID is %d\n",id);
		fprintf(stderr, "-----------------------------------------------\n\n");
		return 1;
	}
	else if(id <= 0)
	{
		fprintf(stderr, "Didn't Receive Valid Server. Please try again later.\n\n" );
		return 0;
	} 

	return -1;

}

int client_udp_server(char *buffer_rec, char *message, struct sockaddr_in addr2, char *ip2, int check)
{
	int  id,  aip1, aip2, aip3, aip4, upt;
	char aux[128];
	struct hostent *h;
	struct in_addr conv_csip;
	int fd;

	if(check == 1)
	{
		sscanf(buffer_rec, "OK %d;%d.%d.%d.%d;%d", &id, &aip1, &aip2, &aip3, &aip4, &upt);
		sprintf(ip2, "%d.%d.%d.%d", aip1, aip2, aip3, aip4);
	}


	if((h=gethostbyname(ip2))==NULL)
		exit(1);//error
	conv_csip=*(struct in_addr*)h->h_addr_list[0];

	if(check == 1)		
	{
		addr2.sin_family=AF_INET;
		addr2.sin_addr= conv_csip;
		addr2.sin_port=htons(upt);		
	}

	
	fd=socket(AF_INET,SOCK_DGRAM,0);
	if(fd == -1) 
	{
		fprintf(stderr, "Error: Could not create Socket.\n");
		exit(0);
	}

	strcpy(aux,udp_message(fd, message, addr2, buffer_rec));
	close(fd);
	
	if(strcmp(aux, "YOUR_SERVICE ON") == 0)
		return 1;
	else if (strcmp(aux, "YOUR_SERVICE OFF") == 0)
		return 0;

	return -1;
}

void client_udp_central(struct sockaddr_in addr)
{
	int fd, s, flag = 0;
	char buffer_read[128], *buffer_rec, *message, *ip2, *aux;
	struct sockaddr_in addr2;

	memset((void*)&addr2,(int)'\0',sizeof(addr2));

	buffer_rec = malloc(sizeof(char)*128);
	message = malloc(sizeof(char)*128);
	ip2 = malloc(sizeof(char)*32);
	aux = malloc(sizeof(char)*128);

	fd=socket(AF_INET,SOCK_DGRAM,0);

	if(fd == -1) 
	{
		fprintf(stderr, "Error: Could not create Socket.\n");
		exit(0);
	}

	while(1)
	{
		if(fgets(buffer_read, 32 , stdin) != NULL)
		{			
			if(strstr(buffer_read, "request_service") != NULL )
			{
				sscanf(buffer_read, "request_service %d", &s);
				sprintf(message, "GET_DS_SERVER %d",s);
				buffer_rec = udp_message(fd, message, addr, buffer_rec);

				if(check_valid_server(buffer_rec) == 1)
				{
					strcpy(message, "MY_SERVICE ON\0");
					strcpy(aux , buffer_rec);
					flag = client_udp_server(buffer_rec, message, addr2, ip2, 1);		
					if( flag == 1)
					{
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nYour Service is On\n" );
						fprintf(stderr, "-----------------------------------------------\n\n");
					}
					else if(flag == -1)						
						fprintf(stderr, "\nWe're Sorry. Couldn't Start Service.\n" );	
					strcpy(buffer_rec , aux);
				}
					
			}
			else if( strstr(buffer_read, "rs") != NULL)
			{
				sscanf(buffer_read, "rs %d", &s);
				sprintf(message, "GET_DS_SERVER %d",s);
				buffer_rec = udp_message(fd, message, addr, buffer_rec);

				if(check_valid_server(buffer_rec) == 1)
				{
					strcpy(message, "MY_SERVICE ON");
					strcpy(aux , buffer_rec);
					flag = client_udp_server(buffer_rec, message, addr2, ip2, 1);	
					if( flag == 1)
					{
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nYour Service is On\n" );
						fprintf(stderr, "-----------------------------------------------\n\n");
					}
					else if(flag == -1)						
						fprintf(stderr, "\nWe're Sorry. Couldn't Start Service.\n" );	
					strcpy(buffer_rec , aux);
				}
					
			}
			else if(strstr(buffer_read, "terminate_service") != NULL )
			{	
				if(flag == 0) 
				{
					fprintf(stderr, "\nThere isn's a Service on going.\n");
				}
				else 
				{				
					strcpy(message, "MY_SERVICE OFF\0");
					flag = client_udp_server(buffer_rec, message, addr2, ip2, 0);					
					if( flag == 0)
					{
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nYour Service is Off\n" );
						fprintf(stderr, "-----------------------------------------------\n\n");
					}
					else if(flag == -1)						
						fprintf(stderr, "\nWe're Sorry. Couldn't End Service.\n" );	
				
				}
			}
			else if(strstr(buffer_read, "ts") != NULL)
			{	
				if(flag == 0) 
				{
					fprintf(stderr, "\nThere isn's a Service on going.\n");
				}
				else
				{
					strcpy(message, "MY_SERVICE OFF\0");
					flag = client_udp_server(buffer_rec, message, addr2, ip2, 1);					
					if( flag == 0)
					{
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nYour Service is Off\n" );
						fprintf(stderr, "-----------------------------------------------\n\n");
					}
					else if(flag == -1)						
						fprintf(stderr, "\nWe're Sorry. Couldn't End Service.\n" );	
					else 
						fprintf(stderr, "\nSomething weird is going on...\n");
				}
			}
			else if(strstr(buffer_read, "exit") != NULL) 
				break;		
			else 
				fprintf(stderr, "Instruction not recognized, please try again.\n");

			
		}	
	}

	free(buffer_rec);
	free(message);
	free(ip2);
	close(fd);

	return;
}


int main( int argc, char *argv[])
{
	int cspt, addrs, fip = 0, fpt = 0, i;
	char csip[128];
	struct hostent *h;
	struct in_addr conv_csip;
	struct sockaddr_in addr;

	

	if(argc > 1)
		for( i = 1; i < argc; i++) //ler inputs
		{
			if(strcmp(argv[i],"-i") == 0)
			{
				strcpy(csip, argv[i+1]);
				i++;
					
				addrs = inet_aton(csip, &conv_csip);
				if( addrs == 0 ) 
				{
					fprintf(stderr, "Error: Invalid Adress \n" );
					exit(0);
				}

				fip = 1;
			}
			else if(strcmp(argv[i],"-p") == 0)
			{
				cspt = atoi(argv[i+1]);
				i++;

				fpt = 1;
			}
		}
	if(fip == 0)
	{
		strcpy(csip, "tejo.tecnico.ulisboa.pt");
		if((h=gethostbyname(csip))==NULL)
		{
			fprintf(stderr, "Error: Invalid Adress \n" );
			exit(0);
		}
		conv_csip=*(struct in_addr*)h->h_addr_list[0];
	}
	if(fpt == 0)
		cspt = 59000;

	memset((void*)&addr,(int)'\0',sizeof(addr));
	
	addr.sin_family=AF_INET;
	addr.sin_addr= conv_csip;
	addr.sin_port=htons(cspt);
	
	client_udp_central(addr);
	
	return 1;
}