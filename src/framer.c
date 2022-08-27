// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Framer interface
 *
 * For P300 framing is supported here
 *
 * Control is by a controlling byte P300_LEADING = 0x41
 *
 * with open and close P300 Mode is switched on, there is new xml-tag <pid> with protocol
 * definition which controls the switching of vitotronic to P300 mode.
 * additional assuming PDUs start by P300_LEADIN, else transferred as send by client
 * TODO: when PID is set, there is no need for defining a controlling x41 in getaddr etc.
 *
 * semaphore handling in vcontrol.c is changed to cover all from open until close to avoid
 * disturbance by other client trying to do uncoordinated open/close
 *
 * 2013-01-31 vheat
 */

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "common.h"
#include "io.h"
#include "framer.h"

typedef unsigned short int uint16;

// general marker of P300 protocol
#define P300_LEADIN 0x41
#define P300_RESET  0x04
#define P300_ENABLE { 0x16, 0x00, 0x00 }

// message type
#define P300_REQUEST      0x00
#define P300_RESPONSE     0x01
#define P300_ERROR_REPORT 0x03
#define P300X_LINK_MNT    0x0f

// function
#define P300_READ_DATA  0x01
#define P300_WRITE_DATA 0x02
#define P300_FUNCT_CALL 0x07

#define P300X_OPEN     0x01
#define P300X_CLOSE    0x00
#define P300X_ATTEMPTS 3

// response
#define P300_ERROR             0x15
#define P300_NOT_INIT          0x05
#define P300_INIT_OK           0x06

// message buffer structure
#define P300_LEADIN_OFFSET     0
#define P300_LEN_OFFSET        1
#define P300_TYPE_OFFSET       2
#define P300_FCT_OFFSET        3
#define P300_ADDR_OFFSET       4
#define P300_RESP_LEN_OFFSET   6
#define P300_BUFFER_OFFSET     7

#define P300_LEADIN_LEN        1
#define P300_LEN_LEN           1
#define P300_CRC_LEN           1
#define P300_EXTRA_BYTES       (P300_LEADIN_LEN + P300_LEN_LEN + P300_CRC_LEN)

#define FRAMER_READ_ERROR      (-1)
#define FRAMER_LINK_STATUS(st) (0xFE00 + st)
#define FRAMER_READ_TIMEOUT    0

#define FRAMER_NO_ADDR         ((uint16) (-1))

// current active command
static uint16 framer_current_addr = FRAMER_NO_ADDR; // stored value depends on Endianess

// current active protocol
static char framer_pid = 0;

// status handling of current command
static void framer_set_actaddr(void *pdu)
{
    char string[100];
    uint16 framer_old_addr;

    if (framer_current_addr != FRAMER_NO_ADDR) {
        logIT(LOG_ERR, ">FRAMER: addr was still active %04X",
                 framer_current_addr);
    }
    framer_old_addr = framer_current_addr;
    framer_current_addr = *(uint16 *) (((char *) pdu) + P300_ADDR_OFFSET);
    logIT(LOG_DEBUG, ">FRAMER: framer_set_actaddr framer_current_addr = %04X (was %04X)",
            framer_current_addr, framer_old_addr);
}

static void framer_reset_actaddr(void)
{
    char string[100];
    logIT(LOG_DEBUG, ">FRAMER: framer_reset_actaddr framer_current_addr = FRAMER_NO_ADDR (was %04X)",
            framer_current_addr);
    framer_current_addr = FRAMER_NO_ADDR;
}

static int framer_check_actaddr(void *pdu)
{
    char string[100];

    if (framer_current_addr != *(uint16 *) (((char *) pdu) + P300_ADDR_OFFSET)) {
        logIT(LOG_ERR, ">FRAMER: addr corrupted stored %04X, now %04X",
                 framer_current_addr,
                 *(uint16 *) (((char *) pdu) + P300_ADDR_OFFSET));
        return -1;
    }
    return 0;
}

// TODO: could cause trouble on addr containing 0xFE
static void framer_set_result(char result)
{
    char string[100];
    logIT(LOG_DEBUG, ">FRAMER: framer_reset_actaddr framer_current_addr = FRAMER_LINK_STATUS(%02X) (was %04X)",
            result, framer_current_addr);
    framer_current_addr = FRAMER_LINK_STATUS(result);
}

