////출처: http://remocon33.tistory.com/465

// 공용 캘린더 관리 프로그램
// 개발날짜 : 2025.05.22 ~ 2025.06.18
// 개발자 : 김재헌
// git주소 : https://github.com/jae-heonKim/Cadv_12_kim

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>

#define BUF_SIZE 1024      // 최대 메시지 길이
#define MAX_CLIENT 256     // 최대 접속자 수
#define MAX_ROOM_MEMBER 50 // 방 최대 인원
#define MAX_ID_LEN 30
#define MAX_PW_LEN 30
#define MAX_NAME_LEN 30
#define MAX_ROOM_MEMBER 50
#define MAX_TITLE_LEN 30
#define MAX_CONTENT_LEN 100
#define MAX_TYPE_LEN 10
#define MAX_DATE_LEN 11 // YYYY-MM-DD + '\0'
#define MAX_TIME_LEN 6  // HH:MM + '\0'

typedef enum
{
    TYPE_ERROR = 0,
    TYPE_LOGIN,
    TYPE_LOGOUT,
    TYPE_USER_LIST,
    TYPE_USER_ADD,
    TYPE_USER_DELETE,
    TYPE_ROOM_CREATE,
    TYPE_ROOM_JOIN,
    TYPE_ROOM_LEAVE,
    TYPE_ROOM_DELETE,
    TYPE_SCHEDULE_CREATE,
    TYPE_SCHEDULE_LIST,
    TYPE_SCHEDULE_DELETE,
    TYPE_SCHEDULE_EDIT,
    TYPE_SCHEDULE_SEARCH
} RequestType;

typedef struct User
{
    char id[MAX_ID_LEN];
    char pw[MAX_PW_LEN];
    char name[MAX_NAME_LEN];
    int isAdmin;
} User;

typedef struct Room
{
    char title[MAX_TITLE_LEN];
    int memberCount;
    char membersId[MAX_ROOM_MEMBER][MAX_ID_LEN];
    struct Room *next;
} Room;

typedef struct Schedule
{
    int index; // 일정은 중복 가능이기 때문에 찾을 수 없음
    char roomTitle[MAX_TITLE_LEN];
    char writerId[MAX_ID_LEN];

    char startDate[MAX_DATE_LEN]; // YYYY-MM-DD
    int isAllDay;                 // 하루 종일 여부: 1이면 시간 무시

    char endDate[MAX_DATE_LEN];   // YYYY-MM-DD
    char startTime[MAX_TIME_LEN]; // HH:MM
    char endTime[MAX_TIME_LEN];   // HH:MM

    char content[MAX_CONTENT_LEN];
    struct Schedule *next;
} Schedule;

// [쓰레드 및 에러 처리]
unsigned WINAPI HandleClient(void *arg); // 클라이언트 전용 쓰레드 함수
void ErrorHandling(char *msg);           // 에러 메시지 출력 후 종료

// [파일 로딩 및 저장]
void loadUser(void);     // 사용자 정보 파일(users.txt) 불러오기
void loadRoom(void);     // 방 정보 파일(rooms.txt) 불러오기
void loadSchedule(void); // 일정 정보 파일(schedules.txt) 불러오기
void saveUser(void);     // 사용자 정보 저장
void saveRoom(void);     // 방 정보 저장
void saveSchedule(void); // 일정 정보 저장

// [요청 처리 및 공통 기능]
int checkRequest(char *msg, User *userRequest, Room *roomRequest, Schedule *scheduleRequest); // 클라이언트 요청 종류 판별
const char *getUserNameById(const char *id);                                                  // ID로부터 사용자 이름 반환
int compareSchedule(const void *a, const void *b);                                            // 일정 정렬 기준 (날짜순)

// [로그인 검증]
void checkLogin(SOCKET clientSock, const char *id, const char *pw); // 로그인 검증

// [사용자 관리 기능]
void getUserList(SOCKET clientSock);                                         // 사용자 목록 출력 (관리자 전용)
void addUser(SOCKET clientSock, char *id, const char *pw, const char *name); // 사용자 추가 (회원가입)
void deleteUser(SOCKET clientSock, const char *id);                          // 사용자 삭제 (관리자 전용)

// [방 관리 기능]
void getRoomList(SOCKET clientSock, const char *id);                          // 방 목록 출력
void createRoom(SOCKET clientSock, const char *title, const char *creatorId); // 방 생성
void joinRoom(SOCKET clientSock, const char *roomTitle, const char *id);      // 방 참가
void leaveRoom(SOCKET clientSock, const char *roomTitle, const char *userId); // 방 퇴장
void deleteRoom(SOCKET clientSock, const char *roomTitle, const char *id);    // 방 삭제 (관리자 전용)
void deleteEmptyRoom(Room *cur, Room *curPrev);                               // 방 자동 삭제

// [일정 관리 기능]
void createSchedule(SOCKET clientSock, Schedule *newSchedule);                          // 일정 추가
void getScheduleList(SOCKET clientSock, const char *roomTitle);                         // 일정 목록 출력
void editSchedule(SOCKET clientSock, const char *roomTitle, int index, Schedule *edit); // 일정 수정
void deleteSchedule(SOCKET clientSock, int index);                                      // 일정 삭제
void searchSchedule(SOCKET clientSock, const char *roomTitle, const char *keyword);     // 일정 내용 검색

