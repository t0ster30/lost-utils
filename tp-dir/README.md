# Homepage of TEXTPRINT
## by Dieter Schoppitsch - (August 2003)

Textdraw is a simple (ascii) chart generator. Data in a textfile (ie provided by a spreadsheet like sc) can be converted into simple xy-plots, chart- or column-charts which can be printed to a textfile. Further treatment with your favorite editor is simple.

Following example of a parabolic plot shows the required format of the data-textfile:

2 5
 0 0
 1 1
 2 4
 3 9
 4 16

The first two figures show the amount of data (2 colums, 5 values) - don't forget to add them.

Homepage of Textprint: [http://web.uta4you.at/shop/tp/index.htm] (c) August/2003 by Dieter Schoppitsch (shop@uta4you.at).

PS: Start textprint with "tp 'data-file-name'"

PPS: Get some further help by pressing "h" after starting the program.

PPPS: compile with: gcc -Wall -s -O3 tp.c -lncurses

Feel free to improve this program under the conditions of the general GNU-license-agreement.

Send me comments, suggestions and bugs under: shop@uta4you.at

Dieter Schoppitsch
