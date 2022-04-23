#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#define BUF_SZ 256
#define MAX_PIPE 512
#define MAX_PATH 1050
#define MAX_FILENAME 256
#define MAX_LINE 1050       /* max line size */
#define PARM_DELIM " \t\n"  /* delim used for input parsing*/
#define MAX_BKGD_PROCESS 64 /*maximum number of background process */
#define MAXJOBS 16     /* max jobs at any point in time */

#define Q_REPLACER 20       // Device Control 4
#define TRUE 1
#define FALSE 0
#define PARENT_NORMAL 1
#define PARENT_EXIT   2
#define CHILD_NORMAL  3
//#define DEBUG 1

#define PROCESS_DONE     1
#define PROCESS_RUNNING  2

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */


const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_CD = "cd";
const char* COMMAND_JOBS ="jobs";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";
const char* COMMAND_OUT_ADD = ">>";
const char* COMMAND_PIPE = "|";

//internal state number
enum {
    RESULT_NORMAL,
    ERROR_FORK,
    ERROR_COMMAND,
    ERROR_WRONG_PARAMETER,
    ERROR_MISS_PARAMETER,
    ERROR_MISS_PROGRAM,
    ERROR_TOO_MANY_ARGUMENTS,
    ERROR_SYNTAX,
    ERROR_CD,
    ERROR_SYSTEM,
    ERROR_EXIT,

    /* redirection errors */
    ERROR_MANY_IN,
    ERROR_MANY_OUT,
    ERROR_MANY_ADD,
    ERROR_FILE_NOT_EXIST,

    /* pipe errors */
    ERROR_PIPE,
    ERROR_PIPE_MISS_PARAMETER
};


typedef struct redir_t{     
    int in;         
    int out;   
    int add;  
    int desIn;// file descriptor
    int desOut;  // file descriptor  
    char inFileName[MAX_FILENAME];   
    char outFileName[MAX_FILENAME];  
}redir_t;




const char *homedir;
//char username[BUF_SZ];
//char hostname[BUF_SZ];
char curPath[BUF_SZ];
char commands[BUF_SZ][BUF_SZ];
int commandNum;
char lastDir[MAX_PATH];                // last directory
char lastPendingDir[MAX_PATH];         // last pending directory
int firstCommandParamPos=0;
int nonExistFilePos;
char errorSyntaxChar;
int currStatus;    
int result;

int isBackground; 
char backGroundCom[MAX_BKGD_PROCESS][MAX_LINE];    // background process command 
int backGroundNum;
int backGroundJob[2*MAX_BKGD_PROCESS];
int lastPid[MAX_PIPE];
char inFileName[MAX_FILENAME];   // input file name related with redirection
char outFileName[MAX_FILENAME];  // output file name related with redirection
int fdStdIn;        // file descriptor for stdin
int fdStdOut;       // file descriptor for stdout
char conjLine[MAX_LINE];     // recombinated line

void prompt(const char *message);
void programInit();
void promptExit();
void debugPrint(const char *message);
void debugPrint_ascii(const char *message);
void debugPrint_int(const int message);
void debugPrint_ul(const unsigned long message);
void debugPrintRedir(redir_t* redi);
void debugPrintCommands(int num);
int preProcessInput(char *argv);
int checkEmptyLine(char *argv);
int isCommandExist(const char* command);
//void getUsername();
//void getHostname();
int getCurWorkDir();
//int getBackGround(char* argv);
int dealWithBackGround(char* argv,unsigned long AndSignPos);
int parsingQuotation(char* argv,char specialList[MAX_LINE],int *specialCnt);
int splitCommands(char command[MAX_LINE],int * commandNum,redir_t * redi);
int getRediInfo(redir_t *redi,int commandNum);
int checkIOError(int ioErrorFlag);
int parsingPipe(int commandNum,int *cmdHeadDict,int *pipeCnt);
int recreateSpecialChar(int commandNum,char *specialList,int specialCnt);
int creatingPipe(int *pipeFd,int pipeCnt);



int callExit();
int callPwd();
int callCommand(int commandNum);
int callCommandWithPipe(int left, int right);
int callCommandWithRedi(int left, int right);
int callBuiltIn(int commandNum,int cmdHead,int cmdOffset);
int callJobs();
int exeCommand(int index,int cmdCnt,int cmdOffset,int cmdHead,int pipeCnt,int* pipeFd,redir_t *redi);
int connectChildPipeFd(int index,int cmdCnt,int pipeCnt,int* pipeFd);
int checkRedi(int index ,int cmdCnt, redir_t* redi);
int deleteRediSynAndExe(int left,int right);
void handleErrorCd(int result);
void handleErrorOther(int result);

struct sigaction old_action;
struct sigaction action;


void sigint_handler()
{
    sigaction(SIGINT, &old_action, NULL);
    debugPrint("debug:sending Ctrl-C\n");
    if(currStatus == PARENT_NORMAL) {
        debugPrint("current Status: Parent Normal.\n");
        currStatus = PARENT_EXIT;
    }
    else if(currStatus == CHILD_NORMAL){
        debugPrint("current Status: Child Normal.\n");
        exit(0);
    }
}

