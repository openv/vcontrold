/*
 * Framer interface
 *
 * For P300 framing is supported here
 *
 * Control is by a controlling byte defining the action
 *
 * 2013-01-31 vheat
 */

/*
 * Main indentification of P300 Protocol is about leadin 0x41
 */

#define FRAMER_ERROR    0
#define FRAMER_SUCCESS    1

int framer_send(int fd,char *s_buf, int len);

int framer_waitfor(int fd, char *w_buf,int w_len);

int framer_receive(int fd, char *r_buf, int r_len, unsigned long *petime);

int framer_openDevice(char *device, char pid);

void framer_closeDevice(int fd);