Room *roomHead = NULL;
Room *roomTail = NULL;

Schedule *scheduleHead = NULL;
Schedule *scheduleTail = NULL;
int scheduleIdCounter = 1;

int userCount = 0;      // 현재 저장된 사용자 수
User users[MAX_CLIENT]; // 사용자 정보 배열

int clientCount = 0;
SOCKET clientSocks[MAX_CLIENT]; // 클라이언트 소켓 보관용 배열
HANDLE hMutex;                  // 뮤텍스

int main()
{
    WSADATA wsaData;
    SOCKET serverSock, clientSock;
    SOCKADDR_IN serverAddr, clientAddr;
    int clientAddrSize;
    HANDLE hThread;

    char port[100];

    printf("Input port number : ");
    gets(port);
    /*
        if(argc!=2){
            printf("Usage : %s <port>\n",argv[0]);
            exit(1);
        }
        */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) // 윈도우 소켓을 사용하겠다는 사실을 운영체제에 전달
        ErrorHandling("WSAStartup() error!");

    hMutex = CreateMutex(NULL, FALSE, NULL);      // 하나의 뮤텍스를 생성한다.
    serverSock = socket(PF_INET, SOCK_STREAM, 0); // 하나의 소켓을 생성한다.

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(atoi(port));

    // 데이터 파일 읽기
    loadUser();
    loadRoom();
    loadSchedule();

    if (bind(serverSock, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) // 생성한 소켓을 배치한다.
        ErrorHandling("bind() error");
    if (listen(serverSock, 5) == SOCKET_ERROR) // 소켓을 준비상태에 둔다.
        ErrorHandling("listen() error");

    printf("listening...\n");

    while (1)
    {
        clientAddrSize = sizeof(clientAddr);
        clientSock = accept(serverSock, (SOCKADDR *)&clientAddr, &clientAddrSize); // 서버에게 전달된 클라이언트 소켓을 clientSock에 전달
        WaitForSingleObject(hMutex, INFINITE);                                     // 뮤텍스 실행
        clientSocks[clientCount++] = clientSock;                                   // 클라이언트 소켓배열에 방금 가져온 소켓 주소를 전달
        ReleaseMutex(hMutex);                                                      // 뮤텍스 중지

        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, (void *)&clientSock, 0, NULL); // HandleClient 쓰레드 실행, clientSock을 매개변수로 전달

        printf("Connected Client IP : %s\n", inet_ntoa(clientAddr.sin_addr));
    }
    closesocket(serverSock); // 생성한 소켓을 끈다.
    WSACleanup();            // 윈도우 소켓을 종료하겠다는 사실을 운영체제에 전달
    return 0;
}

// 클라이언트마다 생성되는 스레드 함수
unsigned WINAPI HandleClient(void *arg)
{
    SOCKET clientSock = *((SOCKET *)arg);
    char msg[BUF_SIZE];
    int strLen = 0, i;

    User userRequest;
    Room roomRequest;
    Schedule scheduleRequest;

    while ((strLen = recv(clientSock, msg, sizeof(msg), 0)) > 0)
    {
        msg[strLen] = '\0';

        RequestType reqType = checkRequest(msg, &userRequest, &roomRequest, &scheduleRequest);

        switch (reqType)
        {
        case TYPE_LOGIN:
            checkLogin(clientSock, userRequest.id, userRequest.pw);
            break;

        case TYPE_LOGOUT:
            send(clientSock, "SV//USER//LOGOUT//1", strlen("SV//USER//LOGOUT//1"), 0);
            break;

        case TYPE_USER_LIST:
            getUserList(clientSock);
            break;

        case TYPE_USER_ADD:
            addUser(clientSock, userRequest.id, userRequest.pw, userRequest.name);
            break;

        case TYPE_USER_DELETE:
            deleteUser(clientSock, userRequest.id);
            break;

        case TYPE_ROOM_CREATE:
            createRoom(clientSock, roomRequest.title, userRequest.id);
            break;

        case TYPE_ROOM_JOIN:
            joinRoom(clientSock, roomRequest.title, userRequest.id);
            break;

        case TYPE_ROOM_LEAVE:
            leaveRoom(clientSock, roomRequest.title, userRequest.id);
            break;

        case TYPE_ROOM_DELETE:
            deleteRoom(clientSock, roomRequest.title, userRequest.id);
            break;

        case TYPE_SCHEDULE_CREATE:
        {
            Schedule *newSch = (Schedule *)malloc(sizeof(Schedule));
            if (newSch != NULL)
            {
                *newSch = scheduleRequest;
                createSchedule(clientSock, newSch);
            }
            else
            {
                send(clientSock, "SV//SCHEDULE//CREATE//0", strlen("SV//SCHEDULE//CREATE//0"), 0);
            }
            break;
        }

        case TYPE_SCHEDULE_LIST:
            getScheduleList(clientSock, scheduleRequest.roomTitle);
            break;

        case TYPE_SCHEDULE_EDIT:
            editSchedule(clientSock, scheduleRequest.roomTitle, scheduleRequest.index, &scheduleRequest);
            break;

        case TYPE_SCHEDULE_DELETE:
            deleteSchedule(clientSock, scheduleRequest.roomTitle, scheduleRequest.index);
            break;

        case TYPE_SCHEDULE_SEARCH:
            searchSchedule(clientSock, scheduleRequest.roomTitle, scheduleRequest.content);
            break;

        default:
            send(clientSock, "SV//ERROR//UNKNOWN_TYPE", strlen("SV//ERROR//UNKNOWN_TYPE"), 0);
            break;
        }
    }

    // 연결 종료 처리
    printf("client left the server\n");

    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < clientCount; i++)
    {
        if (clientSock == clientSocks[i])
        {
            while (i++ < clientCount - 1)
                clientSocks[i] = clientSocks[i + 1];
            break;
        }
    }

    clientCount--;

    // 파일에 데이터 저장
    saveUser();
    saveRoom();
    saveSchedule();

    ReleaseMutex(hMutex);
    closesocket(clientSock);
    return 0;
}

