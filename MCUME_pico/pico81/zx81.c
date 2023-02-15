
#define PROGMEM
#include <string.h>
#include "Z80.h"
//#include "Arduino.h"
#include "zx80rom.h"
#include "zx81rom.h"
#include "emuapi.h"
#include "common.h"
#include "AY8910.h"
#ifdef USB_HID
#include "hid_app.h"
#include "hid_usb.h"
#endif

#define MEMORYRAM_SIZE 0x10000

static AY8910 ay;
//byte memo[ MEMORYRAM_SIZE ];
byte * mem = 0;
unsigned char *memptr[64];
int memattr[64];
int unexpanded=0;
int nmigen=0,hsyncgen=0,vsync=0;
int vsync_visuals=1;
int signal_int_flag=0;
int interrupted=0;
int ramsize=32; //32;

/* the keyboard state and other */
static byte keyboard[ 8 ] = {0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff};;
static byte * XBuf=0; 
int zx80=0;
int autoload=1;


struct { unsigned char R,G,B; } Palette[16] = {
  {   0,   0,   0},
  { 255, 255, 255}
};

/* the ZX81 char is used to index into this, to give the ascii.
 * awkward chars are mapped to '_' (underscore), and the ZX81's
 * all-caps is converted to lowercase.
 * The mapping is also valid for the ZX80 for alphanumerics.
 * WARNING: this only covers 0<=char<=63!
 */
static char zx2ascii[64]={
/*  0- 9 */ ' ', '_', '_', '_', '_', '_', '_', '_', '_', '_',
/* 10-19 */ '_', '\'','#', '$', ':', '?', '(', ')', '>', '<',
/* 20-29 */ '=', '+', '-', '*', '/', ';', ',', '.', '0', '1',
/* 30-39 */ '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
/* 40-49 */ 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
/* 50-59 */ 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
/* 60-63 */ 'w', 'x', 'y', 'z'
};

static char ascii2zx[96]=
  {
   0, 0,11,12,13, 0, 0,11,16,17,23,21,26,22,27,24,
  28,29,30,31,32,33,34,35,36,37,14,25,19,20,18,15,
  23,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,
  53,54,55,56,57,58,59,60,61,62,63,16,24,17,11,22,
  11,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,
  53,54,55,56,57,58,59,60,61,62,63,16,24,17,11, 0
  };

const byte map_qw[8][5] = {
    {25, 6,27,29,224},// vcxz<caps shift=Lshift>
    {10, 9, 7,22, 4}, // gfdsa
    {23,21, 8,26,20}, // trewq
    {34,33,32,31,30}, // 54321
    {35,36,37,38,39}, // 67890
    {28,24,12,18,19}, // yuiop
    {11,13,14,15,40}, // hjkl<enter>
    { 5,17,16,1,44},  // bnm. <space>
};

static char tapename[64]={0};
static const int kBuf[]={13,25,19,25,19,40}; //,21,40}; // LOAD "" (J shift p shift p, R ENTER) 
static const int tBuf[]={2,0,2,0,2,2};//200!,2,2};
static int kcount=0;
static int timeout=100;



unsigned int in(int h, int l)
{

  int ts=0;    /* additional cycles*256 */
  int tapezeromask=0x80;  /* = 0x80 if no tape noise (?) */

  if(!(l&4)) l=0xfb;
  if(!(l&1)) l=0xfe;

  switch(l)
  {
  //case 0xfb:
  //  return(printer_inout(0,0));
    
  case 0xfe:
    /* also disables hsync/vsync if nmi off
     * (yes, vsync requires nmi off too, Flight Simulation confirms this)
     */
    if(!nmigen)
    {
      hsyncgen=0;

      /* if vsync was on before, record position */
      if(!vsync)
        vsync_raise();
      vsync=1;

    }

    switch(h)
    {
        case 0xfe:  return(ts|(keyboard[0]^tapezeromask));
        case 0xfd:  return(ts|(keyboard[1]^tapezeromask));
        case 0xfb:  return(ts|(keyboard[2]^tapezeromask));
        case 0xf7:  return(ts|(keyboard[3]^tapezeromask));
        case 0xef:  return(ts|(keyboard[4]^tapezeromask));
        case 0xdf:  return(ts|(keyboard[5]^tapezeromask));
        case 0xbf:  return(ts|(keyboard[6]^tapezeromask));
        case 0x7f:  return(ts|(keyboard[7]^tapezeromask));
        default:
          {
            int i,mask,retval=0xff;
          
            /* some games (e.g. ZX Galaxians) do smart-arse things
            * like zero more than one bit. What we have to do to
            * support this is AND together any for which the corresponding
            * bit is zero.
            */
            for(i=0,mask=1;i<8;i++,mask<<=1)
              if(!(h&mask))
                retval&=keyboard[i];
            return(ts|(retval^tapezeromask));
          }
    }
    break;
  }

  return(ts|255);
}

