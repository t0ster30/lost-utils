/*
C is a simple and tiny scientific RPN-Calculator. It tries to minimize the necessary keystrokes.

Note that changing sign could be done by pressing SPACE.

One nice 'non-scientific' feature is the calculation of annuities ... 8% in 5 years equals a annuity of 3.99 (.08 ENTER 5a).

Get some further help by pressing "h" after starting the program.

Homepage of C: http://web.uta4you.at/shop/c/index.htm
(c) May 2004 by Dieter Schoppitsch (shop@uta4you.at)

Compile C with: gcc -Wall -s -O3 c.c -lm
*/

#include <stdlib.h>
#include <math.h>

#define PI 3.141592653589793238462643 // pi
#define ESC 0x1b // escape key
#define SPACE 0x20 // space
#define ENTER 0xa // enter
#define BKSPC 0x7f // backspace
#define STRLEN 128 // max stringlen

void help() // print help screen
{printf("\nc - THEtinyRPNsciCALC - 2004 Dieter Schoppitsch\n\n");
 printf("Q sqr/sqrt E ee      R rot>/<  T tan/arc I invert  P pi/pwr  L ln/exp\n");
 printf("A annu     S sin/arc D deg/rad F fix/sci G gamma!  H help    K log/10^x\n");
 printf("X swap     C cos/arc V sinh/ar B cosh/ar N tanh/ar M rcl/sto\n");
 printf("ESC exit   BKSPC clx # clr     SPC chs\n");
}

void disp(char s[STRLEN]) // write string
{printf("\r%s","                                                  ");
 printf("\r%s",s);
}

void draw(long double x,long double y,long double z,long double u,int f) // display stack
{if(f<100)
 {printf("\n: %4.*Lf\n",f,u); printf(": %4.*Lf\n",f,z);
  printf(": %4.*Lf\n",f,y); printf(": %4.*Lf\n\n",f,x);
 }
 else
 {printf("\n: %LE\n",u); printf(": %LE\n",z);
  printf(": %LE\n",y); printf(": %LE\n\n",x);
 }
}

void main() /* MAIN */
{int i,ise,ipos,fmt=2;
 char n;
 char s[STRLEN]="";
 long double x=0,y=0,z=0,u=0,tmp=0,sto=0,rad=PI/180;
 
 system("stty -icanon -echo");
 help(); draw(x,y,z,u,fmt);

 while((n=getchar())!=ESC) // quit
 {if((n>='0')&&(n<='9')||(n==',')||(n=='.')||(n=='E')||(n=='e'))
  {if(n==',') n='.';
   if(strlen(s)<STRLEN) {strcat(s," "); s[strlen(s)-1]=n;}
   disp(s);
  }
  else if((strlen(s)>0)&&(n==BKSPC)) {s[strlen(s)-1]='\0'; disp(s);}
  else if((strlen(s)>0)&&(n==SPACE))
  {ise=0; ipos=1;
   for(i=1;i<=strlen(s);i++) if((s[i]=='e')||(s[i]=='E')) {ise=1; ipos=i;}
   if(ise)
    {if(s[ipos+1]=='-') for(i=ipos+1;i<strlen(s);i++) s[i]=s[i+1];
     else {for(i=strlen(s);i>ipos;i--) s[i+1]=s[i]; s[ipos+1]='-';}
     disp(s);
    }
  }
  else
  {if(strlen(s)>0) {u=z; z=y; y=x; x=atof(s);}
   if((strlen(s)==0)&&(n==ENTER)) {u=z; z=y; y=x;}
   else if(n==BKSPC) {x=y; y=z; z=u;}
   else if(n=='#') x=y=z=u=0;

   else switch(n)
   {case 'h': case 'H': help(); break;
    case 'x': case 'X': tmp=x; x=y; y=tmp; break;
    case 'r': tmp=x; x=y; y=z; z=u; u=tmp; break;
    case 'R': tmp=u; u=z; z=y; y=x; x=tmp; break;
    case 'F': fmt=100; break;
    case 'd': rad=PI/180; break;
    case 'D': rad=1; break;
    case 'f': fmt=x; x=y; y=z; z=u; break;
    case 'M': sto=x; break;
    case 'm': u=z; z=y; y=x; x=sto; break;

    case '+': x=x+y; y=z; z=u; break;
    case '-': x=y-x; y=z; z=u; break;
    case '*': x=x*y; y=z; z=u; break;
    case '/': x=y/x; y=z; z=u; break;

    case 'i': case 'I': x=1/x; break;
    case ' ': x=-x; break;
    case 'q': x=x*x; break;
    case 'Q': x=sqrt(x); break;
    case 'P': x=exp(x*log(y)); y=z; z=u; break;

    case 'p': u=z; z=y; y=x; x=PI; break;
    case 'l': x=log(x); break;
    case 'L': x=exp(x); break;
    case 's': x=sin(x*rad); break;
    case 'S': x=atan(x/(sqrt(1-x*x)))/rad; break;
    case 'c': x=cos(x*rad); break;
    case 'C': x=(PI/2-atan(x/(sqrt(1-x*x))))/rad; break;
    case 't': x=sin(x*rad)/cos(x*rad); break;
    case 'T': x=atan(x)/rad; break;
    case 'k': x=log(x)/log(10); break;
    case 'K': x=exp(x*log(10)); break;
    case 'v': x=(exp(x)-exp(-x))/2; break; 
    case 'V': x=log(x+sqrt(x*x+1)); break;
    case 'b': x=(exp(x)+exp(-x))/2; break;
    case 'B': x=log(x+sqrt(x*x-1)); break;
    case 'n': x=(exp(x)-exp(-x))/(exp(x)+exp(-x)); break;
    case 'N': x=log(sqrt((1+x)/(1-x))); break;
    case 'g': for(tmp=i=1;i<=x;i++) tmp=tmp*i; x=tmp; break;
    case 'a': x=(1-1/exp(x*log(1+y)))/y; y=z; z=u;  break;
   }
   draw(x,y,z,u,fmt);
   s[0]='\0';
  }
 }
 system("stty icanon echo");
}