// 프로토콜 분리 및 요청 종류 반환
int checkRequest(char *msg, User *userRequest, Room *roomRequest, Schedule *scheduleRequest)
{
    char temp[BUF_SIZE];
    char *type, *category, *function;
    char *token1, *token2, *token3, *token4, *token5, *token6, *token7, *token8, *token9;

    strcpy(temp, msg);
    temp[strcspn(temp, "\n")] = '\0';

    type = strtok(temp, "//");
    category = strtok(NULL, "//");
    function = strtok(NULL, "//");

    if (type == NULL || category == NULL || function == NULL || strcmp(type, "CL") != 0)
        return TYPE_ERROR;

    // ============ LOGIN ============
    if (strcmp(category, "LG") == 0)
    {
        token1 = strtok(NULL, "//"); // ID
        token2 = strtok(NULL, "//"); // PW

        if (token1 == NULL || token2 == NULL)
            return TYPE_ERROR;

        strcpy(userRequest->id, token1);
        strcpy(userRequest->pw, token2);

        return TYPE_LOGIN;
    }

    // ============ USER ============
    if (strcmp(category, "USER") == 0)
    {
        if (strcmp(function, "LIST") == 0)
            return TYPE_USER_LIST;

        token1 = strtok(NULL, "//"); // ID
        token2 = strtok(NULL, "//"); // PW

        if (token1 == NULL || token2 == NULL)
            return TYPE_ERROR;

        strcpy(userRequest->id, token1);
        strcpy(userRequest->pw, token2);

        if (strcmp(function, "ADD") == 0)
        {
            token3 = strtok(NULL, "//"); // NAME

            if (token3 == NULL)
                return TYPE_ERROR;

            strcpy(userRequest->name, token3);

            return TYPE_USER_ADD;
        }
        else if (strcmp(function, "DELETE") == 0)
            return TYPE_USER_DELETE;
        else if (strcmp(function, "LOGOUT") == 0)
            return TYPE_LOGOUT;
    }

    // ============ ROOM ============
    if (strcmp(category, "ROOM") == 0)
    {
        token1 = strtok(NULL, "//"); // roomTitle
        token2 = strtok(NULL, "//"); // userId

        if (strcmp(function, "DELETE") == 0)
        {
            if (token1 == NULL)
                return TYPE_ERROR;
            strcpy(roomRequest->title, token1);
            return TYPE_ROOM_DELETE;
        }

        if (token1 == NULL || token2 == NULL)
            return TYPE_ERROR;

        strcpy(roomRequest->title, token1);
        strcpy(userRequest->id, token2);

        if (strcmp(function, "CREATE") == 0)
            return TYPE_ROOM_CREATE;
        else if (strcmp(function, "JOIN") == 0)
            return TYPE_ROOM_JOIN;
        else if (strcmp(function, "LEAVE") == 0)
            return TYPE_ROOM_LEAVE;
    }

    // ============ SCHEDULE ============
    if (strcmp(category, "SCHEDULE") == 0)
    {
        if (strcmp(function, "CREATE") == 0 || strcmp(function, "EDIT") == 0)
        {
            token1 = strtok(NULL, "//"); // roomTitle
            token2 = strtok(NULL, "//"); // writerId
            token3 = strtok(NULL, "//"); // startDate
            token4 = strtok(NULL, "//"); // isAllDay
            token5 = strtok(NULL, "//"); // endDate
            token6 = strtok(NULL, "//"); // startTime
            token7 = strtok(NULL, "//"); // endTime
            token8 = strtok(NULL, "//"); // content

            if (token1 == NULL || token2 == NULL || token3 == NULL || token4 == NULL ||
                token5 == NULL || token6 == NULL || token7 == NULL || token8 == NULL)
                return TYPE_ERROR;

            strcpy(scheduleRequest->roomTitle, token1);
            strcpy(scheduleRequest->writerId, token2);
            strcpy(scheduleRequest->startDate, token3);
            scheduleRequest->isAllDay = atoi(token4);
            strcpy(scheduleRequest->endDate, token5);
            strcpy(scheduleRequest->startTime, token6);
            strcpy(scheduleRequest->endTime, token7);
            token8[strcspn(token8, "\n")] = '\0';
            strcpy(scheduleRequest->content, token8);

            if (strcmp(function, "EDIT") == 0)
            {
                token9 = strtok(NULL, "//"); // index

                if (token9 == NULL)
                    return TYPE_ERROR;

                scheduleRequest->index = atoi(token9);

                return TYPE_SCHEDULE_EDIT;
            }

            return TYPE_SCHEDULE_CREATE;
        }

        if (strcmp(function, "LIST") == 0)
        {
            token1 = strtok(NULL, "//"); // roomTitle
            if (token1 == NULL)
                return TYPE_ERROR;

            strcpy(scheduleRequest->roomTitle, token1);

            return TYPE_SCHEDULE_LIST;
        }
        else if (strcmp(function, "DELETE") == 0)
        {
            token1 = strtok(NULL, "//"); // roomTitle
            token2 = strtok(NULL, "//"); // index

            if (token1 == NULL || token2 == NULL)
                return TYPE_ERROR;

            strcpy(scheduleRequest->roomTitle, token1);
            scheduleRequest->index = atoi(token2);

            return TYPE_SCHEDULE_DELETE;
        }
        else if (strcmp(function, "SEARCH") == 0)
        {
            token1 = strtok(NULL, "//"); // roomTitle
            token2 = strtok(NULL, "//"); // keyword

            if (token1 == NULL || token2 == NULL)
                return TYPE_ERROR;

            strcpy(scheduleRequest->roomTitle, token1);
            strcpy(scheduleRequest->content, token2);

            return TYPE_SCHEDULE_SEARCH;
        }
    }

    return TYPE_ERROR;
}

