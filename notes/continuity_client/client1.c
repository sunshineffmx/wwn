#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <libpmem.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
//#include <stdatomic.h>
#include "debug.h"
#include "continuity_hashing.h"
#include "hash.h"

#define RDMA_BUFFER_SIZE 64
#define QP_DEPTH 16000
#define INIT_SIZE 16000
#define WORKLOAD_SIZE 16000

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
    GORUNPHASE,
    REGNUMBEROFWRITES,
    REGNUMBEROFWRITESOK,
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
    uint64_t segment_num;
    uint64_t seed;
    int buffer_num;             //向服务器请求的用于imm_write的远程buffer的数目（暂定等于thread_num)
    uint32_t thread_id;
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

// for thread
struct thread_val {
    uint8_t key[KEY_LEN];
    uint8_t op;         // 0: insert, 1: update, 2: delete, 3: query
};

// for thread
struct sub_thread {
    pthread_t thread;           // current thread
    uint32_t id;                // thread id
    uint32_t op_num;            // 每个线程要执行的操作数
    struct thread_val* arr;     // the data that every thread need to run
    struct connection* conn;
};

// 每个线程有自己的用于imm_write的buffer和用于remote read的read_buffer
struct thread_connection {
    struct ibv_mr *buffer_mr;       // 用于imm_write
    struct ibv_mr *read_mr;         // 用于remote_read

    char *buffer;
    pthread_mutex_t write_mutex;

    char *read_buffer;
    pthread_mutex_t read_mutex;
    uint8_t canRead;
    uint8_t canWrite;

    // for read operation
    uint64_t hash_idx;
    uint8_t hash_key[KEY_LEN];
    //struct timespec start[2], finish;
    //struct timespec start2[2], finish2;
    struct timespec start, finish;
    struct timespec start2, finish2;
    //uint8_t start_idx, start2_idx;
    int start_flag, start2_flag;
};

struct connection {
    struct ibv_qp *qp;

    int sockfd;
    //uint32_t rkey;
    uint16_t lid;
    uint32_t qpn;
    uint32_t dest_psn;
    //union ibv_gid gid;
    uint8_t gid[16];
    uint32_t my_psn;

    struct ibv_mr *recv_mr;
    struct ibv_mr *send_mr;

    struct ibv_mr peer_mr;          // 保存用于WRITE_WITH_IMM的远程区域信息
    struct ibv_mr hash_mr;          // 保存远程哈希表的内存区域信息

    struct message *recv_buffer;
    struct message *send_buffer;

    //enum CONN_STATE state;

    uint64_t segment_num;
    uint64_t seed;

    struct thread_connection* thread_conn;      // thread connection array
};

struct context {
    struct ibv_context *ctx;
    uint32_t port;
    uint32_t server_port;
    struct ibv_port_attr port_attribute;
    int gidx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    pthread_t cq_poller_thread;
    struct connection *conn;
};

static struct context *s_ctx = NULL;
static int thread_num = 1;
static char* workload_init_file = "../../../index-microbench/workloads/loadh_unif_int.dat";
static char* workload_run_file = "../../../index-microbench/workloads/txnsh_unif_int.dat";
//static char* workload_init_file = "../workloadb.txt";
//static char* workload_run_file = "../workloadf.txt";
//atomic_long acnt = 0;
//struct timespec start, finish;
struct timespec start1, finish1;

void create_resources(char* device_name, uint32_t server_port);
int socket_connect(struct sockaddr *dst_addr);
int create_qp(struct connection* conn);
int exchange_conn_data(int sockfd, int size, char* my_meta, char* dest_meta);
int change_qp_to_INIT(struct ibv_qp *qp);
int change_qp_to_RTR(struct ibv_qp *qp, uint32_t dest_qp_num, uint16_t dest_local_id, uint32_t dest_psn, uint8_t *dest_gid);
int change_qp_to_RTS(struct ibv_qp *qp, uint32_t my_psn);
void register_memory(struct connection *conn);
void post_receives(struct connection *conn);
void load_run_workloads(struct connection *conn);
void* thread_run(void* args);
void insert(struct connection *conn, const uint8_t *key, const uint8_t *val, uint32_t thread_id);
void update(struct connection *conn, const uint8_t *key, const uint8_t *val, uint32_t thread_id);
void delete(struct connection *conn, const uint8_t *key, uint32_t thread_id);
void query(struct connection *conn, const uint8_t *key, uint32_t thread_id);
void read_remote(struct connection* conn, uint32_t thread_id, uint32_t len, uint64_t offset);
void write_with_imm(struct connection *conn, uint32_t thread_id);
int on_connection(struct connection *conn);
void get_table(struct connection* conn);
void* poll_cq(void *ctx);
void on_completion(struct ibv_wc *wc);
void send_msg(struct connection *conn);
int on_disconnect(struct connection *conn);

