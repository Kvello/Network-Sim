#include "udp_client.h"

int rec = 0;
bool finished = false;
pthread_mutex_t mux1 = PTHREAD_MUTEX_INITIALIZER;
typedef struct{
    int socket;
    struct StopAndWaitMessage* recv;
    struct sockaddr* src_addr;
    socklen_t addr_len;
}async_receive_message_args;


void* async_receive_message(void* args){
    async_receive_message_args* arg = (async_receive_message_args*)args;
    rec = msg_receive_message(arg->socket,arg->recv,arg->src_addr,arg->addr_len);
    if(utils_rand_bool(LOST_PROB)){
        printf("Simulating lost message\n");
        while(1){
            //Block
        }
    }
    pthread_mutex_lock(&mux1);
    finished = true;
    pthread_mutex_unlock(&mux1);
    pthread_exit(NULL);
}

int main(int argc,char* argv[]){
    printf("Starting client\n");
    pthread_t rx_tread;
    async_receive_message_args rx_args;
    int sock = udp_get_sock(AF_INET, UDP_CLIENT_PORT, INADDR_ANY);
    rx_args.socket = sock;
    rx_args.src_addr = (struct sockaddr*)NULL;
    struct sockaddr_in server_addr;
    udp_get_sockaddr(AF_INET,argv[1],UDP_SERVER_PORT,&server_addr);
    FILE* fp = utils_open_file(argv[2],"r");
    uint8_t buf[MSG_MAX_DATA_SIZE];

    struct StopAndWaitMessage out_message;
    struct StopAndWaitMessage ack_message;
    uint8_t dummy_data = 0;
    ack_message.data = &dummy_data;
    ack_message.header = msg_create_header(0,0,1,utils_calculate_32crc(CRC_DIVISOR,&dummy_data,1));
    out_message.header = msg_create_header(1,0,0,0);
    int read_bytes=0;
    unsigned long long total_bytes=0;
    struct timeval send_start, send_end;
    gettimeofday(&send_start, NULL);
    while((read_bytes=fread(buf,1,MSG_MAX_DATA_SIZE,fp))>0){
        total_bytes += read_bytes;
        int crc = utils_calculate_32crc(CRC_DIVISOR,buf,read_bytes);
        struct StopAndWaitHeader header = msg_create_header(!out_message.header.seq_num,0,read_bytes,crc);
        out_message = msg_create_message(header,buf);

        while(ack_message.header.ack == out_message.header.seq_num){
            if((float)rand()/(float)RAND_MAX<BITFLIP_PROB){
                printf("Simulating error\n");
                out_message.data[0] ^= 0b00000001; // Simulate single bit flip error 
            }
            int ret = msg_send_message(sock,&out_message, (struct sockaddr*)&server_addr);
            if(ret == -1){
                printf("error in msg_send_message\n");
                exit(1);
            }
            struct StopAndWaitMessage temp;
            temp.header = ack_message.header;
            uint8_t temp_data = *ack_message.data;
            temp.data = &temp_data;
            rx_args.addr_len = sizeof(server_addr.sin_addr);
            rx_args.recv = &temp;
            // Start timer
            struct timeval p_start, now;
            gettimeofday(&p_start, NULL);
            ret = pthread_create(&rx_tread,NULL,async_receive_message,&rx_args);
            if(ret != 0){
                printf("error in pthread_create\n");
                exit(1);
            }
            gettimeofday(&now, NULL);
            bool timed_out = true;
            while(utils_time_diff(&p_start,&now) < TIMEOUT){
                pthread_mutex_lock(&mux1);
                if(finished){
                    pthread_mutex_unlock(&mux1);
                    finished = false;
                    timed_out = false;
                    break;
                }
                pthread_mutex_unlock(&mux1);
                gettimeofday(&now, NULL);
            }
            pthread_mutex_lock(&mux1);
            if(timed_out){
                printf("Timed out, resending\n");
                pthread_cancel(rx_tread);
                pthread_mutex_unlock(&mux1);
                continue;
            }
            pthread_mutex_unlock(&mux1);
            if(rec == -1){
                printf("msg_receive_message detected an error, resending\n");
            }else{
                // Correctly received ack. Store and continue
                ack_message.header = temp.header;
                ack_message.data = temp.data;
            }
        }

    }
    gettimeofday(&send_end, NULL);
    double time = utils_time_diff(&send_start,&send_end);
    printf("Sent %llu bytes in %f micro seconds\n",total_bytes,time);
    printf("That is %.2e bytes per second\n",1e6*((double)total_bytes)/time);
}