// 사용자 파일에서 읽기
void loadUser()
{
    char buffer[256];
    char *temp;

    userCount = 0;

    FILE *fp = fopen("users.txt", "r");

    if (fp == NULL)
    {
        return;
    }

    // 한 줄씩 읽고 "//"을 구분자로 ID, PW 파싱
    while (fgets(buffer, sizeof(buffer), fp))
    {
        buffer[strcspn(buffer, "\n")] = '\0';

        temp = strtok(buffer, "//"); // id

        if (temp != NULL)
            strcpy(users[userCount].id, temp);
        else
            continue;

        temp = strtok(NULL, "//"); // pw

        if (temp != NULL)
        {
            strcpy(users[userCount].pw, temp);
        }
        else
            continue;

        temp = strtok(NULL, "//"); // name

        if (temp != NULL)
        {
            strcpy(users[userCount].name, temp);
        }
        else
            continue;

        temp = strtok(NULL, "//"); // isAdmin

        if (temp)
            users[userCount].isAdmin = atoi(temp);
        else
            users[userCount].isAdmin = 0;

        userCount++;

        // 배열 범위 초과 제한
        if (userCount >= MAX_CLIENT)
        {
            break;
        }
    }

    fclose(fp);

    printf("users.txt 파일에서 %d명의 사용자 정보를 로드했습니다.\n", userCount);
}

// 방 파일에서 읽기
void loadRoom()
{
    char buffer[BUF_SIZE];
    char *token;
    int i;

    Room *newRoom;

    FILE *fp = fopen("rooms.txt", "r");

    // 파일 열기 실패
    if (!fp)
        return;

    // 파일에서 한줄 씩 읽기
    while (fgets(buffer, sizeof(buffer), fp))
    {
        buffer[strcspn(buffer, "\n")] = 0;

        newRoom = (Room *)malloc(sizeof(Room));

        if (newRoom == NULL)
            continue;

        token = strtok(buffer, "//");
        if (token == NULL)
            continue;
        strcpy(newRoom->title, token);

        token = strtok(NULL, "//");
        if (token == NULL)
            continue;
        newRoom->memberCount = atoi(token);

        for (i = 0; i < newRoom->memberCount; i++)
        {
            token = strtok(NULL, "//");
            if (token != NULL)
                strcpy(newRoom->membersId[i], token);
        }

        newRoom->next = NULL;

        // 연결리스트에 추가
        if (roomHead == NULL) // 첫 노드 생성
        {
            roomHead = roomTail = newRoom;
        }
        else // 기존 노드 뒤에 추가
        {
            roomTail->next = newRoom;
            roomTail = newRoom;
        }
    }

    fclose(fp);
}

// 일정 파일 읽기
void loadSchedule()
{
    char buffer[BUF_SIZE];
    char *token;
    Schedule *newSchedule;

    FILE *fp = fopen("schedules.txt", "r");

    // 파일 열기 실패
    if (fp == NULL)
        return;

    // 파일에서 한줄 씩 읽기
    while (fgets(buffer, sizeof(buffer), fp))
    {
        buffer[strcspn(buffer, "\n")] = 0; // 개행 문자 제거

        newSchedule = (Schedule *)malloc(sizeof(Schedule)); // 메모리 할당

        if (newSchedule == NULL)
            continue;

        // token: index
        token = strtok(buffer, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        newSchedule->index = atoi(token);
        // token: 방 제목
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->roomTitle, token);

        // token: 작성자 ID
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->writerId, token);

        // token: 시작 날짜
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->startDate, token);

        // token: 종일 일정 여부
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        newSchedule->isAllDay = atoi(token);

        // token: 종료 날짜
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->endDate, token);

        // token: 시작 시간
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->startTime, token);

        // token: 종료 시간
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->endTime, token);

        // token: 일정 내용
        token = strtok(NULL, "//");
        if (token == NULL)
        {
            free(newSchedule);
            continue;
        }
        strcpy(newSchedule->content, token);

        newSchedule->next = NULL;

        // 연결리스트에 추가
        if (scheduleHead == NULL) // 첫 노드 생성
        {
            scheduleHead = scheduleTail = newSchedule;
        }
        else // 기존 노드 뒤에 추가
        {
            scheduleTail->next = newSchedule;
            scheduleTail = newSchedule;
        }
    }

    fclose(fp);
}