int main() {
    action.sa_handler = &sigint_handler;

    memset(lastDir,0,MAX_PATH);
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    
    memset(lastPendingDir,0,MAX_PATH);
    char tmpcwd[MAX_PATH];
    if(getcwd(tmpcwd, sizeof(tmpcwd)) != NULL){
        debugPrint("debug: pwd, copying current directory.\n");
        strcpy(lastPendingDir,tmpcwd);
    }else{ strcpy(lastPendingDir,homedir);}
    /* start the shell */
    char argv[MAX_LINE];
    
    while (TRUE) {
        sigaction(SIGINT, &action, &old_action);
        currStatus = PARENT_NORMAL;

        prompt("mumsh $ ");
        /* get the command user entered */
        //fgets(argv, MAX_LINE, stdin);
        //unsigned long len = strlen(argv);
        redir_t rediInfo;
        rediInfo.in=0;rediInfo.out=0;rediInfo.add=0;
        memset(rediInfo.inFileName,'\0',sizeof(rediInfo.inFileName));
        memset(rediInfo.outFileName,'\0',sizeof(rediInfo.outFileName));
        programInit();
        
        int fgetsErrorFlag=preProcessInput(argv);//if >,<,| ," but missing arguments, we print >  and go on input
        /**preProcessInput() :
        output the pre-processed input line, 
        quotations, 
        missing files output > 
        conjLine is  the pre-processed input line, */
        
        if(fgetsErrorFlag==1 || currStatus==PARENT_EXIT){
            debugPrint("debug:fgetsError.\n");
            promptExit();//should check this
            /*prompt("exit\n");
            result = callExit();
            if (ERROR_EXIT == result) {
                exit(-1);
            }*/            
            continue;
        }
        fflush(stdin);

        int isEmptyLine=checkEmptyLine(conjLine);
        if(isEmptyLine){
            debugPrint("debug:you entered empty line.\n");
            promptExit();//should check this
            /*prompt("exit\n");
            result = callExit();
            if (ERROR_EXIT == result) {
                exit(-1);
            }*/ 
            continue;
        }

        memset(commands,'\0',sizeof(commands));
        debugPrint("debug:you input:");debugPrint(conjLine);debugPrint("\n");
        char specialList[MAX_LINE];
        int specialCnt=0;
        /*int bg=*/ //getBackGround(conjLine);
        parsingQuotation(conjLine,specialList,&specialCnt);
        debugPrint("debug:after parsingQuotation:");debugPrint(conjLine);debugPrint("\n");
        debugPrint("debug:after parsingQuotation:ASCII:");debugPrint_ascii(conjLine);debugPrint("\n");
        int commandNum=0;
        
        int ioErrorFlag = splitCommands(conjLine,&commandNum,&rediInfo);
        ioErrorFlag=getRediInfo(&rediInfo,commandNum);
        //inFileName,outFileName,ioErrorFlag
        ioErrorFlag=checkIOError(ioErrorFlag);
        if(ioErrorFlag){
            //for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
            //free(mArgv);
            promptExit();
            continue;
        }
        int pipeCnt=0; // number of pipes in the line
        int cmdHeadDict[MAX_PIPE]; // location dictionary of command heads
        int cmdCnt=parsingPipe(commandNum,cmdHeadDict,&pipeCnt);
        recreateSpecialChar(commandNum,specialList,specialCnt);
        debugPrint("debug:after recreating quot:");debugPrintCommands(commandNum);debugPrint("\n");
        //int pipeFd[pipeCnt*2+2];
        int * pipeFd = (int *)malloc(sizeof(int)*(unsigned long)(pipeCnt*2+2+5));
        //creatingPipe(pipeFd,pipeCnt);
        for(int i=0;i<pipeCnt;i++){
        if(pipe(pipeFd + i*2) < 0){
            debugPrint("debug: Error: pipe failure.\n");
            //for(int i=0;i<commandNum;i++) if(mArgv[i]!=NULL) free(commands[i]);
            //free(commands);
            promptExit();
            //for(int i=0;i<backGroundNum;i++) free(backGroundCom[i]);
            //if(lastDir!=NULL) free(lastDir);
            exit(0);
            }debugPrint("debug: pipeFd");debugPrint_int(i*2);debugPrint(" = ");debugPrint_int(pipeFd[i*2]);debugPrint("\n");
        }
        //the following is executing the commands
        int childStatus;
        for(int index=0;index<cmdCnt;index++){
            int cmdHead = cmdHeadDict[index];
            int cmdOffset = cmdHeadDict[index+1] - cmdHead - 1;
            if (commandNum != 0 && ioErrorFlag==0) { // user has input the command//and no io error
                if (strcmp(commands[cmdHead], COMMAND_EXIT) == 0) { // exit command
                    debugPrint("debug: we sense that command=exit\n");
                    prompt("exit\n");
                    result = callExit();
                    if (ERROR_EXIT == result) {
                        exit(-1);
                    }
                }else if (strcmp(commands[cmdHead], COMMAND_CD) == 0) { // call built-in commands like cd
                    result = callBuiltIn(commandNum,cmdHead,cmdOffset);
                    //handleErrorCd(result);
                }else if (strcmp(commands[cmdHead], COMMAND_JOBS) == 0) {
                    //TODO: callJobs()
                    callJobs();
                } else { // other commands
                    result=exeCommand(index,cmdCnt,cmdOffset,cmdHead,pipeCnt,pipeFd,&rediInfo);
                    //result = callCommand(commandNum);
                    debugPrint("debug: return result is: ");debugPrint_int(result);debugPrint("\n");
                    //for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
                    //free(mArgv);
                    promptExit();
                    //handleErrorOther(result);
                }
            }
        }           
        for(int i=0;i<2*pipeCnt;i++){
            close(pipeFd[i]); // parent closing pipes
        }
        //free(pipeFd);
        if(isBackground == 0){
            for(int i=0;i<pipeCnt+1;i++){ // parent waiting for child process
                // wait(&childStatus);
                // char tmpMsg[108];
                // sprintf(tmpMsg,"Child process status: %d\n",childStatus);
                // debugMsg(tmpMsg);
                waitpid(lastPid[i],NULL,WUNTRACED);
            }
        }else if(isBackground == 1){ // The process is running in the background
            waitpid(backGroundJob[(backGroundNum-1)*2],&childStatus,WNOHANG);
        }
        free(pipeFd);
        //fflush(stdout);
        //fflush(stdout);
    }
}

