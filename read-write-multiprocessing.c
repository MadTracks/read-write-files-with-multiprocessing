#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdlib.h>
#include <string.h>

void SIGUSR1_handler(int signa);
void SIGUSR2_handler(int signa);
void SIGINT_handler(int signa);
void parse_string(double * x,double * y,int size,char * string);
double lagrange_interpolation(double * x,double * y,int size,double x_point);
double find_last_value(char * str);
volatile sig_atomic_t interrupt = 1;


int main(int argc, char ** argv){
    
    int fd1=0;
    pid_t process;
    pid_t children[8];
    char string[10000];
    char printstr[10000];
    double x[8],y[8];
    sigset_t cur_mask,old_mask;
    struct sigaction usr1act,usr2act,intact,old1act,old2act,oldintact;

    if(argc!=2){
        fprintf(stderr,"Exactly 1 input needed.\n");
        return -1;
    }
    
    fd1=open(argv[1],O_RDWR);
    if(fd1<=-1){
        fprintf(stderr,"File cannot open.\n");
        return -1;
    }

    usr1act.sa_handler=SIGUSR1_handler;
    usr1act.sa_flags=SA_NODEFER;
    sigaction(SIGUSR1,&usr1act,&old1act);
    usr2act.sa_handler=SIGUSR2_handler;
    usr2act.sa_flags=SA_NODEFER;
    sigaction(SIGUSR2,&usr2act,&old2act);
    intact.sa_handler=SIGINT_handler;
    sigaction(SIGINT,&intact,&oldintact);

    sigemptyset(&cur_mask);
    sigaddset(&cur_mask,SIGUSR1);
    sigaddset(&cur_mask,SIGUSR2);
    int i=0; 
    for(i=0;i<8;i++){
        process=fork();
        if(process!=0){
            children[i]=process;
        }
        else{
            sigprocmask(SIG_BLOCK,&cur_mask,&old_mask);
            while(interrupt>=1){
                sigsuspend(&old_mask);
            }
            sigprocmask(SIG_UNBLOCK,&cur_mask,NULL);
            i=9;
        }
                             
    }
    if(process==0){
        sprintf(printstr,"Children:%d\n",getpid());
        write(1,printstr,strlen(printstr));
        
        char c;
        int p=0;
        off_t cursor;
        cursor=0;
        double y_p;

        struct flock lock;
        memset(&lock, 0, sizeof(lock));
        lock.l_type = F_RDLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"error locking file\n");
        }
        read(fd1,&c,1);
        string[p]=c;
        p++;
        while(c!='\n'){
            read(fd1,&c,1);
            string[p]=c;
            p++;            
        }
        cursor=lseek(fd1,0,SEEK_CUR);
        if(cursor < 0){
            fprintf(stderr,"LSEEK ERROR.\n");
        }
        string[p-1]='\0';
        sprintf(printstr,"Reading Row:%s\n",string);
        write(1,printstr,strlen(printstr));
        lock.l_type = F_UNLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"Error unlocking.\n");
        }
        parse_string(x,y,8,string);
        for(i=0;i<8;i++){
            sprintf(printstr,"x[%d]:%.1lf, y[%d]:%.1lf\n",i,x[i],i,y[i]);
            write(1,printstr,strlen(printstr));
        }
        y_p=lagrange_interpolation(x,y,6,x[7]);
        sprintf(printstr,"Interpolation of x[%.1lf] is %.1lf.\n",x[7],y_p);
        write(1,printstr,strlen(printstr));
        
        
        lock.l_type = F_WRLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"error locking file\n");
        }
        off_t size;
        size=lseek(fd1,0,SEEK_END)-lseek(fd1,cursor-1,SEEK_SET);
        lseek(fd1,cursor-1,SEEK_SET);        
        char buf[1000000];
        read(fd1,buf,size);
        lseek(fd1,cursor-1,SEEK_SET);   
        char tmp[10];
        sprintf(tmp,",%.1lf",y_p);
        write(fd1,tmp,strlen(tmp));
        write(fd1,buf,size);
        cursor+=strlen(tmp);
        lseek(fd1,cursor,SEEK_SET);
        lock.l_type = F_UNLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"Error unlocking.\n");
        }
        kill(getppid(),SIGUSR1);

        interrupt=1;
        sigprocmask(SIG_BLOCK,&cur_mask,&old_mask);
        while(interrupt>=1){
            sigsuspend(&old_mask);
        }
        sigprocmask(SIG_UNBLOCK,&cur_mask,NULL);
        y_p=lagrange_interpolation(x,y,7,x[7]);
        sprintf(printstr,"Interpolation of x[%.1lf] is %.1lf.\n",x[7],y_p);
        write(1,printstr,strlen(printstr));
        lock.l_type = F_WRLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"error locking file\n");
        }

        read(fd1,&c,1);
        while(c!='\n'){
            read(fd1,&c,1);           
        }

        cursor=lseek(fd1,0,SEEK_CUR);
        size=lseek(fd1,0,SEEK_END)-lseek(fd1,cursor-1,SEEK_SET);
        lseek(fd1,cursor-1,SEEK_SET);  
        read(fd1,buf,size);
        lseek(fd1,cursor-1,SEEK_SET);   
        sprintf(tmp,",%.1lf",y_p);
        write(fd1,tmp,strlen(tmp));
        write(fd1,buf,size);
        cursor+=strlen(tmp);
        lseek(fd1,cursor,SEEK_SET);
        lock.l_type = F_UNLCK;
        if(fcntl(fd1, F_SETLKW, &lock) < 0)
        {
            fprintf(stderr,"Error unlocking.\n");
        }
        kill(getppid(),SIGUSR2);
        
        sprintf(printstr,"EXITING CHILDREN %d...\n",getpid());
        write(1,printstr,strlen(printstr));
    }
    else{
        sprintf(printstr,"Parent:%d\n",getpid());        
        write(1,printstr,strlen(printstr));
        write(1,"ROUND 1\n",strlen("ROUND 1\n"));
        for(i=0;i<8;i++){
            //sprintf(printstr,"SIGNAL SENT TO CHILDREN %d\n",children[i]);
            //write(1,printstr,strlen(printstr));
            kill(children[i],SIGUSR1);            
        }
        int temp=8;
        interrupt=1;
        sigprocmask(SIG_BLOCK,&cur_mask,&old_mask);
        while(temp>=1){
            sigsuspend(&old_mask);
            if(interrupt==0){
                interrupt=1;
                temp--;
            }
        }
        sigprocmask(SIG_UNBLOCK,&cur_mask,NULL);
        
        char c;
        int p=0;
        double x[9],y[9];
        lseek(fd1,0,SEEK_SET);
        for(i=0;i<8;i++){
            p=0;
            read(fd1,&c,1);
            string[p]=c;
            p++;
            while(c!='\n'){
                read(fd1,&c,1);
                string[p]=c;
                p++;            
            }
            string[p-1]='\0';          
            parse_string(x,y,9,string);
            sprintf(printstr,"Absolute error of row %d is:%.1lf\n",i,y[7]-x[8]);
            write(1,printstr,strlen(printstr));
        }
        lseek(fd1,0,SEEK_SET);
        
        write(1,"ROUND 2\n",strlen("ROUND 2\n"));
        for(i=0;i<8;i++){
            //sprintf(printstr,"SIGNAL SENT TO CHILDREN %d\n",children[i]);
            //write(1,printstr,strlen(printstr));
            kill(children[i],SIGUSR2);            
        }
        
        int t=0;
        int status;
        for(t=0;t<8;t++){
            waitpid(-1,&status,0);
        }

        lseek(fd1,0,SEEK_SET);
        for(i=0;i<8;i++){
            p=0;
            read(fd1,&c,1);
            string[p]=c;
            p++;
            while(c!='\n'){
                read(fd1,&c,1);
                string[p]=c;
                p++;            
            }
            string[p-1]='\0';          
            parse_string(x,y,9,string);
            sprintf(printstr,"Absolute error of row %d is:%.1lf\n",i,y[7]-y[8]);
            write(1,printstr,strlen(printstr));
        }
        lseek(fd1,0,SEEK_SET);


        write(1,"EXITING PARENT...\n",strlen("EXITING PARENT...\n"));     
    }    
    close(fd1);
    return 0;
}

