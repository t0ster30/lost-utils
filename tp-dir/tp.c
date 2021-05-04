/*

Textdraw is a simple (ascii) chart generator. Data in a textfile 
(ie provided by a spreadsheet like sc) can be converted into simple
xy-plots, chart- or column-charts which can be printed to a textfile.
Further treatment with your favorite editor is simple.

Following example of a parabolic plot shows the required format
of the data-textfile:

2 5
 0 0
 1 1
 2 4
 3 9
 4 16

The first two figures show the amount of data (2 colums, 5 values) -
don't forget to add them.

Homepage of Textprint: http://web.uta4you.at/shop/tp/index.htm
(c) August/2003 by Dieter Schoppitsch (shop@uta4you.at).

PS: Start textprint with "tp <data-file-name>"

PPS: Get some further help by pressing "h" after starting the program.

PPPS:  compile with:  gcc -Wall -s -O3 tp.c -lncurses
*/

#include <ncurses.h>

#define MAXCOL 10 // max. data colums
#define MAXROW 100 // max. data rows
#define MAXTXTLEN 200 // max. string length

char chart[MAXTXTLEN][MAXTXTLEN]; //max chart (console) dimensions
char string[MAXTXTLEN]="";
int maxy=25, maxx=80, meny=24;
int dmaxy=25, dmaxx=80;
int row=1, col=2;
int typ=0;
int movex=0, movey=0;

float x[MAXROW][MAXCOL];

FILE *fil;
char *filename;

void help() // print help screen
{clear();
 printw("--------------------------------------------------------\n");
 printw("  TEXTPRINT 0.1 (c) by Dieter Schoppitsch\n");
 printw("  start textprint with \"tp <data-file-name\"\n");
 printw("--------------------------------------------------------\n");
 printw("  h   ... this help\n");
 printw("  p   ... print chart to file 'print'\n");
 printw("  S/s ... scale up/down\n");
 printw("  t   ... choose type of chart\n");
 printw("  X/x ... move chart right/left\n");
 printw("  Y/y ... move chart down/up\n");
 printw("  Z/z ... zoom in/out\n");
 printw("--------------------------------------------------------\n");
 printw("  type any key to continue ...\n");

 refresh();
 getch();
}

void type() // choose diagram type
{clear();
 printw("--------------------------------------------------------\n");
 printw("  CHART TYPE:  \n\n");
 printw("   Points         0\n");
 printw("   Column beside  1\n");
 printw("          stapled 2\n");
 printw("   Bar    beside  3\n");
 printw("          stapled 4\n");
 printw("--------------------------------------------------------\n");
 printw("input desired type:\n");

 typ=getch()-'0'; if(typ<0 || typ>4) typ=0;

 refresh();
}

void drawrec(int x1,int x2,int y1,int y2) // draws rectangle to chart-array
{int i;
 for(i=x1+1;i<x2;i++)
 {chart[y1][i]='_';
  chart[y2][i]='_';
 }
 for(i=y1;i<y2;i++)
 {chart[i+1][x1]='|';
  chart[i+1][x2]='|';
 }
}

void drawpoint() // draws point-chart to chart-array
{int i,j;
 float xmax=0,xmin=0,ymax=0,ymin=0,xf,yf;

 for(i=0;i<row;i++)
 {if(x[0][i]>xmax) xmax=x[0][i]; if(x[0][i]<xmin) xmin=x[0][i];
  for(j=1;j<col;j++)
  {if(x[j][i]>ymax) ymax=x[j][i]; if(x[j][i]<ymin) ymin=x[j][i];
  }
 }

 xf=dmaxx/(xmax-xmin); yf=dmaxy/(ymax-ymin);

 for(i=0;i<=dmaxx;i++) chart[(int)(ymax*yf+movey)][i+movex]='-';
 for(i=0;i<=dmaxy;i++) chart[i+movey][(int)(-xmin*xf+movex)]='|';

 sprintf(string,"xmin=%f xmax=%f ymin=%f ymax=%f",xmin,xmax,ymin,ymax);
 for(i=0;i<strlen(string);i++) chart[0][i]=string[i];

 for(i=0;i<row;i++) for(j=1;j<col;j++) 
  chart[(int)((ymax-x[j][i])*yf+movey)][(int)((abs(xmin)+x[0][i])*xf+movex)]='*';
}

