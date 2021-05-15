/*

Textdraw (td) is a small utility that allows do draw (ascii-based) line-,
rectangle-, ellipse- and text-objects with copy/paste/move features.

Homepage of Textdraw: http://web.uta4you.at/shop/td/index.htm
(c) July/2002 by Dieter Schoppitsch (shop@uta4you.at)

*/

#include <ncurses.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#define trunc(x) ((int)(x))

#define ESC 27		// escape key
#define ENTER 10	// enter key
#define DEL 330		// del key
#define MAXOBJECTS 100	// maximum of possible objects
#define MAXTXTLEN 200	// max string length

						/* VARIABLES*/
int x=0, y=0;
int maxrow=23, maxcol=79, menrow=24;
int lastlin=0, lastrec=0, lasttxt=0, lastell=0;
char chart[MAXTXTLEN][MAXTXTLEN];	// max chart (console) dimensions
char string[MAXTXTLEN]="";
FILE *fil;
char *filename;

struct _line_
{int x1,y1,x2,y2; char a;};
struct _line_ lin[MAXOBJECTS];

struct _rect_
{int x1,y1,x2,y2;};
struct _rect_ rec[MAXOBJECTS];

struct _ell_
{int x1,y1,x2,y2;};
struct _ell_ ell[MAXOBJECTS];

struct _text_
{int x,y; char a; char t[MAXTXTLEN];};
struct _text_ txt[MAXOBJECTS];

struct _mark_
{char a; int nr;};
struct _mark_ mark;

struct _buf_
{int x1,y1,x2,y2; char c,a; char t[MAXTXTLEN];};
struct _buf_ buf;

						/* SUBPROGRAMS */
int round(float x)	// round function
{	if(x>=0) return trunc(x+0.5);
	else return trunc(x-0.5);
}

void clearcomment()	// clears comment line
{	int i;
	char s[MAXTXTLEN];
	for(i=0;i<=maxcol;i++) s[i]=' ';
	mvprintw(menrow, 0,"%s",s);
}

void comment(char *txt)	// print comment
{	clearcomment(); mvprintw(menrow, 0,"%s", txt);}

void comment_error(char *where)
{	char buf[128];
	snprintf(buf, sizeof(buf), "%s: %s", where, strerror(errno));
	comment(buf);
}

void curpos()	//position cursor with printing coordinates
{	mvprintw(menrow,maxcol-10,"          ");
	mvprintw(menrow,maxcol-10,"x=%d y=%d",x,y);
	mvprintw(y,x,"");
}

void cursor()	// move cursor to a new position
{	int c;
	while((c=getch()) != ENTER)
	{	switch(c)
		{
			case KEY_UP:
				y-=1; if(y<0) y=0;
				curpos();
				break;
			case KEY_DOWN:
				y+=1; if(y>maxrow) y=maxrow;
				curpos();
				break;
			case KEY_LEFT:
				x-=1; if(x<0) x=0;
				curpos();
				break;
			case KEY_RIGHT:
				x+=1; if(x>maxcol) x=maxcol;
				curpos();
				break;
			case KEY_HOME:
				x=0; curpos();
				break;
			case KEY_END:
				x=maxcol; curpos();
				break;
			case KEY_PPAGE:
				y=0; curpos();
				break;
			case KEY_NPAGE:
				y=maxrow; curpos();
				break;
		}		
	}
}

void input()	// reads (inputs) value for string
{	clearcomment();
	mvprintw(menrow,0,"%s ",string);
	echo(); getstr(string); noecho();
}

void help()	// print help screen
{	int c; clear();
	printw("\nTEXTDRAW 0.2 (c) by Dieter Schoppitsch\n\n");
	printw("c	copy		copy marked object to buffer\n");
	printw("d	delete		delete marked object\n");
	printw("e	ellipse		draw ellipse-object\n");
	printw("h	help		this help page\n");
	printw("l	line		draw line-object\n");
	printw("m	move		move marked object\n");
	printw("p	print		print to ascii-file 'print'\n");
	printw("q, ESC	exit		exit (without saving!)\n");
	printw("r	rectangle	draw rectangle-object\n");
	printw("s	save		save file\n");
	printw("t	text		draw text-object\n");
	printw("v	paste		paste buffer\n");
	printw("\n");
	printw("position cursor with cursor keys\n");
	printw("mark object at 'hot spot'\n");
	printw("to load existing file: <td filename>\n");
	printw("try also: home, end, pgdn, pgup\n");
	refresh(); c=getch();
}

						/* INIT */
