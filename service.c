#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define max(A,B) ((A)>=(B)?(A):(B))

/*Código para tcpflag*/
#define TOKEN_S 0
#define NEW 1
#define TOKEN_L 2
#define NEW_START 3

/*Código para udpflag*/
#define SDS 0
#define WDS 1
#define SS 2
#define WS 3
#define GS 4

/*Código para Estados*/
#define DISP 0
#define INDISP 1

/*Global Variable*/
int leave =-1;

void check_status(int ready, int ringstatus, int status, int id2, int company, int dsflag)
{
	fprintf(stderr, "\n--------------------STATUS---------------------\n");
	if(ready == 1)
	{
		if(ringstatus == DISP && company == 1)
		{
			if(status == DISP && dsflag == 1)
				fprintf(stderr, "Server is Available\nServer is Dispatch Server\nRing is Available\nServer Sucessor's ID is %d\n", id2);
			else if(status == DISP && dsflag == 0)
				fprintf(stderr, "Server is Available\nServer is not Dispatch Server\nRing is Available\nServer Sucessor's ID is %d\n", id2);
			else if (status == INDISP)				
				fprintf(stderr, "Server is Unavailable\nRing is Available\nServer Sucessor's ID is %d\n", id2);
			else
				fprintf(stderr, "Something wrong happened, could not check Server Availablity");
		} 
		else if(ringstatus == INDISP && company == 1)
		{
			if(status == INDISP)
				fprintf(stderr, "Server is Unavailable\nRing is Unavailable\nServer Sucessor's ID is %d\n", id2);
			else if (status == DISP)
				fprintf(stderr, "Weirdly, Server is Available, but Ring is Unavailable.\nServer Sucessor's ID is %d\n", id2);
			else
				fprintf(stderr, "Something wrong happened, could not check Server Availablity");
		}
		else if(company == 0)
		{
			if(status == DISP )
				fprintf(stderr, "Server is Available\nServer is Dispatch Server\nRing is Available.\nThere is only one Server in the ring.\n");
			else if (status == INDISP)				
				fprintf(stderr, "Server is Unavailable\nRing is Unavailable\nThere is only one Server in the ring.\n");
			else
				fprintf(stderr, "Something wrong happened, could not check Server Availablity\n");
		}
		else
			fprintf(stderr, "Something wrong happened, could not check Ring Availablity\n" );
	}
	else if( ready == 0)
	{
		fprintf(stderr, "Server didnt join any Service yet\n");
	}
	fprintf(stderr, "-----------------------------------------------\n\n");
	return;
}

char* udp_message(int fd, char *message, struct sockaddr_in addr, char *buffer_rec)
{
	int n;
	socklen_t addrlen;

	n=sendto(fd,message,strlen(message)+1,0,(struct sockaddr*)&addr,(socklen_t)sizeof(addr));
	if(n < 0)
	{
		fprintf(stderr, "Couldn't send UDP message:\n");
		leave = 1;
		return NULL;
	}
	
	addrlen=sizeof(addr);

	n=recvfrom(fd,buffer_rec,128,0,(struct sockaddr*)&addr,&addrlen);
	if(n < 0)
	{
		fprintf(stderr, "Couldn't receive UDP message.\n");
		leave = 1;
		return NULL;
	}
	
	return buffer_rec;
}

int send_tcp_message(int fd, char *message)
{
	if(write(fd,message,strlen(message))==-1)
	{
		fprintf(stderr, "Error sending TCP message\n");
		leave = 1;
		return -1;
	}
	return 0;
}

