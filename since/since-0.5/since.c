#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sysexits.h>

#include <sys/stat.h>
#include <sys/types.h>

/****************************************************************************/

/* BUGS: performance is O(nm) where n=files to be displayed, m=all files    */
/*       previously displayed. With more effort it could be O(log(mn))      */
/*                                                                          */
/*       using since on NFS mounted file systems accross different          */
/*       architectures probably won't work. Could be made to work by        */
/*       adding a bit more intelligence to the sincefile parser             */
/*                                                                          */
/*       no paranoid since file locking. With a bit of luck the fwrite      */
/*       will be atomic, so that is not quite as important                  */

/* SINCEFILE format: each line contains stat information about a file       */
/*      st_dev:st_ino:st_size\n                                             */
/*      where st_dev+st_ino is the lookup key and st_size the old file size */

/****************************************************************************/

#define SINCE_LONG     4096	/* large buffer (for displaying file) */
#define SINCE_SHORT     256	/* small buffer (for reading bits of .since) */
#define SINCE_FALLBACK "/tmp/since"	/* if everything else fails */
#define SINCE_VERSION  "0.5"

char since_format[SINCE_SHORT];
int line_offset = 0;
int line_length = 0;
const char *compressed_extensions[] = { ".gz", ".bz2", ".bz", ".zip", NULL };

/****************************************************************************/
/* Does       : Checks to see if a file is compressed                       */

int
test_exclusion (char *s)
{
  int i, j, k, m;

  m = strlen (s);

  for (k = 0; compressed_extensions[k]; k++)
    {
      j = strlen (compressed_extensions[k]);
      if (m >= j)
	{
#ifdef DEBUG
	  fprintf (stderr, "test_exclusion(): testing <%s> against <%s>\n", s,
		   compressed_extensions[k]);
#endif
	  for (i = m; (j >= 0) && (s[i] == compressed_extensions[k][j]);
	       i--, j--);
	  if (j < 0)
	    {
	      return 1;
	    }
	}
    }

  return 0;
}

/****************************************************************************/
/* Does       : Ugly runtime guess at the size a type, return the format    */
/*              modifier                                                    */

int
guess_size (int type, char *s, int len)
{
  int i;

  if (len < 9)
    {
      fprintf (stderr,
	       "since: impossible error: buffer too short to prepare format string\n");
      exit (EX_DATAERR);
    }

  i = snprintf (s, len, "%%0%d", 2 * type);

  if (sizeof (int) == type)
    {
      /* do nothing */
    }
  else if (sizeof (long int) == type)
    {
      s[i++] = 'l';
    }
  else if (sizeof (long long int) == type)
    {
      s[i++] = 'l';
      s[i++] = 'l';
    }
  else if (sizeof (short int) == type)
    {
      s[i++] = 'h';
    }
  else if (sizeof (char) == type)
    {
      s[i++] = 'h';
      s[i++] = 'h';
    }
  else
    {
      fprintf (stderr, "since: impossible error: type length %d is unknown\n",
	       type);
      exit (EX_DATAERR);
    }

  s[i++] = 'x';
  s[i] = '\0';

  return i;
}

/****************************************************************************/
/* Does       : Ugly runtime guess at the size of the types of the stat     */
/*              field                                                       */
/* Returns    : nothing, but sets up the global since_format string         */
/* Others     : could be done at compile-time                               */

void
prepare_format ()
{
  struct stat st;
  char buffer[SINCE_SHORT];
  int print_len, calc_len;
  int i = 0;

  i += guess_size (sizeof (st.st_dev), since_format + i, SINCE_SHORT - i);
  since_format[i++] = ':';
  i += guess_size (sizeof (st.st_ino), since_format + i, SINCE_SHORT - i);
  since_format[i++] = ':';
  i += guess_size (sizeof (st.st_size), since_format + i, SINCE_SHORT - i);
  since_format[i++] = '\n';
  since_format[i++] = '\0';

#ifdef DEBUG
  fprintf (stderr, "prepare_format(): format is <%s>\n", since_format);
#endif

  print_len =
    snprintf (buffer, SINCE_SHORT - 1, since_format, st.st_dev, st.st_ino,
	      st.st_size);
  calc_len =
    3 + 2 * (sizeof (st.st_dev) + sizeof (st.st_ino) + sizeof (st.st_size));

  if (calc_len != print_len)
    {
      fprintf (stderr, "since: conflicting data format lengths %d!=%d\n",
	       calc_len, print_len);
      exit (1);
    }

  line_offset = 1 + 2 * (sizeof (st.st_dev) + sizeof (st.st_ino));
  line_length = calc_len;
}

/****************************************************************************/
/* Does       : Gets the last displayed offset of file statted (via st)     */
/*              from the database (from file argument)                      */
/* Parameters : file - database, st - statted file to be found, update -    */
/*              option to control if changes get written                    */
/* Returns    : recorded offset or 0 on failure                             */
/* Errors     : will exit() on errors                                       */