void drawcolbeside() //draws bar chart to chart-array
{int i,j,d=1;
 int x1=movex,x2,y1,y2;
 float ymax=0,ymin=0,yf;

 for(i=0;i<row;i++) for(j=0;j<col;j++)
 {if(x[j][i]>ymax) ymax=x[j][i]; if(x[j][i]<ymin) ymin=x[j][i];}

 yf=dmaxy/(ymax-ymin);
 d=(int)((dmaxx-row-2)/row/col);

 for(i=0;i<=dmaxx;i++) chart[(int)(ymax*yf+movey)][i+movex]='_';
 for(i=0;i<=dmaxy;i++) chart[i+movey][movex]='|';

 sprintf(string,"ymin=%f ymax=%f",ymin,ymax);
 for(i=0;i<strlen(string);i++) chart[0][i]=string[i];

 for(i=0;i<row;i++)
 {x1+=2;
  for(j=0;j<col;j++)
  {x2=x1+d;
   if(x[j][i]<0) {y1=ymax*yf+movey; y2=(int)((ymax-x[j][i])*yf+movey);} 
   else {y1=(int)((ymax-x[j][i])*yf+movey); y2=ymax*yf+movey;}
   drawrec(x1,x2,y1,y2);
   x1+=d;
  } 
 }
}

void drawcolstapled() // draws barchart to char-array
{int i,j,d=1;
 int x1=2+movex,x2,y1,y2;
 float ymax=0,sum,yf;

 for(i=0;i<row;i++)
 {sum=0; for(j=0;j<col;j++) sum+=x[j][i];
  if(sum>ymax) ymax=sum;
 }

 yf=dmaxy/ymax;
 d=(int)((dmaxx-row-2)/row);

 for(i=0;i<=dmaxx;i++) chart[(int)(dmaxy+movey)][i+movex]='_';
 for(i=0;i<=dmaxy;i++) chart[i+movey][movex]='|';

 sprintf(string,"ymax=%f",ymax);
 for(i=0;i<strlen(string);i++) chart[0][i]=string[i];

 for(i=0;i<row;i++)
 {sum=0;
  for(j=0;j<col;j++)
  {x2=x1+d; sum+=x[j][i]; y1=(int)((ymax-sum)*yf+movey); y2=dmaxy+movey;
   drawrec(x1,x2,y1,y2);
  }
 x1+=d+1;
 }
}

void drawbarbeside() //draws bar to chart-array
{int i,j,d=1;
 int x1,x2,y1=movey,y2;
 float xmax=0,xmin=0,xf;

 for(i=0;i<row;i++) for(j=0;j<col;j++)
 {if(x[j][i]>xmax) xmax=x[j][i]; if(x[j][i]<xmin) xmin=x[j][i];}

 xf=dmaxx/(xmax-xmin);
 d=(int)((dmaxy-row-2)/row/col);

 for(i=0;i<=dmaxy;i++) chart[i+movey][(int)(-xmin*xf+movex)]='|';
 for(i=0;i<=dmaxx;i++) chart[movey][i+movex]='_';

 sprintf(string,"xmin=%f xmax=%f",xmin,xmax);
 for(i=0;i<strlen(string);i++) chart[0][i]=string[i];

 for(i=0;i<row;i++)
 {y1+=2;
  for(j=0;j<col;j++)
  {y2=y1+d;
   if(x[j][i]<0) {x2=-xmin*xf+movex; x1=(int)((x[j][i]-xmin)*xf+movex);} 
   else {x2=(int)((x[j][i]-xmin)*xf+movex); x1=-xmin*xf+movex;}
   drawrec(x1,x2,y1,y2);
   y1+=d;
  } 
 }
}

