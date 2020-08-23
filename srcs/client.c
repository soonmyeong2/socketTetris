#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>
#include "chat.h"

#define	MAX_BUF		256
int		Sockfd;
/////////////////////////// start
#include "conio.h"
#include "tetris.h"

static struct termios savemodes;
static int havemodes = 0;
char temp[2];
char a = 0XFF;
char score[MAX_BUF];

#define TL     -B_COLS-1	/* top left */
#define TC     -B_COLS		/* top center */
#define TR     -B_COLS+1	/* top right */
#define ML     -1		/* middle left */
#define MR     1		/* middle right */
#define BL     B_COLS-1		/* bottom left */
#define BC     B_COLS		/* bottom center */
#define BR     B_COLS+1		/* bottom right */

/* These can be overridden by the user. */
#define DEFAULT_KEYS "jkl pq"
#define KEY_LEFT   0
#define KEY_RIGHT  2
#define KEY_ROTATE 1
#define KEY_DROP   3
#define KEY_PAUSE  4
#define KEY_QUIT   5

int flag=1;
char *keys = DEFAULT_KEYS;
int level = 1;
int points = 0;
int lines_cleared = 0;
int board[B_SIZE], shadow[B_SIZE];

int *peek_shape;		/* peek preview of next shape */
int *shape;

int shapes[] = {
		7, TL, TC, MR,
		8, TR, TC, ML,
		9, ML, MR, BC,
		3, TL, TC, ML,
		12, ML, BL, MR,
		15, ML, BR, MR,
		18, ML, MR,  2,		/* sticks out */
		0, TC, ML, BL,
		1, TC, MR, BR,
		10, TC, MR, BC,
		11, TC, ML, MR,
		2, TC, ML, BC,
		13, TC, BC, BR,
		14, TR, ML, MR,
		4, TL, TC, BC,
		16, TR, TC, BC,
		17, TL, MR, ML,
		5, TC, BC, BL,
		6, TC, BC,  2 * B_COLS, /* sticks out */
};

void timer3(void)
{
		flag=0;
		usleep(1000*310);
		flag=1;
}

int update(void)
{
		int x, y;

		//#ifdef ENABLE_PREVIEW
		const int start = 5;
		int preview[B_COLS * 10];
		int shadow_preview[B_COLS * 10];

		/* Display piece preview. */
		memset(preview, 0, sizeof(preview));
		preview[2 * B_COLS + 1] = 7;
		preview[2 * B_COLS + 1 + peek_shape[1]] = 7;
		preview[2 * B_COLS + 1 + peek_shape[2]] = 7;
		preview[2 * B_COLS + 1 + peek_shape[3]] = 7;

		for (y = 0; y < 4; y++) {
				for (x = 0; x < B_COLS; x++) {
						if (preview[y * B_COLS + x] - shadow_preview[y * B_COLS + x]) {
								shadow_preview[y * B_COLS + x] = preview[y * B_COLS + x];
								gotoxy(x * 2 + 26 + 28, start + y);
								printf("\e[%dm  ", preview[y * B_COLS + x]);
						}
				}
		}
		//#endif

		/* Display board. */
		for (y = 1; y < B_ROWS - 1; y++) {
				for (x = 0; x < B_COLS; x++) {
						if (board[y * B_COLS + x] - shadow[y * B_COLS + x]) {
								shadow[y * B_COLS + x] = board[y * B_COLS + x];
								gotoxy(x * 2 + 28, y);
								printf("\e[%dm  ", board[y * B_COLS + x]);
						}
				}
		}

		/* Update points and level */
		while (lines_cleared >= 10) {
				lines_cleared -= 10;
				level++;
		}

		//#ifdef ENABLE_SCORE
		/* Display current level and points */
		textattr(RESETATTR);
		gotoxy(26 + 28, 2);
		printf("Level  : %d", level);
		gotoxy(26 + 28, 3);
		printf("Points : %d", points);
		//#endif
		//#ifdef ENABLE_PREVIEW
		gotoxy(26 + 28, 5);
		printf("Preview:");
		//#endif
		gotoxy(26 + 28, 10);
		printf("Keys:");
		fflush(stdout);

		char ch = getchar();
		pthread_t  tid;
		if(flag) {
				sprintf(temp, "%c", ch);
				////////////////////////////
				if(ch != a) {
						pthread_create(&tid, NULL, (void*)timer3,NULL);
						if (send(Sockfd, temp, 3, 0) < 0) {
								perror("send");
								exit(1);
						}
				}
				if ((recv(Sockfd, temp, 3, MSG_PEEK)) > 0) {
						if (recv(Sockfd, temp, 3, 0) < 0) {
								perror("recv");
								exit(1);
						}
						ch = temp[0];
				}
				return ch;
		}
		/////////////////////////////

}