void SIGUSR1_handler(int signa){
    write(1,"SIGUSR1 recieved.\n",strlen("SIGUSR1 recieved.\n"));
    interrupt=0;
    signal(signa,SIGUSR1_handler);
}
void SIGUSR2_handler(int signa){
    write(1,"SIGUSR2 recieved.\n",strlen("SIGUSR2 recieved.\n"));
    interrupt=0;
    signal(signa,SIGUSR2_handler);
}

void SIGINT_handler(int signa){
    write(1,"Exiting...\n",strlen("Exiting...\n"));
    exit(-1);
}

void parse_string(double * x,double * y,int size,char * string){
    char * temp=string;
    int i=0;
    int rest=0;
    while(i<size){
        sscanf(temp,"%lf%n",&x[i],&rest);
        temp+=rest+1;
        sscanf(temp,"%lf%n",&y[i],&rest);
        temp+=rest+1;
        i++;
    }
}

double lagrange_interpolation(double * x,double * y,int size,double x_point){
    int i,j;
    char str[1000];
    double p;
    double y_point;
    for(i=0;i<size;i++){
        p=1.0;
        for(j=0;j<size;j++){
            if(i!=j){
                p=p*(x_point - x[j])/(x[i] - x[j]);
            }
        }
        y_point = y_point + p * y[i];
        sprintf(str,"Coefficient %d: %lf\n",i,p);
        write(1,str,strlen(str));
    }
    return y_point;
}