int main(int argc, char** argv) {
    struct addrinfo *addr;
    if (argc != 4) {
        printError("usage: client <server-address> <server-port> <thread_num>.");
    }
    
    if (getaddrinfo(argv[1], argv[2], NULL, &addr)) {
        printError("getaddrinfo failed.");
    }

    // get thread num
    thread_num = atoi(argv[3]);
    printInfo("thread_num: %d.", thread_num);

    create_resources(NULL, 5678);

    pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL);

    int sockfd = socket_connect(addr->ai_addr);
    struct connection* conn = (struct connection *)malloc(sizeof(struct connection));
    conn->thread_conn = (struct thread_connection *)malloc(thread_num * sizeof(struct thread_connection));
    conn->sockfd = sockfd;
    if (create_qp(conn)) {
        printError("create_qp failed.");
    } else {
        s_ctx->conn = conn;
        register_memory(conn);
        post_receives(conn);

        on_connection(conn);
    }
    while(1);
    on_disconnect(conn);
}

void create_resources(char* device_name, uint32_t server_port) {
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
    s_ctx->cq = ibv_create_cq(s_ctx->ctx, QP_DEPTH, NULL, NULL, 0);
    if (!s_ctx->cq)
        printError("create completion queue failed.");
}

int socket_connect(struct sockaddr *dst_addr) {
    int sockfd;
    /*
    * struct timeval
    * {
    *     __time_t tv_sec;         // Seconds. 
    *     __suseconds_t tv_usec;   // Microseconds. 
    * };
    */
    struct timeval timeout = {3, 0};
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printError("create socket failed.");
    }
    // 设置接收时限
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    if (ret < 0)
        printError("setsockopt failed.");

    int t = 3;
    while ( t >= 0 && connect(sockfd, dst_addr, sizeof(struct sockaddr)) < 0) {
        printInfo("Fail to connect to the server, retry.");
        t -= 1;
        sleep(1);
    }
    if (t < 0) {
        printError("connect failed.");
    }
    return sockfd;
}

int create_qp(struct connection* conn) {
    /*
    * 4.create queue pair
    */
    struct ibv_qp_init_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_type = IBV_QPT_RC;
    attr.send_cq = s_ctx->cq;
    attr.recv_cq = s_ctx->cq;

    attr.cap.max_send_wr = QP_DEPTH;
    attr.cap.max_recv_wr = QP_DEPTH;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;

    attr.sq_sig_all = 0;

    conn->qp = ibv_create_qp(s_ctx->pd, &attr);
    if (!conn->qp) {
        printError("create queue pair failed.");
    }

    /*
    * 5.exchange identifier information to establish connection
    */
    struct exchangeMeta my_meta, dest_meta;
    memset(my_meta.gid, 0, 16);
    memset(dest_meta.gid, 0, 16);
    union ibv_gid mygid;
    my_meta.lid = s_ctx->port_attribute.lid;
    if (s_ctx->gidx >= 0) {
        if (ibv_query_gid(s_ctx->ctx, s_ctx->port, s_ctx->gidx, &mygid)) {
            printError("can't get gid of index: %d", s_ctx->gidx);
        }
    }
    else {
        memset(&mygid, 0, sizeof(mygid));
    }
    memcpy(my_meta.gid, &mygid, 16);
    my_meta.qpn = conn->qp->qp_num;
    //my_meta.psn = lrand48() & 0xffffff;
    my_meta.psn = 3185;
    conn->my_psn = my_meta.psn;
    printInfo("my lid: %u, psn: %lu", my_meta.lid, my_meta.psn);

    if (exchange_conn_data(conn->sockfd, sizeof(struct exchangeMeta), (char *)&my_meta, (char *)&dest_meta) < 0) {
        printError("exchange identifier information failed.");
    }
    printInfo("dest lid: %u, psn: %lu.", dest_meta.lid, dest_meta.psn);
    conn->qpn = dest_meta.qpn;
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
    return ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
}

int change_qp_to_RTR(struct ibv_qp *qp, uint32_t dest_qp_num, uint16_t dest_local_id, uint32_t dest_psn, uint8_t *dest_gid) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_512;
    attr.rq_psn = dest_psn;
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
    return ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
}

