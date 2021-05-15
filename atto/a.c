/*

Atto is a simple, fast (doesn't need ncurses) and small (<16k) line editor.
Nevertheless it provides features like a buffer, search&replace,
an ascii table and a simple hex dumper.

Two things are tricky:
* It's not possible to edit a line directly. Instead the line is printed
  and an "edit string" will be read below.
  - Everything after an 'i' in this string will be inserted in the line.
  - Everything after an 'd' in this string will be deleted in the line.
  - Everything after an 'c' in this string will be changed in the line.
  - A 's' in this string splits the line.
* Inserting new line(s) after the actual line (command 'i') could be
  ended with a '.' in a line itself.

Homepage of Atto: http://web.uta4you.at/shop/atto/index.htm
(c) December/2002 by Dieter Schoppitsch (shop@uta4you.at).

PS: Get some further help by pressing "h" after starting the program.

PPPS:  compile with:  gcc -Wall -s -O3 a.c

I wrote atto as reminiscence to my first programming lesson 1981 on
a honeywell bull host.

*/

#include <stdio.h>

#define ESC 0x1B // escape key
#define BS 0x8 // backspace
#define SPACE 0x20 // space key
#define BACKSPACE 0x7F // backspace key
#define ENTER 0xa // enter
#define STRINGSIZE 1024 // max. stringlength
#define DUMPLEN 100 // characters to dump
#define BUFSIZE 1048575 // max. filesize

FILE *fil;
char *filename;

char buf[BUFSIZE]; // buffer
char tbuf[BUFSIZE]; // temporary buffer

long int p=0; // pointer (actual position)
long int ps=0; // pointer for searching
char ss[STRINGSIZE]=""; // searchstring

void getstring(char *s) // inputs string
{char c,*st;
 st=s;
 while((c=getchar())!=ENTER)
  if(c==BACKSPACE) {if(s-st){putchar(BS); s--;}}
  else {*s++=c; putchar(c);}
 *s='\0';
 putchar(ENTER);
}

int pmax() // calculate size and max. pointer
{long int i=0;
 while(buf[i]!=EOF && i<BUFSIZE) i++;
 return i;
}