unsigned int out(int h,int l,int a)

{
  /* either in from fe or out to ff takes one extra cycle;
   * experimentation strongly suggests not only that out to
   * ff takes one extra, but that *all* outs do.
   */
  int ts=1;  /* additional cycles */



  /* the examples in the manual (using DF/0F) and the
   * documented ports (CF/0F) don't match, so decode is
   * important for that.
   */
  if(!(l&0xf0))   /* not sure how many needed, so assume all 4 */
    l=0x0f;
  else
    if(!(l&0x20))   /* bit 5 low is common to DF and CF */
      l=0xdf;


  if(!(l&4)) l=0xfb;
  if(!(l&2)) l=0xfd;
  if(!(l&1)) l=0xfe;


  switch(l)
  {
    case 0x0f:    /* Zon X data */   
      WrData8910(&ay,a);
      break;
    case 0xdf:    /* Zon X reg. select */  
      WrCtrl8910(&ay,(a &0x0F));
      break;
  
    case 0xfb:
      return(ts/*|printer_inout(1,a)*/);
    case 0xfd:
      nmigen=0;
      if(vsync)
        vsync_lower();
      vsync=0;
      hsyncgen=1;
      break;
    case 0xfe:
      if(!zx80)
      {
        nmigen=1;
        break;
      }
      /* falls through, if zx80 */
    case 0xff:  /* XXX should *any* out turn off vsync? */
      /* fill screen gap since last raising of vsync */
      if(vsync)
        vsync_lower();
      vsync=0;
      hsyncgen=1;
      break;
  }

  return(ts);
}



void sighandler(int a)
{
  signal_int_flag=1;
}

void frame_pause(void)
{
  signal_int_flag=0;

  if(interrupted<2)
    interrupted=1;
}

void do_interrupt()
{
  /* being careful here not to screw up any pending reset... */
  if(interrupted==1)
    interrupted=0;
}

static int ik;
static int ihk;

void z81_Input(int bClick) {
  ik  = emu_GetPad();
  ihk = emu_ReadI2CKeyboard();
}

#if defined(USE_VGA) && defined(SUPPORT_1BPP)
void bitbufBlit(unsigned char * buf, int liney)
{
  emu_DrawVsync();

  int margin = (ZX_VID_MARGIN>>1);
  if (liney >= (HEIGHT+(3<<3)))
  {
    // Have drawn more than HEIGHT lines. Allow the display
    // to grow down for 3 rows, then adjust the vertical margin
    // to display as many lines as possible,
    // making sure top of display is still visible
    int inc = ((liney - HEIGHT)>>1);
    margin += (inc < (ZX_VID_MARGIN>>1)) ? inc : ZX_VID_MARGIN>>1;
  }

  buf = buf + (margin*(ZX_VID_FULLWIDTH>>3))+1+(BORDER>>3);
  for(int y=0;y<HEIGHT;y++)
  {
    // Draw 34 characters = 34*8 = 272 pixels
    emu_DrawLine1BPP(buf, BORDER, y, 34<<3, true);
    buf += (ZX_VID_FULLWIDTH>>3);
  }
}

