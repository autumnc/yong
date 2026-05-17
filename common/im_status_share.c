#include "im_status_share.h"
#include <sys/shm.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int shm_id = -1;
static IMStatus *shared_status = NULL;

int im_status_init(void)
{
    // 尝试创建或获取共享内存
    shm_id = shmget(IM_STATUS_SHM_KEY, IM_STATUS_SHM_SIZE, 
                    0666 | IPC_CREAT);
    if (shm_id == -1) {
        fprintf(stderr, "Failed to create shared memory: %s\n", strerror(errno));
        return -1;
    }
    
    // 映射共享内存
    shared_status = (IMStatus *)shmat(shm_id, NULL, 0);
    if (shared_status == (void *)-1) {
        fprintf(stderr, "Failed to attach shared memory: %s\n", strerror(errno));
        return -1;
    }
    
    // 初始化
    if (shared_status->magic != 0x494D5354) {
        memset(shared_status, 0, IM_STATUS_SHM_SIZE);
        shared_status->magic = 0x494D5354;
        shared_status->version = 1;
        shared_status->pid = getpid();
    }
    
    return 0;
}

void im_status_update(void)
{
    if (!shared_status) return;
    
    CONNECT_ID *id = y_xim_get_connect();
    
    shared_status->update_count++;
    shared_status->pid = getpid();
    
    if (id) {
        shared_status->state = id->state;
        shared_status->lang = id->lang;
        shared_status->corner = id->corner;
        shared_status->biaodian = id->biaodian;
        shared_status->trad = id->trad;
        
        y_xim_put_connect(id);
    } else {
        shared_status->state = 0;
        shared_status->lang = 0;
        shared_status->corner = 0;
        shared_status->biaodian = 0;
        shared_status->trad = 0;
    }
    
    shared_status->im_index = im.Index;
    
    // 获取输入法名称
    char *name = y_im_get_im_name(im.Index);
    if (name) {
        strncpy(shared_status->im_name, name, sizeof(shared_status->im_name) - 1);
        shared_status->im_name[sizeof(shared_status->im_name) - 1] = '\0';
        l_free(name);
    } else {
        strcpy(shared_status->im_name, "Unknown");
    }
}

void im_status_destroy(void)
{
    if (shared_status && shared_status != (void *)-1) {
        shmdt(shared_status);
    }
    if (shm_id != -1) {
        // 注意：不删除共享内存，以便其他进程可以继续读取
        // shmctl(shm_id, IPC_RMID, NULL);
    }
}