// 로그인 검증
void checkLogin(SOCKET clientSock, const char *id, const char *pw)
{
    char buffer[64];
    int i;

    // 저장된 모든 ID, PW와 입력한 ID, PW가 맞는지 검증
    for (i = 0; i < userCount; i++)
    {
        if (strcmp(users[i].id, id) == 0 && strcmp(users[i].pw, pw) == 0)
        {
            sprintf(buffer, "SV//LG//1//%d", users[i].isAdmin); // 사용자인지 관리자인지 확인
            send(clientSock, buffer, strlen(buffer), 0);
            return;
        }
    }

    send(clientSock, "SV//LG//0", strlen("SV//LG//0"), 0);
}

// 회원가입 함수(사용자 전용)
void addUser(SOCKET clientSock, char *id, const char *pw, const char *name)
{
    int i;

    WaitForSingleObject(hMutex, INFINITE);

    // 배열 범위 초과 제한
    if (userCount >= MAX_CLIENT)
    {
        send(clientSock, "SV//USER//ADD//0", strlen("SV//USER//ADD//0"), 0);
        ReleaseMutex(hMutex);
        return;
    }

    // 중복 시 실패
    for (i = 0; i < userCount; i++)
    {
        if (strcmp(users[i].id, id) == 0)
        {
            send(clientSock, "SV//USER//ADD//0", strlen("SV//USER//ADD//0"), 0);
            ReleaseMutex(hMutex);
            return;
        }
    }

    // 사용자 입력 정보 저장(회원가입)
    strcpy(users[userCount].id, id);
    strcpy(users[userCount].pw, pw);
    strcpy(users[userCount].name, name);
    users[userCount].isAdmin = 0; // 회원가입은 사용자만 함
    userCount++;

    saveUser();
    ReleaseMutex(hMutex);

    send(clientSock, "SV//USER//ADD//1", strlen("SV//USER//ADD//1"), 0);
}

// 사용자 목록 불러오는 함수(관리자 전용)
void getUserList(SOCKET clientSock)
{
    char buffer[BUF_SIZE];
    int i;

    // 사용자가 없으면 실패
    if (userCount == 0)
    {
        send(clientSock, "SV//USER//LIST//0", strlen("SV//USER//LIST//0"), 0);
        return;
    }

    // 사용자 정보 한 줄씩 전송
    for (i = 0; i < userCount; i++)
    {
        sprintf(buffer, "%d. ID: %s, 이름: %s, 권한: %s\n", i + 1, users[i].id, users[i].name, users[i].isAdmin ? "관리자" : "사용자");

        send(clientSock, buffer, strlen(buffer), 0);
        Sleep(10);
    }

    send(clientSock, "SV//USER//LIST//1", strlen("SV//USER//LIST//1"), 0);
}

// 사용자 삭제 함수(관리자 전용)
void deleteUser(SOCKET clientSock, const char *id)
{
    int i, j;
    int found = 0;
    int adminCount = 0;

    WaitForSingleObject(hMutex, INFINITE);

    // 관리자 수 확인
    for (int i = 0; i < userCount; i++)
    {
        if (users[i].isAdmin)
            adminCount++;
    }

    for (i = 0; i < userCount; i++)
    {
        if (strcmp(users[i].id, id) == 0)
        {
            // 마지막 관리자 삭제 금지
            if (adminCount <= 1 && users[i].isAdmin == 1)
            {
                send(clientSock, "SV//USER//ERROR//LAST_ADMIN", strlen("SV//USER//ERROR//LAST_ADMIN"), 0);
                ReleaseMutex(hMutex);
                return;
            }

            // 해당 사용자 삭제 (앞으로 한 칸씩 당김)
            for (j = i; j < userCount - 1; j++)
            {
                users[j] = users[j + 1];
            }
            userCount--;
            found = 1;

            break;
        }
    }

    saveUser();
    ReleaseMutex(hMutex);

    // found가 1이면 성공, 0이면 실패
    if (found == 0)
        send(clientSock, "SV//USER//DELETE//0", strlen("SV//USER//DELETE//0"), 0);
    else
        send(clientSock, "SV//USER//DELETE//1", strlen("SV//USER//DELETE//1"), 0);
}

// 방 목록 보기(메인메뉴)
void getRoomList(SOCKET clientSock, const char *id)
{
    Room *cur = roomHead;
    char buffer[BUF_SIZE];
    int isJoin = 0, i, j = 1;

    if (cur == NULL)
    {
        send(clientSock, "SV//ROOM//LIST//0", strlen("SV//ROOM//LIST//0"), 0);
        return;
    }

    // 참여중 여부 판단
    while (cur != NULL)
    {
        isJoin = 0; // 초기화

        for (i = 0; i < cur->memberCount; i++)
        {
            if (strcmp(cur->membersId[i], id) == 0) // membersId에 id 존재하면 참여중
            {
                isJoin = 1;
                break;
            }
        }

        if (isJoin == 1) // 참여
            sprintf(buffer, "%d. %s 방 %d명 [참여중]\n", j++, cur->title, cur->memberCount);
        else // 미참여
            sprintf(buffer, "%d. %s 방 %d명\n", j++, cur->title, cur->memberCount);

        send(clientSock, buffer, strlen(buffer), 0);
        Sleep(10);

        cur = cur->next; // 다음 방으로 이동
    }

    send(clientSock, "SV//ROOM//LIST//1", strlen("SV//ROOM//LIST//1"), 0);
}

