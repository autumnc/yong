// im_monitor.c - 独立监控程序
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

#define SHM_KEY 0x594F4E47  // "YONG" 的十六进制

typedef struct {
    int lang;        // 0=中文, 1=英文
    int corner;      // 0=半角, 1=全角
    int biaodian;    // 标点状态
    int trad;        // 繁简状态
    char im_name[64]; // 输入法名称
    int active;      // 是否激活
} IM_STATUS;

int main() {
    int shmid;
    IM_STATUS *status;
    
    // 获取共享内存
    shmid = shmget(SHM_KEY, sizeof(IM_STATUS), 0666);
    if (shmid == -1) {
        printf("输入法未运行或共享内存不可用\n");
        return 1;
    }
    
    status = (IM_STATUS *)shmat(shmid, NULL, 0);
    if (status == (void *)-1) {
        printf("无法附加到共享内存\n");
        return 1;
    }
    
    // 输出状态
    printf("═══════════════════════════════════\n");
    printf("  小狼毫输入法状态监控\n");
    printf("═══════════════════════════════════\n");
    printf("输入法: %s\n", status->im_name[0] ? status->im_name : "默认");
    printf("状态: %s\n", status->active ? "激活" : "未激活");
    printf("语言: %s\n", status->lang == 0 ? "中文" : "英文");
    printf("标点: %s\n", status->corner == 0 ? "半角" : "全角");
    printf("标点类型: %s\n", status->biaodian == 0 ? "中文标点" : "英文标点");
    printf("文字: %s\n", status->trad == 0 ? "简体" : "繁体");
    printf("═══════════════════════════════════\n");
    
    shmdt(status);
    return 0;
}