void init()	// sets initial values
{	int i,j;
	for(i=0;i<=maxrow;i++) for(j=0;j<=maxcol;j++) chart[i][j]=' ';
}


						/* DRAW */
void drawtxt() // draws textobjects to chart-array
{	int i,j;
	for(i=0;i<lasttxt;i++)	for(j=0;j<=strlen(txt[i].t)-1;j++)
	if(txt[i].a=='u')
	{	if(txt[i].y-j>=0 && txt[i].x+j<=maxcol)
			chart[txt[i].y-j][txt[i].x+j]=txt[i].t[j];
	}
	else if(txt[i].a=='v')
	{	if(j+txt[i].y<=maxrow)
			chart[txt[i].y+j][txt[i].x]=txt[i].t[j];
	}
	else if(txt[i].a=='d')
	{	if(j+txt[i].y<=maxrow && j+txt[i].x<=maxcol)
			chart[txt[i].y+j][txt[i].x+j]=txt[i].t[j];
	}
	else 
	{	if(j+txt[i].x<=maxcol)
			chart[txt[i].y][txt[i].x+j]=txt[i].t[j];
	}
}

void drawlin()	// draws line in chart-array
{	int i,j;
	float k,d;

	for(i=0;i<lastlin;i++)
	if(lin[i].x1!=lin[i].x2 || lin[i].y1!=lin[i].y2)
	{	if(abs(lin[i].x1-lin[i].x2)>abs(lin[i].y1-lin[i].y2))
		{	k=(float)(lin[i].y1-lin[i].y2)/(lin[i].x1-lin[i].x2);
			d=lin[i].y1-k*lin[i].x1;
			if(lin[i].x1<lin[i].x2)
				for(j=lin[i].x1;j<=lin[i].x2;j++)
					chart[round(k*j+d)][j]=lin[i].a;
			else
				for(j=lin[i].x2;j<=lin[i].x1;j++)
					chart[round(k*j+d)][j]=lin[i].a;
		}
		else
		{	k=(float)(lin[i].x1-lin[i].x2)/(lin[i].y1-lin[i].y2);
			d=lin[i].x1-k*lin[i].y1;
			if(lin[i].y1<lin[i].y2)
				for(j=lin[i].y1;j<=lin[i].y2;j++)
					chart[j][round(k*j+d)]=lin[i].a;
			else
				for(j=lin[i].y2;j<=lin[i].y1;j++)
					chart[j][round(k*j+d)]=lin[i].a;
		}
	}
}

void drawrec()	// draws rectangle in chart-array
{	int i,j;

	for(i=0;i<lastrec;i++)
	if(rec[i].x1!=rec[i].x2 || rec[i].y1!=rec[i].y2)
	{	if(rec[i].x2>rec[i].x1 && rec[i].y2>rec[i].y1)
		{	for(j=rec[i].y1+1;j<=rec[i].y2;j++)
			{	chart[j][rec[i].x1]='|';
				chart[j][rec[i].x2]='|';
			}
			for(j=rec[i].x1+1;j<=rec[i].x2-1;j++)
			{	chart[rec[i].y1][j]='_';
				chart[rec[i].y2][j]='_';
			}
		}
		else if(rec[i].x2>rec[i].x1 && rec[i].y1>rec[i].y2)
		{	for(j=rec[i].y2+1;j<=rec[i].y1;j++)
			{	chart[j][rec[i].x1]='|';
				chart[j][rec[i].x2]='|';
			}
			for(j=rec[i].x1+1;j<=rec[i].x2-1;j++)
			{	chart[rec[i].y1][j]='_';
				chart[rec[i].y2][j]='_';
			}
		}
		else if(rec[i].x1>rec[i].x2 && rec[i].y2>rec[i].y1)
		{	for(j=rec[i].y1+1;j<=rec[i].y2;j++)
			{	chart[j][rec[i].x1]='|';
				chart[j][rec[i].x2]='|';
			}
			for(j=rec[i].x2+1;j<=rec[i].x1-1;j++)
			{	chart[rec[i].y1][j]='_';
				chart[rec[i].y2][j]='_';
			}
		}
		else
		{	for(j=rec[i].y2+1;j<=rec[i].y1;j++)
			{	chart[j][rec[i].x1]='|';
				chart[j][rec[i].x2]='|';
			}
			for(j=rec[i].x2+1;j<=rec[i].x1-1;j++)
			{	chart[rec[i].y1][j]='_';
				chart[rec[i].y2][j]='_';
			}
		}
	}
}

