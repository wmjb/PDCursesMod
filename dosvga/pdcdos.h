/* PDCurses */

#include <curspriv.h>
#include <string.h>

/*----------------------------------------------------------------------
 *  MEMORY MODEL SUPPORT:
 *
 *  MODELS
 *    TINY    cs,ds,ss all in 1 segment (not enough memory!)
 *    SMALL   cs:1 segment, ds:1 segment
 *    MEDIUM  cs:many segments, ds:1 segment
 *    COMPACT cs:1 segment, ds:many segments
 *    LARGE   cs:many segments, ds:many segments
 *    HUGE    cs:many segments, ds:segments > 64K
 */

#ifdef __TINY__
# define SMALL 1
#endif
#ifdef __SMALL__
# define SMALL 1
#endif
#ifdef __MEDIUM__
# define MEDIUM 1
#endif
#ifdef __COMPACT__
# define COMPACT 1
#endif
#ifdef __LARGE__
# define LARGE 1
#endif
#ifdef __HUGE__
# define HUGE 1
#endif

#include <dos.h>

# if SMALL || MEDIUM
#  define PDC_FAR far
# else
#  define PDC_FAR
# endif

/* Information about the current video state */
struct PDC_color
{
    short r, g, b;
    unsigned long mapped;
};

struct PDC_video_state
{
    /* Information about the current video mode: */
    unsigned short scrn_mode;
    int linear_sel;
    unsigned long linear_addr;
    unsigned char bits_per_pixel;
    unsigned short video_width;  /* Width of graphics mode in pixels */
    unsigned short video_height; /* Height of graphics mode in pixels */
    unsigned short bytes_per_line; /* Bytes per raster line */
    /* Location of the frame buffer in memory */
    unsigned short window[2];
    unsigned long offset[2];
    unsigned long window_size;
    unsigned window_gran;
    /* Window used to read and write */
    unsigned char read_win;
    unsigned char write_win;
    /* Color mappings */
    unsigned char red_max;
    unsigned char red_pos;
    unsigned char green_max;
    unsigned char green_pos;
    unsigned char blue_max;
    unsigned char blue_pos;

    /* Font support */
    void (*font_close)(bool bold);
    unsigned (*font_char_width)(bool bold);
    unsigned (*font_char_height)(bool bold);
    const unsigned char *(*font_glyph_data)(bool bold, unsigned long pos);

    bool have_bold_font;
    unsigned font_width;     /* Width of font in pixels */
    unsigned font_height;    /* Height of font in pixels */
    unsigned underline;      /* Where to draw the underline */

    /* Cursor state */
    bool cursor_visible;
    int cursor_row;
    int cursor_col;
    unsigned char cursor_start;
    unsigned char cursor_end;

    struct PDC_color colors[PDC_MAXCOL];
};
extern struct PDC_video_state PDC_state;

extern void PDC_private_cursor_off(void);
extern void PDC_private_cursor_on(int row, int col);

#ifdef __DJGPP__        /* Note: works only in plain DOS... */
# define PDC_FLAT 1
# if DJGPP == 2
#  define _FAR_POINTER(s,o) ((((int)(s)) << 4) + ((int)(o)))
# else
#  define _FAR_POINTER(s,o) (0xe0000000 + (((int)(s)) << 4) + ((int)(o)))
# endif
# define _FP_SEG(p)     (unsigned short)((((long)p) >> 4) & 0xffff)
#else
# ifdef __TURBOC__
#  define _FAR_POINTER(s,o) MK_FP(s,o)
# else
#  if defined(__WATCOMC__) && defined(__FLAT__)
#   define PDC_FLAT 1
#   define _FAR_POINTER(s,o) ((((int)(s)) << 4) + ((int)(o)))
#  else
#   define _FAR_POINTER(s,o) (((long)s << 16) | (long)o)
#  endif
# endif
# define _FP_SEG(p)     (unsigned short)(((long)p) >> 4)
#endif
#define _FP_OFF(p)       ((unsigned short)p & 0x000f)

#ifdef __DJGPP__
# include <sys/movedata.h>
unsigned char getdosmembyte(int offs);
unsigned short getdosmemword(int offs);
unsigned long getdosmemdword(int offs);
void setdosmembyte(int offs, unsigned char b);
void setdosmemword(int offs, unsigned short w);
void setdosmemdword(int offs, unsigned long d);
#else
# define getdosmembyte(offs) \
    (*((unsigned char PDC_FAR *) (offs)))
# define getdosmemword(offs) \
    (*((unsigned short PDC_FAR *) (offs)))
# define getdosmemdword(offs) \
    (*((unsigned long PDC_FAR *) (offs)))
# define setdosmembyte(offs,x) \
    (*((unsigned char PDC_FAR *) (offs)) = (x))
# define setdosmemword(offs,x) \
    (*((unsigned short PDC_FAR *) (offs)) = (x))
# define setdosmemdword(offs,x) \
    (*((unsigned long PDC_FAR *) (offs)) = (x))
#endif

#if defined(__WATCOMC__) && defined(__386__)

typedef union
{
    struct
    {
        unsigned long edi, esi, ebp, res, ebx, edx, ecx, eax;
    } d;

    struct
    {
        unsigned short di, di_hi, si, si_hi, bp, bp_hi, res, res_hi,
                       bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi,
                       flags, es, ds, fs, gs, ip, cs, sp, ss;
    } w;

    struct
    {
        unsigned char edi[4], esi[4], ebp[4], res[4],
                      bl, bh, ebx_b2, ebx_b3, dl, dh, edx_b2, edx_b3,
                      cl, ch, ecx_b2, ecx_b3, al, ah, eax_b2, eax_b3;
    } h;
} pdc_dpmi_regs;

void PDC_dpmi_int(int, pdc_dpmi_regs *);

#endif

#ifdef __DJGPP__
# include <dpmi.h>
# define PDCREGS __dpmi_regs
# define PDCINT(vector, regs) __dpmi_int(vector, &regs)
#else
# ifdef __WATCOMC__
#  ifdef __386__
#   define PDCREGS pdc_dpmi_regs
#   define PDCINT(vector, regs) PDC_dpmi_int(vector, &regs)
#  else
#   define PDCREGS union REGPACK
#   define PDCINT(vector, regs) intr(vector, &regs)
#  endif
# else
#  define PDCREGS union REGS
#  define PDCINT(vector, regs) int86(vector, &regs, &regs)
# endif
#endif

/* Wide registers in REGS: w or x? */

#ifdef __WATCOMC__
# define W w
#else
# define W x
#endif

/* Monitor (terminal) type information */

enum
{
    _NONE, _MDA, _CGA,
    _EGACOLOR = 0x04, _EGAMONO,
    _VGACOLOR = 0x07, _VGAMONO,
    _MCGACOLOR = 0x0a, _MCGAMONO,
    _MDS_GENIUS = 0x30
};