static int framer_preset_result(char *r_buf, int r_len, unsigned long *petime)
{
    char string[100];

    if ((framer_pid == P300_LEADIN) &&
        ((framer_current_addr & FRAMER_LINK_STATUS(0)) == FRAMER_LINK_STATUS(0))) {
        r_buf[0] = (char) (framer_current_addr ^ FRAMER_LINK_STATUS(0));
        logIT(LOG_INFO, ">FRAMER: preset result %02X", r_buf[0]);
        return FRAMER_SUCCESS;
    }

    logIT(LOG_INFO, ">FRAMER: no preset result");
    return FRAMER_ERROR;
}

// Synchronization for P300 + switch to P300, back to normal for close -> repeating P300X_ATTEMPTS
static int framer_close_p300(int fd)
{
    char string[100];
    int i;
    char wbuf = P300_RESET;
    char rbuf = 0;
    unsigned long etime;
    int rlen;

    for (i = 0; i < P300X_ATTEMPTS; i++) {
        if (! my_send(fd, &wbuf, 1)) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: reset not send");
            return FRAMER_ERROR;
        }
        etime = 0;
        rlen = receive_nb(fd, &rbuf, 1, &etime);
        if (rlen < 0) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: close read failure for ack");
            return FRAMER_ERROR;
        } else if (rlen == 0) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: close read timeout for ack");
            return FRAMER_ERROR;
        } else if ((rbuf == P300_INIT_OK) || (rbuf == P300_NOT_INIT)) {
            framer_set_result(P300_NOT_INIT);
            logIT(LOG_INFO, ">FRAMER: closed");
            return FRAMER_SUCCESS;
        } else {
            logIT(LOG_ERR, ">FRAMER: unexpected data 0x%02X", rbuf);
            // continue anyway
        }
    }

    framer_set_result(P300_ERROR);
    logIT(LOG_ERR, ">FRAMER: could not close (%d attempts)", P300X_ATTEMPTS);
    return FRAMER_ERROR;
}

static int framer_open_p300(int fd)
{
    char string[100];
    int i;
    char rbuf = 0;
    char enable[] = P300_ENABLE;
    unsigned long etime;
    int rlen;

    for (i = 0; i < P300X_ATTEMPTS; i++) {
        if (! framer_close_p300(fd)) {
            logIT(LOG_ERR, ">FRAMER: could not set start condition");
            return FRAMER_ERROR;
        }

        if (! my_send(fd, enable, sizeof(enable))) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: enable not send");
            return FRAMER_ERROR;
        }

        etime = 0;
        rlen = receive_nb(fd, &rbuf, 1, &etime);
        if (rlen < 0) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: enable read failure for ack");
            return FRAMER_ERROR;
        } else if (rlen == 0) {
            framer_set_result(P300_ERROR);
            logIT(LOG_ERR, ">FRAMER: enable read timeout for ack");
            return FRAMER_ERROR;
        } else if (rbuf == P300_INIT_OK) {
            // hmueller: Replaced framer_set_result(P300_INIT_OK)
            // by framer_reset_actaddr() to avoid error log
            // >FRAMER: addr was still active FE06
            framer_reset_actaddr();
            logIT(LOG_INFO, ">FRAMER: opened");
            return FRAMER_SUCCESS;
        }
    }

    framer_set_result(P300_ERROR);
    logIT(LOG_ERR, ">FRAMER: could not close (%d attempts)", P300X_ATTEMPTS);

    return FRAMER_ERROR;
}

// calculation check sum for P300, assuming buffer is frame and starts by P300_LEADIN
static char framer_chksum(char *buf, int len)
{
    char sum = 0;
    while (len) {
        sum += *buf;
        //printf("framer chksum %d %02X %02X\n", len, *buf, sum);
        buf++;
        len--;
    }
    //printf("framer chksum %02x\n", sum);
    return sum;
}

/*
 * Frame a message in case P300 protocol is indicated
 *
 * Return 0: Error, else length of written bytes
 * This routine reads the first response byte for success status
 *
 * Format
 * Downlink
 * to framer
 * | type | function | addr | exp len |
 * to Vitotronic
 * | LEADIN | payload len | type | function | addr | exp len | chk |
 */