void drawell()	// draws ellipse in chart-array
{	int i,j;
	float a,b,s;

	for(i=0;i<lastell;i++)
	if(ell[i].x1!=ell[i].x2 || ell[i].y1!=ell[i].y2)
	{	a=(float)abs(ell[i].x2-ell[i].x1)/2;
		b=(float)abs(ell[i].y2-ell[i].y1)/2;
		if(abs(ell[i].x1-ell[i].x2)>abs(ell[i].y1-ell[i].y2)) // y(x)
		{	if(ell[i].x1<ell[i].x2 && ell[i].y1<ell[i].y2)
			for(j=ell[i].x1;j<=ell[i].x2;j++)
			{	
	s=sqrt((1-(j-ell[i].x1-a)*(j-ell[i].x1-a)/a/a)*b*b);
				chart[ell[i].y1+round(b-s)][j]='*';
				chart[ell[i].y1+round(b+s)][j]='*';
			}
			else if(ell[i].x1<ell[i].x2 && ell[i].y1>ell[i].y2)
			for(j=ell[i].x1;j<=ell[i].x2;j++)
			{	
	s=sqrt((1-(j-ell[i].x1-a)*(j-ell[i].x1-a)/a/a)*b*b);
				chart[ell[i].y1-round(b-s)][j]='*';
				chart[ell[i].y1-round(b+s)][j]='*';
			}
			else if(ell[i].x1>ell[i].x2 && ell[i].y1<ell[i].y2)
			for(j=ell[i].x2;j<=ell[i].x1;j++)
			{	
	s=sqrt((1-(j-ell[i].x2-a)*(j-ell[i].x2-a)/a/a)*b*b);
				chart[ell[i].y1+round(b-s)][j]='*';
				chart[ell[i].y1+round(b+s)][j]='*';
			}
			else
			for(j=ell[i].x2;j<=ell[i].x1;j++)
			{	
	s=sqrt((1-(j-ell[i].x2-a)*(j-ell[i].x2-a)/a/a)*b*b);
				chart[ell[i].y1-round(b-s)][j]='*';
				chart[ell[i].y1-round(b+s)][j]='*';
			}
		}
		else						      // x(y)
		{	if(ell[i].x1<ell[i].x2 && ell[i].y1<ell[i].y2)
			for(j=ell[i].y1;j<=ell[i].y2;j++)
			{	
	s=sqrt((1-(j-ell[i].y1-b)*(j-ell[i].y1-b)/b/b)*a*a);
				chart[j][ell[i].x1+round(a-s)]='*';
				chart[j][ell[i].x1+round(a+s)]='*';
			}
			else if(ell[i].x1<ell[i].x2 && ell[i].y1>ell[i].y2)
			for(j=ell[i].y2;j<=ell[i].y1;j++)
			{
	s=sqrt((1-(j-ell[i].y2-b)*(j-ell[i].y2-b)/b/b)*a*a);
				chart[j][ell[i].x1+round(a-s)]='*';
				chart[j][ell[i].x1+round(a+s)]='*';
			}
			else if(ell[i].x1>ell[i].x2 && ell[i].y1<ell[i].y2)
			for(j=ell[i].y1;j<=ell[i].y2;j++)
			{	
	s=sqrt((1-(j-ell[i].y1-b)*(j-ell[i].y1-b)/b/b)*a*a);
				chart[j][ell[i].x1-round(a-s)]='*';
				chart[j][ell[i].x1-round(a+s)]='*';
			}
			else
			for(j=ell[i].y2;j<=ell[i].y1;j++)
			{	
	s=sqrt((1-(j-ell[i].y2-b)*(j-ell[i].y2-b)/b/b)*a*a);
				chart[j][ell[i].x1-round(a-s)]='*';
				chart[j][ell[i].x1-round(a+s)]='*';
			}
		}
	}

}

