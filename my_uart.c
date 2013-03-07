#include "maindefs.h"
#include <usart.h>
#include "my_uart.h"

static uart_comm *uc_ptr;
static uart_comm *uc_ptr_2;


void uart_recv_int_handler()
{
	if (DataRdyUSART()) {
		uc_ptr->buffer[uc_ptr->buflen] = ReadUSART();
		uc_ptr->buflen++;
		// check if a message should be sent
		if (uc_ptr->buffer[uc_ptr->buflen-1] >= 0x80) {
			//if(uc_ptr->buffer[0] >= 0x80) {					<-- CHECK THIS, SHOULDNT HAPPEN, MEANS SEND OR RECEIVE IS BACKWARDS
			// if the last byte received was a checksum
			if (uc_ptr->buflen == MAXUARTBUF) {
				// if the correct amount of bytes were received
				if(uc_ptr->buffer[uc_ptr->buflen-1] == RADIO_ADDR){
				//if(uc_ptr->buffer[0] == RADIO_ADDR){							<-- CHECK THIS
					// message was addressed to this device
					if(loconet_demod(uc_ptr->buffer) == 0){				// <-- LOCONET DEMODULATION
						// checksum valid
						ToMainLow_sendmsg(uc_ptr->buflen,MSGT_UART_DATA,(void *) uc_ptr->buffer);
						LATBbits.LATB3 = 0;
						LATBbits.LATB4 = 0;
					}
					else{
						// checksum invalid
						//ToMainLow_sendmsg(uc_ptr->buflen,MSGT_UART_DATA,(void *) uc_ptr->buffer);
						LATBbits.LATB3 = 1;
						LATBbits.LATB4 = 1;
					}						
					uc_ptr->buflen = 0;
					
				}
				else{
					// message was addressed to another device, discard message
					uc_ptr->buflen = 0;
					LATBbits.LATB3 = 0;
					LATBbits.LATB4 = 1;
				}
			}
			else{
				// a byte was missed, discard message
				uc_ptr->buflen = 0;
				LATBbits.LATB3 = 1;
				LATBbits.LATB4 = 0;
			}
		}
	}
	if (USART_Status.OVERRUN_ERROR == 1) {
		// we've overrun the USART and must reset
		// send an error message for this
		RCSTAbits.CREN = 0;
		RCSTAbits.CREN = 1;
		ToMainLow_sendmsg(0,MSGT_OVERRUN,(void *) 0);
	}
}
void uart_send_int_handler()
{
	int i;
	//int temp_buflen;
	//temp_buflen = uc_ptr_2->buflen;
	if(uc_ptr_2->buflen > 0){
		while(BusyUSART());
		WriteUSART(uc_ptr_2->buffer[uc_ptr_2->buflen - 1]);
		uc_ptr_2->buflen--;
	}
	if(uc_ptr_2->buflen == 0){
		if(uc_ptr_2->msg_count == 2){
			for(i=0;i<uc_ptr_2->backup_buflen;i++){
				uc_ptr_2->buffer[i] = uc_ptr_2->backup_buffer[i];
			}
			uc_ptr_2->buflen = uc_ptr_2->backup_buflen;
			uc_ptr_2->backup_buflen = 0;
			while(BusyUSART());
			WriteUSART(uc_ptr_2->buffer[uc_ptr_2->buflen]);
			if(uc_ptr_2->buflen == 0){
				uc_ptr_2->msg_count = 0;
			}
		}
		else{
			PIE1bits.TXIE = 0;
			PIR1bits.TXIF = 0;
		}
		uc_ptr_2->msg_count = uc_ptr_2->msg_count - 1;
	}
	//temp_buflen = uc_ptr_2->buflen;
}


int write_to_uart4(int length, unsigned char* msgbuffer){
	int i;
	unsigned char temp_char;

	i = 0x0;

	if(PIE1bits.TXIE == 1){
		// a message is already being written
		if(uc_ptr_2->msg_count == 1){
			// there is room for a message in the FIFO
			for(i=0;i<length;i++){
				uc_ptr_2->backup_buffer[i] = msgbuffer[i];
			}
			uc_ptr_2->backup_buflen = length - 1;
			loconet_mod(uc_ptr_2->backup_buffer);					//<-- LOCONET MODULATION
			uc_ptr_2->backup_buflen = length_mod(uc_ptr_2->backup_buflen);
			return 0;
		}
		else{
			// there is no room for a message in the FIFO
			return -1;
		}
	}

	PIE1bits.TXIE = 1;
	uc_ptr_2->buflen = length - 1;
	for(i=0;i<length;i++){
		temp_char = msgbuffer[i];
		uc_ptr_2->buffer[i] = msgbuffer[i];
	}
	loconet_mod(uc_ptr_2->buffer);									//<-- LOCONET MODULATION
	uc_ptr_2->buflen = length_mod(uc_ptr_2->buflen);
	while(BusyUSART());
	WriteUSART(uc_ptr_2->buffer[length - 1]);
	if(length == 1){
		PIE1bits.TXIE = 0;
		uc_ptr_2->msg_count = 0;
	}
	else{
		uc_ptr_2->msg_count = 1;
	}
	return 0;
}

