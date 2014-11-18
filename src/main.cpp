#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pwd.h>
#include <fcntl.h>
using namespace std;

static int *glob_flag;

//output login @ machine $
void prompt(){
	struct passwd *log;
	int pid = fork();
	if(pid == -1){
		perror("fork() presented error");
		exit(1);
	}
	else if(pid==0){
		char host[50];
		if ((gethostname(host, sizeof(host)-1))==-1) {
		host[0] = 'h';
		host[1] = 'o';
		host[2] = 's';
		host[3] = 't';
		host[4] = '\0';
		perror("Error trying to get hostname");
		}
		log = getpwuid(getuid());
		if(log == '\0'){
			perror("Error trying to get user login");
		}

		cout << log->pw_name << "@" << host << "$ ";
		exit(1);
	}else if(pid>0){
		if(-1 == wait(0))
		perror("wait() presented error");

	}	
}

void execute(char *str[], int size){
	
	char * newstr[512];
	char * connector;
	int i, j, aux;

	for (i = 0; i<size; i++){
		*glob_flag = 0;
		//newstr[0] receives commands, first parameter after a connector
		newstr[0] = str[i];
		if(memcmp(newstr[0],"exit",4)==0) exit(0);
		aux = 1;
		//test command without flags carryng a ';'
		connector = strchr(newstr[0], ';');
		if (connector != NULL) newstr[0][connector - newstr[0]] = '\0';
		else
			for (j = i + 1; j<size; j++){
			connector = strchr(str[j], ';');
			if (connector != NULL){
				//erase the last character if ';'
				str[j][connector - str[j]] = '\0';
			}
			//check for && setting a flag to 1
			if (memcmp(str[j], "&&", 2) == 0){
				*glob_flag = 1;
				i = j;
				break;
			}
			//check for || setting a flag to 3
			if (memcmp(str[j], "||", 2) == 0){
				*glob_flag = 3;
				i = j;
				break;
			}

			//add flags to newstr
			newstr[aux] = str[j];
			aux++;
			i = j;

			if (connector != NULL) break;
			}

		int pid = fork();
		if (pid == -1){
			perror("fork() presented error");
			exit(1);
		}
		else if (pid == 0){
			if (-1 == execvp(newstr[0], newstr)){
				perror("There was an error");
				//flag 1 means, first command must be successfull to apply the second
				if (*glob_flag == 1) *glob_flag = 2;
				//flag 3 means, first command must be failed to apply the second
				//number 5 won't pass in the breaking loop
				if (*glob_flag == 3) *glob_flag = 5;
			}
			exit(1);
		}
		else if (pid>0){

			int status;

			wait(&status);
			if (-1 == status)
				perror("wait() presented error");
			else if (status>0){
				//flag 1 means, first command must be successfull to apply the second
				if (*glob_flag == 1){
					int flag2 = 0;
					for (int p = i; p<size; p++){
						//in case command fails look for a semicolon to reestart from there
						connector = strchr(str[p], ';');
						if (connector != NULL){
							i = p;
							//changing this flag will make the parent loop do not break because a semicolon was found
							flag2 = 1;
							break;
						}
					}
					//if there is no more semicolons parent loop will break
					if (flag2 == 0) *glob_flag = 2;
				}
				//flag 3 means, first command must be failed to apply the second
				//number 5 won't pass in the breaking loop
				if (*glob_flag == 3) *glob_flag = 5;
			}

		}

		// clear the vector newstr in order to execute new commands
		for (int k = 0; k<aux; k++)newstr[k] = '\0';

		//break loop due to a non valid previous command connected by &&					
		if (*glob_flag == 2) break;
		//break loop due to a valid previous command connected by ||
		if (*glob_flag == 3) break;
	}

}
//function recieves commands and arguments before '>' and file to be output
void out_redirect(char *newstr[], char *file_out, bool symbol, int fd_number){
	int fdo;
		//open file descriptor as the argument after '>'
		if(symbol){
			fdo = open(file_out, O_RDWR|O_CREAT|O_APPEND, 0666);
			if(fdo == -1){
				perror("open failed");
				exit(1);
			}
		}
		else{
			fdo = open(file_out, O_RDWR|O_CREAT, 0666);
			if(fdo == -1){
				perror("open failed");
				exit(1);		
			}
		}
		if(fd_number!=0){	
			if(dup2(fdo,fd_number) == -1){
				perror("dup failed");
				exit(1);
			}	
		}
		
	if (execvp(newstr[0], newstr) == -1) 
		perror("execvp 'out' failed");
}


