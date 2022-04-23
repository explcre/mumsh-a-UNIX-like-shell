<div style="width:100%;height:200px;text-align:center;border:14px solid #808080;border-top:none;border-left:none;border-bottom:none;display:inline-block">
    <div style="border:4px solid #808080;border-radius:8px;width:95%;height:100%;background-color: rgb(209, 209, 209);">
        <div style="width:100%;height:30%;text-align:center;line-height:60px;font-size:26px;font-family:'Lucida Sans', 'Lucida Sans Regular', 'Lucida Grande', 'Lucida Sans Unicode', Geneva, Verdana, sans-serif;">VE482 Project Report</div>
        <div style="width:100%;height:18%;text-align:center;line-height:26px;font-size:20px;font-familny:'Lucida Sans', 'Lucida Sans Regular', 'Lucida Grande', 'Lucida Sans Unicode', Geneva, Verdana, sans-serif;"><b>Project 1</b> - Fall 2021</div>
        <div style="width:100%;height:57%;text-align:center;font-size:16px;line-height:22px;font-family: 'Courier New', Courier, monospace;font-weight:300;"><br><b>Name: Pengcheng Xu<br>ID: 518370910177<br>Email: xu_pengcheng@sjtu.edu.cn<br>







[TOC]

## Introduction



It's a bash-like shell implemented by c.



cmake file is as follows:

```cmake
 cmake_minimum_required(VERSION 2.7)

 project(ve482p1)

 set(CMAKE_C_EXTENSIONS ON)
 set(CMAKE_C_STANDARD 11)
 set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic -Wno-unused-result")
 file(GLOB SOURCE_FILES "*.c")

 add_executable(mumsh ${SOURCE_FILES})
 add_executable(mumsh_memory_check ${SOURCE_FILES})

 target_compile_options(mumsh_memory_check PUBLIC -fsanitize=address,undefined,integer, -fno-omit-frame-pointer)
 target_link_libraries(mumsh_memory_check -fsanitize=address,undefined,integer)

```



## Functions

1. Support a working read/parse/execute loop and an exit command; [5] 

2.  Handle single commands without arguments (e.g. ls); [5] 2

3.  Support commands with arguments (e.g. apt-get update or pkgin update); [5] 

4.  File I/O redirection: [5+5+5+2]

   4.1. Output redirection by overwriting a file (e.g. echo 123 > 1.txt);

   4.2. Output redirection by appending to a file (e.g. echo 465 >> 1.txt); 

   4.3. Input redirection (e.g. cat < 1.txt); 4.4. Combine 4.1 and 4.2 with 4.3; 

5. Support for bash style redirection syntax (e.g. cat < 1.txt 2.txt > 3.txt 4.txt); [8] 

6.  Pipes: [5+5+5+10] 

   6.1. Basic pipe support (e.g. echo 123 | grep 1); 

   6.2. Run all ‘stages’ of piped process in parallel. (e.g. yes ve482 | grep 482); 

   6.3. Extend 6.2 to support requirements 4. and 5. (e.g. cat < 1.txt 2.txt | grep 1 > 3.txt); 

   6.4. Extend 6.3 to support arbitrarily deep “cascade pipes” (e.g. echo 123 | grep 1 | grep 1 | grep 1) Note: the sub-processes must be reaped in order to be awarded the marks. 

7. Support CTRL-D (similar to bash, when there is no/an unfinished command); [5] 

8. Internal commands: [5+5+5] 

   8.1. Implement pwd as a built-in command;

   8.2. Allow changing working directory using cd; 

   8.3. Allow pwd to be piped or redirected as specified in requirement 4.; 

9. Support CTRL-C: [5+3+2+10]

   9.1. Properly handle CTRL-C in the case of requirement 4.; 

   9.2. Extend 9.1 to support subtasks 6.1 to 6.3; 

   9.3. Extend 9.2 to support requirement 7., especially on an incomplete input;

   9.4. Extend 9.3 to support requirement 6.;

10. Support quotes: [5+2+3+5] 

    10.1. Handle single and double quotes (e.gm. echo "de'f' ghi" '123"a"bc' a b c);

    10.2. Extend 10.1 to support requirement 4. and subtasks 6.1 to 6.3; 

    10.3. Extend 10.2 in the case of incomplete quotes (e.g. Input echo "de, hit enter and input cd"); 

    10.4. Extend 10.3 to support requirements 4. and 6., together with subtask 9.3; 

11. Wait for the command to be completed when encountering >, <, or |: [3+2]

    11.1. Support requirements 3. and 4. together with subtasks 6.1 to 6.3; 

    11.2. Extend 11.1 to support requirement 10.; 