int write_to_uart5(int length, unsigned char* msgbuffer){
	int i;
	
	if(length <= 15){
		switch(uc_ptr_2->msg_count){
			case 0:
				// nothing in the fifo yet
				if(length > 1){
					PIE1bits.TXIE = 1;
					uc_ptr_2->msg_count = 1;
				}
				else{
					PIE1bits.TXIE = 0;
					uc_ptr_2->msg_count = 0;
				}
				uc_ptr_2->buflen = length_mod(length - 1);
				for(i=0;i<length;i++){
					uc_ptr_2->buffer[i] = msgbuffer[i];
				}
				for(i=length;i<MAXUARTBUF;i++){
					uc_ptr_2->buffer[i] = 0x00;
				}
				loconet_mod(uc_ptr_2->buffer);
				while(BusyUSART());
				WriteUSART(uc_ptr_2->buffer[length - 1]);
				return 0;
			case 1:
				// a message is already being written
				uc_ptr_2->msg_count = 2;
				for(i=0;i<length;i++){
					uc_ptr_2->backup_buffer[i] = msgbuffer[i];
				}
				loconet_mod(uc_ptr_2->backup_buffer);
				uc_ptr_2->backup_buflen = length_mod(length - 1);
				return 0;
			case 2:
				// the fifo is full
				return -1;		
		}
	}
	else{
		// message is too big
		return -1;
	}
}

void init_uart_recv(uart_comm *uc)
{	uc_ptr = uc;
	uc_ptr->buflen = 0;
}

void init_uart_send(uart_comm_2 *uc2)
{
	uc_ptr_2 = uc2;
	uc_ptr_2->buflen = 0;
	uc_ptr_2->backup_buflen = 0;
	uc_ptr_2->msg_count = 0;
}

void loconet_mod(char* msg_buffer){
    char temp_msg_data[MAXUARTBUF];
    int checksum[7];
    int checksum_mask;
    int i,j,k;

	for(i = 0; i < MAXUARTBUF;i++){
		temp_msg_data[i] = msg_buffer[i];
	}

    msg_buffer[17] = ((temp_msg_data[0] & 0x7F) >> 0);
    msg_buffer[16] = ((temp_msg_data[0] & 0x80) >> 7) + ((temp_msg_data[1] & 0x3F) << 1);
    msg_buffer[15] = ((temp_msg_data[1] & 0xC0) >> 6) + ((temp_msg_data[2] & 0x1F) << 2);
    msg_buffer[14] = ((temp_msg_data[2] & 0xE0) >> 5) + ((temp_msg_data[3] & 0x0F) << 3);
    msg_buffer[13] = ((temp_msg_data[3] & 0xF0) >> 4) + ((temp_msg_data[4] & 0x07) << 4);
    msg_buffer[12] = ((temp_msg_data[4] & 0xF8) >> 3) + ((temp_msg_data[5] & 0x03) << 5);
    msg_buffer[11] = ((temp_msg_data[5] & 0xFC) >> 2) + ((temp_msg_data[6] & 0x01) << 6);
    msg_buffer[10] = ((temp_msg_data[6] & 0xFE) >> 1);
    msg_buffer[9] = ((temp_msg_data[7] & 0x7F) >> 0);
    msg_buffer[8] = ((temp_msg_data[7] & 0x80) >> 7) + ((temp_msg_data[8] & 0x3F) << 1);
    msg_buffer[7] = ((temp_msg_data[8] & 0xC0) >> 6) + ((temp_msg_data[9] & 0x1F) << 2);
    msg_buffer[6] = ((temp_msg_data[9] & 0xE0) >> 5) + ((temp_msg_data[10] & 0x0F) << 3);
    msg_buffer[5] = ((temp_msg_data[10] & 0xF0) >> 4) + ((temp_msg_data[11] & 0x07) << 4);
    msg_buffer[4] = ((temp_msg_data[11] & 0xF8) >> 3) + ((temp_msg_data[12] & 0x03) << 5);
	msg_buffer[3] = ((temp_msg_data[12] & 0xFC) >> 2) + ((temp_msg_data[13] & 0x01) << 6);
	msg_buffer[2] = ((temp_msg_data[13] & 0xFE) >> 1);
	msg_buffer[1] = ((temp_msg_data[14] & 0xEF) >> 0);
	msg_buffer[0] = 0x0;
 
 
    for(i = 0; i < 7; i++){
        checksum_mask = 1;
		checksum[i] = 1;
        for(k = 0; k < i; k++){
            checksum_mask = checksum_mask * 2;
        }
        for(j = 0; j < 14; j++){            
            checksum[i] = ( checksum[i] == ((msg_buffer[j] & (checksum_mask)) >> i ) ) ? 0 : 1;
			
        }
		msg_buffer[0] = msg_buffer[0] + (checksum[i] << i);
    }
	msg_buffer[0] = msg_buffer[0] | 0x80;
	
}

