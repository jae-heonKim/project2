// 출처: http://remocon33.tistory.com/465

// 공용 캘린더 관리 프로그램
// 개발날짜 : 2025.05.22 ~ 2025.06.18
// 개발자 : 김재헌
// git주소 : https://github.com/jae-heonKim/project2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <conio.h>

#define BUF_SIZE 1024
#define NAME_SIZE 20

unsigned WINAPI SendMsg(void *arg); // 쓰레드 전송함수
void ErrorHandling(char *msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main()
{
	WSADATA wsaData;
	SOCKET sock;
	SOCKADDR_IN serverAddr;
	HANDLE sendThread;

	char serverIp[100];
	char port[100];
	/*
	if(argc!=4){
		printf("Usage : %s <IP> <port> <name>\n",argv[0]);
		exit(1);
	}
	*/
	printf("Input server IP : ");
	gets(serverIp);

	printf("Input server port : ");
	gets(port);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) // 윈도우 소켓을 사용한다고 운영체제에 알림
		ErrorHandling("WSAStartup() error!");

	sock = socket(PF_INET, SOCK_STREAM, 0); // 소켓을 하나 생성한다.

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(serverIp);
	serverAddr.sin_port = htons(atoi(port));

	if (connect(sock, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) // 서버에 접속한다.
		ErrorHandling("connect() error");

	// 접속에 성공하면 이 줄 아래가 실행된다.

	sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void *)&sock, 0, NULL); // 메시지 전송용 쓰레드가 실행된다.

	WaitForSingleObject(sendThread, INFINITE); // 전송용 쓰레드가 중지될때까지 기다린다.
	// 클라이언트가 종료를 시도한다면 이줄 아래가 실행된다.
	closesocket(sock); // 소켓을 종료한다.
	WSACleanup();	   // 윈도우 소켓 사용중지를 운영체제에 알린다.
	return 0;
}

unsigned WINAPI SendMsg(void *arg)
{									// 전송용 쓰레드함수
	SOCKET sock = *((SOCKET *)arg); // 서버용 소켓을 전달한다.
	char nameMsg[NAME_SIZE + BUF_SIZE];
	while (1)
	{								 // 반복
		fgets(msg, BUF_SIZE, stdin); // 입력을 받는다.
		if (!strcmp(msg, "q\n"))
		{						   // q를 입력하면 종료한다.
			send(sock, "q", 1, 0); // nameMsg를 서버에게 전송한다.
		}
		sprintf(nameMsg, "%s %s", name, msg);	 // nameMsg에 메시지를 전달한다.
		send(sock, nameMsg, strlen(nameMsg), 0); // nameMsg를 서버에게 전송한다.
	}
	return 0;
}

// 로그인 입력 및 검증
void login(SOCKET sock)
{
	char sendMsg[BUF_SIZE], recvMsg[BUF_SIZE];
	char id[30], pw[30];
	char ch;
	int i = 0, recvLen;

	while (1)
	{
		printf("아이디를 입력하세요 : ");
		fgets(id, sizeof(id), stdin);
		id[strcspn(id, "\n")] = '\0';

		i = 0;
		pw[0] = '\0';

		inputPassword("비밀번호를 입력하세요 : ", pw, sizeof(pw));

		memset(sendMsg, 0, sizeof(sendMsg));
		sprintf(sendMsg, "CL//LG//%s//%s", id, pw);

		send(sock, sendMsg, strlen(sendMsg), 0);

		recvLen = recv(sock, recvMsg, BUF_SIZE - 1, 0);
		if (recvLen <= 0)
		{
			printf("서버 응답 오류 또는 연결 종료됨\n");
			break;
		}
		recvMsg[recvLen] = '\0'; // 문자열 종료 처리

		if (strcmp(recvMsg, "SV//LG//1") == 0)
		{
			break;
		}
		else
		{
			printf("로그인에 실패하였습니다. 다시 입력하세요.\n\n");
		}
	}
}

// 사용자 추가, 삭제 시 비밀번호 암호화
void inputPassword(char *inputMsg, char *buffer, int size)
{
	char ch;
	int i = 0;

	printf("%s", inputMsg);

	while (1) // 비밀번호 입력 * 표사
	{
		ch = getch();

		if (ch == 13) // 13 : 아스키코드 엔터
		{
			buffer[i] = '\0';
			printf("\n");
			break;
		}
		else if (ch == 8) // 8 : 아스키코드 백스페이스
		{
			if (i > 0)
			{
				i--;

				buffer[i] = '\0';
				printf("\b \b");
			}
		}
		else if (i < (size - 1)) // 최대 길이 초과 방지
		{
			buffer[i++] = ch;
			printf("*");
		}
	}
}

void ErrorHandling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}