void prompt(const char *message){
    printf("%s", message);
    fflush(stdout);
}

int preProcessInput(char* argv){
    int isInputNotEnd = 0, isFirstfgets = 1;
    int isSinQuoNotClosed = 0, isDouQuoNotClosed = 0;
    int fgetsErrorFlag = 0;
    while(isInputNotEnd || isFirstfgets){
        isInputNotEnd = 0;
        if(isFirstfgets){isFirstfgets = 0;}
        else {prompt("> ");}

        if(fgets(argv, MAX_LINE, stdin) == NULL){//interrupt
            debugPrint("debug: fgets quit.\n");
            /*fflush(stdout);
            prompt("exit\n");
            result = callExit();
            if (ERROR_EXIT == result) {
                exit(-1);
            }
            exit(0);*/
            // if(feof(stdin)) {fgetsErrorFlag=1; break;}
            if(errno == EINTR) {errno=0; fgetsErrorFlag=1; break;} // fgets meet SIGINT
            prompt("exit\n");
            promptExit();
            result = callExit();//added
            if (ERROR_EXIT == result) {//added
                exit(-1);
            }//added
            //free(line);
            //free(conjLine);
            
            //for(int i=0;i<bgCnt;i++) free(bgCommand[i]);
            //if(lastDir!=NULL) free(lastDir);
            //free(lastPendingDir);
            exit(0);
        }
        unsigned long lineLen = strlen(argv);
        /* check incomplete quotation mark and update the flag */
        for(unsigned long i=0;i<lineLen;i++){
            if(argv[i]=='\''){
                if(!isSinQuoNotClosed && !isDouQuoNotClosed) isSinQuoNotClosed = 1;
                else if(isSinQuoNotClosed) isSinQuoNotClosed = 0;
            }
            else if(argv[i]=='\"'){
                if(!isSinQuoNotClosed && !isDouQuoNotClosed) isDouQuoNotClosed = 1;
                else if(isDouQuoNotClosed && (i==0 || (i>0 && argv[i-1]!='\\'))) isDouQuoNotClosed = 0;
            }
        }
        if(isSinQuoNotClosed || isDouQuoNotClosed){
            strcat(conjLine, argv);
            isInputNotEnd = 1;
            continue;
        }
        if(lineLen>1) for(unsigned long i=lineLen-2;i>=0;i--){ // assume that the last character is newline
            if(argv[i]==' ') continue;
            else if(argv[i]=='>' || argv[i]=='<' || argv[i]=='|'){
                isInputNotEnd = 1;
                break;
            }
            else break;
        }
        if(isInputNotEnd){
            argv[strlen(argv)-1] = ' '; // replace newline with blank
            strcat(conjLine, argv);
            /* check for syntax error */
            for(unsigned long k=strlen(conjLine)-1;k>0;k--){
                int inBranch = 0;
                if(conjLine[k]=='>' || conjLine[k]=='<' || conjLine[k]=='|'){
                    if(k<=0){break;}
                    inBranch = 1;
                    char tk = conjLine[k];
                    k--;
                    if(k<=0){break;}
                    while(k>0 && (conjLine[k]==' ' || conjLine[k]=='\n')){ k--;}
                    if(conjLine[k]=='>' || conjLine[k]=='<'){
                        char tmpMsg[MAX_LINE];
                        sprintf(tmpMsg,"syntax error near unexpected token `%c\'\n",tk);
                        prompt(tmpMsg);//error message
                        fgetsErrorFlag=1;
                    }
                }
                if(inBranch) break;
            }
            if(fgetsErrorFlag) break;
            continue;
        }
        /* concatenation */
        strcat(conjLine, argv);
        isInputNotEnd = 0; // break;
    }
    return fgetsErrorFlag;
}

int checkEmptyLine(char* argv){
    int isEmptyLine=1;
    for(unsigned int i=0;i<strlen(argv);i++){
        if(argv[i]!=32 && argv[i]!=10) isEmptyLine=0;
    }
    return isEmptyLine;
}

int isCommandExist(const char* command) { // to judge whether the command exist
    if (command == NULL || strlen(command) == 0) return FALSE;

    int result = TRUE;

    int fds[2];
    if (pipe(fds) == -1) {
        result = FALSE;
    } else {
        /* temporarily save the input / output redirection symbol */
        int inFd = dup(STDIN_FILENO);
        int outFd = dup(STDOUT_FILENO);

        pid_t pid = vfork();
        if (pid == -1) {
            result = FALSE;
        } else if (pid == 0) {
            /* redirect the result to the file identifier */
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
            close(fds[1]);

            char tmp[BUF_SZ];
            sprintf(tmp, "command -v %s", command);
            system(tmp);
            exit(1);//origin
            //exit(0);
        } else {
            waitpid(pid, NULL, 0);
            /* input redirection */
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);

            if (getchar() == EOF) { //no data means command does not exist
                result = FALSE;
            }
            /* restore the input / output redirection */
            dup2(inFd, STDIN_FILENO);
            dup2(outFd, STDOUT_FILENO);
        }
    }
    return result;
}