void ascii() // print ascii table
{
puts("               0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
puts("0000  0 0   0 NL SH SX EX ET EQ AK BL BS HT LF VT FF CR SO SI");
puts("0001  1 1  16 DL D1 D2 D3 D4 NK SN EB CN EM SB ES FS GS RS US");
puts("0010  2 2  32 SP  !  \"  #  $  %  &  '  (  )  *  +  ,  -  .  /");
puts("0011  3 3  48  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?");
puts("0100  4 4  64  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O");
puts("0101  5 5  80  P  Q  R  S  T  U  V  W  X  Y  Z  [  \\  ]  ^  _");
puts("0110  6 6  96  `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o");
puts("0111  7 7 112  p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~ DL");
puts("1000  8 8 128    ‚ƒ„:x");
puts("1001  9 9 144");
puts("1010 10 A 160");
puts("1011 11 B 176");
puts("1100 12 C 192");
puts("1101 13 D 208");
puts("1110 14 E 224");
puts("1111 15 F 240");
}

void printtab() //print tabulator ruler
{puts("....:....1....:....2....:....3....:....4....:....5....:....6....:....7....:....8");}

void shiftup(int n) // shift buf from position p n times up
{int long i; for(i=pmax();i>=p;i--) buf[i+n]=buf[i];}

void shiftdown(int n) // shift buf from position p n times down
{int long i,maxn=pmax()-n; for(i=p;i<=maxn;i++) buf[i]=buf[i+n];}

void help()	// print help screen
{
 puts("--------------------------------------------------------");
 puts("ATTO, the simple line editor             usage: <a file>");
 puts("Version 0.1         2002           by Dieter Schoppitsch");
 puts("\nENTER ... print new line");
 puts("SPACE ... goto/list next line");
 puts("+     ... goto next line");
 puts("-     ... goto previous line\n");
 puts("Ascii Table         inFo                  Replace all");
 puts("Backlist            Goto line             Search");
 puts("Copy to buffer      Help                  print Tab");
 puts("Delete line         Insert new line       saVe buffer as");
 puts("Edit line           Join next line        Write file");
 puts("  - Insert string   List line             eXit");
 puts("  - Delete string   search Next           y ... hex dump");
 puts("  - Change string   Open file to buffer");
 puts("  - Split line      Paste buffer");
 puts("\nType number before command to multiple.");
 puts("--------------------------------------------------------");
}

int line(void) // calculates line number
{long int i,n=1;
 for(i=1;i<=p;i++) if(buf[i]=='\n') n++;
 return n;
}

void prev(int n) // n lines backward
{while(n+1 && p) if(buf[p--]=='\n') n--;
 if(p) p+=2;
}

void next(int n) // n lines forward
{int long max=pmax(); while(n && p<max) if(buf[p++]=='\n') n--;}

void gotoline(int n) // goto line n
{p=0; next(n-1);}

void list(int n) // list actual n lines
{long int pt=p,max=pmax();
 while(n && pt<max) if(putchar(buf[pt++])=='\n') n--;
 if(pt==max && buf[pt-1]!='\n') putchar(ENTER);
}

void listback(int n) // lists from n lines back 2*n lines
{long int pt=p;
prev(n+1); list(2*n+1); p=pt;
}

void info() // print some info
{long int i,max=pmax(); int lines=0;
 for(i=0;i<=max;i++) if(buf[i]=='\n') lines++;
 printf("File %s with %ld bytes, line %d of %d\n",filename,i-1,line(),lines);
}

void load() // load file
{long int i=0,pt=0; 
 if((fil=fopen(filename,"r")))
 {while((buf[pt++]=getc(fil))!=EOF && i++<BUFSIZE-1) ;
  fclose(fil);
  puts("file loaded");
 }
 else puts("new file opened");
}

void readbuf() // load file to tbuffer
{long int i=0,pt=0; char fnam[STRINGSIZE]="";
 puts("read file:"); getstring(fnam);
 if((fil=fopen(filename,"r")))
 {while((tbuf[pt++]=getc(fil))!=EOF && i++<BUFSIZE-1) ;
  tbuf[pt-1]='\0';
  fclose(fil);
  puts("file loaded to buffer");
 }
 else puts("buffer loading error");
}

int save() // save file
{long int i=0,pt=0; 
 if((fil=fopen(filename,"w+")))
 {while(buf[pt]!=EOF && i++<BUFSIZE-1) putc(buf[pt++],fil);
  fclose(fil); puts("file saved"); return 1;
 }
 else {puts("file saving error"); return -1;}
}

void savebuf() // save tbuffer as
{long int i=0,pt=0; char fnam[STRINGSIZE]="";
 puts("save as:"); getstring(fnam);
 if((fil=fopen(fnam,"w+")))
 {while(tbuf[pt]!='\0' && tbuf[pt]!=EOF && i++<BUFSIZE-1) putc(tbuf[pt++],fil);
  fclose(fil);
  puts("buffer saved");
 }
 else puts("buffer saving error");
}

void edit() // edit actual line
{int i,j,len; long int pt=p; char s[STRINGSIZE]="";
 putchar('-'); putchar('>'); list(1); putchar('?'); getstring(s);
 len=strlen(s);
 if(len)
 {for(i=0;i<len && s[i]!='i' && s[i]!='d' && s[i]!='c' && s[i]!='s';i++) ;
  if(s[i]=='i')
  {p+=i; shiftup(len-i-1); p=pt; for(j=i+1;j<len;j++) buf[p+j-1]=s[j];}
  if(s[i]=='d') {p+=i; shiftdown(len-i-1); p=pt;}
  if(s[i]=='c') for(j=i+1;j<len;j++) buf[p+j-1]=s[j];
  if(s[i]=='s') {p+=i; shiftup(1); buf[p]='\n'; p++;} 
 }
}

void insert() // insert next line(s) end with '.' in a line itself
{int i,len,loop=1; char s[STRINGSIZE]="";
 while(loop)
 {getstring(s); len=strlen(s);
  if(len==1 && s[0]=='.') loop=0;
  else
  {next(1); shiftup(len+1);
   for(i=0;i<=len;i++) buf[p+i]=s[i];
   buf[p+len]='\n'; next(1);
  }
 }
}

long int copy(int n) // copy n lines to tbuffer
{long int i,pt=p,max=pmax();
 while(n && pt<max) if(buf[pt++]=='\n') n--;
 for(i=p;i<pt;i++) tbuf[i-p]=buf[i];
 tbuf[pt-p]='\0';
 return pt;
}

void delete(int n) // delete lines
{shiftdown(copy(n)-p);}

void paste() // insert tbuffer before actual line
{long int i,len=strlen(tbuf);
 shiftup(len);
 for(i=0;i<len;i++) buf[p+i]=tbuf[i];
}

void joinline() // join next line to actual line
{long int pt=p;
 next(1); p--;
 if(buf[p]=='\n') shiftdown(1);
 p=pt;
}

int search(char *s) // search s from pointer ps on
{int i,len=strlen(s),wrap=0,loop=1; long int count=0,max=pmax();
 char s1[STRINGSIZE]="";
 if(max>len) max-=len;
 while(loop)
 {if(ps>=max && wrap==0) {wrap=1; ps=0; puts("wrapped");}
  if(count>=max) {loop=0; return 0;}
  for(i=0;i<len;i++) s1[i]=buf[ps+i];
  if(strcmp(s,s1)==0) {p=ps; prev(0); loop=0; puts("string found");}
  ps++; count++;
 }
}

void searchstring() // search string
{puts("search for:"); getstring(ss);
 ps=p;
 if(search(ss)==0) puts("string not found");
}

void searchnext() // search next
{search(ss);}

void replace() // replace all
{int i,count=0; long int pt=p,max=pmax(); char s[STRINGSIZE]="",s1[STRINGSIZE]="";
 printf("search for:"); getstring(ss); printf("replace with:"); getstring(s);
 if(max>strlen(ss)) max-=strlen(ss);
 for(p=0;p<max;p++)
 {for(i=0;i<strlen(ss);i++) s1[i]=buf[p+i];
  if(strcmp(ss,s1)==0)
  {shiftdown(strlen(ss)); shiftup(strlen(s));
   for(i=0;i<strlen(s);i++) buf[p+i]=s[i];
   count++;
  }
 }
 p=pt; printf("%d items replaced\n",count);
}

void puthex(char c) // putchar c in hex
{if(c>9) putchar('A'-10+c); else putchar('0'+c);}

void dump(int n) // hexdump from line n on
{long int pt=0,max=pmax(); char c;
 while(n-1 && pt<max) if(buf[pt++]=='\n') n--;
 for(n=0;n<DUMPLEN && pt+n<=max;n++)
 {c=buf[pt+n];
  puthex((c>>4)&0xF); puthex(c&0xF);
  if(c>=' ') putchar(c); else putchar('?');
  putchar(' ');
 }
 putchar(ENTER);
}

void main(int argc, char *argv[]) /* MAIN */
{char n; int i=0,loop=1;

 buf[0]=EOF;
 if(argc<2) help();
 else {filename=argv[1]; load();}

 system("stty -icanon -echo");

 while(loop && (n=getchar())!='q')	// quit
  switch(n)
  {case '1': case '2': case '3': case '4': case '5':
   case '6': case '7': case '8': case '9': case '0':
    i=10*i+(int)(n-'0'); break;
   case ENTER: putchar(ENTER); i=0; break;
   case SPACE: next(1); list(1); i=0; break;
   case '+': if(!i) i=1; next(i); i=0; break; // next line
   case '-': if(!i) i=1; prev(i); i=0; break; // prev line
   case 'a': ascii(); i=0; break; // ascii table
   case 'b': if(!i) i=1; listback(i); i=0; break; // list back
   case 'c': if(!i) i=1; copy(i); i=0; break; // copy to buffer
   case 'd': if(!i) i=1; delete(i); i=0; break; // delete line
   case 'e': edit(); i=0; break; // edit line
   case 'f': info(); i=0; break; // info
   case 'g': if(!i) i=1; gotoline(i); i=0; break; // goto line
   case 'h': help(); i=0; break; // help
   case 'i': insert(); i=0; break; // insert new line
   case 'j': joinline(); i=0; break; // join next line
   case 'l': if(!i) i=1; list(i); i=0; break; // list actual line
   case 'n': searchnext(); i=0; break; // search next string
   case 'o': readbuf(); i=0; break; // read file to buffer
   case 'p': paste(); i=0; break; // paste buffer
   case 'r': replace(); i=0; break; // search and replace all
   case 's': searchstring(); i=0; break; // search string
   case 't': printtab(); i=0; break; //print tabulator
   case 'v': savebuf(); i=0; break; // write file
   case 'w': save(); i=0; break; // write file
   case 'x': if(save()==1) {system("stty icanon echo"); loop=0;} break; // exit
   case 'y': if(!i) i=1; dump(i); i=0; break;

   case 'z': putchar(i); i=0; break;
   default: i=0;
  }
 system("stty icanon echo");
}

/* IDEAS:

*

*/