off_t sincefile (FILE * file, struct stat *st, int update)
{
  char have[SINCE_SHORT];
  char got[SINCE_SHORT];
  int match, got_length;
  char *endptr;
  off_t result = 0;

  snprintf (have, SINCE_SHORT, since_format, st->st_dev, st->st_ino,
	    st->st_size);

  match = 0;
  rewind (file);
  do
    {
      got_length = fread (got, sizeof (char), line_length, file);

#ifdef DEBUG
      fprintf (stderr, "sincefile(): Got bytes=%d\n", got_length);
#endif

      if (got_length == line_length)
	{
	  if (got[got_length - 1] == '\n')
	    {
	      if (!strncmp (got, have, line_offset))
		{
#ifdef DEBUG
		  fprintf (stderr, "sincefile(): match\n");
#endif
		  match = 1;
		  got_length = 0;
		}
	    }
	  else
	    {
	      got_length = (-1);
	    }
	}
    }
  while (got_length == line_length);

  if (got_length)
    {
      fprintf (stderr, "since: could not read .since information: %s\n",
	       strerror (errno));
      exit (EX_DATAERR);
    }
  else
    {

      if (match)
	{
#ifdef DEBUG
	  fprintf (stderr, "sincefile(): got[line_offset]=%c\n",
		   got[line_offset]);
#endif
	  got[line_length] = '\0';
	  result = strtol (got + line_offset + 1, &endptr, 16);
	  if (result > st->st_size)
	    {
	      fprintf (stderr,
		       "since: assuming a truncation, redisplaying entire file\n");
	      result = 0;
	    }
#ifdef DEBUG
	  fprintf (stderr, "sincefile(): Return is <%lx>\n", result);
#endif
	}

      if (update)
	{
	  if (match)
	    {
#ifdef DEBUG
	      fprintf (stderr,
		       "sincefile(): matched - updating an existing record\n");
#endif
	      if (strncmp (have, got, line_length))
		{
#ifdef DEBUG
		  fprintf (stderr,
			   "sincefile(): real update, position before seek=<%ld>\n",
			   ftell (file));
#endif
		  if (fseek (file, ftell (file) - line_length, SEEK_SET))
		    {
		      fprintf (stderr,
			       "since: could not seek .since database: %s\n",
			       strerror (errno));
		      exit (EX_IOERR);
		    }
#ifdef DEBUG
		  fprintf (stderr, "sincefile(): position after seek=<%ld>\n",
			   ftell (file));
#endif
		  if (fwrite (have, sizeof (char), line_length, file) !=
		      line_length)
		    {
		      fprintf (stderr,
			       "since: could not write to .since database: %s\n",
			       strerror (errno));
		      exit (EX_IOERR);
		    }
		  fflush (file);
		}
	    }
	  else
	    {
#ifdef DEBUG
	      fprintf (stderr,
		       "sincefile(): Unmatched - generating a new record\n");
#endif
	      if (fwrite (have, sizeof (char), line_length, file) !=
		  line_length)
		{
		  fprintf (stderr,
			   "since: could not append to .since database: %s\n",
			   strerror (errno));
		  exit (EX_IOERR);
		}
	      fflush (file);
	    }
	}
    }

  return result;
}

/****************************************************************************/
/* Does       : dump bytes beyond offset of file fname to stdout            */
/* Parameters : fname - file name, offset - start of dump                   */
/* Returns    : always zero, though it probably shouldn't                   */
/* Errors     : IO errors                                                   */

int
disp_file (int fd, off_t offset)
{
  char buffer[SINCE_LONG];
  int wl = 0, rl = 0;

  if (lseek (fd, offset, SEEK_SET) != offset)
    {
      fprintf (stderr, "since: could not seek to desired location: %s\n",
	       strerror (errno));
    }
  else
    {
      do
	{
	  rl = read (fd, buffer, SINCE_LONG);
	  if (rl > 0)
	    {
	      wl = fwrite (buffer, sizeof (char), rl, stdout);
	    }
	}
      while ((rl == wl) & (&rl > 0));
      if (rl < 0 || wl < 0)
	{
	  fprintf (stderr, "since: could not display file: %s\n",
		   strerror (errno));
	}
    }

  return 0;
}

void
usage ()
{
  printf ("since : A tail(1) which remembers its last invocation\n"
	  "Usage : since [options] file ...\n"
	  "Options\n"
	  "-h       This help\n"
	  "-n       Do not write updates to .since file\n"
	  "-q       Less verbose output\n"
	  "-v       More verbose output\n"
	  "-x       Exclude files with compressed extensions\n"
	  ".since : Location determined by $SINCE environment variable, else $HOME/.since, if neither are set then "
	  SINCE_FALLBACK "\n");
}