int dealWithBackGround(char* argv,unsigned long AndSignPos){
        isBackground=1;
        AndSignPos=AndSignPos+1-1;
        //argv[AndSignPos] = ' ';//added
        debugPrint("debug: is back ground ");
        debugPrint_int(isBackground);debugPrint(" \n");
        memset(backGroundCom[backGroundNum],0,MAX_LINE);
        strcpy(backGroundCom[backGroundNum], argv);
        debugPrint("debug: backGoundCommand is ");
        debugPrint(backGroundCom[backGroundNum]);debugPrint(" \n");
        backGroundCom[backGroundNum][strlen(backGroundCom[backGroundNum])/*-1*/] = '\0'; 
        return 0;
}
int parsingQuotation(char* argv,char specialList[MAX_LINE],int *specialCnt){
    int is1QuoNotClosed = 0;
    int is2QuoNotClosed = 0;
    //int specialCnt = 0;
    //char specialList[MAX_LINE];
    memset(specialList,0,MAX_LINE);
    unsigned long deleteCnt = 0;
    unsigned long deleteList[MAX_LINE];
    for(unsigned long i=0;i<strlen(argv)-1;i++){ // omitted the newline in the end
        if(is1QuoNotClosed || is2QuoNotClosed){
            if(argv[i]=='>') {argv[i]=Q_REPLACER;specialList[(*specialCnt)++]='>';}
            if(argv[i]=='<') {argv[i]=Q_REPLACER;specialList[(*specialCnt)++]='<';}
            if(argv[i]=='|') {argv[i]=Q_REPLACER;specialList[(*specialCnt)++]='|';}
            if(argv[i]==' ') {argv[i]=Q_REPLACER;specialList[(*specialCnt)++]=' ';}
            if(argv[i]=='\n') {argv[i]=Q_REPLACER;specialList[(*specialCnt)++]='\n';}
        }
        if(argv[i]=='\''){
            if(!is1QuoNotClosed && !is2QuoNotClosed){
                deleteList[deleteCnt++] = i; // DEL '
                is1QuoNotClosed = 1;
            }
            else if(is1QuoNotClosed){
                deleteList[deleteCnt++] = i; // DEL '
                is1QuoNotClosed = 0;
            }
        }
        else if(argv[i]=='\"'){
            if(!is1QuoNotClosed && !is2QuoNotClosed){
                deleteList[deleteCnt++] = i; // DEL "
                is2QuoNotClosed = 1;
            }
            else if(is2QuoNotClosed && i>0 && argv[i-1]=='\\'){
                deleteList[deleteCnt++] = i; // DEL slash
            }
            else if(is2QuoNotClosed && (i==0 || (i>0 && argv[i-1]!='\\'))){
                deleteList[deleteCnt++] = i; // DEL "
                is2QuoNotClosed = 0;
            }
        }
    }
    char tmpLine[MAX_LINE];
    memset(tmpLine,0,MAX_LINE);
    for(unsigned int i=0,j=0,k=0;i<strlen(argv);i++){
        if(j>=deleteCnt || (j<deleteCnt && i!=deleteList[j])) tmpLine[k++]=argv[i];
        else if(j<deleteCnt && i==deleteList[j]) j++;
    }

    memset(argv,0,MAX_LINE);
    strcpy(argv,tmpLine);
        //added
    if (strlen(conjLine) != MAX_LINE) {
            conjLine[strlen(conjLine)-1] = '\0';
    } 
    return 0;
}


int splitCommands(char command[MAX_LINE],int * commandNum,redir_t * redi) { // split command using the blank , return the number of string we get
//support redirection  <, > without blank space, let them separated by space
    int num = 0;
    unsigned long i, j;
    unsigned long len = strlen(command);
    redi->in=0;redi->out=0;redi->add=0;
    //int rediOut=0,rediIn=0,rediAdd=0;
    int ioErrorFlag=0;
    for (i=0, j=0; i<len; ++i) {
        debugPrint("split, i=");debugPrint_ul(i);
        if (command[i] != ' ' ) {
            if(command[i] != '>' && command[i] != '<' && command[i] != '&'){
                commands[num][j++] = command[i];
                debugPrint("this is no < > \n");
            }else if(command[i] == '<'){// <
                if(redi->in ){if(!ioErrorFlag){fprintf(stderr, "error: duplicated input redirection\n");}
                ioErrorFlag=ERROR_MANY_IN;}
                redi->in++;
                if(i>(unsigned long)1 && command[i-1]!=' '){++num;debugPrint("debug:this is <,++num;\n");}
                j=0;
                commands[num][0] = '<';commands[num][1] = '\0';
                ++num;
                debugPrint("this is <\n");
            }else if((command[i] == '>' && i==0 && command[i+1] != '>' ) || (command[i] == '>' && i+1 < len && command[i+1] != '>' )){//>
                if(redi->add || redi->out){if(!ioErrorFlag){fprintf(stderr, "error: duplicated output redirection\n");}
                ioErrorFlag=ERROR_MANY_OUT;}
                redi->out++;
                if(i>(unsigned long)1 && command[i-1]!=' '){++num;debugPrint("debug:this is >,++num;\n");}
                j=0;
                commands[num][0] = '>';commands[num][1] = '\0';
                ++num;
                debugPrint("this is >\n");
            }else if((command[i] == '>' && i==0 && command[i+1] == '>' ) || (command[i] == '>' && i>0  && i+1<len && command[i+1] == '>')  ){//>>
                if(redi->add || redi->out){if(!ioErrorFlag){fprintf(stderr, "error: duplicated output redirection\n");}
                ioErrorFlag=ERROR_MANY_ADD;}
                redi->add++;
                if(i+2<len && command[i+2] == '>'){//added
                    errorSyntaxChar='>';
                    fprintf(stderr, "syntax error near unexpected token `%c\'\n",errorSyntaxChar);
                    ioErrorFlag=ERROR_SYNTAX;
                    //return ioErrorFlag;
                }
                if(i>(unsigned long)1 && command[i-1]!=' '){++num;debugPrint("debug:this is >>,++num;\n");}
                j=0;
                commands[num][0] = '>';commands[num][1] = '>';commands[num][2] = '\0';
                ++num;++i;
                debugPrint("this is >>\n");
            }else if((command[i] == '&' && i+1 < len && command[i+1] != '&' ) ||(command[i] == '&' && i+1 == len )){
                dealWithBackGround(command,i);
            }


        }else {debugPrint("this is empty\n");
            if (j != (unsigned long)0) {
                commands[num][j] = '\0';
                ++num;
                j = 0;
            }
        }

    }
    if (j != 0) {
        commands[num][j] = '\0';
        ++num;
    }
    debugPrintCommands(num);

    (*commandNum)=num;
    return ioErrorFlag;
}