void draw()	// draws chart-array on screen
{	int i,j;

	for(i=0;i<=maxrow;i++) for(j=0;j<=maxcol;j++) chart[i][j]=' ';

	drawlin();
	drawrec();
	drawell();
	drawtxt();

	mvprintw(0,0,"");
	for(i=0;i<=maxrow;i++) printw("%s",chart[i]);
	curpos();
	refresh();
}

						/* INPUTS */
void textinput() // input of text-object-parameter
{	int c;
	txt[lasttxt].x=x; txt[lasttxt].y=y;

	string[0]='\0'; strcpy(string,"input text: ");
	input();
	strcpy(txt[lasttxt].t, string),

	string[0]='\0';
	strcpy(string,
	"alignment - vertical/horizontal/upward/downward (v/h/u/d): ");
	input(); txt[lasttxt].a=string[0];

	c=string[0];
	if(c!='v' && c!='V' && c!='u' && c!='U' && c!='d' && c!='D')
		txt[lasttxt].a='h';

	clearcomment(); curpos();

	lasttxt+=1;
}

void lineinput()	// inputs line-object-parameter
{	lin[lastlin].x1=x; lin[lastlin].y1=y; 
	curpos();
	comment("mark end of line");
	mvprintw(y,x,"");

	cursor();

	lin[lastlin].x2=x; lin[lastlin].y2=y;
	string[0]='\0'; strcpy(string,"line draw character: ");
	input(); lin[lastlin].a=string[0];
	clearcomment(); curpos();

	lastlin+=1;
}

void recinput()		// inputs rectangle-object-paramenter
{	rec[lastrec].x1=x; rec[lastrec].y1=y;

	comment("mark 2nd point of rectangle");
	mvprintw(y,x,"");

	cursor();

	rec[lastrec].x2=x; rec[lastrec].y2=y;
	clearcomment(); curpos();


	lastrec+=1;
}

void ellinput()		// inputs ellipse-object-parameter
{	ell[lastell].x1=x; ell[lastell].y1=y;

	comment("mark 2nd point of ellipse");
	mvprintw(y,x,"");

	cursor();

	ell[lastell].x2=x; ell[lastell].y2=y;
	clearcomment(); curpos();

	lastell+=1;
}

				/* MARK, MOVE/DELETE/COPY/PASTE */

void markobj()		// mark objekt to move, delete, copy, paste
{	int i;
	mark.a=' '; mark.nr=0;

	for(i=0;i<lastlin;i++) if(x==lin[i].x1 && y==lin[i].y1)
	{	mark.a='l'; mark.nr=i;
	}
	for(i=0;i<lastrec;i++) if(x==rec[i].x1 && y==rec[i].y1)
	{	mark.a='r'; mark.nr=i;
	}
	for(i=0;i<lastell;i++) if(x==ell[i].x1 && y==ell[i].y1)
	{	mark.a='e'; mark.nr=i;
	}
	for(i=0;i<lasttxt;i++) if(x==txt[i].x && y==txt[i].y)
	{	mark.a='t'; mark.nr=i;
	}
}