void drawbarstapled() // draws barchart to char-array
{int i,j,d=1;
 int y1=2+movey,y2,x1,x2;
 float xmax=0,sum,sumold,xf;

 for(i=0;i<row;i++)
 {sum=0; for(j=0;j<col;j++) sum+=x[j][i];
  if(sum>xmax) xmax=sum;
 }

 xf=dmaxx/xmax;
 d=(int)((dmaxy-row-2)/row);

 for(i=0;i<=dmaxy;i++) chart[i+movey][movex]='|';
 for(i=0;i<=dmaxx;i++) chart[movey][i+movex]='_';

 sprintf(string,"xmax=%f",xmax);
 for(i=0;i<strlen(string);i++) chart[0][i]=string[i];

 for(i=0;i<row;i++)
 {sum=0;
  for(j=0;j<col;j++)
  {y2=y1+d; sumold=sum; sum+=x[j][i];
   x2=(int)(sum*xf+movex); x1=(int)(sumold*xf+movex);
   drawrec(x1,x2,y1,y2);
  }
 y1+=d+1;
 }
}

void draw() // draws chart array on screen
{int i,j;

 for(i=0;i<=maxy;i++) for(j=0;j<=maxx;j++) chart[i][j]=' ';

 if(typ==1) drawcolbeside();
 else if(typ==2) drawcolstapled();
 else if(typ==3) drawbarbeside();
 else if(typ==4) drawbarstapled();
 else drawpoint();

 mvprintw(0,0,"");
 for(i=0;i<=maxy;i++) printw("%s",chart[i]);
 refresh();
}

void print() // print chart to file 'print'
{int i,j;

 FILE *prnt;
 prnt=fopen("print","w");

 if(!prnt)
 {}

 for(i=0;i<=maxy;i++)
 {j=strlen(chart[i])-1;
  while(chart[i][j]==' ' && j>=0) j--;
  chart[i][j+1]='\0';
 }

 for(i=0;i<=maxy;i++) fprintf(prnt,"%s\n",chart[i]);
 fclose(prnt);
}

void load() // load data from file
{int i,j;
 fil=fopen(filename,"r");

 if(!fil)
 {}

 fscanf(fil,"%d",&col); fscanf(fil,"%d",&row);
 for(i=0;i<row;i++) for(j=0;j<col;j++) fscanf(fil,"%f",&x[j][i]);
}

void main(int argc, char *argv[]) /* MAIN */
{char n; int loop=1;
float zoom=2./3,scale=1;

 initscr(); raw(); keypad(stdscr,TRUE); noecho();
 getmaxyx(stdscr,maxy,maxx);
 maxy-=2; maxx-=1; meny=maxy+1;
 dmaxy=maxy*zoom; dmaxx=maxx*zoom*scale;

 if(argc<2) help();
 else {filename=argv[1]; load();}

 draw();

 while(loop && (n=getchar())!='q')	// quit
  switch(n)
  {
   case 'h': help(); draw(); break; // help
   case 'p': print(); break; // print
   case 's': scale*=2./3; dmaxy=maxy*zoom; dmaxx=maxx*zoom*scale;
             draw(); break; // scale out
   case 'S': scale*=3./2; dmaxy=maxy*zoom; dmaxx=maxx*zoom*scale;
             draw(); break; // scale in
   case 't': type(); draw(); break; // exit
   case 'x': if(movex) movex--; draw(); break; // movex--
   case 'X': if(movex<maxx) movex++; draw(); break; // movex++
   case 'y': if(movey) movey--; draw(); break; // movey--
   case 'Y': if(movey<maxy) movey++; draw(); break; // movey++
   case 'z': zoom*=2./3; dmaxy=maxy*zoom; dmaxx=maxx*zoom*scale;
             draw(); break; // zoom out
   case 'Z': zoom*=3./2; dmaxy=maxy*zoom; dmaxx=maxx*zoom*scale;
             draw(); break; // zoom in
  }
 endwin();
}

/* IDEAS:

*

*/