// 방 생성
void createRoom(SOCKET clientSock, const char *title, const char *creatorId)
{
    Room *cur = roomHead;

    WaitForSingleObject(hMutex, INFINITE);

    // 중복 방 제목 확인
    while (cur != NULL)
    {
        if (strcmp(cur->title, title) == 0)
        {
            send(clientSock, "SV//ROOM//CREATE//0", strlen("SV//ROOM//CREATE//0"), 0); // 실패
            ReleaseMutex(hMutex);
            return;
        }
        cur = cur->next; // 다음 방으로 이동
    }

    // 새 방 생성
    Room *newRoom = (Room *)malloc(sizeof(Room));

    if (newRoom == NULL)
    {
        send(clientSock, "SV//ROOM//CREATE//0", strlen("SV//ROOM//CREATE//0"), 0); // 실패
        ReleaseMutex(hMutex);
        return;
    }

    // newRoom 초기화
    strcpy(newRoom->title, title);
    newRoom->memberCount = 1;
    strcpy(newRoom->membersId[0], creatorId);
    newRoom->next = NULL;

    // 연결리스트에 추가
    if (roomHead == NULL) // 첫 노드 생성
    {
        roomHead = roomTail = newRoom;
    }
    else // 기존 노드 뒤에 추가
    {
        roomTail->next = newRoom;
        roomTail = newRoom;
    }

    saveRoom();
    ReleaseMutex(hMutex);
    send(clientSock, "SV//ROOM//CREATE//1", strlen("SV//ROOM//CREATE//1"), 0); // 성공
}

// 방 참가
void joinRoom(SOCKET clientSock, const char *roomTitle, const char *id)
{
    Room *cur = roomHead;
    int join = 0;
    int i;

    WaitForSingleObject(hMutex, INFINITE);

    // 방 탐색 후 참가
    while (cur != NULL)
    {
        if (strcmp(cur->title, roomTitle) == 0)
        {
            // 인원 초과 확인
            if (cur->memberCount >= MAX_ROOM_MEMBER)
            {
                send(clientSock, "SV//ROOM//JOIN//0", strlen("SV//ROOM//JOIN//0"), 0);
                return;
            }

            // 이미 참여중인 방인지 확인
            for (i = 0; i < cur->memberCount; i++)
            {
                if (strcmp(cur->membersId[i], id) == 0)
                {
                    send(clientSock, "SV//ROOM//JOIN//0", strlen("SV//ROOM//JOIN//0"), 0);
                    return;
                }
            }

            // 참가 처리
            strcpy(cur->membersId[cur->memberCount], id);
            cur->memberCount++;
            join = 1;
            break;
        }

        cur = cur->next; // 다음 방으로 이동
    }

    if (join == 1) // 성공
    {
        saveRoom();
        send(clientSock, "SV//ROOM//JOIN//1", strlen("SV//ROOM//JOIN//1"), 0);
    }
    else // 실패
    {
        send(clientSock, "SV//ROOM//JOIN//0", strlen("SV//ROOM//JOIN//0"), 0);
    }
    ReleaseMutex(hMutex);
}

// 방 퇴장
void leaveRoom(SOCKET clientSock, const char *roomTitle, const char *userId)
{
    Room *cur = roomHead;
    Room *curPrev = NULL;
    int found = 0;
    int i, j;

    WaitForSingleObject(hMutex, INFINITE);

    // 해당 방 탐색
    while (cur != NULL)
    {
        if (strcmp(cur->title, roomTitle) == 0)
            break;

        curPrev = cur;
        cur = cur->next;
    }

    if (cur == NULL)
    {
        send(clientSock, "SV//ROOM//LEAVE//0", strlen("SV//ROOM//LEAVE//0"), 0);
        return;
    }

    // 멤버 배열에서 userId 삭제
    for (i = 0; i < cur->memberCount; i++)
    {
        if (strcmp(cur->membersId[i], userId) == 0)
        {
            for (j = i; j < cur->memberCount - 1; j++)
            {
                strcpy(cur->membersId[j], cur->membersId[j + 1]);
            }
            cur->memberCount--;

            // 방이 비었으면 자동 삭제
            if (cur->memberCount == 0)
                deleteEmptyRoom(cur, curPrev);

            found = 1;
            break;
        }
    }

    if (found == 1)
    {
        saveRoom();
        send(clientSock, "SV//ROOM//LEAVE//1", strlen("SV//ROOM//LEAVE//1"), 0);
    }
    else
    {
        send(clientSock, "SV//ROOM//LEAVE//0", strlen("SV//ROOM//LEAVE//0"), 0);
    }
    ReleaseMutex(hMutex);
}

// 방 자동 삭제
void deleteEmptyRoom(Room *cur, Room *curPrev)
{
    if (cur == NULL)
        return;

    if (cur == roomHead)
        roomHead = cur->next;
    else if (curPrev != NULL)
        curPrev->next = cur->next;

    if (cur == roomTail)
        roomTail = curPrev;

    free(cur);
    saveRoom();
}

