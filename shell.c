// The MIT License (MIT)
//
// Copyright (c) 2016, 2017 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line
#define SEMICOLON ";"      // We want to split our command line up into tokens

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11     // Mav shell only supports five arguments
    pid_t pid;
struct historyItem
{
    char *token[MAX_NUM_ARGUMENTS][MAX_NUM_ARGUMENTS];
    int scolon_count;
};

struct pidItem
{
  pid_t pid;
};
void listPids(struct pidItem *p, int n){
  for(int i=0;i<n-1;++i){
    printf("%d: %d\n",i,p[i].pid );
  }
}
void printHistory(struct historyItem* h,int n){
  for(int i=0;i<n-1;++i){
    printf("%d: ",i);
    for(int j=0;j<h[i].scolon_count;++j){
      for(int k=0;k<MAX_NUM_ARGUMENTS;++k){
        if(h[i].token[j][k]!=NULL&&strlen(h[i].token[j][k])!=0 )
        printf("%s ",h[i].token[j][k]);
      }
      if(j!=h[i].scolon_count-1)
      printf("; ");
    }
    printf("\n");
  }
}

struct historyItem* addHistoryItem(struct historyItem* h, char* token[MAX_NUM_ARGUMENTS][MAX_NUM_ARGUMENTS],int scolon_count,int* k){

if (*k>=50)
{
  *k=50;
  int n=*k;
  for (int j=0;j<h[0].scolon_count;++j)
  for (int i =0;i<MAX_NUM_ARGUMENTS;++i)
    free(h[0].token[j][i]);
  for(int i=1;i<n;++i)
  h[i-1]=h[i];
  n-=1;
  for (int j=0;j<scolon_count;++j)
  for (int i =0;i<MAX_NUM_ARGUMENTS;++i){
    if(token[j][i]!=NULL&&strlen(token[j][i])!=0 ){
    h[n].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
   }else{
     h[n].token[j][i]=NULL;
   }
  }
  h[n].scolon_count=scolon_count;
}else if(*k==0)
{
  int n=*k;
h= (struct historyItem*)malloc(sizeof(struct historyItem));
for (int j=0;j<scolon_count;++j)
for (int i =0;i<MAX_NUM_ARGUMENTS;++i){
  if(token[j][i]!=NULL&&strlen(token[j][i])!=0 ){
  h[0].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
}else{
  h[0].token[j][i]=NULL;
}
}
h[0].scolon_count=scolon_count;
}else
{
  int n=*k;
  h= (struct historyItem*)realloc(h,(n+1)*sizeof(struct historyItem));
  for (int j=0;j<scolon_count;++j)
  for (int i =0;i<MAX_NUM_ARGUMENTS;++i){
    if(token[j][i]!=NULL&&strlen(token[j][i])!=0 ){
    h[n].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
   }else{
     h[n].token[j][i]=NULL;
   }
  }
  h[n].scolon_count=scolon_count;
}
return h;
}

struct pidItem* addPidItem(struct pidItem * p,pid_t pid,int *k){
  if(*k>=15){
    *k=15;
    int n=*k;
    for(int i=1;i<n;++i)
    p[i-1].pid=p[i].pid;
    n-=1;
    p[n].pid = pid;
  }else if(*k==0){
    p= (struct pidItem*)malloc(sizeof(struct pidItem));
    p[0].pid=pid;
  }else{
    int n=*k;
    p= (struct pidItem*)realloc(p,(n+1)*sizeof(struct pidItem));
    p[n].pid=pid;
  }

  return p;
}

void sig_handler(int signaln){
  if(signaln==SIGTSTP&&pid!=0){
  kill(pid,SIGSTOP);
  }
}
int main()
{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  struct historyItem* h;
  struct pidItem* p;
  int nh = 0,np=0;
  struct sigaction act;

  /*
  Zero out the sigaction struct
  */
  memset (&act, '\0', sizeof(act));

  /*
    Set the handler to use the function handle_signal()
  */
  act.sa_handler = &sig_handler;
  if (sigaction(SIGINT , &act, NULL) < 0) {
    perror ("sigaction: ");
    return 1;
  }
  if (sigaction(SIGTSTP , &act, NULL) < 0) {
    perror ("sigaction: ");
    return 1;
  }
  while( 1 )
  {
    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS][MAX_NUM_ARGUMENTS];

    int   token_count = 0, scolon_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *s_ptr,*arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    while ( ( (arg_ptr = strsep(&working_str,SEMICOLON ) ) != NULL) &&
              (scolon_count<MAX_NUM_ARGUMENTS))
    {
      token[scolon_count][token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[scolon_count][token_count] ) == 0 )
      {
        token[scolon_count][token_count] = NULL;
      }
        scolon_count++;
    }

    // Tokenize the input stringswith whitespace used as the delimiter
    for(int i = 0;i<scolon_count;++i){
    token_count=0;
    s_ptr = strndup(token[i][token_count], MAX_COMMAND_SIZE );
    free(token[i][token_count]);
    while ( ( (arg_ptr = strsep(&s_ptr, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[i][token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[i][token_count] ) == 0 &&token_count!=0)
      {
        token[i][token_count] = NULL;
      }else if ( strlen( token[i][token_count] ) != 0)
      {
        token_count++;
      }
    }
    while (token_count<MAX_NUM_ARGUMENTS) {
      token[i][token_count] = NULL;
      token_count++;
    }
    }
    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality


    if(token[0][0]==NULL)
    {
    continue;
    }
    else if(token[0][0][0]=='!')
    {
    int f =atoi(token[0][0]+1);
    for (int j=0;j<h[f].scolon_count;++j)
    for (int i =0;i<MAX_NUM_ARGUMENTS;++i){
      free(token[j][i]);
      if(h[f].token[j][i]!=NULL&&strlen(h[f].token[j][i])!=0){
      token[j][i]=strndup(h[f].token[j][i], MAX_COMMAND_SIZE);
    }else{
      token[j][i]=NULL;
    }
     }
     scolon_count=h[f].scolon_count;
    }
    h=addHistoryItem(h,token,scolon_count,&nh);
    nh++;
    for(int i = 0;i<scolon_count;++i){
    if(strcmp(token[i][0],"bg")==0){
        kill(pid,SIGCONT);
    }else if(strcmp(token[i][0],"listpids")==0){
      listPids(p,np);
    }else if(strcmp(token[i][0],"history")==0)
    {
    printHistory(h,nh);
    }else if(strcmp(token[i][0],"quit")==0||strcmp(token[i][0],"exit")==0)
    {
    _exit(0);
    }else if(strcmp(token[i][0],"cd")==0)
    {
    chdir(token[i][1]);
    }else
    {
    pid = fork();
    int status;
    if (pid == 0)
    {
        int lastexec  = 0,j=0;
	//paths to search for the command in

	char paths[4][300] = {"./","/usr/local/bin/","/usr/bin/","/bin/"};
	for (j=0;j<4;++j)
	{
	lastexec = execv(strcat(paths[j],token[i][0]),token[i]);
		if (lastexec!=-1)
		{
		break;
		}
	}
	if (j==4&&lastexec==-1){
	printf("%s: Command not found.\n",token[i][0]);
	}
	fflush(NULL);
	_exit(status);
    }
    else
    {
    p = addPidItem(p,pid,&np);
    np++;

    (void)waitpid(pid,&status,0);
    }
    }
  }
  free( working_root );
  }
  return 0;
}