int change_qp_to_RTS(struct ibv_qp *qp, uint32_t my_psn) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = my_psn;
    //attr.timeout = 0x14;
    attr.timeout = 0x0;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.max_rd_atomic = 16;
    return ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
}

void register_memory(struct connection *conn) {
    conn->send_buffer = (struct message *)malloc(sizeof(struct message));
    conn->recv_buffer = (struct message *)malloc(sizeof(struct message));

    conn->send_mr = ibv_reg_mr(s_ctx->pd, conn->send_buffer, sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!conn->send_mr) {
        printError("register send_mr failed.");
    }

    conn->recv_mr = ibv_reg_mr(s_ctx->pd, conn->recv_buffer, sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!conn->recv_mr) {
        printError("register recv_mr failed.");
    }

    int i;
    for (i = 0; i < thread_num; i++) {
        // 每个线程用于imm_write的buffer
        conn->thread_conn[i].buffer = malloc(RDMA_BUFFER_SIZE);
        conn->thread_conn[i].buffer_mr = ibv_reg_mr(s_ctx->pd, conn->thread_conn[i].buffer, RDMA_BUFFER_SIZE,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        if (!conn->thread_conn[i].buffer_mr) {
            printError("register buffer_mr failed.");
        }

        // 设定每个线程用于远程读的buffer的大小
        conn->thread_conn[i].read_buffer = malloc(sizeof(continuity_bucket) * (SHARED_BUCKETS_NUM + 1));
        conn->thread_conn[i].read_mr = ibv_reg_mr(s_ctx->pd, conn->thread_conn[i].read_buffer, sizeof(continuity_bucket) * (SHARED_BUCKETS_NUM + 1),
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        if (!conn->thread_conn[i].read_mr) {
            printError("register read_mr failed.");
        }
        // 用于每个线程，表示当前的read_buffer是否可用（为了防止远程读请求太快导致报错）
        //conn->thread_conn[i].canRead = 1;
        //conn->thread_conn[i].canWrite = 1;
        pthread_mutex_init(&conn->thread_conn[i].write_mutex, NULL);
        pthread_mutex_init(&conn->thread_conn[i].read_mutex, NULL);
        //conn->thread_conn[i].start_idx = 0;
        //conn->thread_conn[i].start2_idx = 0;
        conn->thread_conn[i].start_flag = 0;
        conn->thread_conn[i].start2_flag = 0;
    }
}

void post_receives(struct connection *conn) {
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr,0,sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)conn->recv_buffer;
    sge.length = sizeof(struct message);
    sge.lkey = conn->recv_mr->lkey;

    if (ibv_post_recv(conn->qp, &wr, &bad_wr)) {
        printError("ibv_post_recv failed.");
    }
}

double max_time = 0.0;
double min_time = 100000000.0;
double timeall = 0.0;
int time_flag = -1;
void load_run_workloads(struct connection *conn) {
    char buf[50];
    uint8_t key[KEY_LEN];
    uint8_t val[VALUE_LEN];
    double single_time;

    // load phase, only insert operation
    fprintf(stderr, "load workloadfile: %s.\n", workload_init_file);
    FILE* file = fopen(workload_init_file, "r");
    if (file == NULL)
        printError("cannot open init file: %s", workload_init_file);
    memset(conn->thread_conn[0].buffer, 0, RDMA_BUFFER_SIZE);
    while (fgets(buf, 50, file) != NULL) {
        if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
        if (strncmp(buf, "INSERT", 6) == 0) {
            memcpy(key, buf+7, KEY_LEN - 1);
            key[KEY_LEN-1] = '\0';
            memcpy(val, key, VALUE_LEN -1);
            val[VALUE_LEN-1] = '\0';
            insert(conn, key, val, 0);
        }
    }
    fclose(file);
    conn->send_buffer->type = GORUNPHASE;
    send_msg(conn);

    sleep(5);

    // run phase
    time_flag = 0;
    file = fopen(workload_run_file, "r");
    if (file == NULL)
        printError("cannot open run file: %s", workload_run_file);
    int i;
    for (i = 0; i < thread_num; i++) {
        memset(conn->thread_conn[i].buffer, 0, RDMA_BUFFER_SIZE);
    }

    struct thread_val* arr[thread_num];
    uint32_t move[thread_num];

    for (i = 0; i < thread_num; i++) {
        arr[i] = (struct thread_val*)calloc((WORKLOAD_SIZE/thread_num + 1), sizeof(struct thread_val));
        move[i] = 0;
    }

    uint32_t operation_num = 0;
    while (fgets(buf, 50, file) != NULL) {
        if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
        if (strncmp(buf, "INSERT", 6) == 0) {
            memcpy(arr[operation_num % thread_num][move[operation_num % thread_num]].key, buf + 7, KEY_LEN-1);
            arr[operation_num % thread_num][move[operation_num % thread_num]].key[KEY_LEN-1] = '\0';
            arr[operation_num % thread_num][move[operation_num % thread_num]].op = 0;
            move[operation_num % thread_num]++;
        }
        else if (strncmp(buf, "UPDATE", 6) == 0) {
            memcpy(arr[operation_num % thread_num][move[operation_num % thread_num]].key, buf + 7, KEY_LEN-1);
            arr[operation_num % thread_num][move[operation_num % thread_num]].key[KEY_LEN-1] = '\0';
            arr[operation_num % thread_num][move[operation_num % thread_num]].op = 1;
            move[operation_num % thread_num]++;
        }
        else if (strncmp(buf, "DELETE", 6) == 0) {
            memcpy(arr[operation_num % thread_num][move[operation_num % thread_num]].key, buf + 7, KEY_LEN-1);
            arr[operation_num % thread_num][move[operation_num % thread_num]].key[KEY_LEN-1] = '\0';
            arr[operation_num % thread_num][move[operation_num % thread_num]].op = 2;
            move[operation_num % thread_num]++;
        }
        else if (strncmp(buf, "READ", 4) == 0) {
            memcpy(arr[operation_num % thread_num][move[operation_num % thread_num]].key, buf + 5, KEY_LEN-1);
            arr[operation_num % thread_num][move[operation_num % thread_num]].key[KEY_LEN-1] = '\0';
            arr[operation_num % thread_num][move[operation_num % thread_num]].op = 3;
            move[operation_num % thread_num]++;
        }
        operation_num++;
    }

    struct sub_thread* threads = (struct sub_thread*)malloc(thread_num * sizeof(struct sub_thread));
    fprintf(stderr, "RUN phase begin.\n");
    fprintf(stderr, "*******************************************************************\n");
    sleep(2);
    clock_gettime(CLOCK_MONOTONIC, &start1);
    for (i = 0; i < thread_num; i++) {
        threads[i].id = i;
        threads[i].arr = arr[i];
        threads[i].op_num = move[i];
        threads[i].conn = conn;
        pthread_create(&threads[i].thread, NULL, thread_run, &threads[i]);
    }
    for (i = 0; i < thread_num; i++) {
        pthread_join(threads[i].thread, NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &finish1);
    sleep(3);
    single_time = (finish1.tv_sec - start1.tv_sec) + (finish1.tv_nsec - start1.tv_nsec) / 1000000000.0;
    fprintf(stderr, "Run phase throughput: %f operations per second.\n", WORKLOAD_SIZE/single_time);
    fprintf(stderr, "*******************************************************************\n");
    fprintf(stderr, "min_time: %lf, max_time: %lf\n", min_time, max_time);
    fprintf(stderr, "average_latency: %lf ns\n", timeall / WORKLOAD_SIZE * 2);
    fclose(file);
    conn->send_buffer->type = REGNUMBEROFWRITES;
    send_msg(conn);
}

void* thread_run(void* args) {
    struct sub_thread* subthread = (struct sub_thread*)args;
    uint8_t val[VALUE_LEN];
    int i, a1 = 0, a2 = 0;
    for (i = 0; i < subthread->op_num; i++) {
        //if (i == 0) subthread->conn->thread_conn[subthread->id].start_flag = 0;
        if (subthread->arr[i].op == 0) {        // insert
            memcpy(val, subthread->arr[i].key, VALUE_LEN-1);
            val[VALUE_LEN-1] = '\0';
            if (a1 % 2 == 0) clock_gettime(CLOCK_MONOTONIC, &subthread->conn->thread_conn[subthread->id].start);
            insert(subthread->conn, subthread->arr[i].key, val, subthread->id);
            a1++;
        }
        else if (subthread->arr[i].op == 1) {   // update
            memcpy(val, subthread->arr[i].key, VALUE_LEN-1);
            val[VALUE_LEN-1] = '\0';
            if (a1 % 2 == 0) clock_gettime(CLOCK_MONOTONIC, &subthread->conn->thread_conn[subthread->id].start);
            update(subthread->conn, subthread->arr[i].key, val, subthread->id);
            a1++;
        }
        else if (subthread->arr[i].op == 2) {   // delete
            if (a1 % 2 == 0) clock_gettime(CLOCK_MONOTONIC, &subthread->conn->thread_conn[subthread->id].start);
            delete(subthread->conn, subthread->arr[i].key, subthread->id);
            a1++;
        }
        else if (subthread->arr[i].op == 3) {   // read
            if (a2 % 2 == 0) clock_gettime(CLOCK_MONOTONIC, &subthread->conn->thread_conn[subthread->id].start2);
            query(subthread->conn, subthread->arr[i].key, subthread->id);
            a2++;
        }
    }
}

void insert(struct connection *conn, const uint8_t *key, const uint8_t *val, uint32_t thread_id) {
    //while (strlen(conn->thread_conn[thread_id].buffer) != 0) ;
    //while (conn->thread_conn[thread_id].canWrite == 0);
    //conn->thread_conn[thread_id].canWrite = 0;
    //usleep(3000);
    //conn->thread_conn[thread_id].start_flag = 1;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start[conn->thread_conn[thread_id].start_idx]);
    //conn->thread_conn[thread_id].start_idx = 1 - conn->thread_conn[thread_id].start_idx;
    pthread_mutex_lock(&conn->thread_conn[thread_id].write_mutex);
    //conn->thread_conn[thread_id].start_flag = 0;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
   // printInfo("start insert key: %s, thread id: %d.", key, thread_id);
    sprintf(conn->thread_conn[thread_id].buffer, "0 %s  %s", key, val);      // use 0 to signal insert operation
    write_with_imm(conn, thread_id);
}

void update(struct connection *conn, const uint8_t *key, const uint8_t *val, uint32_t thread_id) {
    //while (strlen(conn->thread_conn[thread_id].buffer) != 0) ;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
    //conn->thread_conn[thread_id].start_flag = 1;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start[conn->thread_conn[thread_id].start_idx]);
    //conn->thread_conn[thread_id].start_idx = 1 - conn->thread_conn[thread_id].start_idx;
    pthread_mutex_lock(&conn->thread_conn[thread_id].write_mutex);
    //conn->thread_conn[thread_id].start_flag = 0;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
    //printInfo("start update key: %s, thread id: %d.", key, thread_id);
    sprintf(conn->thread_conn[thread_id].buffer, "1 %s %s", key, val);      // use 1 to signal update operation
    write_with_imm(conn, thread_id);
}

void delete(struct connection *conn, const uint8_t *key, uint32_t thread_id) {
    //while (strlen(conn->thread_conn[thread_id].buffer) != 0) ;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
    //conn->thread_conn[thread_id].start_flag = 1;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start[conn->thread_conn[thread_id].start_idx]);
    //conn->thread_conn[thread_id].start_idx = 1 - conn->thread_conn[thread_id].start_idx;
    pthread_mutex_lock(&conn->thread_conn[thread_id].write_mutex);
    //conn->thread_conn[thread_id].start_flag = 0;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
    //printInfo("start delete key: %s, thread id: %d.", key, thread_id);
    sprintf(conn->thread_conn[thread_id].buffer, "2 %s", key);              // use 2 to signal delete operation
    write_with_imm(conn, thread_id);
}

void query(struct connection *conn, const uint8_t *key, uint32_t thread_id) {
    //while (conn->thread_conn[thread_id].canRead == 0) ;
    //usleep(3);
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start2);
   // conn->thread_conn[thread_id].start2_flag = 1;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start2[conn->thread_conn[thread_id].start2_idx]);
    //conn->thread_conn[thread_id].start2_idx = 1 - conn->thread_conn[thread_id].start2_idx;
    pthread_mutex_lock(&conn->thread_conn[thread_id].read_mutex);
    //conn->thread_conn[thread_id].start2_flag = 0;
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start2);
    //conn->thread_conn[thread_id].canRead = 0;

    uint64_t h = hash((void *)key, strlen(key), conn->seed);
    uint64_t idx = h % (conn->segment_num * 2);
    //printInfo("start query, key is: %s, idx is: %d, thread id: %d.\n", key, idx, thread_id);

    conn->thread_conn[thread_id].hash_idx = idx;
    //memset(conn->thread_conn[thread_id].hash_key, 0, strlen(conn->thread_conn[thread_id].hash_key));
    //memcpy((void *)(conn->thread_conn[thread_id].hash_key), (void *)(key), strlen(key));
    // 可以用一个操作代替吗？
    memcpy((void *)(conn->thread_conn[thread_id].hash_key), (void *)(key), KEY_LEN-1);

    if (idx % 2 == 0) {         // segment中的左桶
        read_remote(conn, thread_id, sizeof(continuity_bucket) * (SHARED_BUCKETS_NUM + 1),
         (idx/2)*sizeof(continuity_segment));
    }
    else {                      // segment中的右桶
        read_remote(conn, thread_id, sizeof(continuity_bucket) * (SHARED_BUCKETS_NUM + 1), 
            (idx/2)*sizeof(continuity_segment) + sizeof(continuity_bucket));
    }
}