void blankScreen()
{
  emu_DrawVsync();
  memset( XBuf, 0xFF, WIDTH>>3);

  for(int y=0;y<HEIGHT;y++)
  {
    // Blank entire screen
    emu_DrawLine1BPP(XBuf, 0, y, WIDTH, false);
  }
}
#else
void bitbufBlit(unsigned char * buf, int liney)
{
  emu_DrawVsync();  
  memset( XBuf, 1, WIDTH );
  int margin = (ZX_VID_MARGIN>>1);
  if (liney >= HEIGHT)
  {
    // Have drawn more than HEIGHT lines, so adjust the
    // vertical margin to display as many lines as possible,
    // making sure top of display is still visible
    int inc = ((liney - HEIGHT)>>1);
    margin += (inc < (ZX_VID_MARGIN>>1)) ? inc : ZX_VID_MARGIN>>1;
  }
  buf = buf + (margin*(ZX_VID_FULLWIDTH>>3))+1;
  int y,x,i;
  byte d;
  for(y=0;y<HEIGHT;y++)
  {
    byte * src = buf + 4;
    for(x=0;x<34;x++)
    {
      byte * dst=&XBuf[(x<<3)+BORDER];
      d = *src++;
      for (i=0;i<8;i++)
      {
        if ( d & 128 )
        {
          *dst++=0;
        }
        else
        {
          *dst++=1;
        }
        d <<= 1;
      }       
    }
    emu_DrawLine(&XBuf[0], WIDTH, HEIGHT, y);   
    buf += (ZX_VID_FULLWIDTH>>3);
  }
}

void blankScreen()
{
  emu_DrawVsync();
  memset( XBuf, 0xFF, WIDTH);

  for(int y=0;y<HEIGHT;y++)
  {
    // Blank entire screen
    emu_DrawLine(&XBuf[0], WIDTH, HEIGHT, y);
  }
}
#endif

static void updateKeyboard(void)
{
#ifdef USB_HID
  readUsbKeyboard();

  // test emulation of joystick
  joystickToKeyboard(1, HID_KEY_7, HID_KEY_6, HID_KEY_5, HID_KEY_8, HID_KEY_0);

#else
  //int k = ik; //emu_GetPad();
  int hk = ihk; //emu_ReadI2CKeyboard();  
  //if ( hk == 0 )  {
  memset(keyboard, 0xff, sizeof(keyboard));
  //}
  //else 
  {
    int shift = hk;
    if (hk >=128) hk -= 128;
    else if (hk >=64) hk -= 64;    
    // scan all possibilities
    for (int j=0;j<8;j++) {
      for(int i=0;i<5;i++){
        if ( /*(k == map_qw[j][i]) ||*/ (hk == map_qw[j][i]) ) {
            keyboard[j] &= ~ (1<<(4-i));
        }   
      }  
    } 
    if (shift >=64) keyboard[0] &= ~ (1<<0);  // SHift 
    //else if (shift >=64) keyboard[7] &= ~ (1<<1);  // SHift symboles     
  }
#endif
}

/*
static void handleKeyBuf(void)
{
  if (timeout) {
    timeout--;
    if (timeout==0) {
      memset(keyboard, 0xff, sizeof(keyboard));
      emu_printf("key up");
    }
  }
  else {
    if (!(kcount == (sizeof(kBuf)/sizeof(int)))) {
      emu_printf("key dw");     
      timeout=tBuf[kcount];
      int k=kBuf[kcount++];
      // scan all possibilities
      for (int j=0;j<8;j++) {
        for(int i=0;i<5;i++){
          if ( (k == map_qw[j][i]) ) {
              keyboard[j] &= ~ (1<<(4-i));
          }   
        }  
      } 
      if (timeout == 0) {
        timeout=tBuf[kcount];
        int k=kBuf[kcount++];
        // scan all possibilities
        for (int j=0;j<8;j++) {
          for(int i=0;i<5;i++){
            if ( (k == map_qw[j][i]) ) {
                keyboard[j] &= ~ (1<<(4-i));
            }   
          }  
        }         
      }      
    }       
  }
}
*/


/* despite the name, this also works for the ZX80 :-) */
void reset81()
{
  interrupted=2;  /* will cause a reset */
  memset(mem+0x4000,0,0xc000);  
}

static unsigned char fname[256];

void load_p(int a)
{
  unsigned char *ptr=mem+(a&32767),*dptr=fname;

  emu_printf("loading...");
  if(a<32768)
  {
    emu_printf("name provided");

    // Try to open the name provided
    memset(fname,0,sizeof(fname));
    strcat(fname,"z81/");
    dptr += strlen(fname);

    do
      *dptr++=zx2ascii[(*ptr)&63];
    while((*ptr++)<128 && dptr<fname+sizeof(fname)-3);

    /* add '.p' */
    strcat(fname,".p");
  }
  else
  {
    // Use value from menu
    strcpy(fname, tapename);
  }

  emu_printf(fname);
  int size = emu_FileSize(fname);
  int f = emu_FileOpen(fname, "r+b");
  if ( !f ) {
    /* the partial snap will crash without a file, so reset */
    if(autoload)
      reset81(),autoload=0;
    return;
  }

  autoload=0;
  emu_FileRead(mem + (zx80?0x4000:0x4009), size, f);
  emu_FileClose(f);

  if(zx80)
    store(0x400b,fetch(0x400b)+1);         
}

