#include "message.h"

struct StopAndWaitHeader create_header(bool seq_num, bool ack, u_int16_t data_size, u_int32_t crc)
{
    struct StopAndWaitHeader header;
    header.seq_num = seq_num;
    header.ack = ack;
    header.data_size = data_size;
    header.crc = crc;
    return header;
}

struct StopAndWaitMessage create_message(struct StopAndWaitHeader header, u_int8_t *data)
{
    struct StopAndWaitMessage message;
    message.header = header;
    message.data = data;
    return message;
}
int findMSBPosition(uint8_t *x, int length, bool little_edian)
{
    /*
        This funciton finds the position of the most significant bit in a byte array.
        It returns -1 if the array is all zeros.
        It returns the same value for big and little edian. It is the value x
        such that the msb symbolizes 2^x.
    */
    int byte_pos = 0;
    int byte_change = 1;
    if (little_edian)
    {
        byte_pos = length - 1;
        byte_change = -1;
    }
    uint8_t y = (uint8_t)x[byte_pos];
    // Find biggest non-zero byte
    while (y == 0 && byte_pos > 0)
    {
        byte_pos += byte_change;
        y = (uint8_t)x[byte_pos];
    }
    if (y == 0)
    {
        return -1;
    }
    int place = (length - 1 - byte_pos) * 8;
    if (little_edian)
    {
        place = (byte_pos) * 8;
    }

    place += floor(log2(y));
    return place;
}
uint32_t calculate_32crc_little_edian(uint64_t crc_divisor, struct StopAndWaitMessage message)
{
    /*
        This function calculates the crc of a whole message. It only works for as intended if both
        sender and receiver use little edian.
        The calculated crc should be included in the message. If the crc is correct, the remainder is 0.
    */
    int header_size = sizeof(message.header);
    int data_size = message.header.data_size;
    int size = header_size + data_size;
    uint8_t data[size];
    memcpy(&data, &message.header, header_size);
    memcpy(&data[header_size], message.data, data_size);
    uint32_t remainder = calculate_32crc(crc_divisor, data, size);
    return remainder;
}
uint32_t calculate_32crc(uint64_t crc_divisor, const uint8_t *data, int data_size)
{
    /*
        This function calculates the crc of the data in a message. It works for both big and little edian.
        However it is meant to be uesed differently than the calculate_32crc_little_edian function.
        The calculated crc should be sent in the message, and simply checked by the receiver.
        This funciton does thus not neccessarly return 0 if the crc is correct.
    */
    bool little_edian = is_little_edian();
    uint8_t remainder[data_size];
    memcpy(&remainder, data, data_size); // copy data for modification
    int MSB_pos = findMSBPosition(remainder, data_size, little_edian);
    // printf("MSB_pos: %d\n", MSB_pos);
    while (MSB_pos >= NUM_BITS_CRC_DIVISOR - 1)
    {
        int start_byte = data_size - 1 - (MSB_pos / 8);
        if (little_edian)
        {
            start_byte = MSB_pos / 8 - (NUM_BYTES_CRC_DIVISOR - 1); // zero-indexed start byte of the window
        }
        // printf("start_byte: %d\n", start_byte);
        int shift = MSB_pos - (start_byte) * 8 - (NUM_BITS_CRC_DIVISOR - 1); // number of bits to shift the divisor
        // printf("shift: %d\n", shift);
        uint64_t data_window = 0;
        memcpy(&data_window, &remainder[start_byte], NUM_BYTES_CRC_DIVISOR);
        // printf("data_window: %lx\n", data_window);
        // printf("crc_divisor: %lx\n", crc_divisor);
        data_window ^= CRC_DIVISOR << shift;
        // printf("data_window after bit flip: %lx\n", data_window);
        memcpy(&remainder[start_byte], &data_window, NUM_BYTES_CRC_DIVISOR);
        MSB_pos = findMSBPosition(remainder, data_size, little_edian);
        // printf("MSB_pos: %d\n", MSB_pos);
    }
    uint32_t remainder_int = 0;
    if (little_edian)
    {
        memcpy(&remainder_int, &remainder, 4);
    }
    else
    {
        memcpy(&remainder_int, &remainder[data_size - 4], 4);
    }
    return remainder_int;
}