void moving()		// moves marked objekt to other position
{	int dx,dy;

	markobj();
	if(mark.a=='t')
	{	mvprintw(txt[mark.nr].x,txt[mark.nr].y,"");
		comment("move text to");
		curpos(); cursor(); clearcomment();
		dx=x-txt[mark.nr].x; dy=y-txt[mark.nr].y;
		txt[mark.nr].x=txt[mark.nr].x+dx;
		txt[mark.nr].y=txt[mark.nr].y+dy;
	}
	if(mark.a=='e')
	{	mvprintw(ell[mark.nr].x1,ell[mark.nr].y1,"");
		comment("move ellipse to");
		curpos(); cursor(); clearcomment();
		dx=x-ell[mark.nr].x1; dy=y-ell[mark.nr].y1;
		ell[mark.nr].x1=ell[mark.nr].x1+dx;
		ell[mark.nr].y1=ell[mark.nr].y1+dy;
		ell[mark.nr].x2=ell[mark.nr].x2+dx;
		ell[mark.nr].y2=ell[mark.nr].y2+dy;
	}
	if(mark.a=='r')
	{	mvprintw(rec[mark.nr].x1,rec[mark.nr].y1,"");
		comment("move rectangle to");
		curpos(); cursor(); clearcomment();
		dx=x-rec[mark.nr].x1; dy=y-rec[mark.nr].y1;
		rec[mark.nr].x1=rec[mark.nr].x1+dx;
		rec[mark.nr].y1=rec[mark.nr].y1+dy;
		rec[mark.nr].x2=rec[mark.nr].x2+dx;
		rec[mark.nr].y2=rec[mark.nr].y2+dy;
	}
	if(mark.a=='l')
	{	mvprintw(lin[mark.nr].x1,lin[mark.nr].y1,"");
		comment("move line to");
		curpos(); cursor(); clearcomment();
		dx=x-lin[mark.nr].x1; dy=y-lin[mark.nr].y1;
		lin[mark.nr].x1=lin[mark.nr].x1+dx;
		lin[mark.nr].y1=lin[mark.nr].y1+dy;
		lin[mark.nr].x2=lin[mark.nr].x2+dx;
		lin[mark.nr].y2=lin[mark.nr].y2+dy;
	}
}

void delete()
{	int i;

	markobj();
	if(mark.a=='t')
	{	for(i=mark.nr+1;i<lasttxt;i++)
		{	txt[i-1].x=txt[i].x; txt[i-1].y=txt[i].y;
			txt[i-1].a=txt[i].a;
			txt[i-1].t[0]='\0'; strcpy(txt[i-1].t,txt[i].t);

		}
		lasttxt-=1;
	}	
	if(mark.a=='e')
	{	for(i=mark.nr+1;i<lastell;i++)
		{	ell[i-1].x1=ell[i].x1; ell[i-1].y1=ell[i].y1;
			ell[i-1].x2=ell[i].x2; ell[i-1].y2=ell[i].y2;
		}
		lastell-=1;
	}	
	if(mark.a=='r')
	{	for(i=mark.nr+1;i<lastrec;i++)
		{	rec[i-1].x1=rec[i].x1; rec[i-1].y1=rec[i].y1;
			rec[i-1].x2=rec[i].x2; rec[i-1].y2=rec[i].y2;
		}
		lastrec-=1;
	}	
	if(mark.a=='l')
	{	for(i=mark.nr+1;i<lastlin;i++)
		{	lin[i-1].x1=lin[i].x1; lin[i-1].y1=lin[i].y1;
			lin[i-1].x2=lin[i].x2; lin[i-1].y2=lin[i].y2;
			lin[i-1].a=lin[i].a;
		}
		lastlin-=1;
	}	
}

