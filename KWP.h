#ifndef __KWP_H
#define __KWP_H

#define KWP_DATA_OFFSET 0x04

uint8_t KWP_initialized = 0;
uint8_t startCommunication[] = {0x81, 0x10, 0xF1, 0x81, 0x03};
uint8_t readDataByLocalIdentifier[] = {0x82, 0x10, 0xF1, 0x21, 0x01, 0xA5};
uint8_t need_ack = 0;

enum KWP_packet {
	Fmt = 0, Tgt, Src, Sld
};
enum KWP_packet_size {
	header_3_byte = 3, header_4_byte
};
void KWP_send(unsigned char *s);
void usartSendChr(uint16_t data);
#endif /* __KWP_H */
