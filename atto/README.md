#Homepage of ATTO
> by Dieter Schoppitsch - (December 2002)
---
Atto is a simple, fast (doesn't need ncurses) and small (<16k) line editor.
Nevertheless it provides features like a buffer, search&replace;, an ascii table and a simple hex dumper.

Two things are tricky:
* It's not possible to edit a line directly. Instead the line is printed and an "edit string" will be read below.
- Everything after an 'i' in this string will be inserted in the line.
- Everything after an 'd' in this string will be deleted in the line.
- Everything after an 'c' in this string will be changed in the line.
- A 's' in this string splits the line.
* Inserting new line(s) after the actual line (command 'i') could be ended with a '.' in a line itself.

Get some further help by pressing "h" after starting the program.

Compile with: 

```
gcc -Wall -s -O3 a.c
```

I wrote atto as reminiscence to my first programming lesson 1981 on a honeywell bull host.

Download: Atto 0.1 (9k5)

Feel free to improve this program under the conditions of the general GNU-license-agreement.

Homepage of Atto: http://web.uta4you.at/shop/atto/index.htm
(c) December/2002 by Dieter Schoppitsch (shop@uta4you.at).

Send me comments, suggestions and bugs under: shop@uta4you.at

Dieter Schoppitsch
