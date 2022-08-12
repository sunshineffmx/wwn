#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <libpmem.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include "debug.h"
#include "continuity_hashing.h"

#define RDMA_BUFFER_SIZE 64
#define QP_DEPTH 16000
#define QP_NUMBER 1

enum MSG_TYPE {
    REGRBUF,
    REGRBUFOK,
    REGTABLE,
    REGTABLEOK,
    DELETEOK,
    DELETEFAIL,
    INSERTOK,
    INSERTFAIL,
    UPDATEOK,
    UPDATEFAIL,
    GORUNPHASE,                 // 进入run phase
    REGNUMBEROFWRITES,          // 查询写次数(client发送)
    REGNUMBEROFWRITESOK,        // 返回写次数消息(server发送)
    WS
}; 
enum CONN_STATE {
    INIT,
    ESTABLISHED,
    RECVMR,
    FINISH
};

struct message {
    enum MSG_TYPE type;
    uint64_t segment_num;       // 哈希表中的segment_num和seed
    uint64_t seed;
    int buffer_num;             // 客户端申请的用于imm write的buffer总数（等于thread_num) 
    uint32_t thread_id;         // 客户端传输这个message的线程id
    struct ibv_mr mr;
    uint64_t number_of_writes;  // 写NVM的次数
};

struct exchangeMeta {
    //uint32_t rkey;
    uint16_t lid;
    uint32_t qpn;
    uint32_t psn;
    //union ibv_gid gid;
    uint8_t gid[16];
};

struct connection {
    struct ibv_qp *qp;

    int sockfd;
    //uint32_t rkey;
    uint16_t lid;
    uint32_t qpn;
    uint32_t dest_psn;
    uint32_t my_psn;
    //union ibv_gid gid;
    uint8_t gid[16];

    struct ibv_mr *recv_mr;             // for ibv_post_recv
    struct ibv_mr *send_mr;             // for ibv_post_send
    struct ibv_mr *buffer_mr;           // 用于imm_write
    struct ibv_mr *read_mr;             // 用于注册哈希表

    //struct ibv_mr peer_mr;

    struct message *recv_region;
    struct message *send_region;
    char *buffer;
    char* read_region;

    //enum CONN_STATE state;
};

struct context {
    struct ibv_context *ctx;
    uint32_t port;
    uint32_t server_port;
    struct ibv_port_attr port_attribute;
    int gidx;
    struct ibv_pd *pd;
    struct ibv_comp_channel *comp_channel;
    struct ibv_cq **cq;
    int cq_num;
    int cq_ptr;
    pthread_t *cq_poller_thread;
    //pthread_t deal_buffer_thread;
    pthread_t socket_thread;
    struct connection *conn_array[10];      // 用来保存多个客户端的信息
};

static struct context *s_ctx = NULL;
static int ID = 0;
static continuity_hash *cont_hash_table = NULL;
static uint64_t hash_init_size = 15;
static uint64_t acnt = 0;

void create_resources(char* device_name, uint32_t server_port, int cq_num);
void socket_listen();
void* socket_accept(void *args);
void register_memory(struct connection* conn);
void register_table(struct connection *conn);
void register_buffer(struct connection *conn, int buffer_num);
void post_receives(struct connection* conn);
int create_qp(struct connection *conn);
int exchange_conn_data(int sockfd, int size, char* my_meta, char* dest_meta);
int change_qp_to_INIT(struct ibv_qp *qp);
int change_qp_to_RTR(struct ibv_qp *qp, uint32_t dest_qp_num, uint16_t dest_local_id, uint32_t dest_psn, uint8_t *dest_gid);
int change_qp_to_RTS(struct ibv_qp *qp, uint32_t my_psn);
int destroy_resources();
void* poll_cq(void *args);
void on_completion(struct ibv_wc *wc);
void send_msg(struct connection *conn);
void* deal_buffer(void* args);

int main(int argc, char **argv) {
    create_resources(NULL, 5678, 2);
    s_ctx->cq_poller_thread = (pthread_t*)malloc(s_ctx->cq_num * sizeof(pthread_t));
    // 因为pthread_create传的参数不能传一个会变化的值（不能直接传i）
    int *temparr = (int*)malloc(s_ctx->cq_num * sizeof(int));
    int i;
    for (i = 0; i < s_ctx->cq_num; i++)
        temparr[i] = i; 
    for (i = 0; i < s_ctx->cq_num; i++) {
        pthread_create(&s_ctx->cq_poller_thread[i], NULL, poll_cq, &temparr[i]);
	//pthread_create(&s_ctx->cq_poller_thread[i], NULL, poll_cq, NULL);
    }
    memset(buffer, 0, sizeof(buffer));
    //pthread_create(&s_ctx->deal_buffer_thread, NULL, deal_buffer, NULL);
    socket_listen();
    while(1);
    destroy_resources();
    free(temparr);
}

