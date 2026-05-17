#!/bin/bash

# 小狼毫输入法状态监控脚本

IM_STATUS_SHM_KEY=0x59594D49

# 使用 ipcs 或编写C程序读取共享内存
MONITOR_PROGRAM=$(cat << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

typedef struct {
    unsigned int magic;
    unsigned int version;
    int pid;
    unsigned int update_count;
    unsigned char lang;
    unsigned char corner;
    unsigned char biaodian;
    unsigned char trad;
    unsigned char im_index;
    char im_name[64];
    unsigned char state;
    unsigned char reserved[23];
} IMStatus;

int main() {
    int shm_id;
    IMStatus *status;
    
    shm_id = shmget(0x59594D49, sizeof(IMStatus), 0666);
    if (shm_id == -1) {
        printf("IM Status not available\n");
        return 1;
    }
    
    status = (IMStatus *)shmat(shm_id, NULL, 0);
    if (status == (void *)-1) {
        printf("Failed to attach shared memory\n");
        return 1;
    }
    
    if (status->magic != 0x494D5354) {
        printf("Invalid status data\n");
        shmdt(status);
        return 1;
    }
    
    printf("PID: %d\n", status->pid);
    printf("Update Count: %u\n", status->update_count);
    printf("State: %s\n", status->state ? "ON" : "OFF");
    printf("Language: %s\n", status->lang ? "English" : "Chinese");
    printf("Corner: %s\n", status->corner ? "Full" : "Half");
    printf("Punctuation: %s\n", status->biaodian ? "English" : "Chinese");
    printf("Trad/Simp: %s\n", status->trad ? "Traditional" : "Simplified");
    printf("IM Index: %d\n", status->im_index);
    printf("IM Name: %s\n", status->im_name);
    
    shmdt(status);
    return 0;
}
EOF
)

# 编译并运行监控程序
gcc -x c -o /tmp/im_monitor_temp - <<< "$MONITOR_PROGRAM" && /tmp/im_monitor_temp
rm -f /tmp/im_monitor_temp