//function recieves commands before '<' and file to be input
void in_redirect(char * newstr[], char * file_in){
	int fdi;
	
		fdi = open(file_in, O_RDONLY);
		if(fdi == -1){
			perror("open failed");
			exit(1);
		}	
		if(dup2(fdi,0) == -1){
			perror("dup failed");
			exit(1);
		}

	if (execvp(newstr[0], newstr) == -1) 
		perror("execvp 'in' failed");
}

void in_redirect2(char * newstr[], int pos, char * str[], int size){

	if (memcmp(str[pos+1], "\"", 1) == 0){				
		int fd[3];
		if (pipe(fd)==-1) {
			perror ("pipe failed");
			exit(1);
		}

		int len = strlen(str[size-1]) -1;
		//removing quote marks
		memmove(&str[pos+1][0], &str[pos+1][0 + 1], strlen(str[pos+1]));
		memmove(&str[size-1][len], &str[size-1][len+1], strlen(str[size-1]) - len);

		//add all the string after '<<<'
		for(int i=pos+1; i<size; i++){		
			write(fd[1],str[i],strlen(str[i]));		
			write(fd[1]," ",1);			
			}	
		write(fd[1],"\n",1);				
		if(dup2(fd[0],0) == -1){
			perror("dup failed");
			exit(1);
			}

		if(close(fd[1]) == -1) {
			perror("Close failed.");
			}

		if (execvp(newstr[0], newstr) == -1) 
			perror("execvp 'in' failed");
	}else if(size == 3) cout<<str[pos+1]<<endl;
	else cout<<"erro: no such file or directory"<<endl;
}

