#ifndef __my_uart_h
#define __my_uart_h

#include "messages.h"

#define MAXUARTBUF 18

#define RADIO_ADDR 0x01

typedef struct __uart_comm {
	unsigned char buffer[MAXUARTBUF];
	unsigned char	buflen;
} uart_comm;

typedef struct __uart_comm {
	unsigned char buffer[MAXUARTBUF];
	unsigned char backup_buffer[MAXUARTBUF];
	unsigned char buflen;
	unsigned char backup_buflen;
	unsigned char msg_count;
} uart_comm_2;

typedef struct {
	unsigned char buffer[MAXUARTBUF];
	unsigned char buflen;
} uart_message;

void init_uart_recv(uart_comm *);
void init_uart_send(uart_comm_2 *);
void uart_recv_int_handler(void);
void uart_send_int_handler(void);
int write_to_uart4(int,unsigned char*);
int write_to_uart5(int,unsigned char*);
void loconet_mod(char*);
int loconet_demod(char*);
int length_mod(int);
int length_demod(int);

#endif