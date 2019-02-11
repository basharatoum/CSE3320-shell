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

#define SEMICOLON ";"           // Using the same technique of splitting the
                                // commands by whitespaces, this code also splits
                                // the commands on semicolon. This is used to achieve
                                // requirment 16.

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11    // Mav shell only supports five arguments


pid_t pid;                      // Using a global variable to store the child's
                                // pid, as the signal handler will need to know
                                // the pid value of the process to SIGSTOP.

pid_t ppid = -1;
// This struct is used to store each command entered by the user, as it makes
// the storing of past commands eaiser.
struct historyItem
{
    // This array is used to store the command after it gets tokenized. It is a
    // 2D array of strings, because it is splitting the command by semicolons as well
    char *token[MAX_NUM_ARGUMENTS][MAX_NUM_ARGUMENTS];
    // this variable stores the number of semicolons in the command. This is stored
    // to help run the command later on.
    int scolon_count;
};


// This struct is used to store all the pids that were run by the shell. This is
// mainly used to make the code more readible.
struct pidItem
{
    // variable used to store the pid of the process.
    pid_t pid;
};


// This function prints is used to print the pids (requirment 14). By sending a
// pointer to a pidItem array, and the length of the array (n in this case), the
// function will print all the pids in a simple for-loop.
void listPids(struct pidItem *p, int n)
{
    // loop used to print the pids. i is defined outside becuase of C99
    int i;
    for(i=0;i<n-1;++i)
    {
        printf("%d: %d\n",i,p[i].pid );
    }
}


// Given an array of historyItem, and the length of the array (n), this function
// will print the commands in that array. This is used to satisfy requirment 15.
void printHistory(struct historyItem* h,int n)
{
    // main loop iterates through history items. i,j,k defined outside because of C99
    int i,j,k;
    for(i=0;i<n-1;++i)
    {
        // for each history item print its number, and then iterate through each
        // token separated by the semicolon. (note the use of scolon_count)
        printf("%d: ",i);
        for(j=0;j<h[i].scolon_count;++j)
        {
            // simply print all the tokens seprated by the whitespace now.
            for(k=0;k<MAX_NUM_ARGUMENTS;++k)
            {
                // check that the token isn't null, avoid SIGFAULT
                if(h[i].token[j][k] != NULL && strlen(h[i].token[j][k]) != 0)
                    printf("%s ",h[i].token[j][k]);
            }
            if(j!=h[i].scolon_count-1)
                printf("; ");
        }
    printf("\n");
    }
}