/*void piping(char * newstr[]){
	*int fd[2];
	if (pipe(fd)==-1) {
		perror ("pipe failed");
		exit(1);
	}*
	if(close(fd[0]) == -1) {
		perror("Close failed.");
		}
	if(dup2(fd[1],1) == -1) {
		perror("Dup failed.");
	}
	if(-1 == execvp(newstr[0], newstr)) {
		perror("Execvp failed.");
	}
}*/
void redirect(char * str[], int size){
	int i, j, aux;
	char * newstr[512];	
	for(i=0;i<size;i++){
		int fd[2];
		*glob_flag = 0;	
		aux = 0;
		for(j=i; j<size; j++){
			if (memcmp(str[j], "<\0", 2) == 0){				
				*glob_flag = 6;	
				i = j;
				// if next argument is | i = j+1
				break;
			}
			if (memcmp(str[j], ">\0", 2) == 0){
				*glob_flag = 7;			
				i = j;
				break;
			}
			if (memcmp(str[j], ">>\0", 3) == 0){
				*glob_flag = 8;
				i = j;
				break;
			}
			if (memcmp(str[j], "|\0", 2) == 0){
				*glob_flag = 9;			
				i = j;
				//int fd[2];
				if (pipe(fd)==-1) {
					perror ("pipe failed");
					exit(1);
				}
				break;
			}
			if (memcmp(str[j], "<<<\0", 4) == 0){
				*glob_flag = 10;
				i = j;
				break;
			}
			if (memcmp(str[j], "2>\0", 3) == 0){
				*glob_flag = 11;	
				i = j;
				break;
			}
			if (memcmp(str[j], "2>>\0", 4) == 0){
				*glob_flag = 12;	
				i = j;
				break;
			}
			if (memcmp(str[j], "1>\0", 3) == 0){
				*glob_flag = 7;	
				i = j;
				break;
			}
			if (memcmp(str[j], "1>>\0", 4) == 0){
				*glob_flag = 8;	
				i = j;
				break;
			}
			if (memcmp(str[j], "0>", 2) == 0){
				*glob_flag = 13;	
				i = j;
				break;
			}
			newstr[aux] = str[j];
			aux++;
		}	

		int pid = fork();
		if(pid<0){
			perror("fork() presented error");
			exit(1);		
		}else if(pid==0){
			if(*glob_flag == 6)in_redirect(newstr, str[j+1]);
			else if(*glob_flag == 7)out_redirect(newstr, str[j+1], false, 1);	
			else if(*glob_flag == 8)out_redirect(newstr, str[j+1], true, 1);	
			else if(*glob_flag == 9){
				//piping(newstr);	
				if(close(fd[0]) == -1) {
						perror("Close failed.");
						}
					if(dup2(fd[1],1) == -1) {
						perror("Dup failed.");
					}
					if(-1 == execvp(newstr[0], newstr)) {
						perror("Execvp failed.");
					}
			}else if(*glob_flag == 10) in_redirect2(newstr, i, str, size);	
			else if(*glob_flag == 11) out_redirect(newstr, str[j+1],false, 2);
			else if(*glob_flag == 12) out_redirect(newstr, str[j+1],true, 2);
			else if(*glob_flag == 13) out_redirect(newstr, str[j+1],true, 0);
		
			exit(1);
		}else if(pid>0){
			if(*glob_flag == 9){
			/*	int c_in = dup(0);
				if(c_in == -1) {
					perror("Dup failed.");
				}
				if(close(fd[1]) == -1) {
					perror("Close failed.");
				}
				if(dup2(fd[0],0) == -1) {
					perror("Dup failed.");
				}
				int status;
				wait(&status);
				if (-1 == status){
					perror("wait() presented error");	
				}
			//	if (dup2(c_in,0) == -1) {
			//		perror("Dup failed.");
			//	}
				cout << flush;*/
			}else{
				int status;
				wait(&status);
				if (-1 == status){
					perror("wait() presented error");	
				}
			}
				
		}


		// clear the vector newstr in order to execute new commands
		for (int k = 0; k<aux; k++)newstr[k] = '\0';
	}
}
//checks which procedure it should follows for I/O redirection
int checkline(char *str[], int size){
	for(int i=0; i<size; i++){
		if (memcmp(str[i], "|\0", 2) == 0){			
			return 0;
		}
		if (memcmp(str[i], "<", 1) == 0){			
			return 0;
		}
		if (memcmp(str[i], ">", 1) == 0){				
			return 0;
		}
		if (memcmp(str[i], "2>", 2) == 0){				
			return 0;
		}	
		if (memcmp(str[i], "1>", 2) == 0){				
			return 0;
		}
		if (memcmp(str[i], "0>", 2) == 0){				
			return 0;
		}
	}
	return 1;
}

int main(){
	int index;
	string line;
	char * str[512];
	char * pch;

	while (true){
		do{
			//output login @ machine $
			prompt();
			getline(cin, line);
		} while (line[0] == '#');

		//look for '#', if find erase everything until the end of line
		size_t found = line.find('#');
		if (found != std::string::npos)
			line.erase(found, line.length() - found);

		//create a dynamic char that is a copy of input
		char * input = new char[line.length() + 1];
		strcpy(input, line.c_str());

		//built in function to finish program when typed 'EXIT'
		if (memcmp(input, "exit", 4) == 0) exit(0);


		index = 0;
		pch = strtok(input, " ");

		while (pch != NULL){
			str[index] = pch;
			pch = strtok(NULL, " ");
			index++;
		}

		str[index] = NULL;

	//create a shared memory to be accessed from child and process
	glob_flag = (int*)mmap(NULL, sizeof *glob_flag, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

		
		if(!checkline(str, index)) redirect(str, index);
		else	execute(str, index);		
	}
	return 0;
}
