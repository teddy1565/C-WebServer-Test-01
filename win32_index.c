#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <Winsock2.h>
#include <process.h>
#include <windows.h>
#ifdef _WIN32
    #include <direct.h>
    #define PATH_SEP '\\'
    #define GETCWD _getcwd
    #define CHDIR _chdir
#else
    #include <unistd.h>
    #define PATH_SEP '/'
    #define GETCWD getcwd
    #define CHDIR chdir
#endif

#define BUFSIZE 8096

struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"exe","text/plain" },
    {0,0}
};

void handle_socket(int fd) {
    int j,file_fd,buflen,len;
    long i, ret;
    char *fstr;
    static char buffer[BUFSIZE+1];

    ret = read(fd,buffer,BUFSIZE);/*讀取瀏覽器請求*/
    if (ret == 0 || ret == -1) {
        /* 網路連線有問題，結束Process*/
        exit(3);
    }

    if (ret>0 && ret<BUFSIZE) {
        /*再讀取到的字串結尾補空字元，方便後續程式判斷結尾*/
        buffer[ret] = 0;
    } else {
        buffer[0] = 0;
    }

    for (i=0;i<ret;i++) {
        /*移除換行字元*/
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            buffer[i] = '0';
        }
    }

    if (strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4)) {
        /*只接受GET請求*/
        exit(3);
    }

    /*將GET /index.html HTTP/1.0 後面的 HTTP/1.0 用空字元隔開*/
    for (i=4;i<BUFSIZE;i++) {
        if (buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    /*阻止越級路徑請求*/
    for (j=0;j<i-1;j++) {
        if (buffer[j] == '.' && buffer[j+1] == '.') {
            exit(3);
        }
    }
    
    if (!strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6)) {
        /*當客戶端請求跟目錄時讀取index.html*/
        strcpy(buffer,"GET /index.html\0");
    }

    /*檢查客戶端所要求的檔案格式*/
    buflen = strlen(buffer);
    fstr = (char *)0;
    
    for (i=0;extensions[i].ext!=0;i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen-len],extensions[i].ext,len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }

    if (fstr == 0) {
        /*檔案格式不支援*/
        fstr = extensions[i-1].filetype;
    }

    if (file_fd = open(&buffer[5],O_RDONLY)==-1) {
        /*開啟檔案*/
        write(fd,"Failed to open file",19);
    }
    /*傳回Http code 200和內容的格式*/
    sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type:%s\r\n\r\n",fstr);
    write(fd,buffer,strlen(buffer));

    while ((ret=read(file_fd,buffer,BUFSIZE))>0) {
        /*讀取檔案內容輸出到Brower*/
        write(fd,buffer,ret);
    }
    exit(1);

}

int main (int argc,char **argv) {
    int i,pid,listenfd,socketfd;
    size_t length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;
    
    if (_chdir(argv[1]) == -1) {
        /*使用/tmp當作網站根目錄*/
        printf("ERROR: Can't Change to directory %s\n",argv[1]);
        exit(4);
    }
    
    if (_beginthread(NULL,0,NULL) != 0) {
        /*背景持續執行*/
        printf("Background Running\n");
        return 0;
    }
    
    
    /*讓父行程不必等待子行程結束*/
    // signal(SIGCLD,SIG_IGN); //Can't on windows
    //signal(JOB_OBJECT_MSG_EXIT_PROCESS,SIG_IGN);
    signal(JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS,SIG_IGN);

    /*開啟網路socket*/
    if ((listenfd=socket(AF_INET,SOCK_STREAM,0)) < 0) {
        
        printf("Can't Open Network Socket\n");
        exit(3);
    }

    /*網路連線設定*/
    serv_addr.sin_family = AF_INET;
    /*使用任何在本機的對外IP*/
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*使用 3000 port*/
    serv_addr.sin_port = htons(3000);

    /*開啟網路監聽器*/
    if (bind(listenfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        printf("Can't Network Listener\n");
        exit(3);
    }

    /*開始監聽網路*/
    if (listen(listenfd,64) < 0) {
        printf("Can't Linsten Network\n");
        exit(3);
    }
    while(1) {
        
        length = sizeof(cli_addr);
        
        /* 等待客戶端連線 */
        if ((socketfd = accept(listenfd,(struct sockaddr*)&cli_addr, &length)) < 0) {
            printf("Wait for client connect\n");
            exit(3);
        }

        /*分出子行程處理請求*/
        if ((pid = _beginthread(NULL,0,NULL)) < 0) {    
            printf("Child Process Error!\n");
            exit(3);
        } else {
            if (pid == 0) {
                /*child process*/
                printf("Child Process Start Running\n");
                close(listenfd);
                handle_socket(socketfd);
            } else {
                /*parent process*/
                printf("Parent Process Start Running\n");
                close(socketfd);
            }
        }
    }
    return 0;
}