int server_udp_cliente(int fd, struct sockaddr_in addr)
{
	int ret, nread, status;
	socklen_t addrlen;
	char message[128], *buffer_rec;
	
	buffer_rec = malloc(sizeof(char)*128);
		
	addrlen= (socklen_t)sizeof(addr);
	nread=recvfrom(fd,buffer_rec,128,0,(struct sockaddr*)&addr,&addrlen);
	if(nread==-1)
	{
		fprintf(stderr, "Couldn't receive UDP message.\n");
		leave = 1;
		return -1;
	}
													
	if(strcmp(buffer_rec, "MY_SERVICE ON")== 0)
	{
		strcpy(message, "YOUR_SERVICE ON\0");

		ret=sendto(fd,message,strlen(message)+1,0,(struct sockaddr*)&addr,addrlen);
		if(ret==-1)
		{
			fprintf(stderr, "Couldn't send UDP message: \n'%s'\n\n", message);
			leave = 1;
			return -1;
		}
		fprintf(stderr, "\n-----------------------------------------------");
		fprintf(stderr, "\nProviding a Service\n" );
		fprintf(stderr, "-----------------------------------------------\n\n");
		status = INDISP;
	}

	else if(strcmp(buffer_rec, "MY_SERVICE OFF\0") == 0)
	{
		strcpy(message, "YOUR_SERVICE OFF\0");

		ret=sendto(fd,message,strlen(message)+1,0,(struct sockaddr*)&addr,addrlen);
		if(ret==-1)
			{
				fprintf(stderr, "Couldn't send UDP message: \n'%s'\n\n", message);
				leave = 1;
				return -1;
			}

		status = DISP;
		fprintf(stderr, "\n-----------------------------------------------");
		fprintf(stderr, "\nNo Longer Providing a Service\n" );
		fprintf(stderr, "-----------------------------------------------\n\n");
	}
	else 
		fprintf(stderr, "Error : Unrecognized message from server.\n");

	free(buffer_rec);

	return status;
}

int server_udp_central(int udpflag, int fdcentral, struct sockaddr_in addrcentral,
 int x, int id, int *id2, char *ip, char (*ip2)[32], int tpt, int *tpt2, int upt, char *buffer_rec, int *dsflag)
{
	int aip1, aip2, aip3, aip4;
	char message[128];

	if(udpflag == GS)
	{
		sprintf(message, "GET_START %i%c%i", x,59,id);
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -1;

		sscanf(buffer_rec, "OK %d;%d;%d.%d.%d.%d;%d", &id, id2, &aip1, &aip2, &aip3, &aip4, tpt2);
		sprintf((*ip2), "%d.%d.%d.%d", aip1, aip2, aip3, aip4);
		if(id == 0)
			fprintf(stderr, "Could not connect properly with central server.\n");
	}
	else if(udpflag == SS)
	{
		sprintf(message, "SET_START %i%c%i%c%s%c%i", x,59,id,59,ip,59,tpt);		
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -1;
		
		sscanf(buffer_rec, "OK %d;0;0.0.0.0;0", &id);
		if(id == 0)
			fprintf(stderr, "Could not set as start server.\n");
	}
	else if(udpflag == SDS)
	{
		sprintf(message, "SET_DS %i%c%i%c%s%c%i", x,59,id,59,ip,59,upt);			
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -1;

		sscanf(buffer_rec, "OK %d;0;0.0.0.0;0", &id);
		if(id == 0)
			fprintf(stderr, "Could not set as dispatch server.\n");

		*dsflag = 1;
	}
	else if(udpflag == WDS)
	{
		sprintf(message, "WITHDRAW_DS %i%c%i", x,59,id);
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -1;

		*dsflag = 0;
	}
	else if(udpflag == WS)
	{
		sprintf(message, "WITHDRAW_START %i%c%i", x,59,id);
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -1;
	}

	return 0;
}

int server_tcp_server(int fd, int m_id, char type, int id, int id2, char *ip, char *ip2, int tpt, int tpt2,
 int aux_id, char *aux_ip, int aux_tpt, int s_id)
{
	char message[128];

	if(m_id == TOKEN_S)
	{
		sprintf(message, "TOKEN %d%c%c\n", s_id,59,type);
		if((send_tcp_message(fd, message)) == -1) return -1;
	}
	else if(m_id == NEW)
	{
		sprintf(message, "NEW %d%c%s%c%d\n", id,59,ip,59,tpt);
		if((send_tcp_message(fd, message)) == -1) return -1;
	}
	else if(m_id == TOKEN_L)
	{
		sprintf(message, "TOKEN %d%c%c%c%d%c%s%c%d\n", s_id,59,type,59,aux_id,59,aux_ip,59,aux_tpt);
		if((send_tcp_message(fd, message)) == -1) return -1;
	}
	else if(m_id == NEW_START)
	{
		sprintf(message, "NEW_START\n");
		if((send_tcp_message(fd, message)) == -1) return -1;
	}
	else
	{
		fprintf(stderr, "Message ID non identified : %d\n", m_id);
		return 0;
	}
	return 1;
}