void read_remote(struct connection* conn, uint32_t thread_id, uint32_t len, uint64_t offset) {
    //usleep(3);
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;
    memset(conn->thread_conn[thread_id].read_buffer, 0, len);

    memset(&wr,0,sizeof(wr));
    
    wr.wr_id = (uintptr_t)(&(conn->thread_conn[thread_id]));
    wr.opcode = IBV_WR_RDMA_READ;
    wr.send_flags = IBV_SEND_SIGNALED;

    wr.wr.rdma.remote_addr = (uintptr_t)conn->hash_mr.addr + offset;
    wr.wr.rdma.rkey = conn->hash_mr.rkey;

    if(len) {
        wr.sg_list = &sge;
        wr.num_sge = 1;
        sge.addr = (uintptr_t)conn->thread_conn[thread_id].read_buffer;
        sge.length = len;
        sge.lkey = conn->thread_conn[thread_id].read_mr->lkey;
    }
    //post_receives(conn);
    if (ibv_post_send(conn->qp, &wr, &bad_wr)) {
        printError("ibv_post_send in read_remote function failed.");
    }
}

void write_with_imm(struct connection *conn, uint32_t thread_id) {
    //printInfo("write_with_imm start.\n");
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    //wr.wr_id = (uintptr_t)conn;
    wr.wr_id = (uintptr_t)(&(conn->thread_conn[thread_id]));
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;

    wr.imm_data = htonl(thread_id);
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_mr.addr + thread_id * RDMA_BUFFER_SIZE;
    wr.wr.rdma.rkey = conn->peer_mr.rkey;

    uint32_t len = strlen(conn->thread_conn[thread_id].buffer);

    if (len) {
        wr.sg_list = &sge;
        wr.num_sge = 1;
        sge.addr = (uintptr_t)conn->thread_conn[thread_id].buffer;
        sge.length = len;
        sge.lkey = conn->thread_conn[thread_id].buffer_mr->lkey;
    }

    //post_receives(conn);
    //usleep(500);
    //clock_gettime(CLOCK_MONOTONIC, &conn->thread_conn[thread_id].start);
    if (ibv_post_send(conn->qp, &wr, &bad_wr)) {
        printError("ibv_post_send with immediate data failed.");
    }
    //post_receives(conn);
}

