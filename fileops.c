#include "fileops.h"

typedef struct{
	char file_name[128];
	struct list_head list;
}msg_file_t;


static  void skip_blank(char *const s,int len){
/*
	if(!s){
		ebug("skip_blank:NULL point not support\n");
		return;
	}
*/
	u8 begin=0;
	char out[256];
	memset(out,0,sizeof(out));
	u8 i=0;
	char *p=s;
	while(*p){
		if(*p == '\"'){
			if(*(p-1) != '\\'){
				p++;
				begin++;
			}else{
				i--;
			}
		}
		if(begin%2 == 0){	
			if(*p==' ' || *p == '\t' || *p == '\n' || *p=='\r' || *p == '\v' || *p == '\b' ) {
				p++;
			}else{
				if(*p == '#'){
					out[i]= '\0';
					break;
				}
				out[i++]=*p++;
			}
		}else{
			if(*p=='\\' && *(p+1)=='n'){
				out[i++]='\r';
				out[i++]='\n';
				p+=2;
			}else{
				out[i++]=*p++;
			}
		}
	}
	if(begin%2){	
//		ebug("please check the configure file in the line which start charactor is %s\r\n",s);
		p=out;
		while(*p) if(*p++ == '#') *(p-1)=0;
	}
	memset(s,0,len);
	memcpy(s,out,strlen(out));
}

/*
 * Return 0:this line has no vailed infomation
 * Return -1: the end of this file
 * >0: has information that we may need
 */
static int get_one_line(FILE * fp,char *line,int len,int skip){
	if(fgets(line,len,fp)){
		if(skip)
			skip_blank(line,len);
		return *line;
	}
	return -1;
}
static char * split_key_value(char * line){
//	debug("get str:%s \n",line);
	char *pos=strchr(line,'=');
	if(pos) *pos++=0;	//++ point to the real value
	return pos;
}

static int memcmpi(const void *s1,const void *s2,int n){
	int i=0,ret=0;
	while(i<n){
		ret=toupper(*((u8 *)s1++))-toupper(*((u8 *)s2++));
		if(0==ret);
		else break;
		i++;
	}
	return ret;
}

static int make_dir(char *dir){
	DIR *pdir=opendir(dir);
	if(pdir){
		closedir(pdir);
	}else{
		if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)){
			debug("create log dir failed\n");
			return -1;
		}
	}
	return 0;
}
FILE * read_server_config(){
#define SIZE	256
	FILE *fp;
//	char server_info[SIZE];
	char server_path[SIZE];
	memset(server_path,0,SIZE);
	make_dir(CONF_DIR);
	sprintf(server_path,"%s/%s",CONF_DIR,SMTP_CONFIG_FILE);
	fp=fopen(server_path,"r");
	if(fp==NULL){
		debug("read server configure failed :%s\n",strerror(errno));
		return NULL;
	}
	return fp;
}

static char * get_client_item(char * line){
	char *head=NULL;
	char *tail = NULL;
	while(*line){
		if(*line == '#') break;
		if(*line == '[') head=line+1;
		if(*line == ']') {
			tail=line;
			*tail=0;
		}
		line++;
	}
	if(tail > head) return head;
	return NULL;;
}

static void del_broken_client(struct list_head *client_head){
	client_info_t *pos=NULL;
	list_for_each_entry(pos, client_head, list){
		if(pos->email[0] == '\0'){
			list_del(&pos->list);
			free(pos);
		}
	}
}
#if DEBUG
void show_client_info(struct list_head *client_head){
	client_info_t *pos=NULL;
	int num=0;
	list_for_each_entry(pos, client_head, list){
		debug("\r\n email:%s\r\n fromName:%s\r\n toName %s\r\n SubJect:%s\r\n begin:%s\r\n end:%s\r\n signature:%s\r\n\r\n",\
			pos->email,pos->fromName,pos->toName,pos->subJect,pos->begin,pos->end,pos->sign);
		num++;
	}
	debug("has total clients %d \n",num);
}
#endif

/* to remember the return value should be freeed when don`t need it anymore*/
smtp_config_t * get_smtp_info(void){
	FILE * fp;
	char line[READ_LEN];
	char *value=NULL;
	smtp_config_t *smtp=NULL;
	memset(line,0,sizeof(line));
	/* use line buffer temporarily */
	sprintf(line,"%s/%s",CONF_DIR,SMTP_CONFIG_FILE);
	fp = fopen(line, "r");
	if (fp == NULL) {
		ebug("open router config failed\n");
		return NULL;
	}

	smtp=(smtp_config_t*)malloc(sizeof(smtp_config_t));
	if(!smtp){
		ebug("malloc smtp structure failed\n");
		return NULL;
	}
	memset(smtp,0,sizeof(smtp_config_t));
	// set default value 
	memcpy(smtp->port,DEF_PORT,strlen(DEF_PORT));
	
	memset(line,0,sizeof(line));
	while( -1 != get_one_line(fp,line,READ_LEN,1)){
		value=split_key_value(line);
		if(value){
			if(!memcmpi("smtp",line,strlen(line)))
				memcpy(smtp->smtp,value,strlen(value));
			else if(!memcmpi("port",line,strlen(line)))
				memcpy(smtp->port,value,strlen(value));
			else if(!memcmpi("account",line,strlen(line)))
				memcpy(smtp->account,value,strlen(value));
			else if(!memcmpi("passwd",line,strlen(line)))
				memcpy(smtp->passwd,value,strlen(value));
		}
	}
	debug("get smtp server info:\r\nsmtp:%s\r\nport:%s\r\naccount:%s\r\npasswd:%s\r\n\r\n",smtp->smtp,\
		smtp->port,smtp->account,smtp->passwd);
	fclose(fp);
	if( !smtp->smtp[0] || !smtp->port[0] || !smtp->account[0] || !smtp->passwd[0]){
		ebug("get server information not compelted\n");
		free(smtp);
		smtp=NULL;
	}
	return smtp;
}

