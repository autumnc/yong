// test-status.c - 测试所有状态
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#define YBUS_TOOL_GET_LANG      2
#define YBUS_TOOL_GET_INDEX     12
#define YBUS_TOOL_GET_BIAODIAN  13
#define YBUS_TOOL_GET_CORNER    14
#define YBUS_TOOL_GET_TRAD      15
#define YBUS_TOOL_GET_ONSPOT    16

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t seq;
    uint16_t len;
    uint16_t flag;
    char method[8];
    uint32_t data[2];
} LCallMsg;

int connect_yong(void) {
    struct sockaddr_un sa;
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/yong-:0");
    
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
        close(s);
        return -1;
    }
    return s;
}

int send_query(int tool, int param) {
    int s = connect_yong();
    if (s < 0) return -1;
    
    LCallMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic = 0x4321;
    msg.seq = 0;
    msg.len = sizeof(LCallMsg);
    msg.flag = 1;
    strncpy(msg.method, "tool", sizeof(msg.method) - 1);
    msg.data[0] = tool;
    msg.data[1] = param;
    
    if (write(s, &msg, sizeof(msg)) <= 0) {
        close(s);
        return -1;
    }
    
    LCallMsg resp;
    memset(&resp, 0, sizeof(resp));
    ssize_t n = read(s, &resp, sizeof(resp));
    close(s);
    
    if (n >= (ssize_t)(sizeof(resp) - 4)) {
        return resp.data[0];
    }
    return -1;
}

int main() {
    printf("=== Yong 输入法实时状态 ===\n\n");
    
    // 循环读取，监控状态变化
    for (int i = 0; i < 10; i++) {
        int lang = send_query(YBUS_TOOL_GET_LANG, 0);
        int biaodian = send_query(YBUS_TOOL_GET_BIAODIAN, 0);
        int corner = send_query(YBUS_TOOL_GET_CORNER, 0);
        int trad = send_query(YBUS_TOOL_GET_TRAD, 0);
        int im_index = send_query(YBUS_TOOL_GET_INDEX, 0);
        
        printf("[%d] 语言:%s 标点:%s 角标:%s 繁简:%s 索引:%d\n",
            i+1,
            lang == 0 ? "中文" : (lang == 1 ? "英文" : "?"),
            biaodian == 0 ? "中文" : (biaodian == 1 ? "英文" : "?"),
            corner == 0 ? "半角" : (corner == 1 ? "全角" : "?"),
            trad == 0 ? "简体" : (trad == 1 ? "繁体" : "?"),
            im_index);
        
        sleep(2);
    }
    
    return 0;
}