void save_p(int a)
{
  unsigned char *ptr=mem+a,*dptr=fname;
  FILE *out;

  if(zx80)
  {
      strcpy(fname,"zx80prog.p");
  }
  else
  {
    /* so the filename is at ptr, in the ZX81 char set, with the last char
    * having bit 7 set. First, get that name in ASCII.
    */
    memset(fname,0,sizeof(fname));
    strcat(fname,"z81/");
    dptr += strlen(fname);

    do
      *dptr++=zx2ascii[(*ptr)&63];
    while((*ptr++)<128 && dptr<fname+sizeof(fname)-3);

    /* add '.p' */
    strcat(fname,".p");
  }
  /* work out how much to write, and write it.
  * we need to write from 0x4009 (0x4000 on ZX80) to E_LINE inclusive.
  */
  if(zx80)
    emu_SaveFile(fname, &mem[0x4000], fetch2(16394)-0x4000);
  else
    emu_SaveFile(fname, &mem[0x4009], fetch2(16404)-0x4009);
}



void zx81hacks()
{
  /* patch save routine */
  mem[0x2fc]=0xed; mem[0x2fd]=0xfd;
  mem[0x2fe]=0xc3; mem[0x2ff]=0x07; mem[0x300]=0x02;

  /* patch load routine */
  mem[0x347]=0xeb;
  mem[0x348]=0xed; mem[0x349]=0xfc;
  mem[0x34a]=0xc3; mem[0x34b]=0x07; mem[0x34c]=0x02;
}

void zx80hacks()
{
  /* patch save routine */
  mem[0x1b6]=0xed; mem[0x1b7]=0xfd;
  mem[0x1b8]=0xc3; mem[0x1b9]=0x83; mem[0x1ba]=0x02;

  /* patch load routine */
  mem[0x206]=0xed; mem[0x207]=0xfc;
  mem[0x208]=0xc3; mem[0x209]=0x83; mem[0x20a]=0x02;
}

static void initmem()
{
  int f;
  int count;

  if(zx80)
  {
    memset(mem+0x1000,0,0xf000);
  }
  else
  {
    memset(mem+0x2000,0,0xe000);
  }


  /* ROM setup */
  count=0;
  for(f=0;f<16;f++)
  {
    memattr[f]=memattr[32+f]=0;
    memptr[f]=memptr[32+f]=mem+1024*count;
    count++;
    if(count>=(zx80?4:8)) count=0;
  }

  /* RAM setup */
  if(unexpanded)
    ramsize=1;
  count=0;
  for(f=16;f<32;f++)
  {
    memattr[f]=memattr[32+f]=1;
    memptr[f]=memptr[32+f]=mem+1024*(16+count);
    count++;
    if(count>=ramsize) count=0;
  }


/* z81's ROM and RAM initialisation code is OK for <= 16K RAM but beyond
 * that it requires a little tweaking.
 * 
 * The following diagram shows the ZX81 + 8K ROM. The ZX80 version is
 * the same except that each 8K ROM region will contain two copies of
 * the 4K ROM.
 * 
 * RAM less than 16K is mirrored throughout the 16K region.
 * 
 * The ROM will only detect up to 8000h when setting RAMTOP, therefore
 * having more than 16K RAM will require RAMTOP to be set by the user
 * (or user program) to either 49152 for 32K or 65535 for 48/56K.
 * 
 *           1K to 16K       32K           48K           56K      Extra Info.
 * 
 *  65535  +----------+  +----------+  +----------+  +----------+ 
 * (FFFFh) | 16K RAM  |  | 16K RAM  |  | 16K RAM  |  | 16K RAM  | DFILE can be
 *         | mirrored |  | mirrored |  |          |  |          | wholly here.
 *         |          |  |          |  |          |  |          | 
 *         |          |  |          |  |          |  |          | BASIC variables
 *         |          |  |          |  |          |  |          | can go here.
 *  49152  +----------+  +----------+  +----------+  +----------+ 
 * (C000h) | 8K ROM   |  | 16K RAM  |  | 16K RAM  |  | 16K RAM  | BASIC program
 *         | mirrored |  |          |  |          |  |          | is restricted
 *  40960  +----------+  |          |  |          |  |          | to here.
 * (A000h) | 8K ROM   |  |          |  |          |  |          | 
 *         | mirrored |  |          |  |          |  |          | 
 *  32768  +----------+  +----------+  +----------+  +----------+ 
 * (8000h) | 16K RAM  |  | 16K RAM  |  | 16K RAM  |  | 16K RAM  | No machine code
 *         |          |  |          |  |          |  |          | beyond here.
 *         |          |  |          |  |          |  |          | 
 *         |          |  |          |  |          |  |          | DFILE can be
 *         |          |  |          |  |          |  |          | wholly here.
 *  16384  +----------+  +----------+  +----------+  +----------+ 
 * (4000h) | 8K ROM   |  | 8K ROM   |  | 8K ROM   |  | 8K RAM   | 
 *         | mirrored |  | mirrored |  | mirrored |  |          | 
 *   8192  +----------+  +----------+  +----------+  +----------+ 
 * (2000h) | 8K ROM   |  | 8K ROM   |  | 8K ROM   |  | 8K ROM   | 
 *         |          |  |          |  |          |  |          | 
 *      0  +----------+  +----------+  +----------+  +----------+ 
 */

  switch(ramsize)
  {
    case 56:
      for(f=8;f<16;f++)
      {
        memattr[f]=1;         /* It's now writable */
        memptr[f]=mem+1024*f;
      }
    case 48:
      for(f=48;f<64;f++)
      {
        memattr[f]=1;
        memptr[f]=mem+1024*f;
      }
    case 32:
      for(f=32;f<48;f++)
      {
        memattr[f]=1;
        memptr[f]=mem+1024*f;
      }
      break;
  }

  if(zx80)
    zx80hacks();
  else
    zx81hacks();
}