// 방 삭제(관리자 전용)
void deleteRoom(SOCKET clientSock, const char *roomTitle, const char *id)
{
    Room *cur = roomHead;
    Room *prev = NULL;
    int found = 0, permit = 0;
    int i;

    WaitForSingleObject(hMutex, INFINITE);

    // 관리자 여부 판단
    for (i = 0; i < userCount; i++)
    {
        if ((strcmp(users[i].id, id) == 0) && (users[i].isAdmin == 1))
        {
            permit = 1;
            break;
        }
    }

    // 관리자가 아니면 실패
    if (permit != 1)
    {
        send(clientSock, "SV//ROOM//DELETE//0", strlen("SV//ROOM//DELETE//0"), 0);
        return;
    }

    // 방 탐색 후 삭제
    while (cur != NULL)
    {
        if (strcmp(cur->title, roomTitle) == 0)
        {
            // 연결 리스트에서 제거
            if (prev == NULL) // 첫 노드일 경우
                roomHead = cur->next;
            else
                prev->next = cur->next;

            if (cur == roomTail)
                roomTail = prev;

            found = 1;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    if (found == 1) // 성공
    {
        saveRoom();
        send(clientSock, "SV//ROOM//DELETE//1", strlen("SV//ROOM//DELETE//1"), 0);
    }
    else // 실패
    {
        send(clientSock, "SV//ROOM//DELETE//0", strlen("SV//ROOM//DELETE//0"), 0);
    }

    ReleaseMutex(hMutex);
}

// 일정 추가
void createSchedule(SOCKET clientSock, Schedule *newSchedule)
{
    if (newSchedule == NULL)
    {
        send(clientSock, "SV//SCHEDULE//CREATE//0", strlen("SV//SCHEDULE//CREATE//0"), 0); // 실패
        return;
    }

    WaitForSingleObject(hMutex, INFINITE);

    newSchedule->next = NULL;

    if (scheduleHead == NULL)
    {
        scheduleHead = scheduleTail = newSchedule;
    }
    else
    {
        scheduleTail->next = newSchedule;
        scheduleTail = newSchedule;
    }
    newSchedule->index = scheduleIdCounter++;

    saveSchedule();
    ReleaseMutex(hMutex);

    send(clientSock, "SV//SCHEDULE//CREATE//1", strlen("SV//SCHEDULE//CREATE//1"), 0); // 성공
}

// 일정 날짜순 정렬
int compareSchedule(const void *a, const void *b)
{
    Schedule *s1 = *(Schedule **)a;
    Schedule *s2 = *(Schedule **)b;

    int cmp = strcmp(s1->startDate, s2->startDate);
    if (cmp != 0)
        return cmp;

    return strcmp(s1->startTime, s2->startTime);
}

// ID로부터 name 얻기
const char *getUserNameById(const char *id)
{
    for (int i = 0; i < userCount; i++)
    {
        if (strcmp(users[i].id, id) == 0)
            return users[i].name;
    }
    return "Unknown";
}

// 일정 조회
void searchSchedule(SOCKET clientSock, const char *roomTitle, const char *keyword)
{
    Schedule *cur = scheduleHead;
    Schedule *list[1000];
    const char *writerName;
    char buffer[BUF_SIZE];
    int count = 0, i;

    // 검색 조건에 맞는 일정들을 배열에 저장
    while (cur != NULL)
    {
        if (strcmp(cur->roomTitle, roomTitle) == 0 && strstr(cur->content, keyword) != NULL)
        {
            list[count++] = cur;
        }
        cur = cur->next;
    }

    // 검색 결과 없음
    if (count == 0)
    {
        send(clientSock, "SV//SCHEDULE//SEARCH//0", strlen("SV//SCHEDULE//SEARCH//0"), 0);
        return;
    }

    // 정렬
    qsort(list, count, sizeof(Schedule *), compareSchedule);

    // 정렬된 결과 출력
    for (i = 0; i < count; i++)
    {
        writerName = getUserNameById(list[i]->writerId);

        if (list[i]->isAllDay)
        {
            sprintf(buffer, "%d. [%s] %s (하루 종일) - %s\n",
                    i + 1, list[i]->startDate, list[i]->content, writerName);
        }
        else
        {
            sprintf(buffer, "%d. [%s %s ~ %s %s] %s - %s\n",
                    i + 1, list[i]->startDate, list[i]->startTime, list[i]->endDate, list[i]->endTime,
                    list[i]->content, writerName);
        }

        send(clientSock, buffer, strlen(buffer), 0);
        Sleep(10);
    }

    send(clientSock, "SV//SCHEDULE//SEARCH//1", strlen("SV//SCHEDULE//SEARCH//1"), 0);
}

// 일정 목록 보기
void getScheduleList(SOCKET clientSock, const char *roomTitle)
{
    Schedule *cur = scheduleHead;
    Schedule *list[1000];
    char buffer[BUF_SIZE];
    int count = 0, i;

    // 해당 방 일정만 배열에 저장
    while (cur != NULL)
    {
        if (strcmp(cur->roomTitle, roomTitle) == 0)
        {
            list[count++] = cur;
        }
        cur = cur->next; // 다음 일정으로 이동
    }

    if (count == 0)
    {
        send(clientSock, "SV//SCHEDULE//LIST//0", strlen("SV//SCHEDULE//LIST//0"), 0);
        return;
    }

    // 정렬
    qsort(list, count, sizeof(Schedule *), compareSchedule);

    // 전송
    for (i = 0; i < count; i++)
    {
        const char *writerName = getUserNameById(list[i]->writerId);
        if (list[i]->isAllDay)
        {
            sprintf(buffer, "%d. [%s] %s (하루 종일) - %s\n",
                    i + 1, list[i]->startDate, list[i]->content, writerName);
        }
        else
        {
            sprintf(buffer, "%d. [%s %s ~ %s %s] %s - %s\n",
                    i + 1, list[i]->startDate, list[i]->startTime, list[i]->endDate, list[i]->endTime, list[i]->content, writerName);
        }

        send(clientSock, buffer, strlen(buffer), 0);
        Sleep(10);
    }

    send(clientSock, "SV//SCHEDULE//LIST//1", strlen("SV//SCHEDULE//LIST//1"), 0);
}

// 일정 수정
void editSchedule(SOCKET clientSock, const char *roomTitle, int index, Schedule *edit)
{
    Schedule *cur = scheduleHead;
    int found = 0;

    WaitForSingleObject(hMutex, INFINITE);

    while (cur != NULL)
    {
        if (strcmp(cur->roomTitle, roomTitle) == 0)
        {
            if (cur->index == index)
            {
                // 일정 수정
                strcpy(cur->writerId, edit->writerId);
                strcpy(cur->startDate, edit->startDate);
                cur->isAllDay = edit->isAllDay;

                if (cur->isAllDay == 0)
                {
                    strcpy(cur->endDate, edit->endDate);
                    strcpy(cur->startTime, edit->startTime);
                    strcpy(cur->endTime, edit->endTime);
                }
                strcpy(cur->content, edit->content);

                found = 1;
                break;
            }
        }
        cur = cur->next;
    }

    if (found == 1)
    {
        saveSchedule();
        send(clientSock, "SV//SCHEDULE//EDIT//1", strlen("SV//SCHEDULE//EDIT//1"), 0);
    }
    else
    {
        send(clientSock, "SV//SCHEDULE//EDIT//0", strlen("SV//SCHEDULE//EDIT//0"), 0);
    }
    ReleaseMutex(hMutex);
}

// 일정 삭제
void deleteSchedule(SOCKET clientSock, int index)
{
    Schedule *cur = scheduleHead;
    Schedule *prev = NULL;
    int found = 0;

    WaitForSingleObject(hMutex, INFINITE);

    while (cur != NULL)
    {
        if (cur->index == index)
        {
            if (prev == NULL)
                scheduleHead = cur->next;
            else
                prev->next = cur->next;

            if (cur == scheduleTail)
                scheduleTail = prev;

            free(cur);
            found = 1;
            break;
        }

        prev = cur;
        cur = cur->next;
    }

    if (found)
    {
        saveSchedule();
        send(clientSock, "SV//SCHEDULE//DELETE//1", strlen("SV//SCHEDULE//DELETE//1"), 0);
    }
    else
    {
        send(clientSock, "SV//SCHEDULE//DELETE//0", strlen("SV//SCHEDULE//DELETE//0"), 0);
    }

    ReleaseMutex(hMutex);
}

// 사용자 정보 저장 함수
void saveUser()
{
    int i;

    FILE *fp = fopen("users.txt", "w"); // 파일 쓰기 모드

    if (fp == NULL) // 파일 열기 실패
    {
        return;
    }

    // 사용자 정보 한 줄씩 저장
    for (i = 0; i < userCount; i++)
    {
        fprintf(fp, "%s//%s//%s//%d\n", users[i].id, users[i].pw, users[i].name, users[i].isAdmin);
    }

    fclose(fp);
}

// 방 정보 저장 함수
void saveRoom()
{
    Room *cur = roomHead;

    FILE *fp = fopen("rooms.txt", "w");

    // 파일 열기 실패
    if (!fp)
        return;

    // 파일에 한 줄 씩 저장
    while (cur != NULL)
    {
        fprintf(fp, "%s//%d", cur->title, cur->memberCount);

        for (int i = 0; i < cur->memberCount; i++)
        {
            fprintf(fp, "//%s", cur->membersId[i]);
        }
        fprintf(fp, "\n");

        cur = cur->next; // 다음 방으로 이동
    }

    fclose(fp);
}

// 일정 정보 저장 함수
void saveSchedule()
{
    Schedule *cur = scheduleHead;

    FILE *fp = fopen("schedules.txt", "w");

    // 파일 열기 실패
    if (!fp)
        return;

    // 파일에 한 줄 씩 저장
    while (cur != NULL)
    {
        fprintf(fp, "%d//%s//%s//%s//%d", cur->index, cur->roomTitle, cur->writerId, cur->startDate, cur->isAllDay);

        // 종일 일정이라면 시작 시간, 종료 날짜 및 시간 필요없음
        if (cur->isAllDay == 1)
        {
            fprintf(fp, "//PASS//PASS//PASS//%s\n", cur->content);

            cur = cur->next; // 다음 방으로 이동
            continue;
        }

        fprintf(fp, "//%s//%s//%s//%s\n", cur->endDate, cur->startTime, cur->endTime, cur->content);

        cur = cur->next; // 다음 방으로 이동
    }

    fclose(fp);
}

// 에러 처리
void ErrorHandling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}