int framer_send(int fd, char *s_buf, int len)
{
    char string[256];

    if ((len < 1) || (! s_buf)) {
        logIT(LOG_ERR, ">FRAMER: invalid buffer %d %p", len, s_buf);
        return FRAMER_ERROR;
    }

    if (framer_pid != P300_LEADIN) {
        return my_send(fd, s_buf, len);
    } else if (len < 3) {
        logIT(LOG_ERR, ">FRAMER: too few for P300");
        return FRAMER_ERROR;
    } else {
        int pos = 0;
        char l_buf[256];
        unsigned long etime;
        int rlen;

        // prepare a new message, fill buffer starting with leadin
        l_buf[P300_LEADIN_OFFSET] = P300_LEADIN;
        l_buf[P300_LEN_OFFSET] = len; // only payload but len may contain other bytes
        memcpy(&l_buf[P300_TYPE_OFFSET], s_buf, len);
        l_buf[P300_LEADIN_LEN + P300_LEN_LEN + len] =
                framer_chksum(l_buf + P300_LEADIN_LEN, len + P300_LEN_LEN);
        if (! my_send(fd, l_buf, len + P300_EXTRA_BYTES)) {
            logIT(LOG_ERR, ">FRAMER: write failure %d",
                     len + P300_EXTRA_BYTES);
            return FRAMER_ERROR;
        }

        etime = 0;
        rlen = receive_nb(fd, l_buf, 1, &etime);
        if (rlen < 0) {
            logIT(LOG_ERR, ">FRAMER: read failure %d", pos + 1);
            return FRAMER_ERROR;
        } else if (rlen == 0) {
            logIT(LOG_ERR, ">FRAMER: timeout for ack %d", pos + 1);
            return FRAMER_ERROR;
        } else if (*l_buf != P300_INIT_OK) {
            logIT(LOG_ERR, ">FRAMER: Error 0x%02X != 0x%02X (P300_INIT_OK)",
                     *l_buf, P300_INIT_OK);
            return FRAMER_ERROR;
        }

        framer_set_actaddr(l_buf);
        logIT(LOG_INFO, ">FRAMER: Command send");

        return FRAMER_SUCCESS;
    }
}

/*
 * Read a framed message in case P300 protocol is indicated
 *
 * Return 0: Error, else length of written bytes
 * This routine reads the first response byte for success status
 *
 * Format
 * from Vitotronic
 * | LEADIN | payload len | type | function | addr | exp len | chk |
 * Uplink
 * from framer
 * | data |
 * This simulates KW return, respective checking of the frame is done in this function
 *
 * etime is forwarded
 * return is FRAMER_ERROR, FRAMER_TIMEOUT or read len
 *
 * WEAKNESS:
 * If any other protocol gets an answer beginning 0x41, then this will return erroneous
 * KW may have values returned starting 0x41
 */