// 제공되는 범위 안에 블록이 로테이션 할수 있는지 체크하는 함수
int fits_in(int *shape, int pos)
{
		if (board[pos] || board[pos + shape[1]] || board[pos + shape[2]] || board[pos + shape[3]])
				return 0;

		return 1;
}


void place(int *shape, int pos, int b)
{
		board[pos] = b;
		board[pos + shape[1]] = b;
		board[pos + shape[2]] = b;
		board[pos + shape[3]] = b;
}

int *next_shape(void)
{
		int *next = peek_shape;

		peek_shape = &shapes[rand() % 7 * 4];
		if (!next)
				return next_shape();

		return next;
}

void show_online_help(void)
{
		const int start = 11;

		textattr(RESETATTR);
		gotoxy(26 + 28, start);
		puts("j     - left");
		gotoxy(26 + 28, start + 1);
		puts("k     - rotate");
		gotoxy(26 + 28, start + 2);
		puts("l     - right");
		gotoxy(26 + 28, start + 3);
		puts("space - drop");
		gotoxy(26 + 28, start + 4);
		puts("p     - pause");
		gotoxy(26 + 28, start + 5);
		puts("q     - quit");
}

/* Code stolen from http://c-faq.com/osdep/cbreak.html */
int tty_init(void)
{
		struct termios modmodes;

		if (tcgetattr(fileno(stdin), &savemodes) < 0)
				return -1;

		havemodes = 1;
		hidecursor();

		/* "stty cbreak -echo" */
		modmodes = savemodes;
		modmodes.c_lflag &= ~ICANON;
		modmodes.c_lflag &= ~ECHO;
		modmodes.c_cc[VMIN] = 1;
		modmodes.c_cc[VTIME] = 0;

		return tcsetattr(fileno(stdin), TCSANOW, &modmodes);
}

int tty_exit(void)
{
		if (!havemodes)
				return 0;

		showcursor();

		/* "stty sane" */
		return tcsetattr(fileno(stdin), TCSANOW, &savemodes);
}

void freeze(int enable)
{
		sigset_t set;

		sigemptyset(&set);
		sigaddset(&set, SIGALRM);

		sigprocmask(enable ? SIG_BLOCK : SIG_UNBLOCK, &set, NULL);
}

void alarm_handler(int signo)
{
		static long h[4];

		/* On init from main() */
		if (!signo)
				h[3] = 200000;

		// 레벨마다 설정
		h[3] -= h[3] / (3000 - 10 * level);
		setitimer(0, (struct itimerval *)h, 0);
}

void exit_handler(int signo)
{
		clrscr();
		tty_exit();
		exit(0);
}

int sig_init(void)
{
		struct sigaction sa;

		SIGNAL(SIGINT, exit_handler);
		SIGNAL(SIGTERM, exit_handler);
		SIGNAL(SIGALRM, alarm_handler);

		/* Start update timer. */
		alarm_handler(0);
}