int check_tcp_message(char *buffer_read, int fdcentral, struct sockaddr_in addrcentral, 
	int x, char *type, int *id, int *id2, char **ip, char (*ip2)[32], int upt,
	int *tpt, int *tpt2, int *aux_id, char (*aux_ip)[32], int *aux_tpt, int *s_id, 
	int status, int *ringstatus, char *buffer_rec, int *dsflag)
{
	int aip1, aip2, aip3, aip4, aux;
	char message[128];

	if(sscanf(buffer_read, "TOKEN %d;%c;%d;%d.%d.%d.%d;%d\n", s_id, type, aux_id, &aip1, &aip2, &aip3, &aip4, aux_tpt) == 8)
	{
		sprintf((*aux_ip), "%d.%d.%d.%d", aip1, aip2, aip3, aip4);
		return TOKEN_L;	
	}
	else if(sscanf(buffer_read, "TOKEN %d;%c\n", s_id, type) == 2)
	{
		if((*type) == 'S')
		{
			if((*s_id) != (*id)) /*Recebeu TOKEN de outro*/
			{
				if(status == DISP) /*Está disponível*/
				{
					if((server_udp_central(SDS, fdcentral, addrcentral, x, (*id), id2, (*ip), ip2, (*tpt), tpt2, upt, buffer_rec, dsflag)) == -1) return-1;
					(*type) = 'T';
					return TOKEN_S;
				}
				else if(status == INDISP) /*Não está disponível*/
				{
					return TOKEN_S;
				}
			}
			else if((*s_id) == (*id)) /*Recebeu TOKEN de si próprio*/
			{
				if(status == INDISP) /*Ainda não está disponível*/
				{
					(*ringstatus) = INDISP;
					(*type) = 'I';
					return TOKEN_S;
				}
				else if(status == DISP) /*Já está disponível*/
				{
					if(leave == 0)
					{
						(*ringstatus) = INDISP;
						(*type) = 'I';
						return TOKEN_S;
					}
					else
					{
						if((server_udp_central(SDS, fdcentral, addrcentral, x, (*id), id2, (*ip), ip2, (*tpt), tpt2, upt, buffer_rec, dsflag)) == -1) return-1;
						return -1;						
					}
				}
			}
		}
		else if((*type) == 'T')
		{
			if((*s_id) != (*id)) /*Recebeu TOKEN de outro*/
				return TOKEN_S;
			
			else if((*s_id) == (*id)) /*Recebeu TOKEN de si próprio*/
			{
				if(leave == 0)
				{
					leave = 2;
				}
				return -1;
			}
		}
		else if((*type) == 'I')
		{
			if((*s_id) != (*id)) /*Recebeu TOKEN de outro*/
			{
				if(status == INDISP)
				{
					(*ringstatus) = INDISP;
					return TOKEN_S;
				}
				else if(status == DISP)
				{
					(*s_id) = (*id);
					(*type) = 'D';
					return TOKEN_S;
				}
			}
			else if((*s_id) == (*id)) /*Recebeu TOKEN de si próprio*/
			{
				if(leave == 0)
					leave = 2;

				(*ringstatus) = INDISP;
				return -1;
			}
		}
		else if((*type) == 'D')
		{
			if((*s_id) != (*id)) /*Recebeu TOKEN de outro*/
			{
				if(status == DISP && ((*id) < (*s_id)))
				{
					return -1;
				}

				else if((*ringstatus) == DISP)
				{
					return -1;
				}

				else if(status == INDISP)
				{
					(*ringstatus) = DISP;
					return TOKEN_S;
				}
			}
			else if((*s_id) == (*id)) /*Recebeu TOKEN de si próprio*/
			{
				if((server_udp_central(SDS, fdcentral, addrcentral, x, (*id), id2, (*ip), ip2, (*tpt), tpt2, upt, buffer_rec, dsflag)) == -1) return-1;
				return -1;
			}
		}
		else
			fprintf(stderr, "Could not recognize TOKEN %c\n", (*type));

		return TOKEN_S;
	}
	else if(sscanf(buffer_read, "NEW %d;%d.%d.%d.%d;%d\n", aux_id, &aip1, &aip2, &aip3, &aip4, aux_tpt) == 6 )
	{
		sprintf((*aux_ip), "%d.%d.%d.%d", aip1, aip2, aip3, aip4);
		(*s_id) = (*id);
		(*type)= 'N'; 
		return TOKEN_L;
	}
	else if (strstr(buffer_read, "NEW_START") != NULL)
	{
		sprintf(message, "SET_START %i%c%i%c%s%c%i", x,59,(*id),59,(*ip),59,(*tpt));
		if((buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec)) == NULL)
			return -2;

		sscanf(buffer_rec, "OK %d;0;0.0.0.0;0", &aux);
		if(aux == 0)
			fprintf(stderr, "Could not set as sucessor start server.\n");
	}
	else if (strlen(buffer_read)!= 0)
		fprintf(stderr, "Receive non identified Message From previous server : %s \n", buffer_read);

	return -1;
}

