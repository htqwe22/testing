#ifndef  __HT__FILEOPS__H
#define __HT__FILEOPS__H

#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif


#define CONF_DIR	"./configure"
#define SMTP_CONFIG_FILE "smtp.txt"
#define CLIENT_INFO_FILE "client.txt"
#define MSG_DIR		"./message"
#define READ_LEN 256
#define DEF_PORT "25"
smtp_config_t * get_smtp_info(void);
void get_clients_info(struct list_head *client_head);

int pack_data(char *buf,int nRead,int newfile ,client_info_t *client);

#ifdef __cplusplus
}
#endif


#endif