// This function is used to add the new command into the history array. Given a
// pointer to the history array, a scolon_count for the new command, a pointer
// to the current length of the array, and the current command, this function will
// add the item to the array. This function is split into three parts. The first part
// mallocs the history array (when n = 0). In the second part, it will realloc the
// history array (1 <= n <= 50). In the third part, it simply shifts the history
// items to get rid of the oldest element. In each part it adds the new command in.
struct historyItem* addHistoryItem(struct historyItem* h, char* token[MAX_NUM_ARGUMENTS][MAX_NUM_ARGUMENTS],int scolon_count,int* k)
{
    // Third part. n>=50. This part removes the first part and shifts the elements
    if (*k>=50)
    {
        // Our main function adds one to the length of the array after calling
        // this function. this is getting rid of that one, since n can't be bigger
        // than 50.
        *k=50;

        // n is simply used to make the code more readible
        int n = *k;

        // i,j defined outside because of C99
        int j,i;

        // These loops are used to free the first element in the history item.
        for (j=0;j<h[0].scolon_count;++j)
            for (i =0;i<MAX_NUM_ARGUMENTS;++i)
                free(h[0].token[j][i]);

        // This loops shifts the array elements.
        for(i=1;i<n;++i)
            h[i-1]=h[i];

        // take one of n, as it will be used to index the last element of the array
        // which is the new space for the new command.
        n-=1;

        // iterate through each semicolon seprated token.
        for (j=0;j<scolon_count;++j)
            // iterate through each whitespace seprated token.
            for (i =0;i<MAX_NUM_ARGUMENTS;++i)
            {
                // make sure it is not null, avoids SIGFAULT
                if(token[j][i]!=NULL && strlen(token[j][i])!=0 )
                {
                    h[n].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
                }
                else
                {
                    // This is done because strsep can return strings with a
                    // a length of zero. This way the code can later only check for
                    // nullity.
                    h[n].token[j][i]=NULL;
                }
        }
        // store the number of semicolons here. Later used to print the command.
        h[n].scolon_count=scolon_count;
    }else if(*k==0)
    {
       // This is the first part. Here the function mallocs the new history array.
       h = (struct historyItem*)malloc(sizeof(struct historyItem));

       // i,j defined outside because of C99
       int i,j;

       // This loop is used to add the new command into the history array. iterates
       // through semicolon seprated tokens.
       for (j=0;j<scolon_count;++j)
           // This iterates through whitespace seprated tokens.
           for (i =0;i<MAX_NUM_ARGUMENTS;++i)
           {
               // make sure it is not null, avoids SIGFAULT
               if(token[j][i]!=NULL&&strlen(token[j][i])!=0 )
               {
                   h[0].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
               }
               else
               {
                   // This is done because strsep can return strings with a
                   // a length of zero. This way the code can later only check for
                   // nullity.
                   h[0].token[j][i]=NULL;
               }
           }

       // store the number of semicolons here. Later used to print the command.
       h[0].scolon_count=scolon_count;
    }else
    {
        // This is the second part. Here the functions rellocs the array to be able
        // to add a new element into the array.
        h= (struct historyItem*)realloc(h,(n+1)*sizeof(struct historyItem));

        // n is simply used to make the code more readible. Used to index the last
        // element of the array.
        int n = *k;

        // i,j defined outside because of C99
        int i,j;

        // This loop is used to add the new command into the history array. iterates
        // through semicolon seprated tokens.
        for (j=0;j<scolon_count;++j)
            // This iterates through whitespace seprated tokens.
            for (i =0;i<MAX_NUM_ARGUMENTS;++i)
            {
                // make sure it is not null, avoids SIGFAULT
                if(token[j][i] != NULL && strlen(token[j][i]) != 0)
                {
                    h[n].token[j][i]=strndup(token[j][i], MAX_COMMAND_SIZE);
                }
                else
                {
                    // This is done because strsep can return strings with a
                    // a length of zero. This way the code can later only check for
                    // nullity.
                    h[n].token[j][i]=NULL;
                }
            }

        // store the number of semicolons here. Later used to print the command.
        h[n].scolon_count=scolon_count;
    }

    return h;
}

// This function is used to add the new process pid to the pidItem array. This is
// used so the shell can implement the "listpids" functionality. Given a pointer
// to the array of past pids, the new process pid, and the number of processes the
// main code processed, this function returns the new modified pid history array.
// Please note that this function uses the same process as the function above.
struct pidItem* addPidItem(struct pidItem * p,pid_t pid,int *k)
{
    if(*k>=15)
    {
        // This is the first part, where there is 15 elements already in the array
        // we simply shift all the elements by one.

        // we set the number of process back to 15, as the main function will add one
        // to the number of elements after calling the function.
        *k=15;

        // n is simply used to make the code more readible. Used to index the last
        // element of the array.
        int n=*k;

        // i defined outside because of C99
        int i;

        // loop to shift the elements by one element. This gets rid of the first element
        for(i=1;i<n;++i)
            p[i-1].pid=p[i].pid;

        // take one of n to be able to index the last element
        n-=1;

        // add the new pid in the new free/unused space.
        p[n].pid = pid;
    }
    else if(*k==0)
    {
        // This is the second part. Here the function simply mallocs the history array,
        // as this is the first element to be added to the array.
        p = (struct pidItem*)malloc(sizeof(struct pidItem));

        // add the pid of the new element.
        p[0].pid=pid;
    }
    else
    {
        // This is the third part. Here the function rellocs the history array,
        // so that the code can add a new pid.
        p= (struct pidItem*)realloc(p,(n+1)*sizeof(struct pidItem));

        // n is simply used to make the code more readible. Used to index the last
        // element of the array.
        int n=*k;
        p[n].pid=pid;
    }