void copy()	// copy marked object to buffer-object
{	buf.c=buf.a='0'; buf.x1=buf.x2=buf.y1=buf.y2=0; buf.t[0]='\0';

	markobj();
	if(mark.a=='t')
	{	buf.c='t';
		buf.a=txt[mark.nr].a;
		buf.x1=txt[mark.nr].x;
		buf.y1=txt[mark.nr].y;
		buf.t[0]='\0'; strcpy(buf.t,txt[mark.nr].t);
		comment("text copied");
	}
	if(mark.a=='e')
	{	buf.c='e';
		buf.x1=ell[mark.nr].x1;
		buf.y1=ell[mark.nr].y1;
		buf.x2=ell[mark.nr].x2;
		buf.y2=ell[mark.nr].y2;
		comment("ellipse copied");
	}
	if(mark.a=='r')
	{	buf.c='r';
		buf.x1=rec[mark.nr].x1;
		buf.y1=rec[mark.nr].y1;
		buf.x2=rec[mark.nr].x2;
		buf.y2=rec[mark.nr].y2;
		comment("rectangle copied");
	}
	if(mark.a=='l')
	{	buf.c='l';
		buf.x1=lin[mark.nr].x1;
		buf.y1=lin[mark.nr].y1;
		buf.x2=lin[mark.nr].x2;
		buf.y2=lin[mark.nr].y2;
		buf.a=lin[mark.nr].a;
		comment("line copied");
	}
}

void paste()	// paste copied object at cursor position
{
	if(buf.c=='t')
	{	txt[lasttxt].a=buf.a;
		txt[lasttxt].x=x;
		txt[lasttxt].y=y;
		txt[lasttxt].t[0]='\0'; strcpy(txt[lasttxt].t,buf.t);
		lasttxt+=1;
	}
	if(buf.c=='e')
	{	ell[lastell].x1=x;
		ell[lastell].y1=y;
		ell[lastell].x2=buf.x2+x-buf.x1;
		ell[lastell].y2=buf.y2+y-buf.y1;
		lastell+=1;
	}
	if(buf.c=='r')
	{	rec[lastrec].x1=x;
		rec[lastrec].y1=y;
		rec[lastrec].x2=buf.x2+x-buf.x1;
		rec[lastrec].y2=buf.y2+y-buf.y1;
		lastrec+=1;
	}
	if(buf.c=='l')
	{	lin[lastlin].x1=x;
		lin[lastlin].y1=y;
		lin[lastlin].x2=buf.x2+x-buf.x1;
		lin[lastlin].y2=buf.y2+y-buf.y1;
		lin[lastlin].a=buf.a;
		lastlin+=1;
	}
}

						/* LOAD, SAVE, PRINT */
void load()		// load from file
{	int i,l;

	fil=fopen(filename,"r");

	if (!fil)
	{	comment_error("Could not load");
		return;
	}

	fscanf(fil,"%d",&lastlin);
	for(i=0;i<lastlin;i++)
	{	fscanf(fil,"%d %d %d %d %c\n",
		&lin[i].x1,&lin[i].y1,&lin[i].x2,&lin[i].y2,&lin[i].a);
	}
	fscanf(fil,"%d\n",&lastrec);
	for(i=0;i<lastrec;i++)
	{	fscanf(fil,"%d %d %d %d\n",
		&rec[i].x1,&rec[i].y1,&rec[i].x2,&rec[i].y2);
	}
	fscanf(fil,"%d\n",&lastell);
	for(i=0;i<lastell;i++)
	{	fscanf(fil,"%d %d %d %d\n",
		&ell[i].x1,&ell[i].y1,&ell[i].x2,&ell[i].y2);
	}
	fscanf(fil,"%d\n",&lasttxt);
	for(i=0;i<lasttxt;i++)
	{	fscanf(fil,"%d %d %c %d",&txt[i].x,&txt[i].y,&txt[i].a,&l);
		fseek(fil, 1, SEEK_CUR);
		fread(txt[i].t,1, l, fil);
		txt[i].t[l]='\0';
	}

	comment("file loaded");
	fclose(fil);
}

