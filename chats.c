#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "chat.h"

#define	DEBUG

#define	MAX_CLIENT	5
#define	MAX_ID		32
#define	MAX_BUF		256

typedef	struct  {
		int			sockfd;
		int			inUse;
		pthread_t	tid;
		char		uid[MAX_ID];
}ClientType;

int				Sockfd;
pthread_mutex_t	Mutex;
int 			inGame = 0;

ClientType		Client[MAX_CLIENT];

int GetUserNumber()
{
		int	i;
		int count = 0;

		for (i = 0 ; i < MAX_CLIENT ; i++)  {
				if (Client[i].inUse == 1)  {
						count++;
				}
		}
		return count;
}

int GetID()
{
		int	i;

		for (i = 0 ; i < MAX_CLIENT ; i++)  {
				if (! Client[i].inUse)  {
						Client[i].inUse = 1;
						return i;
				}
		}
}

void SendToAllClients(char *buf)
{
		int		i;
		char	msg[MAX_BUF+MAX_ID];

		sprintf(msg, "%s", buf);
#ifdef	DEBUG
		printf("%s", msg);
		fflush(stdout);
#endif

		pthread_mutex_lock(&Mutex);
		for (i = 0 ; i < MAX_CLIENT ; i++)  {
				if (Client[i].inUse)  {
						if (send(Client[i].sockfd, msg, strlen(msg)+1, 0) < 0)  {
								perror("send");
								exit(1);
						}
				}
		}
		pthread_mutex_unlock(&Mutex);
}


void ProcessClient(int id)
{
		char	buf[MAX_BUF];
		int		n;

		if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))  {
				perror("pthread_setcancelstate");
				exit(1);
		}
		if (pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))  {
				perror("pthread_setcanceltype");
				exit(1);
		}

		//if ((n = recv(Client[id].sockfd, Client[id].uid, MAX_ID, 0)) < 0)  {
		//		perror("recv");
		//		exit(1);
		//}
		//printf("Client %d log-in(ID: %s).....\n", id, Client[id].uid);
		// 게임 중이 아니고 유저가 1명이면 대기
		char	msg[MAX_BUF] = "Waiting for another user.";
		if (send(Client[id].sockfd, msg, strlen(msg)+1, 0) < 0)  {
				perror("send");
				exit(1);
		}

		while(GetUserNumber() == 1 && !inGame) {
				char dot[MAX_BUF] = ".";

				if (send(Client[id].sockfd, dot, strlen(dot)+1, 0) < 0)  {
						perror("send");
						exit(1);
				}
				sleep(1);
		}
		// 게임 중이라면 대기
		//if (inGame) {
		//}

		char msg_c[MAX_BUF] = "\nStart the game ..";
		send(Client[id].sockfd, msg_c, strlen(msg_c)+1, 0);
		for (int k=5; k>0; k--) {
				char dot[10];
				sprintf(dot, " %d ..", k);
				send(Client[id].sockfd, dot, strlen(dot)+1, 0);
				sleep(1);
		}
		send(Client[id].sockfd, "go", 3, 0);

		while (1)  {
				if ((n = recv(Client[id].sockfd, buf, MAX_BUF, 0)) < 0)  {
						perror("recv");
						exit(1);
				}
				if (n == 0)  {
						printf("Client %d log-out(ID: %s).....\n", id, Client[id].uid);

						pthread_mutex_lock(&Mutex);
						close(Client[id].sockfd);
						Client[id].inUse = 0;
						pthread_mutex_unlock(&Mutex);

						strcpy(buf, "log-out.....\n");
//						SendToAllClients(buf);

						pthread_exit(NULL);
				}

				SendToAllClients(buf);
		}
}


void CloseServer(int signo)
{
		int		i;

		close(Sockfd);

		for (i = 0 ; i < MAX_CLIENT ; i++)  {
				if (Client[i].inUse)  {
						if (pthread_cancel(Client[i].tid))  {
								perror("pthread_cancel");
								exit(1);
						}
						if (pthread_join(Client[i].tid, NULL))  {
								perror("pthread_join");
								exit(1);
						}
						close(Client[i].sockfd);
				}
		}
		if (pthread_mutex_destroy(&Mutex) < 0)  {
				perror("pthread_mutex_destroy");
				exit(1);
		}

		printf("\nChat server terminated.....\n");

		exit(0);
}

void main(int argc, char *argv[])
{
		int					newSockfd, cliAddrLen, id, one = 1;
		struct sockaddr_in	cliAddr, servAddr;

		signal(SIGINT, CloseServer);
		if (pthread_mutex_init(&Mutex, NULL) < 0)  {
				perror("pthread_mutex_init");
				exit(1);
		}

		if ((Sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)  {
				perror("socket");
				exit(1);
		}

		if (setsockopt(Sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)  {
				perror("setsockopt");
				exit(1);
		}

		bzero((char *)&servAddr, sizeof(servAddr));
		servAddr.sin_family = PF_INET;
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servAddr.sin_port = htons(SERV_TCP_PORT);

		if (bind(Sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)  {
				perror("bind");
				exit(1);
		}

		listen(Sockfd, 5);

		printf("Chat server started.....\n");

		cliAddrLen = sizeof(cliAddr);
		while (1)  {
				newSockfd = accept(Sockfd, (struct sockaddr *) &cliAddr, &cliAddrLen);
				if (newSockfd < 0)  {
						perror("accept");
						exit(1);
				}

				id = GetID();
				Client[id].sockfd = newSockfd;

				if (pthread_create(&Client[id].tid, NULL, (void *)ProcessClient, (void *)id) < 0)  {
						perror("pthread_create");
						exit(1);
				}
		}
}
