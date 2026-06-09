
/** 
 *  Command: 0xFD
 *  Response Length:  6 
 *  Description: measure T & RH with high precision (high repeatability) 
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC] 
 * */
#define CMD_MEASURE_HIGH_PRECISION 0xFD

/**
 * Command: 0xF6
 * Response Length:  6
 * Description: measure T & RH with medium precision (medium repeatability)
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_MEASURE_MEDIUM_PRECISION 0xF6

/**
 * Command: 0xE0
 * Response Length:  6
 * Description: measure T & RH with lowest precision (low repeatability)
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_MEASURE_LOW_PRECISION 0xE0

/**
 * Command: 0x89
 * Response Length:  6
 * Description: read serial number
 *         [2 * 8-bit data; 8-bit CRC; 2 * 8-bit data; 8-bit CRC]
 */
#define CMD_READ_SERIAL_NUMBER 0x89

/**
 * Command: 0x94
 * Response Length:  1
 * Description: soft reset
 *         [ACK]
 */
#define CMD_SOFT_RESET 0x94

/**
 * Command: 0x39
 * Response Length:  6
 * Description: activate heater with 200mW for 1s, including a high precision measurement just before deactivation
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_200mW_1S 0x39
/**
 * Command: 0x32
 * Response Length:  6
 * Description: activate heater with 200mW for 0.1s including a high precision
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_200mW_0_1S 0x32
/**
 * Command: 0x2F
 * Response Length:  6
 * Description: activate heater with 110mW for 1s including a high precision
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_110mW_1S 0x2F
/**
 * Command: 0x24
 * Response Length:  6
 * Description: activate heater with 110mW for 0.1s including a high precision
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_110mW_0_1S 0x24
/**
 * Command: 0x1E
 * Response Length:  6
 * Description: activate heater with 20mW for 1s including a high precision
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_20mW_1S 0x1E
/**
 * Command: 0x15
 * Response Length:  6
 * Description: activate heater with 20mW for 0.1s including a high precision
 *         [2 * 8-bit T-data; 8-bit CRC; 2 * 8-bit RH-data; 8-bit CRC]
 */
#define CMD_HEATER_20mW_0_1S 0x15


/**
 * Response length for all commands that return T & RH data. 
 * Each measurement consists of 2 bytes for temperature, 1 byte for CRC, 2 bytes for humidity, and 1 byte for CRC, totaling 6 bytes.
 * For the read serial number command, the response length is also 6 bytes (2 bytes data + CRC + 2 bytes data + CRC).
 */
#define READ_RESPONSE_LENGTH 6


/** I2C glitch protection is offered in form of 8-bit checksum
 * CRC8 Parameters:
 */
#define CRC8_POLYNOMIAL 0x31
#define CRC8_INIT_VALUE 0xFF
#define CRC8_FINAL_XOR 0x00