int loconet_demod(char* msg_data){
    char temp_msg_buffer[MAXUARTBUF];
    int checksum[7];
	int checksum_valid;
	int checksum_mask;
	int i,j,k;
	checksum_valid = 1;

	for(i = 0; i < MAXUARTBUF; i++){
		//remove this if statement!!												<----
		if(i == 0){
			temp_msg_buffer[i] = RADIO_ADDR;
		}
		else{
			temp_msg_buffer[i] = msg_data[i];
		}
	}

    for(i = 0; i < 7; i++){
        checksum_mask = 1;
		checksum[i] = 1;
    	for(k = 0; k < i; k++){
        	checksum_mask = checksum_mask * 2;
        }
        for(j = 0; j < MAXUARTBUF-1; j++){            
            checksum[i] = ( checksum[i] == ((temp_msg_buffer[j] & (checksum_mask)) >> i ) ) ? 0 : 1;
        }
        if(checksum[i] != ((temp_msg_buffer[17] & checksum_mask) >> i ) ){
            // message data corrupted, discard message
			checksum_valid = 0;
        }        
	}

	if(checksum_valid == 1){
         msg_data[0] = (((temp_msg_buffer[0] & 0xFF) >> 0) + ((temp_msg_buffer[1] & 0x01) << 7)) & 0xFF;
         msg_data[1] = (((temp_msg_buffer[1] & 0xFE) >> 1) + ((temp_msg_buffer[2] & 0x03) << 6)) & 0xFF;
         msg_data[2] = (((temp_msg_buffer[2] & 0xFC) >> 2) + ((temp_msg_buffer[3] & 0x07) << 5)) & 0xFF;
         msg_data[3] = (((temp_msg_buffer[3] & 0xF8) >> 3) + ((temp_msg_buffer[4] & 0x0F) << 4)) & 0xFF;
         msg_data[4] = (((temp_msg_buffer[4] & 0xF0) >> 4) + ((temp_msg_buffer[5] & 0x1F) << 3)) & 0xFF;
         msg_data[5] = (((temp_msg_buffer[5] & 0xE0) >> 5) + ((temp_msg_buffer[6] & 0x3F) << 2)) & 0xFF;
         msg_data[6] = (((temp_msg_buffer[6] & 0xC0) >> 6) + ((temp_msg_buffer[7] & 0x7F) << 1)) & 0xFF;
         msg_data[7] = (((temp_msg_buffer[8] & 0xFF) >> 0) + ((temp_msg_buffer[9] & 0x01) << 7)) & 0xFF;
         msg_data[8] = (((temp_msg_buffer[9] & 0xFE) >> 1) + ((temp_msg_buffer[10] & 0x03) << 6)) & 0xFF;
         msg_data[9] = (((temp_msg_buffer[10] & 0xFC) >> 2) + ((temp_msg_buffer[11] & 0x07) << 5)) & 0xFF;
         msg_data[10] = (((temp_msg_buffer[11] & 0xF8) >> 3) + ((temp_msg_buffer[12] & 0x0F) << 4)) & 0xFF;
         msg_data[11] = (((temp_msg_buffer[12] & 0xF0) >> 4) + ((temp_msg_buffer[13] & 0x1F) << 3)) & 0xFF;
		 msg_data[12] = (((temp_msg_buffer[13] & 0xE0) >> 5) + ((temp_msg_buffer[14] & 0x3F) << 2)) & 0xFF;
		 msg_data[13] = (((temp_msg_buffer[14] & 0xC0) >> 6) + ((temp_msg_buffer[15] & 0x7F) << 1)) & 0xFF;
		 msg_data[14] = (((temp_msg_buffer[16] & 0xFF) >> 0));
		 msg_data[15] = 0x0;
		 msg_data[16] = 0x0;
		return 0;    
	}
	else{
		// checksum was not valid, clearing the message buffer
		for(i = 0; i < MAXUARTBUF; i++){
			msg_data[i] = 0x0;
		}
		return -1;
	}
}

int length_mod(int premod_length){
	if(premod_length <= 0){
		return 0;
	}
	else if(premod_length <= 7){
		return (premod_length + 1);
	}
	else if(premod_length <= 14){
		return (premod_length + 2);
	}
	else if(premod_length <= 21){
		return (premod_length + 3);
	}
	else{
		return (premod_length + 4);
	}
}

int length_demod(int mod_length){
	if(mod_length <= 0){
		return 0;
	}
	else if(mod_length <= 8){
		return (mod_length - 1);
	}
	else if(mod_length <= 16){
		return (mod_length - 2);
	}
	else if(mod_length <= 24){
		return (mod_length - 3);
	}
	else{
		return (mod_length - 4);
	}
}

