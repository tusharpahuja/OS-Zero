#include <kern/conf.h>

#if (PS2DRV)

/*
 * 19:49 <PeanutHorst>
 * { == curly brace   < == angle bracket   [ == bracket   ( == parentheses
 */

#include <stdint.h>
#include <sys/io.h>

#include <zero/param.h>
#include <zero/cdecl.h>
#include <zero/mtx.h>
#define VAL(x)        (x)
#define NOP(x)        ((void)0)
#define NOP3(x, y, z) ((void)0)
#define MALLOC(x)     ((void*)VAL(x))
#define FREE          NOP
#define MEMCPY        NOP3
#define RING_ITEM     uint64_t
#define RING_INVAL    UINT64_C(0)
#include <zero/ring.h>

#include <kern/util.h>
#include <kern/unit/x86/trap.h>
#include <kern/unit/x86/trap.h>
#include <kern/io/drv/pc/ps2.h>

extern void *irqvec[];

//void ps2initkbd_us(void);
void ps2kbdintr(void);
void ps2mouseintr(void);

static struct ps2drv ps2drv ALIGNED(PAGESIZE);

#define ps2readkbd(u8)                                                  \
    __asm__ ("inb %w1, %b0\n" : "=a" (u8) : "Nd" (PS2KBD_PORT))
#define ps2sendkbd(u8)                                                  \
    __asm__ ("outb %b0, %w1\n" : : "a" (u8), "Nd" (PS2KBD_PORT))
#define ps2readmouse(u8)                                                \
    __asm__("inb %w1, %b0" : "=a" (u8) : "i" (PS2MOUSE_INPORT))

#if 0
#define ismodkey(val)                                                   \
    (((val) & 0x80000000) && ((val) & 0xfffffff0) == 0xfffffff0)
