/* The KCD Task Template File */

#include "kcd_task.h"

void kcd_task(void)
{
	if (mbx_create(KCD_MBX_SIZE) != RTX_OK) {
		DEBUG_PRINT("FATAL ERROR: mailbox creation failed.");
		return;
	}

	/*
	*  Map ASCII decimals (index) to tids (value).
	*  We use:
	*  a) 48 to 57 for 0 to 9,
	*  b) 65 to 89 for A to Z,
	*  c) 91 to 122 for a to z.
	*  Ignore all other identifiers.
	*
	*  If two tasks try to register the same
	*  identifier, take the latest registration (overwrite)
	*
	*  If the identifier is greater than 1 character, ignore it.
	*/
	task_t identifier[123] = {0};

	U8 command_string[64];      // At most 64 bytes, excludes first '%'
	U32 msg_length = 0;         // KEY_IN queue, excludes first '%'
	U8 is_new_msg = 1;			// Previous KEY_IN was 'enter' (ascii 10)
	U8 is_valid_cmd = 0;		// First character was '%'     (ascii 37)

	static U8 buffer[sizeof(RTX_MSG_HDR) + 64]; // Receive up to one character of data (maximum required by KEY_IN/KCD_REG)
	task_t sender_tid = 0;

	DEBUG_PRINT("kcd_task loop starting...");
    while(1) {
        if (recv_msg(&sender_tid, buffer, sizeof(RTX_MSG_HDR) + 64) != RTX_OK) {
            DEBUG_PRINT("ERROR: receive failed. mailbox is empty, or incoming message is larger than %d", sizeof(RTX_MSG_HDR) + 1);
            continue;
        }

    	// To obtain the ascii value, perform the following steps:
        // 1. Cast the buffer pointer to a U32 to allow for addition.
        // 2. Add the size of RTX_MSG_HDR to get the address of the actual data.
        // 3. Cast the resulting address back to a U8 pointer.
        // 4. Dereference to get the ascii value, which can be compared directly with chars or ints.
        U8 ascii = *((U8*)(((U32)buffer) + sizeof(RTX_MSG_HDR)));

        RTX_MSG_HDR* msg_hdr = (RTX_MSG_HDR*)buffer;

		switch (msg_hdr->type) {
		case KEY_IN: {
            DEBUG_PRINT("received KEY_IN: %c (ascii %d)", ascii, ascii);

			if (sender_tid != TID_UART_IRQ) {
				DEBUG_PRINT("ERROR: sender TID %d was not from TID_UART_IRQ", ascii, sender_tid);
				break;
			}

			if (ascii == 13) {
				DEBUG_PRINT("enter key pressed!", ascii, sender_tid);

				task_t target_tid = identifier[(int)command_string[0]];
				if (is_valid_cmd && msg_length <= 64) {
				    DEBUG_PRINT("command string is valid and 64B or less!");

                    if (!msg_length || target_tid == 0 || // No identifier or identifier never registered
                        g_tcbs[target_tid].state == DORMANT) { // Registered task no longer alive
						DEBUG_PRINT("ERROR: command cannot be processed.!");
						char error[28] = "Command cannot be processed";
						send_putty_error(error, 27);

						reset_state(&is_new_msg, &is_valid_cmd, &msg_length);
						break;
					}

					DEBUG_PRINT("preparing to send command string. sizeof(RTX_MSG_HDR) %d, msg_length: %d", sizeof(RTX_MSG_HDR) + msg_length, sizeof(RTX_MSG_HDR), msg_length);
					U8 payload[sizeof(RTX_MSG_HDR)+ 64];
					RTX_MSG_HDR *msg_hdr = (RTX_MSG_HDR*)payload;
					msg_hdr->type = KCD_CMD;
					msg_hdr->length = sizeof(RTX_MSG_HDR) + msg_length;

					my_memcpy(payload + sizeof(RTX_MSG_HDR), command_string, msg_length);

					if (send_msg(target_tid, (void*)msg_hdr) != RTX_OK) {
						DEBUG_PRINT("ERROR: send_msg failed. command cannot be processed.", msg_length);
						char error[28] = "Command cannot be processed";
						send_putty_error(error, 27);

						reset_state(&is_new_msg, &is_valid_cmd, &msg_length);
						break;
					}

					DEBUG_PRINT("message sent successfully!");
					reset_state(&is_new_msg, &is_valid_cmd, &msg_length);
					break;

				} else {
					DEBUG_PRINT("invalid Command!");
					char error[16] = "Invalid Command";
					send_putty_error(error, 15);

					reset_state(&is_new_msg, &is_valid_cmd, &msg_length);
					break;
				}
			} else if (is_new_msg) {
				DEBUG_PRINT("new message: waiting for the %% character...");
				is_valid_cmd = (ascii == 37);
				is_new_msg = 0;
			} else {
				// Build command_string until limit + 1 (+1 to allow overflow detection, msg_length == 65)
				if (is_valid_cmd && msg_length <= 64) {
					command_string[msg_length] = ascii;
					msg_length++;
					DEBUG_PRINT("ascii %d added to command_string! msg_length: %d...", ascii, msg_length);
				}
				else {
					DEBUG_PRINT("WARN: length exceeded 64 or not valid command (did not start with %%)! msg_length: %d", msg_length);
				}
			}
			break;
        }

		case KCD_REG: {
		    DEBUG_PRINT("received KCD_REG. attempting to register identifier %c to TID %d.", ascii, sender_tid);
	    	/*
	    	 * Command registration
	    	 *
	    	 * We should only register commands when
	    	 * a) the size of the data is 1
	    	 * b) the command is alphanumeric (0-9)(A-Z)(a-z)
	    	 */
			if (msg_hdr->length != sizeof(RTX_MSG_HDR) + 1 || !is_alphanumeric(ascii)) {
				DEBUG_PRINT("registration ignored. %c is not alphanumeric OR identifier was longer than 1 char.", ascii);
				break;
			}
			identifier[ascii] = sender_tid;
			DEBUG_PRINT("KCD_REG success! identifier: %c TID: %d\r\n", ascii, sender_tid);
			break;
		}
		default: { // Ignore any other type
			break;
		}
		}
	}
}

int is_alphanumeric(int ascii_decimal) {
	return (
        (ascii_decimal >= ASCII_0 && ascii_decimal <= ASCII_9) ||	// 0 to 9
		(ascii_decimal >= ASCII_A && ascii_decimal <= ASCII_Z) || 	// A to Z
		(ascii_decimal >= ASCII_a && ascii_decimal <= ASCII_z)	    // a to z
    );
}
// This is much less readable, but also much faster! Left for reference.
// static const int is_alphanumeric[124] = {
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
//     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
//     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 
//     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
// };

void my_memcpy(void* dst, void* src, size_t count) {
	DEBUG_PRINT("starting memcpy. dst: %x, src: %x", dst, src);
    U8* p_dst = (U8*) dst;
    U8* p_src = (U8*) src;
    for (size_t i = 0; i < count; i++) {
        p_dst[i] = p_src[i];
        DEBUG_PRINT("copying index %d: text: %c", i, p_src[i]);
    }
}

void send_putty_error(char *s, int length) {
    for (int i = 0; i < length; ++i) {
        SER_PutChar(1, s[i]);
    }
}

static void reset_state(U8 *is_new_msg, U8 *is_valid_cmd, U32 *msg_length) {
	*is_new_msg = 1;
	*is_valid_cmd = 0;
	*msg_length = 0;
}

static void process_KEY_IN() {
    // for refactor
}

static void process_KCD_REG() {
    // for refactor
}