int on_connection(struct connection *conn) {
    //conn->state = ESTABLISHED;
    printInfo("connection established.");

    // 向服务器发送消息，请求关于continuity hash table的一些信息（segment_num, seed），并让服务器注册整个哈希表内存区域
    get_table(conn);
    sleep(1);

    // 向服务器发送消息，请求用于IMM写的远程区域信息
    conn->send_buffer->type = REGRBUF;
    conn->send_buffer->buffer_num = thread_num;
    conn->send_buffer->thread_id = 1;
    printInfo("send_buffer->buffer_num: %d", conn->send_buffer->buffer_num);
    send_msg(conn);
    sleep(1);

    // 运行数据集
    load_run_workloads(conn);
    sleep(3);

    //conn->send_buffer->type = WS;
    //send_msg(conn);
    return 0;
}

void get_table(struct connection* conn) {
    conn->send_buffer->type = REGTABLE;
    send_msg(conn);
}

void* poll_cq(void *ctx) {
    struct ibv_wc wc;
    //printInfo("poll cq[%d].", id);
    while (1) {
        while (ibv_poll_cq(s_ctx->cq, 1, &wc)) {
            on_completion(&wc);
        }
    }
}

int write_count = 0;
void on_completion(struct ibv_wc *wc) {
    //struct connection *conn = (struct connection *)wc->wr_id;
    if (wc->status != IBV_WC_SUCCESS)
        printInfo("on_completion: status is not IBV_WC_SUCCESS: %d.", wc->status);
    if (wc->opcode == IBV_WC_RDMA_WRITE) {
        write_count++;
        struct thread_connection *threadconn = (struct thread_connection *)(uintptr_t)wc->wr_id;
        clock_gettime(CLOCK_MONOTONIC, &threadconn->finish);
        //struct timespec starttime;
        //if (threadconn->start_flag == 1)
           // starttime = threadconn->start[threadconn->start_idx];
        //else
            //starttime = threadconn->start[1-threadconn->start_idx];
        //starttime = (threadconn->start[threadconn->start_idx] > threadconn->start[1-threadconn->start_idx] ? threadconn->start[1->threadconn->start_idx] : threadconn->start[threadconn->start_idx]);
        //if (threadconn->start.tv_sec == 0) threadconn->start.tv_sec = threadconn->finish.tv_sec;
        double single_time = (threadconn->finish.tv_sec - threadconn->start.tv_sec)*1000000000.0 + (threadconn->finish.tv_nsec - threadconn->start.tv_nsec);
        if (write_count > INIT_SIZE && time_flag == 0) {
            if (threadconn->start_flag == 0) {
                if (single_time > max_time) max_time = single_time;
                if (single_time < min_time) min_time = single_time;
                timeall += single_time;
                //printInfo("single time: %lf, start.tv_sec: %lu, finish.tv_sec: %lu, start.tv_nsec: %lu, finish.tv_nsec: %lu", single_time, threadconn->start.tv_sec, threadconn->finish.tv_sec, threadconn->start.tv_nsec, threadconn->finish.tv_nsec);
            }
            threadconn->start_flag = 1 - threadconn->start_flag;
        }
        //printInfo("single time: %lf", single_time);
        //printInfo("WRITE OPERATION OK.");
        //atomic_fetch_add_explicit(&acnt, 1, memory_order_relaxed); // atomic
        //printInfo("acnt: %ld", acnt);
/*
        uint8_t key[KEY_LEN];
        uint8_t val[VALUE_LEN];
        sscanf(threadconn->buffer+2, "%s %s", key, val);
        printInfo("%s", key);
*/
        //threadconn->canWrite = 1;
        //memset(threadconn->buffer, 0, RDMA_BUFFER_SIZE);
        pthread_mutex_unlock(&threadconn->write_mutex);
/*
        if (acnt == (WORKLOAD_SIZE + INIT_SIZE)) {
            clock_gettime(CLOCK_MONOTONIC, &finish);
            double single_time = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
            printInfo("Run phase throughput: %f operations per second.", WORKLOAD_SIZE/single_time);
            printInfo("*******************************************************************");
        }
*/
    } 
    else if (wc->opcode & IBV_WC_RECV) {
        struct connection *conn = (struct connection *)wc->wr_id;
        if (conn->recv_buffer->type == REGTABLEOK) {
            printInfo("REGTABLEOK.");
            // 将远程哈希表的内存区域信息保存下来
            conn->segment_num = conn->recv_buffer->segment_num;
            conn->seed = conn->recv_buffer->seed;
            printInfo("segment_num: %d, seed: %d.", conn->segment_num, conn->seed);
            memcpy((void *)(&conn->hash_mr), (void *)(&conn->recv_buffer->mr), sizeof(struct ibv_mr));
            printInfo("remote hash_mr addr: %d", (uintptr_t)conn->hash_mr.addr);
        }
        else if (conn->recv_buffer->type == REGRBUFOK) {
            printInfo("REGRBUFOK.");
            // 将服务器端的memory region信息保存下来，用于WRITE_WITH_IMM
            memcpy((void *)(&conn->peer_mr), (void *)(&conn->recv_buffer->mr), sizeof(struct ibv_mr));
            printInfo("remote buffer_mr addr: %d", (uintptr_t)conn->peer_mr.addr);
        }
        else if (conn->recv_buffer->type == REGNUMBEROFWRITESOK) {
            uint64_t number_of_writes = conn->recv_buffer->number_of_writes;
            printInfo("average number of writes per operation: %f", (float)number_of_writes / WORKLOAD_SIZE);
        }
        post_receives(conn);
    }
    else if (wc->opcode == IBV_WC_RDMA_READ) {  // 处理远程读回来的数据
        //atomic_fetch_add_explicit(&acnt, 1, memory_order_relaxed); // atomic
//        printInfo("acnt: %ld", acnt);
        struct thread_connection *threadconn = (struct thread_connection *)(uintptr_t)wc->wr_id;
        continuity_bucket buckets[1 + SHARED_BUCKETS_NUM];
        memcpy((void *)(buckets), (void *)(threadconn->read_buffer), sizeof(continuity_bucket) * (1 + SHARED_BUCKETS_NUM));
        //uint8_t hash_key[KEY_LEN]; 
        //memcpy(hash_key, threadconn->hash_key, KEY_LEN);
        //uint64_t hash_idx = threadconn->hash_idx;
        //pthread_mutex_unlock(&threadconn->read_mutex);

        int m, j, n2, n3;
        int ret = 0;

        if (threadconn->hash_idx % 2 == 0) {
            for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
            {
                for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
                {
                    n2 = m * ASSOC_NUM + j + 1;
                    uint1_t nt2 = ( (buckets[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                    if (nt2 == 1 && strcmp(buckets[m].slot[j].key, threadconn->hash_key) == 0)
                    {
                        //if (strcmp(threadconn->hash_key, buckets[m].slot[j].value) == 0) {
                            //ret = 1;
                        //}
                        //goto READEND;
                        //printInfo("read: for key: %s, value is: %s.", threadconn->hash_key, buckets[m].slot[j].value);
                        ret = 1;
                        //goto READEND;
                        break;
                    }
                }
                if (ret == 1) break;
            }
        }
        else {
            for(m = SHARED_BUCKETS_NUM; m >= 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
            {
                for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
                {
                    n3 = (m + 1) * ASSOC_NUM + j + 1;
                    uint1_t nt3 = ( (buckets[0].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                    if (nt3 == 1 && strcmp(buckets[m].slot[j].key, threadconn->hash_key) == 0)
                    {
                        //if (strcmp(threadconn->hash_key, buckets[m].slot[j].value) == 0) {
                         //   ret = 1;
                        //}
                        //goto READEND;
                        //printInfo("read: for key: %s, value is: %s.", threadconn->hash_key, buckets[m].slot[j].value);
                        ret = 1;
                        //goto READEND;
                        break;
                    }
                }
                if (ret == 1) break;
            }
        }
READEND:
        if (ret == 0) {
            //printInfo("read: for key: %s, value not found.", threadconn->hash_key);
        }

        clock_gettime(CLOCK_MONOTONIC, &threadconn->finish2);
        //struct timespec starttime;
        //if (threadconn->start_flag == 1)
            //starttime = threadconn->start[threadconn->start_idx];
        //else
            //starttime = threadconn->start[1-threadconn->start_idx];
        //starttime = (threadconn->start2[threadconn->start2_idx].tv_nsec > threadconn->start2[1-threadconn->start2_idx].tv_nsec ? threadconn->start2[1->threadconn->start2_idx] : threadconn->start2[threadconn->start2_idx]);
        double single_time = (threadconn->finish2.tv_sec - threadconn->start2.tv_sec)*1000000000.0 + (threadconn->finish2.tv_nsec - threadconn->start2.tv_nsec);
        if (threadconn->start2_flag == 0) {
            if (single_time > max_time) max_time = single_time;
            if (single_time < min_time) min_time = single_time;
            timeall += single_time;
        }
        threadconn->start2_flag = 1 - threadconn->start2_flag;
        //threadconn->canRead = 1;
        pthread_mutex_unlock(&threadconn->read_mutex);
    }
    else if (wc->opcode == IBV_WC_SEND) {
        //if (conn->send_buffer->type == WS) {
            //on_disconnect(conn);
        //}
    }
}

void send_msg(struct connection *conn)
{
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)conn->send_buffer;
    sge.length = sizeof(struct message);
    sge.lkey = conn->send_mr->lkey;

    if (ibv_post_send(conn->qp, &wr, &bad_wr)) {
        printError("ibv_post_send failed.");
    }
    post_receives(conn);
}

int on_disconnect(struct connection *conn) {
    printInfo("disconnected.");
    ibv_destroy_qp(conn->qp);
    ibv_dereg_mr(conn->send_mr);
    ibv_dereg_mr(conn->recv_mr);
    int i;
    for (i = 0; i < thread_num; i++) {
        ibv_dereg_mr(conn->thread_conn[i].buffer_mr);
        ibv_dereg_mr(conn->thread_conn[i].read_mr);
    }
    free(conn->send_buffer);
    free(conn->recv_buffer);
    for (i = 0; i < thread_num; i++) {
        free(conn->thread_conn[i].buffer);
        free(conn->thread_conn[i].read_buffer);
    }
    free(conn->thread_conn);
    free(conn);

    if (s_ctx->pd)
        ibv_dealloc_pd(s_ctx->pd);
    if (s_ctx->ctx)
        ibv_close_device(s_ctx->ctx);
    return 0;
}