// NULL, 1025, 2
void create_resources(char* device_name, uint32_t server_port, int cq_num) {
    /*
    * 1.create an Infiniband context
    */
    struct ibv_context* context = NULL;
    int num_devices;
    // get a list of devices available on the system
    struct ibv_device** device_list = ibv_get_device_list(&num_devices);
    if (!device_list) {
        printError("get ib devices list failed.");
    }
    if (!num_devices) {
        printError("found %d device(s).", num_devices);
    }
    s_ctx = (struct context *)malloc(sizeof(struct context));
    // 每个连接（客户端）的ID，也为当前的连接数
    ID = 0;
    s_ctx->ctx = NULL;
    int i;
    for (i = 0; i < num_devices; i++) {
        if (!device_name) {
            device_name = strdup(ibv_get_device_name(device_list[i]));
        }
        if (!strcmp(ibv_get_device_name(device_list[i]), device_name)) {
            // get a verbs context that can be used for all other verb operations
            s_ctx->ctx = ibv_open_device(device_list[i]);
            break;
        }
    }
    if (!s_ctx->ctx) {
        printError("find device failed.");
    }
    ibv_free_device_list(device_list);

    s_ctx->port = 1;
    s_ctx->server_port = server_port;
    // retrieves the attributes associated with a port
    if (ibv_query_port(s_ctx->ctx, s_ctx->port, &s_ctx->port_attribute)) {
        printError("ibv_query_port failed.");
    }
    s_ctx->gidx = 0;

    struct ibv_device_attr device_attr;
    ibv_query_device(s_ctx->ctx, &device_attr);
    printInfo("max_cq: %d, max_qp: %d, max_qp_wr: %d, max_mr: %d", device_attr.max_cq, device_attr.max_qp, device_attr.max_qp_wr, device_attr.max_mr);

    /*
    * 2.create a protection domain
    */
    s_ctx->pd = ibv_alloc_pd(s_ctx->ctx);
    if (!s_ctx->pd) {
        printError("allocate protection domain failed.");
    }

    /*
    * 3.create completion queue
    */
    s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx);
    if (!s_ctx->comp_channel) {
        printError("create completion channnel failed.");
    }

    s_ctx->cq_num = cq_num;
    s_ctx->cq_ptr = 0;
    s_ctx->cq = (struct ibv_cq **)malloc(cq_num * sizeof(struct ibv_cq *));
    for (i = 0; i < cq_num; i++) {
        s_ctx->cq[i] = ibv_create_cq(s_ctx->ctx, QP_DEPTH, NULL, s_ctx->comp_channel, 0);
        if (!s_ctx->cq[i]) {
            printError("create completion queue failed.");
        }
	if (ibv_req_notify_cq(s_ctx->cq[i], 0)) {
            printError("ibv_req_notify_cq failed.");
        }
    }

    /*
    * to do: call init function for hash table, before connection established
    */
    //第二个参数为write_latency
    init_pflush(2000, 300);
    cont_hash_table = continuity_init(hash_init_size);
    printInfo("cont_hash_table, segment_num: %d, seed: %d.", cont_hash_table->segment_num, cont_hash_table->seed);
}

// listen the client connection and accept
void socket_listen() {
    struct sockaddr_in myaddr;
    int sockfd;
    int on = 1;
    // socket initialization
    memset(&myaddr, 0, sizeof(myaddr));
    // 协议簇，TCP/IP协议
    myaddr.sin_family = AF_INET;
    /*
    * IP地址，当服务器的监听地址是INADDR_ANY时含义是让服务器端计算机上的所有网卡的IP地址都可以作为服务器IP地址，
    * 也即监听外部客户端程序发送到服务器端所有网卡的网络请求
    */
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(s_ctx->server_port);

    // 对应于普通文件的打开操作，创建一个socket描述符，它唯一标识一个socket
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printError("create socket failed.");
    }
    // 设置closesocket（一般不会立即关闭而经历TIME_WAIT的过程）后想继续重用该socket
    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
        printError("setsockopt failed.");
    }
    // 绑定地址
    if (bind(sockfd, (struct sockaddr*)&myaddr, sizeof(struct sockaddr)) < 0) {
        printError("socket bind failed.");
    }
    // 监听连接请求，第二个param表示监听队列中允许保持的尚未处理的最大连接数
    listen(sockfd, 5);

    fprintf(stderr, "listening on port: %d.\n", s_ctx->server_port);

    if (pthread_create(&s_ctx->socket_thread, NULL, socket_accept, &sockfd)) {
        printError("create socket accept thread failed.");
    }
}