int
main (int argc, char **argv)
{

  /* since operations */
  FILE *since;
  char buffer[SINCE_SHORT];
  char *sincename;
  struct stat st;
  off_t seekdest;

  /* vars for parsing cmdline */
  char *ptr;
  int i, j;
  int dashfile;

  /* number of files to be displayed */
  int files;
  /* descriptor for the current file */
  int fd;
  /* for sincefile */
  int sfd, sflags;

  /* options */
  int update;
  int verbose;
  int exclude;

  update = 1;
  verbose = 1;
  exclude = 0;

  /* process options */
  i = 1;
  files = 0;
  dashfile = 0;
  while (i < argc)
    {
      ptr = argv[i];
      if (ptr[0] == '-' && !dashfile)
	{
	  j = 1;
	  while (ptr[j] != '\0')
	    {
	      switch (ptr[j])
		{
		case '?':
		case 'H':
		case 'h':
		  usage ();
		  exit (EX_OK);
		  break;
		case 'V':
		case 'c':
		  printf ("since : Version %s\n", SINCE_VERSION);
		  printf ("        Copyright (c) 1998 by Marc Welz\n");
		  printf
		    ("        May only be distributed in accordance with the terms of the GNU\n");
		  printf
		    ("        General Public License as published by the Free Software Foundation\n");
		  exit (EX_OK);
		  break;
		case 'n':
		  update = 0;
		  break;
		case 'q':
		  verbose--;
		  break;
		case 'v':
		  verbose++;
		  break;
		case 'x':
		  exclude++;
		  break;
		case '-':
		  dashfile = 1;
		  break;
		default:
		  fprintf (stderr,
			   "since: Unrecognized option -%c, try -h for help\n",
			   ptr[j]);
		  exit (EX_USAGE);
		  break;
		}
	      j++;
	    }
	}
      else
	{
	  files++;
	}
      i++;
    }

  if (files == 0)
    {
      fprintf (stderr, "since: Insufficient arguments, try -h for help\n");
      exit (EX_USAGE);
    }

  /* get hold of .since file */
  sincename = getenv ("SINCE");
  if (sincename == NULL)
    {
      sincename = getenv ("HOME");
      if (sincename)
	{
	  snprintf (buffer, SINCE_SHORT, "%s/.since", sincename);
	}
      else
	{
	  fprintf (stderr, "since: warning falling back to %s\n",
		   SINCE_FALLBACK);
	  strcpy (buffer, SINCE_FALLBACK);
	}
      sincename = buffer;
    }

  sflags = O_CREAT | (update ? O_RDWR : O_RDONLY);
#ifdef O_NOFOLLOW
  sflags |= O_NOFOLLOW;
#endif

  /* create file in case it does not exist */
  sfd = open (sincename, sflags, S_IRUSR | S_IWUSR);
  if (sfd == (-1))
    {
      fprintf (stderr, "since: could not open .since file %s: %s\n",
	       sincename, strerror (errno));
      exit (EX_OSERR);
    }

  /* -n means a readonly open */
  since = fdopen (sfd, update ? "r+" : "r");

  if (since == NULL)
    {
      fprintf (stderr, "since: could not open .since file %s: %s\n",
	       sincename, strerror (errno));
      exit (EX_DATAERR);
    }

  prepare_format ();

  signal (SIGPIPE, SIG_IGN);

  /* now process files */
  i = 1;
  dashfile = 0;
  while (i < argc)
    {
      ptr = argv[i];
      if (ptr[0] == '-' && !dashfile)
	{
	  if (ptr[1] == '-')
	    {
	      dashfile = 1;
	    }
	}
      else
	{
	  if (((files > 1) && (verbose > 0)) || (verbose > 1))
	    {
	      printf ("==> %s ", argv[i]);	/* I don't like the header, but tail uses it... */
	    }
	  if (exclude && test_exclusion (argv[i]))
	    {
	      if (((files > 1) && (verbose > 0)) || (verbose > 1))
		{
		  printf ("[excluded] <==\n");
		}
	    }
	  else
	    {
	      fd = open (ptr, O_RDONLY);
	      if (fd == (-1) || fstat (fd, &st))
		{
		  fprintf (stderr,
			   "since: can not open or stat file %s: %s\n", ptr,
			   strerror (errno));
		}
	      else
		{
		  /* find the offset which we last looked at */
		  seekdest = sincefile (since, &st, update);
		  if (verbose > 1)
		    {
		      printf (since_format, st.st_dev, st.st_ino, seekdest);
		      printf ("    ");
		    }
		  /* any changes ? */
		  if (seekdest == st.st_size)
		    {
		      if (((files > 1) && (verbose > 0)) || (verbose > 1))
			{
			  printf ("[nothing new] <==\n");
			}
		    }
		  else
		    {
		      if (((files > 1) && (verbose > 0)) || (verbose > 1))
			{
			  printf ("<==\n");
			}
		      /* dump the selected file to stdout */
		      disp_file (fd, seekdest);
		    }
		  close (fd);
		}
	    }
	}
      i++;
    }

  fclose (since);

  exit (EX_OK);

  /* keep gcc -Wall happy */
  return 0;
}