void connections(struct sockaddr_in addrcentral, int id, char *ip, int tpt, int upt, char *buffer_rec)
{
	int id2 = 0, tpt2 = 0, aux_id = -1, aux_tpt = 0, s_id = 0, a, exit = 0;
	int fdclient = -1, fdcentral = -1, fda = -1, fdw = -1, fds = -1, maxfd = 0, newfd = -1;
	int counter, status = DISP, ringstatus = DISP, x= 36,ready = 0;
	int tcpflag = -1 , conflag =-1, dsflag = 0, company = 0;
	char buffer_read[128], ip2[32], aux_ip[32], type, message[128], tbuffer[128];
	struct sockaddr_in addrclient, addrsuc, addrpre;
	fd_set rfds;
	struct in_addr ipsuc;
	struct hostent *h;
	void (*old_handler)(int); //interrupt handler
	socklen_t addrlen;
	
	/* Create Socket for Central Server UDP communication*/
	fdcentral=socket(AF_INET,SOCK_DGRAM,0);
	if(fdcentral == -1) 
	{
		fprintf(stderr, "Error: Could not create Socket.\n");
		leave = 1;
	}

	while(1)
	{
		maxfd = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		memset(buffer_read,0,128);
		memset(tbuffer,0,128);

		/* CHECK IF ANY FD HAS BEEN ACTIVATED */
		if(fdclient != -1)
		{
			FD_SET(fdclient, &rfds);
			maxfd = max(maxfd, fdclient);
		}
		if(fdw != -1)
		{
			FD_SET(fdw, &rfds);
			maxfd = max(maxfd, fdw);
		}
		if(fda != -1)
		{
			FD_SET(fda, &rfds);
			maxfd = max(maxfd, fda);
		}


	
		counter = select(maxfd+1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)	NULL);
		if(counter <= 0)
		{
			fprintf(stderr, "Error counting fd's\n");
			leave = 1;
		}
		/*CHECK IF ANY FD HAS INFO ON IT */
		if(FD_ISSET(0, &rfds)) //terminal input
		{
			if(read(0, tbuffer , 128) < 0)
			{
				write(2, "An error occurred in the read.\n", 31);		
			}
			if(strstr(tbuffer, "join") != NULL)
			{
				if(status == DISP && ready == 0) // If it didn't join any Service
				{	
					sscanf(tbuffer, "join %d", &x);

					server_udp_central(GS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);

					if(id2 == 0) // Register as Start and Dispatch Server
					{
						server_udp_central(SS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);
						
						server_udp_central(SDS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);

						ready = 1;
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nSucessfully Joined an Empty Ring\n");
						fprintf(stderr, "-----------------------------------------------\n\n");
					}
					else // Receive info on Start Server an create TCP Socket to join ring 
					{
						/*TCP Socket for SUCESSOR*/
						if(fds == -1 && (fds=socket(AF_INET,SOCK_STREAM,0))==-1)
						{
							printf("ERROR: %s\nCREATING SOCKET FDS\n", strerror(errno));		
							leave = 1;
						}
						fprintf(stderr, "\n-----------------------------------------------");
						fprintf(stderr, "\nSucessfully Joined a Ring\n");
						fprintf(stderr, "-----------------------------------------------\n\n");
						ready = 1; conflag = 1; tcpflag = NEW; company = 1;
					}
					if(ready == 1) // Create and Bind Socket for Client UDP and Antecessor TCP communication when service is available
					{
						/*UDP Socket*/
						if(fdclient == -1)
						{
							memset((void*)&addrclient,(int)'\0',sizeof(addrclient));
							addrclient.sin_family=AF_INET;
							addrclient.sin_addr.s_addr=htonl(INADDR_ANY);
							addrclient.sin_port=htons(upt);

							if((fdclient=socket(AF_INET,SOCK_DGRAM,0)) == -1) 
							{
								fprintf(stderr, "Error: Could not create UDP Socket (client).\n");
								leave = 1;
							}
							if(bind(fdclient,(struct sockaddr*)&addrclient,sizeof(addrclient))==-1)
							{
								printf("Error: %s\nBinding CLient's Socket\n", strerror(errno));		
								leave = 1;
							}
						}
						/*TCP Socket for waiting predecessor*/
						if(fdw == -1)
						{
							memset((void*)&addrpre,(int)'\0',sizeof(addrpre));
							addrpre.sin_family=AF_INET;
							addrpre.sin_addr.s_addr=htonl(INADDR_ANY);
							addrpre.sin_port=htons(tpt);

							if((fdw=socket(AF_INET,SOCK_STREAM,0))==-1)
							{
								fprintf(stderr, "Error: Could not create TCP Socket (predecessor).\n");
								leave = 1;
							}
							if(bind(fdw, (struct sockaddr *)&addrpre,sizeof(addrpre)) == -1)
							{
							    printf("Error binding socket FDA\n");
							   	leave = 1;
							}

							if(listen(fdw,5)==-1)
							{
								printf("Error in listen: %s\n", strerror(errno));
								leave = 1;
							}
						}
					}		
				}
				else if(status == INDISP)
					fprintf(stderr,"\nYou already joined a service.\n");
			}
			else if(strstr(tbuffer, "show_state") != NULL)
			{
				check_status(ready, ringstatus, status, id2, company, dsflag);
			}
			else if(strstr(tbuffer, "leave") != NULL)
				leave = 1;
			
			else if(strstr(tbuffer, "exit") != NULL) 
			{
				if(status == INDISP)
				{
					fprintf(stderr, "Can't exit while providing a service!\n");
					leave = -1; // CANCELAR A IDEIA DE SAÍDA 
				}
				else if(ready == 1)
				{
					leave = 1;
					exit = 1;
				}
				else
					break;
			}
		}
		if(FD_ISSET(fdclient, &rfds)) //Cliente
		{
			status = server_udp_cliente(fdclient, addrclient);
			
			if(status == INDISP) /*Está a fornecer serviço*/
			{
				/*Send Token to fin new dispatch server*/
				server_udp_central(WDS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);
				if(company)
				{
					tcpflag = TOKEN_S;
					type = 'S';
					s_id = id;
				}
				else
					ringstatus = INDISP;
			}
			else if(status == DISP) /*Está disponivel de novo*/
			{
				if(!company)
				{
					server_udp_central(SDS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec,  &dsflag);
				}
				else if(ringstatus == INDISP)
				{
					/*Send Token to inform the ring about availability*/
					tcpflag = TOKEN_S;
					type = 'D';
					ringstatus = DISP;
				}
			}
		}
		if(FD_ISSET(fdw, &rfds)) // Esperando Antecessor 
		{
			addrlen=sizeof(addrpre);
			if((newfd =accept(fdw,(struct sockaddr *)&addrpre,&addrlen)) ==-1)
			{
				printf("Error in accept: %s\n", strerror(errno));
				leave = 1;
			}

			if(fda != -1)
			{
				close(fda);fda = -1;
			}
			fda = newfd;
		}
		if(FD_ISSET(fda, &rfds)) // Antecessor 
		{
			if((a = read(fda,buffer_read,128)) == -1)
			{
				printf("Error in read: %s\n", strerror(errno));
				leave = 1;
			}
			buffer_read[a] ='\0';
			tcpflag = check_tcp_message(buffer_read, fdcentral, addrcentral, x, &type, &id, &id2, &ip, &ip2, upt, &tpt, &tpt2, 
				&aux_id, &aux_ip, &aux_tpt, &s_id, status, &ringstatus, buffer_rec, &dsflag);
			
			if(fds == -1 && tcpflag == TOKEN_L) //SERVIDOR DE ARRANQUE NAO TEM SUCESSOR
			{
				/*TCP Socket  para o SUCESSOR*/
				if((fds=socket(AF_INET,SOCK_STREAM,0))==-1)
				{
					printf("ERROR: %s\nCREATING SOCKET FDS\n", strerror(errno));		
					leave = 1;
				}
				strcpy(ip2, aux_ip);
				id2 = aux_id;
				tpt2 = aux_tpt;
				tcpflag = -1;
				conflag = 1;
				company = 1;
				
				if(ringstatus == INDISP && type == 'N')
				{
					tcpflag = TOKEN_S;
					type = 'I';
				}

				fprintf(stderr, "\n-----------------------------------------------");
				fprintf(stderr, "\nSucessor was added, New Sucessor's ID is %d\n", id2);
				fprintf(stderr, "-----------------------------------------------\n\n");
			}
			else if(tcpflag == TOKEN_L && type == 'O' && s_id == id2 && aux_id == id)
			{
				if(fda != -1)
				{
					close(fda);
					fda = -1;
				}
				if(fds !=- 1 && ready && conflag != -1) 
					server_tcp_server(fds, tcpflag, type, id, id2, ip, ip2, tpt, tpt2, aux_id, aux_ip, aux_tpt, s_id);	
				
				if(fds != -1)
				{
					close(fds);
					fds = -1;
				}
				company = 0;
				tcpflag = -1;
				conflag = 0;
			}		
			else if(s_id == id2 && tcpflag == TOKEN_L) //PRECISA DE ALTERAR SUCESSOR
			{
				if(fds !=- 1 && ready && type == 'O' && conflag != -1) 
				{
					server_tcp_server(fds, tcpflag, type, id, id2, ip, ip2, tpt, tpt2, aux_id, aux_ip, aux_tpt, s_id);	
					tcpflag = -1;
				}
				if(fds != -1)
				{
					close(fds);
					fds = -1;
				}
				/*TCP Socket  para o SUCESSOR*/
				if(company)
				{
					if((fds=socket(AF_INET,SOCK_STREAM,0))==-1)
					{
						printf("ERROR: %s\nCREATING SOCKET FDS\n", strerror(errno));		
						leave = 1;
					}
					strcpy(ip2, aux_ip);
					id2 = aux_id;
					tpt2 = aux_tpt;

					tcpflag = -1;
					conflag = 1;

					if(ringstatus == INDISP && type == 'N')
					{
						tcpflag = TOKEN_S;
						type = 'I';
					}
					fprintf(stderr, "\n-----------------------------------------------");
					fprintf(stderr, "\nSucessor was altered, new Sucessor's ID is %d\n", id2);
					fprintf(stderr, "-----------------------------------------------\n\n");
				}
			}
			else if (tcpflag == TOKEN_L && type == 'O' && s_id == id && leave == 0)
			{
				leave = 3;
				tcpflag = -1;
				conflag = 0;
				company = 0;
			}
		}
		if(leave > 0 && ready ) //leave function
		{
			if(status == INDISP)
			{
				fprintf(stderr, "Can't leave while providing a service!\n");
				leave = -1; // CANCELAR A IDEIA DE SAÍDA 
			}
			else if(company)
			{
				if(leave == 1) //dar withdraw caso seja necessário e enviar o token D
				{
					leave = 2;
					sprintf(message, "GET_START %i%c%i", x,59,id);
					buffer_rec = udp_message(fdcentral, message, addrcentral, buffer_rec);
					sscanf(buffer_rec, "OK %d;%d", &aux_id, &s_id);

					if(id == s_id)
					{
						server_udp_central(WS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);
						server_tcp_server(fds, NEW_START, type, id, id2, ip, ip2, tpt, tpt2, aux_id, aux_ip, aux_tpt, s_id);	
						tcpflag = -1;
						
					}
					if(dsflag == 1)
					{
						server_udp_central(WDS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);
						tcpflag = TOKEN_S;
						type = 'S';
						s_id = id;
						leave = 0;
					}
				}
				if(leave == 2) // enviar token O
				{
					tcpflag = TOKEN_L;
					aux_id = id2;
					strcpy(aux_ip, ip2);
					aux_tpt = tpt2;
					s_id = id;
					type = 'O';
					leave = 0;				}
			}
			else if(company == 0 && leave == 1) // estańdo sozinho só é preciso sair do anel
			{
				server_udp_central(WDS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);
				server_udp_central(WS, fdcentral, addrcentral, x, id, &id2, ip, &ip2, tpt, &tpt2, upt, buffer_rec, &dsflag);

				if(fdclient != -1)
				{
					close(fdclient);
					fdclient = -1;
				}
				if(fdw != -1)
				{
					close(fdw);
					fdw = -1;
				}
				if(fda != -1)
				{
					close(fda);
					fda = -1;
				}
				if(fds != -1)
				{
					close(fds);
					fds = -1;
				}
				leave = -1;
				company = 0;
				ready = 0;
				fprintf(stderr, "\n-----------------------------------------------");
				fprintf(stderr, "\nSucessfully left the ring\n");				
				fprintf(stderr, "-----------------------------------------------\n\n");
				if(exit == 1)
					break;
			}
			else if(company == 0 && leave == 3)  // caso se tenha recebido o O , então fechar sockets
			{
				if(fdclient != -1)
				{
					close(fdclient);
					fdclient = -1;
				}
				if(fdw != -1)
				{
					close(fdw);
					fdw = -1;
				}
				if(fda != -1)
				{
					close(fda);
					fda = -1;
				}
				if(fds != -1)
				{
					close(fds);
					fds = -1;
				}
				leave = -1;
				ready = 0;
				company = 0;
				fprintf(stderr, "\n-----------------------------------------------");
				fprintf(stderr, "\nSucessfully left the ring\n");				
				fprintf(stderr, "-----------------------------------------------\n\n");
				if(exit == 1)
					break;
			}
		}
		if(fds !=- 1 && ready) // Sucessor
		{
			if(conflag == 1) //ATIVAR ESTA FLAG QUANDO HOUVER UM NOVO SUCESSOR DEPOIS DE TER MUDADO O ADDRSUC
			{
				if((h=gethostbyname(ip2)) == NULL)
				{
					fprintf(stderr, "Erro in get host\n");
					leave = 1;
				}

				ipsuc=*(struct in_addr*)h->h_addr_list[0];

				memset((void*)&addrsuc,(int)'\0',sizeof(addrsuc));
				addrsuc.sin_family=AF_INET;
				addrsuc.sin_addr=ipsuc;
				addrsuc.sin_port=htons(tpt2);

				if(connect(fds,(struct sockaddr*)&addrsuc, sizeof(addrsuc)) == -1)
				{
					fprintf(stderr,"ERROR CONNECTING WITH SUCESSOR: %s\n", strerror(errno));
				}
				conflag = 0;
			}
			if(conflag != -1 && tcpflag != -1) //NAO SE PODE MANDAR UMA MENSAGEM CASO NAO TENHA HAVIDO UM CONNECT ANTES
			{
				server_tcp_server(fds, tcpflag, type, id, id2, ip, ip2, tpt, tpt2, aux_id, aux_ip, aux_tpt, s_id);	
				tcpflag = -1;
			}

			if((old_handler=signal(SIGPIPE,SIG_IGN))==SIG_ERR)leave = 1;;//error
		}		
	}

	if(fdclient != -1)
	{
		close(fdclient);
	}
	if(fdw != -1)
	{
		close(fdw);
	}
	if(fda != -1)
	{
		close(fda);
	}
	if(fds != -1)
	{
		close(fds);
	}

	return;
}