// 处理连接请求的线程
void* socket_accept(void *args) {
    int sockfd = (int)(*((int*)args));
    struct sockaddr_in remoteaddr;
    socklen_t sin_size = sizeof(struct sockaddr_in);
    int connfd;
    struct connection *conn;
    // 接受客户端的连接请求，返回值是一个新的套接字描述符，它代表的是和客户端的新的连接
    while((connfd = accept(sockfd, (struct sockaddr *)&remoteaddr, &sin_size)) != -1) {
        printInfo("accept a connection.");
        conn = (struct connection *)malloc(sizeof(struct connection));
        conn->sockfd = connfd;

        if (create_qp(conn)) {
            printError("create_qp failed.");
        }
        else {
            /*
            * 一个struct connection的建立相当于有一个客户端的连接，如果要保存多个客户端的连接，s_ctx里应该有一个conn的数组
            */
            s_ctx->conn_array[ID] = conn;
            ID++;
            // 7.register memory region
            register_memory(conn);
	    post_receives(conn);
        }
    }
}

void register_memory(struct connection* conn) {
    conn->send_region = (struct message *)malloc(sizeof(struct message));
    conn->recv_region = (struct message *)malloc(sizeof(struct message));

    conn->send_mr = ibv_reg_mr(s_ctx->pd, conn->send_region, sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (conn->send_mr == NULL) {
        printError("register send_mr failed.");
    }
    
    conn->recv_mr = ibv_reg_mr(s_ctx->pd, conn->recv_region, sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (conn->recv_mr == NULL) {
        printError("register recv_mr failed.");
    }
}

void register_table(struct connection *conn) {
    //注册哈希表的segments区域（数据区域）
    conn->read_region = (char *)cont_hash_table->segments;
    size_t len = cont_hash_table->segment_num * sizeof(continuity_segment);     // 这里的len值应该是这样的吧

    conn->read_mr = ibv_reg_mr(s_ctx->pd, conn->read_region, len, 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    if (!conn->read_mr) {
        printError("register read_mr failed.");
    }
    printInfo("read_mr addr: %d, len: %d", (uintptr_t)conn->read_mr->addr, len);
}

void register_buffer(struct connection *conn, int buffer_num) {
    if (buffer_num <= 0) { 
        printError("buffer_num error: %d.", buffer_num);
    }

    printInfo("buffer_num: %d.", buffer_num);
    conn->buffer = malloc(buffer_num * RDMA_BUFFER_SIZE);
    memset(conn->buffer, 0, buffer_num * RDMA_BUFFER_SIZE);
    conn->buffer_mr = ibv_reg_mr(s_ctx->pd, conn->buffer, buffer_num * RDMA_BUFFER_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    if (!conn->buffer_mr) {
        printError("register buffer_mr failed.");
    }
    printInfo("buffer_mr addr: %d, len: %d", (uintptr_t)conn->buffer_mr->addr, buffer_num * RDMA_BUFFER_SIZE);
}

void post_receives(struct connection* conn) {
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;
    
    memset(&wr,0,sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)conn->recv_region;
    sge.length = sizeof(struct message);
    sge.lkey = conn->recv_mr->lkey;

    if (ibv_post_recv(conn->qp, &wr, &bad_wr)) {
        printError("post rdma_recv failed.");
    }
}

int create_qp(struct connection *conn) {
    /*
    * 4.create queue pair
    */
    struct ibv_qp_init_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_type = IBV_QPT_RC;
    attr.send_cq = s_ctx->cq[s_ctx->cq_ptr];
    attr.recv_cq = s_ctx->cq[s_ctx->cq_ptr];

    attr.cap.max_send_wr = QP_DEPTH;
    attr.cap.max_recv_wr = QP_DEPTH;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;

    attr.sq_sig_all = 0;

    conn->qp = ibv_create_qp(s_ctx->pd, &attr);
    if (!conn->qp) {
        printError("create queue pair failed.");
    }
    s_ctx->cq_ptr += 1;
    if (s_ctx->cq_ptr >= s_ctx->cq_num)
        s_ctx->cq_ptr = 0;

    /*
    * 5.exchange identifier information to establish connection
    */
    struct exchangeMeta my_meta, dest_meta;
    memset(my_meta.gid, 0, 16);
    memset(dest_meta.gid, 0, 16);
    union ibv_gid mygid;
    // my_meta里的数据都是要发送给客户端的
    my_meta.lid = s_ctx->port_attribute.lid;
    if (s_ctx->gidx >= 0) {
        // retrieve an entry in the port’s global identifier (GID) table
        if (ibv_query_gid(s_ctx->ctx, s_ctx->port, s_ctx->gidx, &mygid)) {
            printError("can't get gid of index: %d.", s_ctx->gidx);
        }
    }
    else {
        memset(&mygid, 0, sizeof(mygid));
    }
    memcpy(my_meta.gid, &mygid, 16);
    my_meta.qpn = conn->qp->qp_num;
    //my_meta.psn = lrand48() & 0xffffff;       // 这个值来自于https://github.com/hustcat/rdma-core/blob/v15/libibverbs/examples/rc_pingpong.c
    my_meta.psn = 3185;
    conn->my_psn = my_meta.psn;
    printf("my lid: %u, psn: %lu.\n", my_meta.lid, my_meta.psn);

    if (exchange_conn_data(conn->sockfd, sizeof(struct exchangeMeta), (char *)&my_meta, (char *)&dest_meta) < 0) {
        printError("exchange identifier information failed.");
    }
    //conn->rkey = dest_meta.rkey;
    conn->qpn = dest_meta.qpn;
    printInfo("dest lid: %u, psn: %lu.", dest_meta.lid, dest_meta.psn);
    conn->lid = dest_meta.lid;
    //conn->gid = dest_meta.gid;
    memcpy(conn->gid, dest_meta.gid, 16);
    conn->dest_psn = dest_meta.psn;

    /*
    * 6.change queue pair state
    */
    int ret;
    ret = change_qp_to_INIT(conn->qp);
    if (ret) {
        printError("change qp state to INIT failed.");
    }
    ret = change_qp_to_RTR(conn->qp, conn->qpn, conn->lid, conn->dest_psn, conn->gid);
    if (ret) {
        printError("change qp state to RTR failed.");
    }
    ret = change_qp_to_RTS(conn->qp, conn->my_psn);
    if (ret) {
        printError("change qp state to RTS failed.");
    }
    return 0;
}

int exchange_conn_data(int sockfd, int size, char* my_meta, char* dest_meta) {
    printInfo("exchange_conn_data function.");
    if (write(sockfd, my_meta, size) < size) {
        printError("write operation failed in exchange_conn_data function.");
    }
    int rc = 0, read_bytes = 0, total_read_bytes = 0;
    while (!rc && total_read_bytes < size) {
        read_bytes = read(sockfd, dest_meta, size);
        if (read_bytes > 0) {
            total_read_bytes += read_bytes;
        } else {
            rc = read_bytes;
        }
    }
    return rc;
}

int change_qp_to_INIT(struct ibv_qp *qp) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = s_ctx->port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    return ibv_modify_qp(qp, &attr,
        IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
}

int change_qp_to_RTR(struct ibv_qp *qp, uint32_t dest_qp_num, uint16_t dest_local_id, uint32_t dest_psn, uint8_t *dest_gid) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_512;
    // IBV_QP_RQ_PSN starting receive packet sequence number (should match remote QP’s sq_psn)
    attr.rq_psn = dest_psn;
    // The number of RDMA Reads & atomic operations outstanding at any time that can be handled by this QP as a destination.
    attr.max_dest_rd_atomic = 16;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dest_local_id;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = s_ctx->port;
    attr.dest_qp_num = dest_qp_num;
    if (s_ctx->gidx >= 0) {
        attr.ah_attr.is_global = 1;
        //attr.ah_attr.grh.dgid = dest_gid; 
        memcpy(&attr.ah_attr.grh.dgid, dest_gid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = s_ctx->gidx;
        attr.ah_attr.grh.traffic_class = 0;
    }
    return ibv_modify_qp(qp, &attr,
        IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
}

int change_qp_to_RTS(struct ibv_qp *qp, uint32_t my_psn) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = my_psn;
    /*
    * The minimum timeout that a QP waits for ACK/NACK from remote QP before retransmitting the packet. 
    * The value zero is special value which means wait an infinite time for the ACK/NACK (useful for debugging).
    */
    //attr.timeout = 0x14;
    attr.timeout = 0x0;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    // The number of RDMA Reads & atomic operations outstanding at any time that can be handled by this QP as an initiator
    attr.max_rd_atomic = 16;
    return ibv_modify_qp(qp, &attr, 
        IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
}

int destroy_resources() {
    printInfo("destroy resources.");
    int i;
    for (i = 0; i < ID; i++) {
        if (s_ctx->conn_array[i] != NULL) {
            struct connection *conn = s_ctx->conn_array[i];
            if (conn->qp)
                ibv_destroy_qp(conn->qp);
            if (conn->recv_mr)
                ibv_dereg_mr(conn->recv_mr);
            if (conn->send_mr)
                ibv_dereg_mr(conn->send_mr);
            if (conn->buffer_mr)
                ibv_dereg_mr(conn->buffer_mr);
            if (conn->recv_region)
                free(conn->recv_region);
            if (conn->send_region)
                free(conn->send_region);
            if (conn->buffer)
                free(conn->buffer);
            free(conn);
        }
    }

    if (cont_hash_table) {
        continuity_destroy(cont_hash_table);
    }
    if (s_ctx->pd)
        ibv_dealloc_pd(s_ctx->pd);
    if (s_ctx->ctx)
        ibv_close_device(s_ctx->ctx);
    return 0;
}

void* poll_cq(void *args) {
    int i = (int)(*((int*)args));
    //printInfo("poll cq[%d].", i);
    void *ctx;
    struct ibv_wc wc;
    struct ibv_cq *cq;
    while (1) {
/*
        if (ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx)) {
            printError("get completion queue event failed.");
        } 
        ibv_ack_cq_events(cq, 1);
        if (ibv_req_notify_cq(cq, 0)) {
            printError("re-armed ibv_req_notify_cq failed.");
        }
*/
        while (ibv_poll_cq(s_ctx->cq[i], 1, &wc)) {
        //while (ibv_poll_cq(cq, 1, &wc)) {
            on_completion(&wc);
        }
    }
}

struct timespec start, finish;
uint8_t old_key[KEY_LEN];
uint64_t tmp_number_of_writes = 0;
int expand_num = 0;
void on_completion(struct ibv_wc *wc) {
    struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;
    if (wc->status != IBV_WC_SUCCESS)
        printInfo("on_completion: status is not IBV_WC_SUCCESS: %d.", wc->status);
    //post_receives(conn);
    if (wc->opcode & IBV_WC_RECV) {
        if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            uint32_t thread_id = ntohl (wc->imm_data);
            //char buffer[RDMA_BUFFER_SIZE];
            uint8_t key[KEY_LEN];
            uint8_t val[VALUE_LEN];
            //uint8_t op[10];
            // 将buffer清零，用于下一次接受IMM信息
            //memset(conn->buffer + thread_id * RDMA_BUFFER_SIZE, 0, RDMA_BUFFER_SIZE);
            //post_receives(conn);

            //acnt++;
            //printInfo("acnt: %lu.", acnt);

            if (conn->buffer[thread_id * RDMA_BUFFER_SIZE] == '0') {    // insert operation
                acnt++;
                //clock_gettime(CLOCK_MONOTONIC, &start);
                //printInfo("acnt: %lu.", acnt);
                sscanf(conn->buffer + thread_id * RDMA_BUFFER_SIZE + 2, "%s %s", key, val);
                //if (strcmp(old_key, key) == 0) printInfo("%s", key);
                //else memcpy(old_key, key, KEY_LEN);
                //post_receives(conn);
                //printInfo("insert: key is %s.", key);
                uint8_t ret = continuity_insert(cont_hash_table, key, val);
                //printInfo("ret: %d.", ret);
                if (ret != 0) printInfo("failed to insert key: %s", key);
                if (ret != 0) {
                    FILE* fp = fopen("UTIL.txt", "a+");
                    expand_num++;
                    float utilization = (float)cont_hash_table->continuity_item_num / (cont_hash_table->segment_num * (SHARED_BUCKETS_NUM + 2) * ASSOC_NUM);
                    fprintf(fp, "%d\t%lf\n", expand_num, utilization);
                    printInfo("%d\t%lf", expand_num, utilization);
                    fclose(fp);
                    continuity_expand(cont_hash_table);
                    continuity_insert(cont_hash_table, key, val);
                    register_table(conn);
                    sleep(3);
                    conn->send_region->type = REGTABLEOK;
                    conn->send_region->segment_num = cont_hash_table->segment_num;
                    conn->send_region->seed = cont_hash_table->seed;
                    memcpy((void *)&(conn->send_region->mr), (void *)(conn->read_mr), sizeof(struct ibv_mr));
                    send_msg(conn);
                }
                //clock_gettime(CLOCK_MONOTONIC, &finish);
                //double single_time = (finish.tv_sec - start.tv_sec) * 1000000000.0 + (finish.tv_nsec - start.tv_nsec);
                //printInfo("single_time: %lf", single_time);
            }
            else if (conn->buffer[thread_id * RDMA_BUFFER_SIZE] == '1') {    // update operation
                acnt++;
                sscanf(conn->buffer + thread_id * RDMA_BUFFER_SIZE + 2, "%s %s", key, val);
                //if (strcmp(old_key, key) == 0) printInfo("%s", key);
                //else memcpy(old_key, key, KEY_LEN);
                //printInfo("update: key is %s.", key);
                uint8_t ret = continuity_update(cont_hash_table, key, val);
                if (ret != 0) printInfo("failed to update key: %s", key);
            }
            else if (conn->buffer[thread_id * RDMA_BUFFER_SIZE] == '2') {    // delete operation
                acnt++;
                sscanf(conn->buffer + thread_id * RDMA_BUFFER_SIZE + 2, "%s", key);
                //printInfo("delete: key is %s.", key);
                uint8_t ret = continuity_delete(cont_hash_table, key);
                if (ret != 0) printInfo("failed to update key: %s", key);
            }
            // 将buffer清零，用于下一次接受IMM信息
            //memset(conn->buffer + thread_id * RDMA_BUFFER_SIZE, 0, RDMA_BUFFER_SIZE);
            post_receives(conn);
        }
        else if (conn->recv_region->type == REGTABLE) {     //注册哈希表的segments区域，并将注册信息返回给client
            printInfo("client request -- REGTABLE.");

            register_table(conn);

            conn->send_region->type = REGTABLEOK;
            conn->send_region->segment_num = cont_hash_table->segment_num;
            conn->send_region->seed = cont_hash_table->seed;
            memcpy((void *)&(conn->send_region->mr), (void *)(conn->read_mr), sizeof(struct ibv_mr));
            send_msg(conn);
        }
        else if (conn->recv_region->type == REGRBUF) {      //申请用于imm写的区域，并将注册信息返回给client
            printInfo("client request -- REGRBUF.");

            // 这里的buffer_num设定为client端的thread_num
            printInfo("recv_region->buffer_num: %d.", conn->recv_region->buffer_num);
            register_buffer(conn, conn->recv_region->buffer_num);

            // 将用于IMM写的区域传递给客户端
            conn->send_region->type = REGRBUFOK;
            memcpy(&conn->send_region->mr, conn->buffer_mr, sizeof(struct ibv_mr));
            send_msg(conn);
            //int i;
            //for (i = 0; i < QP_DEPTH; i++)
                //post_receives(conn);
        }
        else if (conn->recv_region->type == GORUNPHASE) {
            // 进入run phase阶段，为客户端的多个线程提前post_receive
            int i, k = conn->recv_region->buffer_num;
            k = (k == 0 ? 1 : k);
            for (i = 0; i < k; i++) post_receives(conn);
            //for (i = 0; i < QP_DEPTH; i++) post_receives(conn);
            //post_receives(conn);
            // 记录load phase阶段的写次数
            tmp_number_of_writes = cont_hash_table->number_of_writes;
        }
        else if (conn->recv_region->type == REGNUMBEROFWRITES) {
    	    printInfo("acnt: %lu, continuity_item_num: %lu", acnt, cont_hash_table->continuity_item_num);
    	    printInfo("REGNUMBEROFWRITES");
    	    conn->send_region->type = REGNUMBEROFWRITESOK;
            // 返回run phase阶段的写次数给客户端
    	    conn->send_region->number_of_writes = cont_hash_table->number_of_writes - tmp_number_of_writes;
    	    send_msg(conn);
        }
        else if (conn->recv_region->type = WS) {
            // printInfo("client work done.")
        }
    }
}

void send_msg(struct connection *conn) {
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)conn->send_region;
    sge.length = sizeof(struct message);
    sge.lkey = conn->send_mr->lkey;

    if (ibv_post_send(conn->qp, &wr, &bad_wr)) {
        printError("ibv_post_send failed.");
    }

    post_receives(conn);
}
