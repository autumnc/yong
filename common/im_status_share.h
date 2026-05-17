#ifndef IM_STATUS_SHARE_H
#define IM_STATUS_SHARE_H

#include <stdint.h>
#include <sys/types.h>

#define IM_STATUS_SHM_KEY 0x59594D49  // "YYMI"
#define IM_STATUS_SHM_SIZE sizeof(IMStatus)

typedef struct {
    uint32_t magic;           // 魔数，用于验证 0x494D5354
    uint32_t version;         // 版本号 1
    pid_t pid;                // 输入法进程ID
    uint32_t update_count;    // 更新计数
    
    // 状态字段
    uint8_t lang;             // 0=中文, 1=英文
    uint8_t corner;           // 0=半角, 1=全角
    uint8_t biaodian;         // 0=中文标点, 1=英文标点
    uint8_t trad;             // 0=简体, 1=繁体
    uint8_t im_index;         // 当前输入法索引
    char im_name[64];         // 当前输入法名称
    uint8_t state;            // 0=关闭, 1=开启
    uint8_t reserved[23];     // 保留字段
} IMStatus;

// 初始化共享内存
int im_status_init(void);

// 更新状态到共享内存
void im_status_update(void);

// 销毁共享内存
void im_status_destroy(void);

#endif