  return p;
}

// This is the signal handler used to stop the current process if the shell recieves
// a SIGSTP signal (ctrl+z). It is also used to ignore the ctrl+c signal.
void sig_handler(int signaln)
{
    // if the signal is a ctrl+z one, send a SIGSTOP signal to the process. notice
    // that the pid is stored in a global variable.
    if(signaln==SIGTSTP&&pid!=0)
    {
        kill(pid,SIGSTOP);
        ppid = pid;
    }
}

int main()
{
    // This is the string used to read the command line.
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

    // This is the history array of commands. check the struct above.
    struct historyItem* h;

    // This is the history array of the process. check the struct above.
    struct pidItem* p;

    // nh is used to store the number of commands in the history array. np is Used
    // to store the number of pids in the processes history array.
    int nh = 0,np=0;

    // This is used to define the new singal handler.
    struct sigaction act;

    // This gets rid of all the information in the act object.
    memset (&act, '\0', sizeof(act));

    // points the object to our function (signal handler)
    act.sa_handler = &sig_handler;

    // This tells the compiler to use this signal handler incase of a SIGINT signal
    // the if statement is used to return in case of an error
    if (sigaction(SIGINT , &act, NULL) < 0)
    {
        perror ("sigaction: ");
        return 1;
    }

    // This tells the compiler to use this signal handler incase of a SIGTSTP signal
    // the if statement is used to return in case of an error
    if (sigaction(SIGTSTP , &act, NULL) < 0)
    {
        perror ("sigaction: ");
        return 1;
    }

    // infinte loop to be able to keep taking the user's input.
    while( 1 )
    {
        // !!!this marks the start of code taken from the class repository!!!

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
        // !!!this marks the end of code taken from the class repository!!!

        // this loops uses strsep to seprate the command given by semicolons. This
        // technique is inspired by the code taken from the class repository.
        while ( ( (arg_ptr = strsep(&working_str,SEMICOLON ) ) != NULL) &&
              (scolon_count<MAX_NUM_ARGUMENTS))
        {
            token[scolon_count][token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[scolon_count][token_count] ) == 0 )
            {
                token[scolon_count][token_count] = NULL;
            }
            // save the number of semicolons, to be later used in saving the history
            // of commands
            scolon_count++;
        }
        // this part has elements of code from the class repository
        // Tokenize the input stringswith whitespace used as the delimiter
        // iterates through the strings separated be semicolons to seprate them
        // by whitespaces.
        for(int i = 0;i<scolon_count;++i)
        {
            token_count=0;
            // this takes each command split by a semicoln into s_ptr.
            s_ptr = strndup(token[i][token_count], MAX_COMMAND_SIZE );

            // this frees the previously occupied space used by that command.
            free(token[i][token_count]);

            // this splits the string by whitespaces using strsep. Check class repository
            // for more information.
            while ( ( (arg_ptr = strsep(&s_ptr, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
            {
                token[i][token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
                if( strlen( token[i][token_count] ) == 0 &&token_count!=0)
                {
                    token[i][token_count] = NULL;
                }
                else if ( strlen( token[i][token_count] ) != 0)
                {
                    token_count++;
                }
            }

            // this loop nullifies any unsed part of our 2d array of strings.
            while (token_count<MAX_NUM_ARGUMENTS)
            {
                token[i][token_count] = NULL;
                token_count++;
            }
        }

        // this if statement avoids any errors, that could have been caused by strsep
        // or by the above code.
        if(token[0][0]==NULL)
        {
            continue;
        }
        else if(token[0][0][0]=='!')
        {
            // This code implements the processing of old commands in the history
            // array.

            // This stores the history number into f. (EX: converts the "99" to 99)
            int f =atoi(token[0][0]+1);

            // i,j defined outside because of C99
            int i,j;

            // this loop iterates through the semicolon seprated strings.
            for (j=0;j<h[f].scolon_count;++j)
                // this loop iterates through the whitespace seprated strings.
                for (i =0;i<MAX_NUM_ARGUMENTS;++i)
                {
                    // now we free the current token, and store the  old token in
                    // its place.
                    free(token[j][i]);
                    if(h[f].token[j][i]!=NULL&&strlen(h[f].token[j][i])!=0)
                    {
                        token[j][i]=strndup(h[f].token[j][i], MAX_COMMAND_SIZE);
                    }
                    else
                    {
                        // this nullifies any unused spaces.
                        token[j][i]=NULL;
                    }
                }
            // store the old semicolon count, to be able to iterate over the tokens.
            scolon_count=h[f].scolon_count;
        }

        // adds this new command to the history array. check the function above.
        h=addHistoryItem(h,token,scolon_count,&nh);

        //adds one to the number of history items.
        nh++;

        // i,j defined outside because of C99
        int i, j;

        // iterate through each semicolon command
        for(i = 0;i<scolon_count;++i){

            if(strcmp(token[i][0],"bg")==0){
                // if given a bg command send a SIGCONT to the stopped pid.
                kill(ppid,SIGCONT);
                ppid = -1;
            }
            else if(strcmp(token[i][0],"listpids")==0){
                // if given a listPids command print the pid history array.
                // check function above.
                listPids(p,np);
            }
            else if(strcmp(token[i][0],"history")==0)
            {
                // if given a history command print the comman history array.
                // check the function above.
                printHistory(h,nh);
            }
            else if(strcmp(token[i][0],"quit")==0||strcmp(token[i][0],"exit")==0)
            {
                // if given a quit command, call _exit() with a zero status.
                _exit(0);
            }
            else if(strcmp(token[i][0],"cd")==0)
            {
                // if given a cd command, use the chdir to change the directory
                // of the parent process.
                chdir(token[i][1]);
            }
            else
            {
                // if none of the above commands were given. we need to check
                // the bins and exec if we find the command.

                // creat a process to execute the command.
                pid = fork();
                int status;

                if (pid == 0)
                {
                    // in this process execute the command (child process)

                    // this variable are used to know if exec didn't find the command
                    int lastexec  = 0,j=0;

	                  //paths to search for the command in
	                  char paths[4][300] = {"./","/usr/local/bin/","/usr/bin/","/bin/"};

                    // for each path above, try to exec the given command
	                  for (j=0;j<4;++j)
	                  {
                        // if the exec doesnt find the command save it as -1.
	                      lastexec = execv(strcat(paths[j],token[i][0]),token[i]);
		                    if (lastexec!=-1)
		                    {
                            // if the exec did find the command leave the loop.
                            // no need to search in the other paths.
		                        break;
		                    }
	                  }
                    // if the lastexec didn't find the command, and we searched all the paths.
                    // print command not found.
	                  if (j==4&&lastexec==-1)
                    {
	                      printf("%s: Command not found.\n",token[i][0]);
	                  }
                    // flush the output
	                  fflush(NULL);
                    // exit the child process since the code is done executing.
	                  _exit(status);
                }
                else
                {
                    // in the parent process add the pid to the history
                    p = addPidItem(p,pid,&np);

                    // add one to the number of process history items
                    np++;

                    // wait for the child process to stop
                    (void)waitpid(pid,&status,0);
                }
            }
        }
        free( working_root );
    }
  return 0;
}