void get_clients_info(struct list_head *client_head){
	
	FILE * fp;
	char line[READ_LEN];
	char *value=NULL;
	client_info_t *client=NULL;
	char *item=NULL;
	
	memset(line,0,sizeof(line));
	/* use line buffer temporarily */
	sprintf(line,"%s/%s",CONF_DIR,CLIENT_INFO_FILE);
	fp = fopen(line, "r");
	if (fp == NULL) {
		ebug("open client config failed\n");
		return ;
	}
	memset(line,0,sizeof(line));
	
	while( -1 != get_one_line(fp,line,READ_LEN,0)){
//		debug("read line %s\r\n",line);
		item=get_client_item(line);
		if(item){
//			debug("find client :%s \r\n",item);
			client=(client_info_t*)malloc(sizeof(client_info_t));
			if(client){
				memset(client,0,sizeof(client_info_t));
				list_add_tail(&client->list,client_head);
			}
		}else{
//			debug("get info %s \r\n",line);
			skip_blank(line,READ_LEN);
			value=split_key_value(line);
			if(client && value){
//				debug("find key:%s value:%s\r\n",line,value);

				if(!memcmpi("email",line,strlen(line)))
					memcpy(client->email,value,strlen(value));
				else if(!memcmpi("fromName",line,strlen(line)))
					memcpy(client->fromName,value,strlen(value));
				else if(!memcmpi("toName",line,strlen(line)))
					memcpy(client->toName,value,strlen(value));
				else if(!memcmpi("subJect",line,strlen(line)))
					memcpy(client->subJect,value,strlen(value));
				else if(!memcmpi("begining",line,strlen(line)))
					memcpy(client->begin,value,strlen(value));
				else if(!memcmpi("ending",line,strlen(line)))
					memcpy(client->end,value,strlen(value));
				else if(!memcmpi("signature",line,strlen(line)))
					memcpy(client->sign,value,strlen(value));
				else if(!memcmpi("msgFile",line,strlen(line)))
					memcpy(client->msgFile,value,strlen(value));
			}
		}

	}
	fclose(fp);
	del_broken_client(client_head);
#if DEBUG
	show_client_info(client_head);
#endif
}

LIST_HEAD(list_msg_file);
static void get_all_files(char *dir_name,struct list_head *head){
	DIR *dp;
	struct dirent *dirp;
	msg_file_t *msg_file=NULL;
	char d_name[128];
	memset(d_name,0,sizeof(d_name));
	
	if((dp=opendir(dir_name))==NULL){
		ebug("opendir %s failed\r\n",dir_name);
		return;
	}
	while((dirp=readdir(dp)) !=NULL){
		if(dirp->d_type == DT_REG && memcmpi(dirp->d_name,"ReadMe.txt",strlen(dirp->d_name))){
			msg_file=(msg_file_t *)malloc(sizeof(msg_file_t));
			if(msg_file){
				memset(msg_file->file_name, 0, 128);
				sprintf(msg_file->file_name,"%s/%s",dir_name,dirp->d_name);
				debug("##################### find files %s\r\n",msg_file->file_name);

				list_add_tail(&msg_file->list, head);	//add it to 
			}
		}
		if(dirp->d_type == DT_DIR &&  memcmp(dirp->d_name,".",strlen(dirp->d_name)) &&  memcmp(dirp->d_name,"..",strlen(dirp->d_name))){
			sprintf(d_name,"%s/%s",dir_name,dirp->d_name);
			get_all_files(d_name,head);
		}
	}
	closedir(dp);
}
int pack_data(char *buf,int nRead,int newfile ,client_info_t *client){
	static  int files_num=0;
	msg_file_t *msg_file = list_entry(list_msg_file.next,msg_file_t , list);
//	int nRead=len-strlen(buf);
	int find_file=0;
	static FILE *fd=NULL;
	int rand_index;
	if(!files_num){
		get_all_files(MSG_DIR,&list_msg_file);
	 	list_for_each_entry(msg_file,&list_msg_file,list) files_num++;
		debug("get %d msg new files %d\r\n",files_num,newfile);

	}

	if(files_num){
		if(!newfile){	//start read an new file
//			debug("rand_index = %d \r\n",rand_index);
			if(client->msgFile[0]){
				list_for_each_entry(msg_file,&list_msg_file,list){
					if(!memcmpi(client->msgFile,msg_file->file_name, strlen(client->msgFile))){
						find_file=1;
						break;
					}
				}
			}
			if(!find_file){
				srand(time(0)%1000);
				rand_index=rand()%files_num;
				do{
					msg_file = list_entry(msg_file->list.next,msg_file_t , list);
				}while(0<rand_index--);
			}

//			debug("msg_file :%p while open file %s\n",msg_file,msg_file->file_name);
			fd=fopen(msg_file->file_name,"r");
			if(fd != NULL){
				return fread(buf,1,nRead,fd);
			}
		}else{	//keep on reading last file
			return fread(buf,1,nRead,fd);
			#if 0
			if(feof(fd)) {
				fclose(fd);
				return 0;
			}
			debug("message not end\n");
			return fread(buf,1,nRead,fd);
			#endif

		}
	}
	return 0;	
}