void sortInt(int number[], int num) {
int temp;
for(int i = 0 ; i < num-1 ; i ++) {
		for(int j = i+1 ; j < num ; j ++) {
				if(number[i] < number[j]) {
						temp = number[j];
						number[j] = number[i];
						number[i] = temp;
				}
		}
}
}
int run()
{
		int c = 0, i, j, *ptr;
		int pos = 17;
		int *backup;

		/* Initialize board */
		ptr = board;
		for (i = B_SIZE; i; i--)
				*ptr++ = i < 25 || i % B_COLS < 2 ? 7 : 0;

		srand((unsigned int)time(NULL));
		if (tty_init() == -1)
				return 1;

		/* Set up signals */
		sig_init();

		clrscr();
		show_online_help();

		shape = next_shape();
		while (1) {
				if (c < 0) {
						if (fits_in(shape, pos + B_COLS)) {
								pos += B_COLS;
						} else {
								place(shape, pos, 7);
								++points;
								for (j = 0; j < 252; j = B_COLS * (j / B_COLS + 1)) {
										for (; board[++j];) {
												if (j % B_COLS == 10) {
														lines_cleared++;

														for (; j % B_COLS; board[j--] = 0)
																;
														//c = update();

														for (; --j; board[j + B_COLS] = board[j])
																;
														//c = update();
												}
										}
								}
								shape = next_shape();
								if (!fits_in(shape, pos = 17))
										c = keys[KEY_QUIT];
						}
				}

				if (c == keys[KEY_LEFT]) {
						if (!fits_in(shape, --pos))
								++pos;
				}

				if (c == keys[KEY_ROTATE]) {
						backup = shape;
						shape = &shapes[4 * *shape];	/* Rotate */
						/* Check if it fits, if not restore shape from backup */
						if (!fits_in(shape, pos))
								shape = backup;
				}

				if (c == keys[KEY_RIGHT]) {
						if (!fits_in(shape, ++pos))
								--pos;
				}

				if (c == keys[KEY_DROP]) {
						for (; fits_in(shape, pos + B_COLS); ++points)
								pos += B_COLS;
				}

				if (c == keys[KEY_PAUSE] || c == keys[KEY_QUIT]) {
						freeze(1);

						//게임 종료 시
						if (c == keys[KEY_QUIT]) {
								clrscr();
								gotoxy(0, 0);
								textattr(RESETATTR);

								printf("Your score: %d points x level %d = %d\n\n", points, level, points * level);
								sprintf(score, "s%d", points*level);
								send(Sockfd, score, strlen(score)+1, 0);
								recv(Sockfd, score, MAX_BUF, 0);


								int scoreNum[50], j = 0;
								char *ptr = strtok(score, "s");

								while(ptr != NULL)
								{
										scoreNum[j] = atoi(ptr);
										ptr = strtok(NULL, "s");
										j++;
								}

								sortInt(scoreNum, j);
								
								
								printf("     score board\n");
								for(int i=0; i<j; i++)
								{
										printf("%d: %d\n", i+1, scoreNum[i]);
								}


								break;
						}

						for (j = B_SIZE; j--; shadow[j] = 0)
								;

						while (getchar() - keys[KEY_PAUSE])
								;

						//			puts("\e[H\e[J\e[7m");
						freeze(0);
				}

				place(shape, pos, 7);
				c = update();	
				place(shape, pos, 0);
		}

		if (tty_exit() == -1)
				return 1;

		return 0;
}
/////////////////////////// end
void ChatClient(void)
{
		char	buf[MAX_BUF];
		int		count, n;
		fd_set	fdset;

		printf("Press ^C to exit\n");

		int sig = 0;
		while (1)  {
				FD_ZERO(&fdset);
				FD_SET(Sockfd, &fdset);
				FD_SET(STDIN_FILENO, &fdset);

				if ((count = select(10, &fdset, (fd_set *)NULL, (fd_set *)NULL,
												(struct timeval *)NULL)) < 0)  {
						perror("select");
						exit(1);
				}
				while (count--)  {
						if (FD_ISSET(Sockfd, &fdset))  {
								if ((n = recv(Sockfd, buf, MAX_BUF, 0)) < 0)  {
										perror("recv");
										exit(1);
								}
								if (n == 0)  {
										fprintf(stderr, "Server terminated.....\n");
										close(Sockfd);
										exit(1);
								}
								printf("%s", buf);
								fflush(stdout);
								if (strcmp(buf, "go")==0)
										sig = 1;
						}
						else if (FD_ISSET(STDIN_FILENO, &fdset))  {
								fgets(buf, MAX_BUF, stdin);
								if ((n = send(Sockfd, buf, strlen(buf)+1, 0)) < 0)  {
										perror("send");
										exit(1);
								}
						}
				}
				if (sig == 1)
						break;
		}

		recv(Sockfd, buf, MAX_BUF, 0);
		run();
}


void CloseClient(int signo)
{
		close(Sockfd);
		printf("\nChat client terminated.....\n");

		exit(0);
}

// 서버와 접속만 시켜주고 클라이언트 일 함
void main(int argc, char *argv[])
{
		struct sockaddr_in	servAddr;
		struct hostent		*hp;

		if (argc != 2)  {
				fprintf(stderr, "Usage: %s ServerIPaddress\n", argv[0]);
				exit(1);
		}

		if ((Sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)  {
				perror("socket");
				exit(1);
		}

		bzero((char *)&servAddr, sizeof(servAddr));
		servAddr.sin_family = PF_INET;
		servAddr.sin_port = htons(SERV_TCP_PORT);

		if (isdigit(argv[1][0]))  {
				servAddr.sin_addr.s_addr = htonl(inet_addr(argv[1]));
		}
		else  {
				if ((hp = gethostbyname(argv[1])) == NULL)  {
						fprintf(stderr, "Unknown host: %s\n", argv[1]);
						exit(1);
				}
				memcpy(&servAddr.sin_addr, hp->h_addr, hp->h_length);
		}

		if (connect(Sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)  {
				perror("connect");
				exit(1);
		}

		signal(SIGINT, CloseClient);
		ChatClient();
}