int checkIOError(int ioErrorFlag){
    //int dupFlag=0;
    //commandNum=commandNum+1-1;
    char **tmpArgv = (char **)malloc(sizeof(char *)*MAX_LINE); // arguments from the input line
    //char tmpArgv[MAX_LINE][MAX_LINE];
    for(int i=0;i<MAX_LINE;i++) tmpArgv[i]=NULL; // init the parameter array
    int tmpArgc = 0; // argument count in the input line
    char *tmpToken;
    tmpToken = strtok(conjLine, PARM_DELIM);
    while (tmpToken != NULL){
        tmpArgv[tmpArgc] = (char *)malloc(sizeof(char)*(strlen(tmpToken)+1));
        memset(tmpArgv[tmpArgc], 0, strlen(tmpToken)+1); // init parameter
        strcpy(tmpArgv[tmpArgc], tmpToken);
        tmpArgc++;
        tmpToken = strtok(NULL, PARM_DELIM);
    }

    int dupFlag=0;
    if(!ioErrorFlag) for(int i=0;i<tmpArgc;i++){
        if(tmpArgv[i][0]=='|') dupFlag=1;
        if(tmpArgv[i][0]=='<' && dupFlag){
            fprintf(stderr, "error: duplicated input redirection\n");
            ioErrorFlag = ERROR_MANY_IN;
            break;
        }
    }
            dupFlag=0;
        for(int i=0;i<tmpArgc;i++){
            if(ioErrorFlag) break;
            if(i==0 && tmpArgv[i][0]=='|'){
                fprintf(stderr, "error: missing program\n");
               
                ioErrorFlag = ERROR_MISS_PROGRAM;
                break;
            }
            if(tmpArgv[i][0]=='|' && i<tmpArgc-1 && tmpArgv[i+1][0]=='|'){
                fprintf(stderr, "error: missing program\n");
                ioErrorFlag = ERROR_MISS_PROGRAM;
                break;
            }
            if(tmpArgv[i][0]=='>' && i<tmpArgc-1 && tmpArgv[i+1][0]=='>'){
                errorSyntaxChar='>';
                fprintf(stderr, "syntax error near unexpected token `%c\'\n",errorSyntaxChar);
                ioErrorFlag = ERROR_SYNTAX;
                break;
            }
            if(tmpArgv[i][0]=='>' && i<tmpArgc-1 && tmpArgv[i+1][0]=='<'){
                errorSyntaxChar='<';
                fprintf(stderr, "syntax error near unexpected token `%c\'\n",errorSyntaxChar);
                ioErrorFlag = ERROR_SYNTAX;
                break;
            }
            if(tmpArgv[i][0]=='>' && i<tmpArgc-1 && tmpArgv[i+1][0]=='|'){
                errorSyntaxChar='|';
                fprintf(stderr, "syntax error near unexpected token `%c\'\n",errorSyntaxChar);
                ioErrorFlag = ERROR_SYNTAX;
                break;
            }
            if(tmpArgv[i][0]=='>') dupFlag=1;
            if(tmpArgv[i][0]=='|' && dupFlag){
                fprintf(stderr, "error: duplicated output redirection\n");
                ioErrorFlag = ERROR_MANY_OUT;
                break;
            }
        }
        debugPrint("debug: checkIOError: ioErrorFlag=");
        debugPrint_int(ioErrorFlag);debugPrint("\n");

       for(int i=0;i<tmpArgc;i++) if(tmpArgv[i]!=NULL) free(tmpArgv[i]);
        free(tmpArgv);
        //free(dupsLine);
        /*if(ioErrorFlag){
            for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
            free(mArgv);
            promptExit();
            continue;
        }*/
    return ioErrorFlag;
}