int framer_receive(int fd, char *r_buf, int r_len, unsigned long *petime)
{
    char string[256];
    int rlen;
    int total;
    int rtmp;
    char l_buf[256];
    unsigned long etime;
    char chk;

    // to identify TimerWWMi bug
    l_buf[P300_ADDR_OFFSET] = 0;
    l_buf[P300_ADDR_OFFSET + 1] = 0;

    if ((r_len < 1) || (! r_buf)) {
        logIT(LOG_ERR, ">FRAMER: invalid read buffer %d %p", r_len, r_buf);
        return FRAMER_ERROR;
    }

    if (framer_preset_result(r_buf, r_len, petime)) {
        framer_reset_actaddr();
        return FRAMER_SUCCESS;
    }

    *petime = 0;
    rtmp = receive_nb(fd, l_buf, r_len, petime);
    if (rtmp < 0) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: read failure");
        return FRAMER_READ_ERROR;
    } else if (rtmp == 0) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: read timeout");
        return FRAMER_READ_TIMEOUT;
    } else if (framer_pid != P300_LEADIN) {
        // no P300 frame, just forward
        memcpy(r_buf, l_buf, r_len);
        return rtmp;
    }

    // this is not GWG / KW we know now
    etime = 0;
    // read at least the length info
    if (rtmp < 2) {
        rlen = receive_nb(fd, l_buf + 1, 1, &etime);
        *petime += etime;
        if (rlen < 0) {
            framer_reset_actaddr();
            logIT(LOG_ERR, ">FRAMER: read failure");
            return FRAMER_READ_ERROR;
        } else if (rlen == 0) {
            framer_reset_actaddr();
            logIT(LOG_ERR, ">FRAMER: read timeout");
            return FRAMER_READ_TIMEOUT;
        }
        rtmp += rlen;
    }

    total = l_buf[P300_LEN_OFFSET] + P300_EXTRA_BYTES;
    rlen = total - rtmp;
    if (rlen <= 0) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: strange read %d", rlen);
        return rtmp; // strange should not happen here
    }

    // now read what is extra
    rtmp = receive_nb(fd, l_buf + rtmp, rlen, &etime);
    *petime += etime;

    // check for leadin
    if (l_buf[P300_LEADIN_OFFSET] != P300_LEADIN) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: read leadin error received 0x%02X expected 0x%02X",
                 l_buf[P300_LEADIN_OFFSET], P300_LEADIN);
        return FRAMER_READ_ERROR;
    }

    // bug in Vitotronic getTimerWWMi, we got it, but complete
    if ((l_buf[P300_ADDR_OFFSET] == 0x21) &&
            (l_buf[P300_ADDR_OFFSET + 1] == 0x10) &&
            (rtmp == -1)) {
        logIT(LOG_ERR, ">FRAMER: bug of getTimerWWMi - omit checksum");
    } else {
        if (rtmp < 0) {
            framer_reset_actaddr();
            logIT(LOG_ERR, ">FRAMER: read final failure");
            return FRAMER_READ_ERROR;
        } else if (rtmp == 0) {
            framer_reset_actaddr();
            logIT(LOG_ERR, ">FRAMER: read final timeout");
            return FRAMER_READ_TIMEOUT;
        }

        chk = framer_chksum(l_buf + P300_LEADIN_LEN, total - 2);
        if (l_buf[total - 1] != chk) {
            framer_reset_actaddr();
            logIT(LOG_ERR, ">FRAMER: read chksum error received 0x%02X calc 0x%02X",
                    (unsigned char)l_buf[total - 1], (unsigned char)chk);
            return FRAMER_READ_ERROR;
        }
    }

    if (l_buf[P300_TYPE_OFFSET] == P300_ERROR_REPORT) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: ERROR address %02X%02X code %d",
                 l_buf[P300_ADDR_OFFSET], l_buf[P300_ADDR_OFFSET + 1],
                 l_buf[P300_BUFFER_OFFSET]);
        return FRAMER_READ_ERROR;
    }

    // TODO: could add check for address receive matching address send before
    if (framer_check_actaddr(l_buf)) {
        framer_reset_actaddr();
        logIT(LOG_ERR, ">FRAMER: not matching response addr");
        return FRAMER_READ_ERROR;
    }

    if ((l_buf[P300_FCT_OFFSET] == P300_WRITE_DATA) && (r_len == 1)) {
        // TODO: could add check for length matching previously requested data length
        if (r_len != (l_buf[P300_LEN_OFFSET] - 4)) {
            logIT(LOG_ERR, ">FRAMER: unexpected length r_len=0x%02X != 0x%02X=l_buf[P300_LEN_OFFSET]-4",
                    r_len, l_buf[P300_LEN_OFFSET] - 4);
            framer_reset_actaddr();
            return FRAMER_READ_ERROR;
        }
        // if we have a P300 setaddr we do not get data back ...
        if (l_buf[P300_TYPE_OFFSET] == P300_RESPONSE) {
            // OK
            r_buf[rtmp] = 0x00;
        } else {
            // NOT OK
            r_buf[rtmp] = 0x01;
        }
    } else {
        if (r_len != (l_buf[P300_RESP_LEN_OFFSET])) {
            logIT(LOG_ERR, ">FRAMER: unexpected length r_len=0x%02X != 0x%02X=l_buf[P300_RESP_LEN_OFFSET]",
                    r_len, l_buf[P300_RESP_LEN_OFFSET]);
            framer_reset_actaddr();
            return FRAMER_READ_ERROR;
        }
        memcpy(r_buf, &l_buf[P300_BUFFER_OFFSET], r_len);
    }

    framer_reset_actaddr();
    return r_len;
}

int framer_waitfor(int fd, char *w_buf, int w_len)
{
    unsigned long etime;

    if (framer_preset_result(w_buf, w_len, &etime)) {
        framer_reset_actaddr();
        return FRAMER_SUCCESS;
    }

    return waitfor(fd, w_buf, w_len);
}

// Device handling, with open and close the mode is also switched to P300/back
int framer_openDevice(char *device, char pid)
{
    char string[100];
    int fd;

    logIT(LOG_INFO, ">FRAMER: open device %s ProtocolID %02X", device, pid);

    if ((fd = openDevice(device)) == -1) {
        return -1;
    }

    if (pid == P300_LEADIN) {
        if (! framer_open_p300(fd)) {
            closeDevice(fd);
            return -1;
        }
    }

    framer_pid = pid;
    return fd;
}

void framer_closeDevice(int fd)
{
    if (framer_pid == P300_LEADIN) {
        framer_close_p300(fd);
    }

    framer_pid = 0;
    closeDevice(fd);
}
