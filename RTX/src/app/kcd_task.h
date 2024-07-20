#include "common.h"
#include "k_msg.h"
#include "string.h"
#include "Serial.h"
#include "rtx.h"

#define ASCII_0 48
#define ASCII_9 57
#define ASCII_A 65
#define ASCII_Z 89
#define ASCII_a 97
#define ASCII_z 122

// #define DEBUG_KCD
#ifdef DEBUG_KCD
    #include "printf.h"
    #define DEBUG_PRINT(fmt, ...) printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) (void)0
#endif /* ! DEBUG_KCD */

void kcd_task(void);
void my_memcpy(void* dst, void* src, size_t count);
void send_putty_error(char *s, int length);
int is_alphanumeric(int ascii_decimal);
static void reset_state(U8 *is_new_msg, U8 *is_valid_command, U32 *message_length);