int parsingPipe(int commandNum,int *cmdHeadDict,int *pipeCnt){
    //int pipeCnt=0; // number of pipes in the line
    //int cmdHeadDict[MAX_PIPE]; // location dictionary of command heads
    cmdHeadDict[0]=0;
    for(int i=0,j=1;i<commandNum;i++){
        if(!strcmp(commands[i], "|")){
            debugPrint("debug: parsing pipe pipeCnt");debugPrint_int(*pipeCnt);
            debugPrint("\n");debugPrint("debug: parsing pipe,cmdHeadDict ");
            debugPrint_int(j);debugPrint(" = ");debugPrint_int(i+1);
            debugPrint("\n");
            (*pipeCnt)++;
            //free(mArgv[i]);
            commands[i][0]='\0';//CHECK:temporary removed// will need when pipe is rewrite
            
            cmdHeadDict[j++]=i+1;
        }
        else if(!strcmp(commands[i], "&")) {/*free(commands[i]);*/ 
        //commands[i][0]='\0';//CHECK:temporary removed// will need when pipe is rewrite
        strcpy(commands[i],"\0");
        //(*commandNum--;)
        }
    }
    int cmdCnt = (*pipeCnt) + 1; // number of commands in the line
    cmdHeadDict[cmdCnt] = commandNum + 1; // for the purpose of calculating offset
    debugPrint("debug: parsing pipe cmdCnt = ");
    debugPrint_int(cmdCnt);debugPrint("\n");
    return cmdCnt;
}


int recreateSpecialChar(int commandNum, char *specialList,int specialCnt){
    /** Recreating special characters
     */ 
    int spIndex = 0;
    for(unsigned long i=0;i<(unsigned long)commandNum;i++){
        if(commands[i]==NULL || commands[i][0]=='\0') continue;
        for(unsigned long j=0;j<strlen(commands[i]);j++){
            if(commands[i][j] == Q_REPLACER && spIndex<specialCnt) commands[i][j] = specialList[spIndex++];
        }
    }
    return 0;
}


int creatingPipe(int *pipeFd,int pipeCnt){
    for(int i=0;i<pipeCnt;i++){
        if(pipe(pipeFd + i*2) < 0){
            debugPrint("debug: Error: pipe failure.\n");
            //for(int i=0;i<commandNum;i++) if(mArgv[i]!=NULL) free(commands[i]);
            //free(commands);
            promptExit();
            //for(int i=0;i<backGroundNum;i++) free(backGroundCom[i]);
            //if(lastDir!=NULL) free(lastDir);
            exit(0);
        }debugPrint("debug: pipeFd");debugPrint_int(i*2);debugPrint(" = ");debugPrint_int(pipeFd[i*2]);debugPrint("\n");
    }
    return 0;
}
void debugPrint(const char *message){
    #ifdef DEBUG
    printf("%s", message);
    #endif
    char a = message[0];
    a++;
}
void debugPrint_ascii(const char *message){
    #ifdef DEBUG
    for(unsigned long i=0;i<strlen(message);i++){
        printf("%d ", (int)message[i]);
    }
    
    #endif
    char a = message[0];
    a++;
}
void debugPrint_int(const int message){
    #ifdef DEBUG
    printf("%d", message);
    #endif
    int a = message;
    a++;
}
void debugPrint_ul(const unsigned long message){
    #ifdef DEBUG
    printf("%lu", message);
    #endif
    unsigned long a = message;
    a++;
}

void debugPrintCommands(int num){
    for(int m=0;m<num;m++){
        debugPrint("m=");debugPrint_int(m);debugPrint("\n");
        debugPrint("spliting:");debugPrint(commands[m]);debugPrint("\n");
    } 
}
int callExit() { // end terminal signal to exit the process
    pid_t pid = getpid();
    if (kill(pid, SIGTERM) == -1)
        return ERROR_EXIT;
    else return RESULT_NORMAL;
}

int getRediInfo(redir_t *redi,int commandNum){
    redi->in=0;redi->out=0;redi->add=0;redi->desIn=0;redi->desOut=0;
    memset(redi->inFileName,'\0',sizeof(redi->inFileName));
    memset(redi->outFileName,'\0',sizeof(redi->outFileName));
    int left=0,right=commandNum;
    
    for (int i=left; i<right; ++i) {
        if (strcmp(commands[i], COMMAND_IN) == 0) { // input redirection
            ++redi->in;debugPrint("in redir , ++redi->in\n");
            if (i+1 < right){
                strcpy(redi->inFileName,commands[i+1]);
                //redi->inFileName = commands[i+1];
                debugPrint("debug:nonExistFilePos=");debugPrint_int(i+1);debugPrint("\n");
                nonExistFilePos=i+1;
            }   
            else return ERROR_MISS_PARAMETER; // after redirection symbol miss file name 
            //if (endIdx == right) endIdx = i;
        } else if (strcmp(commands[i], COMMAND_OUT) == 0) { // output redirection >
            ++redi->out;debugPrint("out redir , ++redi->out\n");
            if (i+1 < right){
                strcpy(redi->outFileName,commands[i+1]);
                //redi->outFileName = commands[i+1];
            }else return ERROR_MISS_PARAMETER; // after redirection symbol miss file name 

            //if (endIdx == right) endIdx = i;
        }else if (strcmp(commands[i], COMMAND_OUT_ADD) == 0) { // output redirection >>
            ++redi->add;debugPrint("add redir , ++redi->add\n");
            if (i+1 < right){
                strcpy(redi->outFileName,commands[i+1]);
                //redi->outFileName = commands[i+1];
            }
            else return ERROR_MISS_PARAMETER; // after redirection symbol miss file name 
            //if (endIdx == right) endIdx = i;//why????
        }
    }
    debugPrintRedir(redi);
    debugPrint("debug: We've given value to redirection file names\n");
    /* handling redirection */
    if (redi->in == 1) {
        FILE* fp = fopen(redi->inFileName, "r");
        if (fp == NULL) // input redirection file does not exist
            {fprintf(stderr, "%s: No such file or directory\n",commands[nonExistFilePos]);
            return ERROR_FILE_NOT_EXIST;}
        fclose(fp);
    }
    debugPrint("debug:opened input redirection file\n");
    if (redi->in > 1) { // input redirection sumbol more than one
        debugPrint("debug:error: too many in redirections\n");
        return ERROR_MANY_IN;
    } else if ((redi->out + redi->add) > 1) { // output redirection sumbol more than one
        debugPrint("debug:error: too many out redirections\n");
        return ERROR_MANY_OUT;
    }else{debugPrint("debug:no error about too many redirections\n");}
    return 0;
}