#define ps2setkeycode(name)                                             \
    (((((name) >> 8) & 0xff) == PS2KBD_UP_BYTE)                         \
     ? (ps2drv.keytabup[name >> 16] = name##_SYM | PS2KBD_UP_BIT)       \
     : ((((name) & 0xff) == PS2KBD_PREFIX_BYTE)                         \
        ? (ps2drv.keytabmb[name >> 8] = name##_SYM,                     \
           ps2drv.keytabup[name >> 8] = name##_SYM | PS2KBD_UP_BIT)     \
        : (ps2drv.keytab1b[name] = name##_SYM,                          \
           ps2drv.keytabup[name] = name##_SYM | PS2KBD_UP_BIT)))
#endif

//uint8_t         kbdbuf[PAGESIZE] ALIGNED(PAGESIZE);
struct ringbuf  kbdbuf;
struct ringbuf *kbdring = &kbdbuf;

void
ps2initkbd(void)
{
    uint8_t u8;

    ringinit(kbdring,
             kbdbuf.data,
             (PAGESIZE - offsetof(struct ringbuf, data)) / sizeof(RING_ITEM));
    /* enable keyboard */
    ps2sendkbd(PS2KBD_ENABLE);
    do {
        ps2readkbd(u8);
    } while (u8 != PS2KBD_ACK);
    /* choose scancode set 1 */
    ps2sendkbd(PS2KBD_SETSCAN);
    do {
        ps2readkbd(u8);
    } while (u8 != PS2KBD_ACK);
    ps2sendkbd(0x01);
    do {
        ps2readkbd(u8);
    } while (u8 != PS2KBD_ACK);
//    ps2initkbd_us();
    kprintf("PS/2 keyboard with US keymap initialized\n");
    irqvec[IRQKBD] = ps2kbdintr;
    kprintf("PS/2 keyboard interrupt enabled\n");

    return;
}

#if 0
void
ps2initkbd_us(void)
{
    /* modifiers. */
    ps2setkeycode(PS2KBD_LEFTCTRL);
    ps2setkeycode(PS2KBD_LEFTSHIFT);
    ps2setkeycode(PS2KBD_RIGHTSHIFT);
    ps2setkeycode(PS2KBD_LEFTALT);
    ps2setkeycode(PS2KBD_RIGHTALT);
    ps2setkeycode(PS2KBD_CAPSLOCK);
    ps2setkeycode(PS2KBD_NUMLOCK);
    ps2setkeycode(PS2KBD_SCROLLLOCK);

    /* single-byte keys. */

    ps2setkeycode(PS2KBD_ESC);
    ps2setkeycode(PS2KBD_1);
    ps2setkeycode(PS2KBD_2);
    ps2setkeycode(PS2KBD_3);
    ps2setkeycode(PS2KBD_4);
    ps2setkeycode(PS2KBD_5);
    ps2setkeycode(PS2KBD_6);
    ps2setkeycode(PS2KBD_7);
    ps2setkeycode(PS2KBD_8);
    ps2setkeycode(PS2KBD_9);
    ps2setkeycode(PS2KBD_0);
    ps2setkeycode(PS2KBD_MINUS);
    ps2setkeycode(PS2KBD_PLUS);
    ps2setkeycode(PS2KBD_BACKSPACE);

    ps2setkeycode(PS2KBD_TAB);
    ps2setkeycode(PS2KBD_q);
    ps2setkeycode(PS2KBD_w);
    ps2setkeycode(PS2KBD_e);
    ps2setkeycode(PS2KBD_r);
    ps2setkeycode(PS2KBD_t);
    ps2setkeycode(PS2KBD_y);
    ps2setkeycode(PS2KBD_u);
    ps2setkeycode(PS2KBD_i);
    ps2setkeycode(PS2KBD_o);
    ps2setkeycode(PS2KBD_p);
    ps2setkeycode(PS2KBD_OPENBRACKET);
    ps2setkeycode(PS2KBD_CLOSEBRACKET);

    ps2setkeycode(PS2KBD_ENTER);

    ps2setkeycode(PS2KBD_LEFTCTRL);

    ps2setkeycode(PS2KBD_a);
    ps2setkeycode(PS2KBD_s);
    ps2setkeycode(PS2KBD_d);
    ps2setkeycode(PS2KBD_f);
    ps2setkeycode(PS2KBD_g);
    ps2setkeycode(PS2KBD_h);
    ps2setkeycode(PS2KBD_i);
    ps2setkeycode(PS2KBD_j);
    ps2setkeycode(PS2KBD_k);
    ps2setkeycode(PS2KBD_l);
    ps2setkeycode(PS2KBD_SEMICOLON);
    ps2setkeycode(PS2KBD_QUOTE);

    ps2setkeycode(PS2KBD_BACKQUOTE);

    ps2setkeycode(PS2KBD_LEFTSHIFT);

    ps2setkeycode(PS2KBD_BACKSLASH);

    ps2setkeycode(PS2KBD_z);
    ps2setkeycode(PS2KBD_x);
    ps2setkeycode(PS2KBD_c);
    ps2setkeycode(PS2KBD_v);
    ps2setkeycode(PS2KBD_b);
    ps2setkeycode(PS2KBD_n);
    ps2setkeycode(PS2KBD_m);
    ps2setkeycode(PS2KBD_COMMA);
    ps2setkeycode(PS2KBD_DOT);
    ps2setkeycode(PS2KBD_SLASH);

    ps2setkeycode(PS2KBD_RIGHTSHIFT);

    ps2setkeycode(PS2KBD_KEYPADASTERISK);

    ps2setkeycode(PS2KBD_SPACE);

    ps2setkeycode(PS2KBD_CAPSLOCK);

    ps2setkeycode(PS2KBD_F1);
    ps2setkeycode(PS2KBD_F2);
    ps2setkeycode(PS2KBD_F3);
    ps2setkeycode(PS2KBD_F4);
    ps2setkeycode(PS2KBD_F5);
    ps2setkeycode(PS2KBD_F6);
    ps2setkeycode(PS2KBD_F7);
    ps2setkeycode(PS2KBD_F8);
    ps2setkeycode(PS2KBD_F9);
    ps2setkeycode(PS2KBD_F10);

    ps2setkeycode(PS2KBD_NUMLOCK);
    ps2setkeycode(PS2KBD_SCROLLLOCK);

    ps2setkeycode(PS2KBD_F11);
    ps2setkeycode(PS2KBD_F12);

    ps2setkeycode(PS2KBD_KEYPAD7);
    ps2setkeycode(PS2KBD_KEYPAD8);
    ps2setkeycode(PS2KBD_KEYPAD9);

    ps2setkeycode(PS2KBD_KEYPADMINUS2);

    ps2setkeycode(PS2KBD_KEYPAD4);
    ps2setkeycode(PS2KBD_KEYPAD5);
    ps2setkeycode(PS2KBD_KEYPAD6);

    ps2setkeycode(PS2KBD_KEYPADPLUS);

    ps2setkeycode(PS2KBD_KEYPADEND);
    ps2setkeycode(PS2KBD_KEYPADDOWN);
    ps2setkeycode(PS2KBD_KEYPADPGDN);

    ps2setkeycode(PS2KBD_KEYPADINS);
    ps2setkeycode(PS2KBD_KEYPADDEL);

    ps2setkeycode(PS2KBD_SYSRQ);

    /* dual-byte sequences. */

    ps2setkeycode(PS2KBD_KEYPADENTER);
    ps2setkeycode(PS2KBD_RIGHTCTRL);
    ps2setkeycode(PS2KBD_FAKELEFTSHIFT);
    ps2setkeycode(PS2KBD_KEYPADMINUS3);
    ps2setkeycode(PS2KBD_FAKERIGHTSHIFT);
    ps2setkeycode(PS2KBD_CTRLPRINTSCREEN);
    ps2setkeycode(PS2KBD_RIGHTALT);
    ps2setkeycode(PS2KBD_CTRLBREAK);
    ps2setkeycode(PS2KBD_HOME);
    ps2setkeycode(PS2KBD_UP);
    ps2setkeycode(PS2KBD_PGUP);
    ps2setkeycode(PS2KBD_LEFT);
    ps2setkeycode(PS2KBD_RIGHT);
    ps2setkeycode(PS2KBD_END);
    ps2setkeycode(PS2KBD_DOWN);
    ps2setkeycode(PS2KBD_PGDN);
    ps2setkeycode(PS2KBD_INS);
    ps2setkeycode(PS2KBD_DEL);

    /* acpi codes. */
    ps2setkeycode(PS2KBD_POWER);
    ps2setkeycode(PS2KBD_SLEEP);
    ps2setkeycode(PS2KBD_WAKE);
    ps2setkeycode(PS2KBD_POWERUP);
    ps2setkeycode(PS2KBD_SLEEPUP);
    ps2setkeycode(PS2KBD_WAKEUP);

    return;
}
#endif /* 0 */

void
ps2kbdflush(uint64_t keycode, int32_t keyval)
{
    ;
}

void
ps2kbdaddkey(uint64_t keycode)
{
    mtxlk(&kbdring->lk);
    ringput(kbdring, keycode);
    mtxunlk(&kbdring->lk);
}

/* keyboard interrupt handler. */
void
ps2kbdintr(void)
{
    uint64_t keycode;
    uint8_t  u8;
    
    ps2readkbd(u8);
    keycode = u8;
    if (u8 == PS2KBD_PAUSE_BYTE1) {
        /* pause/break. */
        ps2readkbd(u8); /* 0x1d */
        keycode = UINT64_C(0x1d) << 8;
        ps2readkbd(u8); /* 0x45 */
        keycode |= UINT64_C(0x45) << 16;
        ps2readkbd(u8); /* 0xe1 */
        keycode |= UINT64_C(0xe1) << 24;
        ps2readkbd(u8); /* 0x9d */
        keycode |= UINT64_C(0x9d) << 32;
        ps2readkbd(u8); /* 0xc5 */
        keycode |= UINT64_C(0xc5) << 40;
    } else if (u8 & PS2KBD_PREFIX_BYTE) {
        /* 0xe0-prefixed. */
        ps2readkbd(u8); /* 0xe0 */
        keycode |= UINT64_C(0xe0) << 8;
        if (u8 == PS2KBD_PRINT_BYTE2
            || u8 == PS2KBD_CTRLPAUSE_BYTE2
            || u8 == PS2KBD_UP_BYTE) {
            ps2readkbd(u8);
            keycode |= (uint64_t)u8 << 16;
        }
    }
    ps2kbdaddkey(keycode);

    return;
}

void
ps2initmouse(void)
{
    irqvec[IRQMOUSE] = ps2mouseintr;
    kprintf("PS/2 mouse interrupt enabled\n");

    return;
}

void
ps2mouseintr(void)
{
    uint32_t val;
    int32_t  xmov;
    int32_t  ymov;
    int32_t  zmov;
    int32_t  xtra;
    int32_t  shift;
    int32_t  tmp;
    uint8_t  mask;
    uint8_t  state;
    uint8_t  u8;

    ps2readmouse(mask);
    ps2readmouse(u8);
    xmov = u8;
    ps2readmouse(u8);
    ymov = u8;

    val = ps2drv.mousestate.flags;
    zmov = 0;
    val &= PS2MOUSE_WHEELMASK;                     /* scroll-wheel?. */
    state = mask & PS2MOUSE_3BTNMASK;               /* button 1, 2 & 3 states. */
    if (val) {
        /* mouse with scroll-wheel, extra (4th) data byte. */
        ps2readmouse(u8);
        xtra = u8;
        val &= PS2MOUSE_WHEEL5BTN;                /* 5-button?. */
        zmov = xtra & PS2MOUSE_ZMASK;              /* z-axis movement. */
        tmp = xtra & PS2MOUSE_ZSIGN;              /* extract sign bit. */
        if (val) {
            state |= (xtra >> 1) & PS2MOUSE_XBTNMASK; /* button 4 & 5 states. */
        }
        if (tmp) {
            zmov = -zmov;
        }
    }
    ps2drv.mousestate.state = state;

    shift = ps2drv.mousestate.shift;               /* scale (speed) value. */

    val = ps2drv.mousestate.x;
    tmp = mask & PS2MOUSE_XOVERFLOW;
    if (tmp) {
        xmov = 0xff;
    } else if (shift > 0) {
        xmov <<= shift;
    } else {
        xmov >>= shift;
    }
    tmp = mask & PS2MOUSE_XSIGN;
    if (tmp) {
        xmov = -xmov;
    }
    if (xmov < 0) {
        ps2drv.mousestate.x = (val < -xmov) ? 0 : (val + xmov);
    } else {
        tmp = ps2drv.mousestate.xmax;
        ps2drv.mousestate.x = (val < tmp - val) ? (val + xmov) : tmp;
    }

    val = ps2drv.mousestate.y;
    tmp = mask & PS2MOUSE_YOVERFLOW;
    if (tmp) {
        ymov = 0xff;
    } else if (shift > 0) {
        ymov <<= shift;
    } else {
        ymov >>= shift;
    }
    tmp = mask & PS2MOUSE_YSIGN;
    if (tmp) {
        ymov = -ymov;
    }
    if (ymov < 0) {
        ps2drv.mousestate.y = (val < -ymov) ? 0 : (val + ymov);
    } else {
        tmp = ps2drv.mousestate.ymax;
        ps2drv.mousestate.y = (val < tmp - val) ? (val + ymov) : tmp;
    }

    if (zmov) {
        val = ps2drv.mousestate.z;
        if (zmov < 0) {
            ps2drv.mousestate.z = (val < -zmov) ? 0 : (val + zmov);
        } else {
            tmp = ps2drv.mousestate.zmax;
            ps2drv.mousestate.z = (val < tmp - val) ? (val + zmov) : tmp;
        }
    }

    return;
}

void
ps2init(void)
{
    ps2initkbd();
    ps2initmouse();
}

#endif /* PS2DRV */

