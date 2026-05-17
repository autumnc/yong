// yong-vim-fixed.c - 修复32位兼容性问题
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/time.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 使用 __attribute__((packed)) 确保结构体在32/64位下大小一致
typedef struct __attribute__((packed)) {
    uint16_t magic;      // 0x4321
    uint16_t seq;        // 序列号
    uint16_t len;        // 消息长度
    uint16_t flag;       // 标志位 (0=不等待响应, 1=等待响应)
    char method[8];      // 方法名，如 "tool"
    uint32_t data[2];    // 参数数据
} LCallMsg;

// 验证结构体大小
#define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]
STATIC_ASSERT(sizeof(LCallMsg) == 24, sizeof_LCallMsg_should_be_24);

static void l_call_build_path(char *path)
{
    char *p;
    p = getenv("DISPLAY");
    if (!p) {
        // 如果没有 DISPLAY，尝试默认值
        sprintf(path, "/tmp/yong-:0");
        return;
    }
    sprintf(path, "/tmp/yong-%s", p);
    p = strchr(path, '.');
    if (p) *p = 0;
}

int l_call_connect(void)
{
    struct sockaddr_un sa;
    int s;
    struct timeval timeo;
    
    timeo.tv_sec = 1;
    timeo.tv_usec = 0;
    
    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    l_call_build_path(sa.sun_path);
    
    // 确保字符串以 null 结尾
    sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';
    
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
        // 尝试备用路径
        strcpy(sa.sun_path, "/tmp/yong-:0");
        if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
            perror("connect");
            close(s);
            return -1;
        }
    }
    
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
    
    return s;
}

int main(int argc, char *argv[])
{
    int s;
    int ret;
    int i;
    int res = -1;
    int wait_result = 0;
    int tool = 1;
    int cmd = 0;
    LCallMsg msg;
    
    // 初始化消息结构
    memset(&msg, 0, sizeof(msg));
    msg.magic = 0x4321;
    msg.seq = 0;
    msg.len = sizeof(LCallMsg);
    msg.flag = 0;
    strncpy(msg.method, "tool", sizeof(msg.method) - 1);
    msg.data[0] = 1;  // 默认 tool = 1
    msg.data[1] = 0;  // 默认 cmd = 0
    
    // 解析命令行参数
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--reload-all") == 0) {
            msg.flag = 1;
            msg.data[0] = 7;  // RELOAD_ALL
            cmd = -1;  // 特殊标记
        }
        else if (strcmp(argv[i], "-w") == 0) {
            msg.flag = 1;  // 等待响应
            wait_result = 1;
        }
        else if (strcmp(argv[i], "-t") == 0 && i < argc - 1) {
            tool = atoi(argv[++i]);
            msg.data[0] = tool;
        }
        else {
            // 参数是数字
            cmd = atoi(argv[i]);
            msg.data[1] = cmd;
        }
    }
    
    // 连接 socket
    s = l_call_connect();
    if (s < 0) {
        fprintf(stderr, "无法连接到 Yong 输入法，请确保 yong 正在运行\n");
        return -1;
    }
    
    // 发送消息
    ret = write(s, &msg, sizeof(msg));
    if (ret <= 0) {
        perror("write");
        close(s);
        return -1;
    }
    
    // 如果需要等待响应
    if (msg.flag != 0) {
        LCallMsg response;
        memset(&response, 0, sizeof(response));
        ret = read(s, &response, sizeof(response));
        if (ret >= (int)(sizeof(response) - 4)) {
            res = response.data[0];
        } else if (ret > 0) {
            // 部分读取，尝试重新读取
            fprintf(stderr, "部分读取: %d 字节\n", ret);
        }
    }
    
    close(s);
    
    // 输出结果
    if (res >= 0) {
        printf("%d\n", res);
    }
    
    return res < 0 ? -1 : 0;
}