int callPwd(){
    char cwd[MAX_PATH];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
        debugPrint("debug: pwd working.\n");
        prompt(cwd);
        prompt("\n");
    }else{
        debugPrint("Error: pwd not working.\n");
        return 1;
    }
    return 0;
}

int callBuiltIn(int commandNum,int cmdHead,int cmdOffset) { // execute built-in commands  like cd 
    int result = RESULT_NORMAL;
    if(strcmp(commands[cmdHead],"cd")==0){//cd command
        if (cmdOffset > 2 ) {// >2 arguments //CHECK: when piped 
            result =ERROR_TOO_MANY_ARGUMENTS;
        } else if(cmdOffset >=1) {//1 argument after cd
            int ret=0;
            if(strcmp(commands[cmdHead+1],"~")==0 || commandNum == 1 || cmdOffset==1){//cd ~  : go to home directory
                debugPrint("debug:now home dir is");debugPrint(homedir);debugPrint("\n");
                ret = chdir(homedir);   
                //if(lastDir==NULL) lastDir = (char *)malloc(sizeof(char)*MAX_PATH);
                    strcpy(lastDir,lastPendingDir);
                    strcpy(lastPendingDir,homedir);             
            }else if(strcmp(commands[cmdHead+1],"-")==0){ //cd - : go to the last working directory
                    if(lastDir[0]==0){
                        debugPrint("debug: No last dir\n");
                    }else{
                        chdir(lastDir);
                        prompt(lastDir);
                        prompt("\n");
                    }
            }else{// cd /xx/xxx  : normal, go to directory we entered, and save the last directory 
                ret = chdir(commands[cmdHead+1]);
                //if(lastDir==NULL){lastDir = (char *)malloc(sizeof(char)*MAX_PATH);}
                if (ret<0){
                    debugPrint("debug: Error: cd not working.\n");
                    fprintf(stderr, "%s: No such file or directory\n", commands[cmdHead+1]);
                    result = ERROR_WRONG_PARAMETER;
                }//error message "cd: no such file or directory"
                else{//success get cd
                    memset(lastDir,0,MAX_PATH);
                    strcpy(lastDir,lastPendingDir);
                    char cwd[MAX_PATH];
                    if(getcwd(cwd, sizeof(cwd)) != NULL){
                        debugPrint("debug: pwd copying absolute directory.\n");
                    }
                    strcpy(lastPendingDir,cwd);  
                }              
            }
            
        }
    }
    return result;
}


int callJobs(){
    for(int i=0;i<backGroundNum;i++){
        char tmpPrint[MAX_LINE];
        if(waitpid(backGroundJob[i*2],NULL,WNOHANG)==0) sprintf(tmpPrint,"[%d] running %s\n",i+1,backGroundCom[i]);
        else sprintf(tmpPrint,"[%d] done %s\n",i+1,backGroundCom[i]);
        prompt(tmpPrint);
    }
    return 0;
}


int exeCommand(int index,int cmdCnt,int cmdOffset,int cmdHead,int pipeCnt,int* pipeFd,redir_t *redi){
     /* forking */
    pid_t pid = fork(); // TODO: check cd running in background
    lastPid[index] = pid;
    if(pid > 0 && isBackground==1 && index==0){
        backGroundJob[backGroundNum*2] = pid;
        backGroundJob[backGroundNum*2+1] = PROCESS_RUNNING;
        backGroundNum++;
        char tmpPrint[MAX_LINE];
        sprintf(tmpPrint,"[%d] %s\n",backGroundNum,backGroundCom[backGroundNum-1]);
        prompt(tmpPrint);
    }
    if(pid < 0){ // fork error
        debugPrint("Error: fork failed.\n");
        //for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
        //free(mArgv);
        promptExit();
        //for(int i=0;i<bgCnt;i++) free(bgCommand[i]);
        //if(lastDir!=NULL) free(lastDir);
        exit(0);
    }else if (pid == 0){ // child process
        sigaction(SIGINT, &action, &old_action);
        currStatus = CHILD_NORMAL;
        //connectChildPipeFd(index,cmdCnt,pipeCnt,pipeFd);
         /* connecting child pipeFd */
        if(index+1 < cmdCnt){ // not the last command
            debugPrint("index+1<cmdCnt\n");
            if(pipeCnt>0 && dup2(pipeFd[index*2+1], 1) <= 0){
                debugPrint("pipeCnt>0 && dup2(pipeFd[index*2+1], 1) <= 0\n");
                debugPrint("debugError: dup2-stdout failure.\n");
                //for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
                //free(mArgv);
                promptExit();
                //for(int i=0;i<bgCnt;i++) free(bgCommand[i]);
                //if(lastDir!=NULL) free(lastDir);
                exit(0);
            }
        }
        if(index!=0){ // not the first command
            debugPrint("index!=0\n");
            if(pipeCnt>0 && dup2(pipeFd[index*2-2], 0) < 0){
                debugPrint("pipeCnt>0 && dup2(pipeFd[index*2-2], 0) < 0\n");
                debugPrint("debug:Error: dup2-stdin failure.\n");
                //for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
                //free(mArgv);
                promptExit();
                //for(int i=0;i<bgCnt;i++) free(bgCommand[i]);
                //if(lastDir!=NULL) free(lastDir);
                exit(0);
            }
        }
        checkRedi(index ,cmdCnt,redi);
        /* closing child pipeFd */
        for(int i=0;i<2*pipeCnt;i++){
            close(pipeFd[i]);
        }
        /* running pwd */
        if(!strcmp(commands[cmdHead],"pwd")){
            callPwd();
            exit(0);
        }
        
        deleteRediSynAndExe(cmdHead,cmdHead+cmdOffset);
        exit(0);
    }else{ // parent process
                
    }
    return 0;

}