void save()		// save to file
{	int i;

	fil=fopen(filename,"w+");

	if (!fil)
	{	comment_error("Could not save");
		return;
	}

	fprintf(fil,"%d\n",lastlin);
	for(i=0;i<lastlin;i++)
	{	fprintf(fil,"%d %d %d %d %c\n",
		lin[i].x1,lin[i].y1,lin[i].x2,lin[i].y2,lin[i].a);
	}
	fprintf(fil,"%d\n",lastrec);
	for(i=0;i<lastrec;i++)
	{	fprintf(fil,"%d %d %d %d\n",
		rec[i].x1,rec[i].y1,rec[i].x2,rec[i].y2);
	}
	fprintf(fil,"%d\n",lastell);
	for(i=0;i<lastell;i++)
	{	fprintf(fil,"%d %d %d %d\n",
		ell[i].x1,ell[i].y1,ell[i].x2,ell[i].y2);
	}
	fprintf(fil,"%d\n",lasttxt);
	for(i=0;i<lasttxt;i++)
	{	fprintf(fil,"%d %d %c %d %s\n",
		txt[i].x,txt[i].y,txt[i].a,strlen(txt[i].t),txt[i].t);
	}

	comment("file saved");
	fclose(fil);
}

void print()		// print chart to file 'print'
{	int i,j;

	FILE *prnt;
	prnt=fopen("print","w");

	if (!prnt)
	{	comment_error("Could not print");
		return;
	}

	for(i=0;i<=maxrow;i++)
	{	j=strlen(chart[i])-1;
		while(chart[i][j]==' ' && j>=0) j--;
		chart[i][j+1]='\0';
	}

	for(i=0;i<=maxrow;i++) fprintf(prnt,"%s\n",chart[i]);
	comment("file 'print' printed");

	fclose(prnt);
}


int main(int argc, char *argv[])			/* MAIN */
{	int c;

	initscr(); raw(); keypad(stdscr, TRUE); noecho();
	getmaxyx(stdscr,maxrow,maxcol);
	maxrow-=2; maxcol-=1; menrow=maxrow+1;
	init();

	if(argv[1]=='\0')	// new file
	{
	filename="new";
	comment("file 'new' opened - to open existing file: <td filename>");
	}
	else			// open file
	{
	filename=argv[1];
	load();
	}

	draw();
	curpos();

	while((c = getch())!=ESC && c!='q' && c!='Q')	// quit
	{	switch(c)
		{
			case KEY_UP:			// up
				y-=1; if(y<0) y=0;
				curpos();
				break;
			case KEY_DOWN:			// down
				y+=1; if(y>maxrow) y=maxrow;
				curpos();
				break;
			case KEY_LEFT:			// left
				x-=1; if(x<0) x=0;
				curpos();
				break;
			case KEY_RIGHT:			// right
				x+=1; if(x>maxcol) x=maxcol;
				curpos();
				break;
			case KEY_HOME:			// home
				x=0; curpos();
				break;
			case KEY_END:			// end
				x=maxcol; curpos();
				break;
			case KEY_PPAGE:			// pgup
				y=0; curpos();
				break;
			case KEY_NPAGE:			// pgdn
				y=maxrow; curpos();
				break;
			case 'c': case 'C':		// copy
				copy(); draw();
				break;
			case 'd': case 'D': case DEL:	// delete
				delete(); draw();
				break;
			case 'e': case 'E':		// ellipse
				ellinput(); draw();
				break;
			case 'h': case 'H':		// help
				help(); draw();
				break;
			case 'l': case 'L':		// line
				lineinput(); draw();
				break;
			case 'm': case 'M':		// move
				moving(); draw();
				break;
			case 'p': case 'P':		// print
				print(); draw();
				break;
			case 'r': case 'R':		// rectangle
				recinput(); draw();
				break;
			case 's': case 'S':		// save
				save(); draw();
				break;
			case 't': case 'T':		// text
				textinput(); draw();
				break;
			case 'v': case 'V':		// paste
				paste(); draw();
				break;
		}		
	}

	endwin();
	return 0;
}

/* IDEAS:

* user defineable keys
* colors
* white surface
* mouse support
* drop down menus
* rotate/mirror object
* group object
* put object infront/behind
* objects with/without frame/filled_color/pattern
* object polygon
* ascii-art-import (bitmap)
* multi windows
* presentation features (effects)
* save as html

*/