int main( int argc, char *argv[])
{
	struct hostent *h;
	struct sockaddr_in addr;
	int i, id, upt, tpt, cspt, fip = 0, fpt = 0;
	char csip[128], ip[32], *buffer_rec;
	struct in_addr conv_csip;	

	buffer_rec = malloc(sizeof(char)*128);
	

	for(i = 1; i < argc; i++)//ler inputs
	{
		if(strcmp(argv[i],"-n") == 0) 

		{	
			id = atoi(argv[i+1]);	

			i++;
		}
		else if(strcmp(argv[i],"-j") == 0) 
		{
			strcpy(ip, argv[i+1]);
			i++;
		}
		else if(strcmp(argv[i],"-u") == 0) 
		{
			upt = atoi(argv[i+1]) ;
			i++;
		}
		else if(strcmp(argv[i],"-t") == 0) 
		{
			tpt = atoi(argv[i+1]);
			i++;
		}
		else if(strcmp(argv[i],"-i") == 0)
		{
			strcpy(csip, argv[i+1]);
			i++;
			fip = 1;
		}
		else if(strcmp(argv[i],"-p") == 0)
		{
			cspt = atoi(argv[i+1]);
			i++;
			fpt = 1;
		}
		else
		{
			fprintf(stderr, "Invalid parameter : %s\n", argv[i]);
			return 0;
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

	/*Open Socket*/

	connections(addr, id, ip, tpt, upt, buffer_rec);

	free(buffer_rec);

	return 1;
}