int deleteRediSynAndExe(int left,int right){
    char* comm[MAX_LINE];
    int endIdx=right;
    //char* comm[BUF_SZ];
    debugPrint("endIndex=");debugPrint_int(endIdx);debugPrint("\n");
    debugPrint("right=");debugPrint_int(right);debugPrint("\n");
    //printf("right=%d,endIdx=%d\n", right,endIdx);
    int p=0;//index of comm[]
    for (int i=left; i<endIdx; ++i){
        if(strcmp(commands[i],COMMAND_OUT)==0 || strcmp(commands[i],COMMAND_IN)==0 || strcmp(commands[i],COMMAND_OUT_ADD)==0
            || strcmp(commands[i],"&")==0){
            i++;
        }else{if(p==0){firstCommandParamPos=i;}
            comm[p++] = commands[i];
            debugPrint("comm=:");debugPrint(comm[p-1]);debugPrint("\n");
            /*if(strcmp(commands[i],"cat")==0 && inNum > 0 && i+1<endIdx){
                    int inFds = dup(STDIN_FILENO);
                    dup2(inFds, STDIN_FILENO);
            }*/
        }
    }if(p!=0)comm[p] = NULL;
    
    if (comm[0]==NULL){
        fprintf(stderr, "error: missing program\n");
        exit(errno);//added for m3 
        return ERROR_MISS_PROGRAM;
    }else if (!isCommandExist(comm[0])) { // command doesn't exist
    debugPrint("command ");debugPrint(comm[0]);debugPrint(" doesn't exist\n");
        fprintf(stderr, "%s: command not found\n",commands[firstCommandParamPos]);//TODO: send a commands[0]
        exit(errno);//added for m3 
        return ERROR_COMMAND;
    }else{debugPrint("command ");debugPrint(comm[0]);debugPrint(" exists\n");
    }
            /* running bash command */
    if(strcmp(comm[0],"pwd")==0){callPwd();}
    else if(execvp(comm[0], comm) < 0){
            debugPrint("debug:Error: execvp not working.\n");
            fprintf(stderr, "%s: command not found\n",comm[0]);//TODO: send a commands[0]
            // exit(0);
    }
    
    
    return 0;
}


int checkRedi(int index ,int cmdCnt, redir_t* redi){
    /* checking redirection */
    if(index==0 && redi->in){
        redi->desIn = open(redi->inFileName,O_RDONLY); // TODO: check the parameters
        if(redi->desIn<=0){
            if(errno == ENOENT){
                fprintf(stderr, "%s: No such file or directory\n",redi->inFileName);
                exit(0);
            }
        }
        dup2(redi->desIn, 0); // replace stdin(0) with desIn
        close(redi->desIn);
    }
    if(index+1== cmdCnt && redi->out){
        redi->desOut = open(redi->outFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if(redi->desOut<=0){
            if(errno==EPERM || errno==EROFS){ // not permitted
                //errMsg(outFileName);cd
                fprintf(stderr, "%s: Permission denied\n",redi->outFileName);
                exit(0);
            }
        }
        dup2(redi->desOut, 1); // replace stdout(1) with desOut
        close(redi->desOut);
    }
    if(index+1==cmdCnt && redi->add ){
        redi->desOut = open(redi->outFileName, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        if(redi->desOut<=0){
            if(errno==EPERM || errno==EROFS){ // not permitted
                fprintf(stderr, "%s: Permission denied\n",redi->outFileName);
                exit(0);
            }
        }
        dup2(redi->desOut,1);
        close(redi->desOut);
    }
    debugPrintRedir(redi);
    return 0;
}


void debugPrintRedir(redir_t* redi){redi->add=redi->add+1-1;
    debugPrint("in: ");debugPrint_int(redi->in);debugPrint("\n");
    debugPrint("out: ");debugPrint_int(redi->out);debugPrint("\n");
    debugPrint("add: ");debugPrint_int(redi->add);debugPrint("\n");
    debugPrint("desIn: ");debugPrint_int(redi->desIn);debugPrint("\n");
    debugPrint("desOut: ");debugPrint_int(redi->desOut);debugPrint("\n");
    debugPrint("infileName: ");debugPrint(redi->inFileName);debugPrint("\n");
    debugPrint("outfileName: ");debugPrint(redi->outFileName);debugPrint("\n");
}

void programInit(){
    memset(conjLine,'\0',sizeof(conjLine));
    memset(inFileName,'\0',sizeof(inFileName));
    memset(outFileName,'\0',sizeof(outFileName));
    //memset(commands,'\0',sizeof(commands));
    //memset(backGroundCom,'\0',sizeof(backGroundCom));
    fdStdIn = dup(0);
    fdStdOut = dup(1);
    isBackground = 0;
}

void promptExit(){
    dup2(fdStdIn, 0);
    dup2(fdStdOut, 1);
    //free(inFileName);
    //free(outFileName);
}