void emu_KeyboardOnDown(int keymodifer, int key) {
}

void emu_KeyboardOnUp(int keymodifer, int key) {
}


void z81_Init(void) 
{
#if HAS_SND
  emu_sndInit(); 
#endif 

#ifdef USB_HID
  initialiseHid(keyboard);
#endif
  
  if (XBuf == 0) XBuf = (byte *)emu_Malloc(WIDTH*8);
  /* Set up the palette */
  int J;
  for(J=0;J<2;J++)
    emu_SetPaletteEntry(Palette[J].R,Palette[J].G,Palette[J].B, J);


  emu_printf("Allocating RAM");
  if (mem == 0) mem = emu_Malloc(MEMORYRAM_SIZE); //&memo[0];
  
  Reset8910(&ay,3500000,0);
  
  /* load rom with ghosting at 0x2000 */
  int siz=(zx80?4096:8192);  
  if(zx80)
  {
    memcpy( mem + 0x0000, zx80rom, siz );    
  }
  else 
  {
    memcpy( mem + 0x0000, zx81rom, siz );   
  }
  memcpy(mem+siz,mem,siz);
  if(zx80)
    memcpy(mem+siz*2,mem,siz*2);
    
  initmem();
 
  /* reset the keyboard state */
  memset( keyboard, 255, sizeof( keyboard ) );  

  ResetZ80();
 }


void z81_Step(void)
{
  ExecZ80();
  sighandler(0);
  //if (strlen(tapename)) handleKeyBuf();   
  updateKeyboard();
  Loop8910(&ay,20);
}

static int endsWith(const char * s, const char * suffix)
{
  int retval = 0;
  int len = strlen(s);
  int slen = strlen(suffix);
  if (len > slen ) {
    if (!strcmp(&s[len-slen], suffix)) {
      retval = 1;
    }
  }
   return (retval);  
}

void z81_Start(char * filename)
{
  char c;
  strncpy(tapename,filename,64);
  int f = emu_FileOpen(tapename, "r+b");
  if ( f ) {
    int fsize = emu_FileRead(&c, 1, f);
    if ( fsize == 0) { 
      autoload = 0;
      emu_printf("no autoload");
    }
    emu_FileClose(f);
  }

  //emu_setKeymap(1); 
  
  if ( (endsWith(filename, ".80")) || (endsWith(filename, ".o")) || (endsWith(filename, ".O")))  {
    zx80=1;
    ramsize=48;
    //emu_setKeymap(0);    
  }
  else if (endsWith(filename, ".56") ) {
    ramsize = 56;
  }
}