12. Handle errors for all supported features. [10] Note: a list of test cases will be published at a later stage. Marks will be awarded based on the number of cases that are correctly handled, i.e. if only if: 

    • A precise error message is displayed (e.g. simply saying “error happened!” is not enough); 

    • The program continues executing normally after the error is identified and handled; 

13. A command ending with an & should be run in background. [10] 3 

    13.1. For any background job, the shell should print out the command line, prepended with the job ID and the process ID (e.g. if the two lines /bin/ls & and /bin/ls | cat & are input the output could be the two lines [1]  /bin/ls & and [2]  /bin/ls | cat & ); 

    13.2. Implement the command jobs which prints a list of background tasks together with their running states (e.g. in the previous case output the two lines [1] done /bin/ls & and [2] running /bin/ls | cat &);

    





## Structure explanation

### 

**main function** is as follows:

```c
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
        int pipeFd[pipeCnt*2+2];

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
        //fflush(stdout);
        //fflush(stdout);
    }
}

```



**void sigint_handler()**

is used for ctrl-c handling

**ERROR_XXX**

```c
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
```



**redir_t**

is a structure defined as follows

```c
typedef struct redir_t{     
    int in;         
    int out;   
    int add;  
    int desIn;// file descriptor
    int desOut;  // file descriptor  
    char inFileName[MAX_FILENAME];   
    char outFileName[MAX_FILENAME];  
}redir_t;
```

It's used for redirections



**currStatus**

has three values:

PARENT_NORMAL

PARENT_EXIT

CHILD_NORMAL



**void prompt(const char *message)cc**

is used for print a message 

```c
void prompt(const char *message){
    printf("%s", message);
    fflush(stdout);
}
```



**int preProcessInput(char* argv)**

is used for task 11.

input: 

​	char *argv;    a line of input we type in.

output: 

​	fgetsErrorFlag;   if error exists, =1, otherwise=0.

**int checkEmptyLine(char* argv)**

check whether we input an empty line.



**int parsingQuotation(char* argv,char specialList[MAX_LINE],int *specialCnt)**

parse the quotation marks.

input :

​	char* argv; input line.

​	char specialList[MAX_LINE];  the input like "|" will be stored in specialList. At first replace it with a special character , and will be restored later.

​	int *specialCnt; number of the elements in specialList.

output:

​	0

**int splitCommands(char command[MAX_LINE],int * commandNum,redir_t * redi)**

split a whole line into several strings.

like input "cat 123 > 123.txt" 

->  output "cat","123",">","123.txt"

input :

​	char command[MAX_LINE];

​	int * commandNum;

​	redir_t * redi

output:

​	int ioErrorFlag;    see enum ERROR_XXX.

**int getRediInfo(redir_t *redi,int commandNum)**

input:

​	redir_t *redi;

​	int commandNum;

output:

​	ioErrorFlag;



**int checkIOError(int ioErrorFlag)**

input:

​	int ioErrorFlag

output:

​	ioErrorFlag;

**int parsingPipe(int commandNum,int *cmdHeadDict,int *pipeCnt)**

input:

​	int commandNum;

​	int *cmdHeadDict;

​	int *pipeCnt;

output:

​	cmdCnt;

**int recreateSpecialChar(int commandNum, char *specialList,int specialCnt)**

input:

​	int commandNum;

​	char *specialList;

​	int specialCnt;

output:

​	0

**int callExit() **

```c
int callExit() { // end terminal signal to exit the process
    pid_t pid = getpid();
    if (kill(pid, SIGTERM) == -1)
        return ERROR_EXIT;
    else return RESULT_NORMAL;
}
```





**int callBuiltIn(int commandNum,int cmdHead,int cmdOffset)**

input:

​	int commandNum;

​	int cmdHead;

​	int cmdOffset;

output:

​	result

**int exeCommand(int index,int cmdCnt,int cmdOffset,int cmdHead,int pipeCnt,int* pipeFd,redir_t *redi)**

input:

​	int index;

​	int cmdCnt;

​	int cmdOffset;

​	int cmdHead;

​	int pipeCnt;

​	int* pipeFd;

​	redir_t *redi;

output:

   0;

**int callJobs()**

```c
int callJobs(){
    for(int i=0;i<backGroundNum;i++){
        char tmpPrint[MAX_LINE];
        if(waitpid(backGroundJob[i*2],NULL,WNOHANG)==0) sprintf(tmpPrint,"[%d] running %s\n",i+1,backGroundCom[i]);
        else sprintf(tmpPrint,"[%d] done %s\n",i+1,backGroundCom[i]);
        prompt(tmpPrint);
    }
    return 0;
}
```

