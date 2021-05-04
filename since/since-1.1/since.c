/* (c) 1998 - 2009 Marc Welz */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_INOTIFY
#include <sys/inotify.h>
#endif

/* for embedded or broken systems where no home exists */
#define SINCE_FALLBACK "/tmp/since"

/* default perms */
#define SINCE_MASK (S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP)

/* max format/data string */
#define MAX_FMT 100

/* number of chars to search back for a newline */
#define LINE_SEARCH 160

/* this is used to compute a valid offset for mmapping data files */
/* apparently one should use getpagesize() instead */
#ifdef PAGE_SIZE
#define IO_BUFFER PAGE_SIZE
#else
#define IO_BUFFER 4096
#endif

struct fmt_map{
  int f_bytes;
  char *f_string;
};

struct data_file{
  int d_fd;
  char *d_name;
  dev_t d_dev;
  ino_t d_ino;
  off_t d_had;
  off_t d_now;
  off_t d_pos;
  int d_offset;
  int d_notify;
  unsigned char d_jump:1;
  unsigned char d_write:1;
  unsigned char d_deleted:1;
  unsigned char d_replaced:1;
  unsigned char d_notable:1;
  unsigned char d_moved:1;
};

struct since_state{
  int s_disk_device;
  int s_disk_inode;
  int s_disk_size;

  int s_arch_device;
  int s_arch_inode;
  int s_arch_size;

  char s_fmt[MAX_FMT];
  int s_fmt_used;
  int s_fmt_output;
  int s_fmt_prefix;

  int s_error;
  int s_readonly;
  int s_verbose;
  int s_relaxed;
  int s_follow;
  int s_discard;
  struct timespec s_delay;
  int s_atomic;
  int s_domap;
  int s_nozip;

  char *s_name;
  int s_fd;

  int s_size;
  char *s_buffer;

  int s_ismap;

  int s_add;
  char *s_append;

  struct data_file *s_data_files;
  unsigned int s_data_count;

  sigset_t s_set;
  int s_notify;

  FILE *s_header;
};

struct fmt_map fmt_table[] = {
  { sizeof(unsigned int), "x" },
  { sizeof(unsigned long int), "lx" },
  { sizeof(unsigned long long int), "llx" },
  { sizeof(unsigned short int), "hx" },
  { 0, NULL }
};

/************************************************************/

static void forget_state_file(struct since_state *sn);
static int tmp_state_file(struct since_state *sn, int (*call)(struct since_state *sn));

volatile int since_run = 1;

/* misc supporting functions ********************************/

void handle_signal(int s)
{
  since_run = 0;
}

static void forget_state_file(struct since_state *sn)
{
  if(sn->s_buffer){
    if(sn->s_ismap){
      munmap(sn->s_buffer, sn->s_size);
      sn->s_ismap = 0;
    } else {
      free(sn->s_buffer);
    }
    sn->s_buffer = NULL;
  }
  sn->s_size = 0;

  if(sn->s_append){
    free(sn->s_append);
    sn->s_append = NULL;
  }
  sn->s_add = 0;
}

static int tmp_state_file(struct since_state *sn, int (*call)(struct since_state *sn))
{
  char canon[PATH_MAX], tmp[PATH_MAX], *tptr;
  int result, tfd, nfd, mode, flags;

  if(realpath(sn->s_name, canon) == NULL){
    fprintf(stderr, "since: unable to establish true location of %s: %s\n", sn->s_name, strerror(errno));
    return -1;
  }

  result = snprintf(tmp, PATH_MAX, "%s.%d", canon, getpid());
  if((result < 0) || (result >= PATH_MAX)){
    fprintf(stderr, "since: tmp file of %s exeeded limits\n", canon);
    return -1;
  }

  if(sn->s_verbose > 1){
    fprintf(stderr, "since: creating tmp file %s\n", tmp);
  }

  mode = SINCE_MASK;
  flags = O_RDWR | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
  flags |= O_NOFOLLOW;
#endif

  nfd = open(tmp, flags, mode);
  if(nfd < 0){
    fprintf(stderr, "since: unable to create tmp file %s: %s\n", tmp, strerror(errno));
    return -1;
  }

  tfd = sn->s_fd;
  tptr = sn->s_name;

  sn->s_fd = nfd;
  sn->s_name = tmp;

  result = (*call)(sn);

  sn->s_fd = tfd;
  sn->s_name = tptr;

  if(result < 0){
    close(nfd);
    unlink(tmp);
  }

  if(rename(tmp, sn->s_name)){
    fprintf(stderr, "since: unable to rename %s to %s: %s\n", tmp, sn->s_name, strerror(errno));
    close(nfd);
    unlink(tmp);
    return -1;
  }

  sn->s_fd = nfd;
  close(tfd);

  return 0;
}

/* sincefile stuff ******************************************/

static void init_state(struct since_state *sn)
{
  struct stat st;

  sn->s_arch_device = sizeof(st.st_dev);
  sn->s_arch_inode = sizeof(st.st_ino);
  sn->s_arch_size = sizeof(st.st_size);

  sn->s_disk_device = sn->s_arch_device;
  sn->s_disk_inode = sn->s_arch_inode;
  sn->s_disk_size = sn->s_arch_size;

  sn->s_fmt_used = 0;
  sn->s_fmt_output = 0;
  sn->s_fmt_prefix = 0;

  sn->s_error = 0;
  sn->s_readonly = 0;
  sn->s_verbose = 1;
  sn->s_relaxed = 0;
  sn->s_follow = 0;
  sn->s_discard = 0;
  sn->s_delay.tv_sec = 1;
  sn->s_delay.tv_nsec = 0;
  sn->s_atomic = 0;
  sn->s_domap = 1;
  sn->s_nozip = 0;

  sn->s_name = NULL;
  sn->s_fd = (-1);

  sn->s_size = 0;
  sn->s_buffer = NULL;
  sn->s_ismap = 0;

  sn->s_add = 0;
  sn->s_append = NULL;

  sn->s_data_files = NULL;
  sn->s_data_count = 0;

  sn->s_notify = (-1);

  sn->s_header = stdout;
}

void destroy_state(struct since_state *sn)
{
  int i;
  struct data_file *df;

  forget_state_file(sn);

  if(sn->s_data_files){
    for(i = 0; i < sn->s_data_count; i++){
      df = &(sn->s_data_files[i]);
      if(df->d_fd >= 0){
        close(df->d_fd);
        df->d_fd = (-1);
      }
      df->d_offset = (-1);
    }
    free(sn->s_data_files);
    sn->s_data_files = NULL;
  }
  sn->s_data_count = 0;

  if(sn->s_notify >= 0){
    close(sn->s_notify);
    sn->s_notify = (-1);
  }

  if(sn->s_fd >= 0){
    close(sn->s_fd);
    sn->s_fd = (-1);
  }

  if(sn->s_name){
    free(sn->s_name);
    sn->s_name = NULL;
  }
}

/* open state files *****************************************/

static int try_state_file(struct since_state *sn, char *path, char *append, int more)
{
  int flags, mode, plen, alen, result;
  char *tmp;

  if(path == NULL){
    return -1;
  }

  if(append){
    plen = strlen(path);
    alen = strlen(append);
    tmp = malloc(plen + alen + 2);
    if(tmp == NULL){
      fprintf(stderr, "since: unable to allocate %d bytes\n", plen + alen + 2);
      return -1;
    }
    if((plen > 0) && (path[plen - 1] == '/')){
      plen--;
    }
    strcpy(tmp, path);
    tmp[plen] = '/';
    strcpy(tmp + plen + 1, append);
  } else {
    tmp = strdup(path);
    if(tmp == NULL){
      fprintf(stderr, "since: unable to duplicate %s\n", path);
      return -1;
    }
  }
  sn->s_name = tmp;

  flags = (sn->s_readonly ? O_RDONLY : (O_RDWR | O_CREAT)) | more;

  mode = SINCE_MASK;

  if(sn->s_verbose > 2){
    fprintf(stderr, "since: attempting to open %s\n", sn->s_name);
  }

  sn->s_fd = open(sn->s_name, flags, mode);
  if(sn->s_fd >= 0){
    if(sn->s_verbose > 1){
      fprintf(stderr, "since: opened %s\n", sn->s_name);
    }
    return 0;
  }

  result = -1;

  if(errno == ENOENT){
    if(sn->s_readonly){
      result = 1;
    }
  }

  if(result < 0){
    fprintf(stderr, "since: unable to open %s: %s\n", sn->s_name, strerror(errno));
  }

  free(sn->s_name);
  sn->s_name = NULL;

  return result;
}

static int open_state_file(struct since_state *sn, char *name)
{
  char *ptr;
  int more;
  struct passwd *pw;
  int result;

  if(name){
    return try_state_file(sn, name, NULL, 0);
  }

  ptr = getenv("SINCE");
  if((result = try_state_file(sn, ptr, NULL, 0)) >= 0){
    return result;
  }

  ptr = getenv("HOME");
  if(ptr == NULL){
    pw = getpwuid(getuid());
    if(pw){
      ptr = pw->pw_dir;
    }
  }
  if((result = try_state_file(sn, ptr, ".since", 0)) >= 0){
    return result;
  }

#ifdef O_NOFOLLOW
  more = O_NOFOLLOW;
#else
  more = 0;
#endif
  if((result = try_state_file(sn, SINCE_FALLBACK, NULL, more)) >= 0){
    if(sn->s_verbose > 0){
      fprintf(stderr, "since: using fallback %s\n", SINCE_FALLBACK);
    }
    return result;
  }

  fprintf(stderr, "since: unable to open any state file\n");
  return -1;
}

/* acquire content of state file ****************************/

static int load_state_file(struct since_state *sn)
{
  struct stat st;
  int prot, flags;
  int rr;
  /* we assume that state file can fit into address space */
  unsigned int rt;

#ifdef DEBUG
  if(sn->s_buffer){
    fprintf(stderr, "since: logic problem: loading over unallocated buffer\n");
    abort();
  }
#endif

  sn->s_ismap = 0;

  if(sn->s_fd < 0){
    /* where there is no state file and invoked as readonly */
    return 1;
  }

  if(fstat(sn->s_fd, &st)){
    fprintf(stderr, "since: unable to stat %s: %s\n", sn->s_name, strerror(errno));
    return -1;
  }

  sn->s_size = st.st_size;
  if(sn->s_size == 0){
    return 0; /* empty, possibly new file */
  }

  if(sn->s_domap){
    /* not really clear if this buys us anything, actually may make updates non-atomic as we rely on mmap to write back changes */
    prot = PROT_READ;
    if(!(sn->s_readonly)) prot |= PROT_WRITE;
    flags = sn->s_atomic ? MAP_PRIVATE : MAP_SHARED;
    sn->s_buffer = mmap(NULL, sn->s_size, prot, flags, sn->s_fd, 0);
    if((void *)(sn->s_buffer) != MAP_FAILED){
#ifdef DEBUG
      fprintf(stderr, "since: have mapped %s (size=%u)\n", sn->s_name, sn->s_size);
#endif
      sn->s_ismap = 1;
      return 0;
    }
  }

  sn->s_buffer = malloc(sn->s_size);
  if(sn->s_buffer == NULL){
    fprintf(stderr, "since: unable to allocate %d bytes to load state file %s: %s\n", sn->s_size, sn->s_name, strerror(errno));
    return -1;
  }

  for(rt = 0; rt < sn->s_size;){
    rr = read(sn->s_fd, sn->s_buffer + rt, sn->s_size - rt);
    if(rr < 0){
      switch(errno){
        case EAGAIN :
        case EINTR :
          break;
        default :
          fprintf(stderr, "since: read of %s failed after %d bytes: %s\n", sn->s_name, rt, strerror(errno));
          return -1;
      }
    }
    if(rr == 0){
      fprintf(stderr, "since: premature end of %s: %d bytes outstanding\n", sn->s_name, (unsigned int)(sn->s_size) - rt);
      return -1;
    }
    rt += rr;
  }

#ifdef DEBUG
  fprintf(stderr, "since: have read %s (size=%u)\n", sn->s_name, sn->s_size);
#endif

  return 0;
}

/* test if state file is sane *******************************/

static int check_state_file(struct since_state *sn)
{
  int i, x, w, d, sep[3];

  sep[0] = ':';
  sep[1] = ':';
  sep[2] = '\n';

  if((sn->s_buffer == NULL) || (sn->s_size == 0)){
    if(sn->s_verbose > 2){
      fprintf(stderr, "since: will not check an empty or nonexistant file\n");
    }
    return 1;
  }

  w = 0;

  for(i = 0, x = 0; (i < sn->s_size) && (x < 3); i++){
    if(isxdigit(sn->s_buffer[i])){
      /* nothing */
    } else if(sn->s_buffer[i] == sep[x]){
      d = (i - w);
      if(d % 2){
        fprintf(stderr, "since: data field has to contain an even number of bytes, not %d\n", d);
        return -1;
      }
      d = d / 2;
#ifdef DEBUG
      fprintf(stderr, "check: field[%d] is %d bytes\n", x, d);
#endif
      switch(x){
        case 0 : sn->s_disk_device = d; break;
        case 1 : sn->s_disk_inode = d; break;
        case 2 : sn->s_disk_size = d; break;
      }
      x++;
      w = i + 1;
    } else {
      fprintf(stderr, "since: corrupt state file %s at %d\n", sn->s_name, i);
      return -1;
    }
  }

  if(x < 3){
    fprintf(stderr, "since: no fields within %d bytes in file %s\n", i, sn->s_name);
    return -1;
  }

  return 0;
}

/* rewrite state file if field size changes *****************/

static int internal_upgrade_state_file(struct since_state *sn)
{
  char line[MAX_FMT], *ptr;
  int result, sw, sr, i, k, x, pad[3], end[3], sep[3];

#define bounded_delta(x, y)   (((x) < (y)) ? 0 : ((x) - (y)))

  pad[0] = bounded_delta(sn->s_arch_device, sn->s_disk_device) * 2;
  pad[1] = bounded_delta(sn->s_arch_inode, sn->s_disk_inode) * 2;
  pad[2] = bounded_delta(sn->s_arch_size, sn->s_disk_size) * 2;

  end[0] = sn->s_disk_device * 2;
  end[1] = sn->s_disk_inode * 2;
  end[2] = sn->s_disk_size * 2;

  sep[0] = ':';
  sep[1] = ':';
  sep[2] = '\n';

#undef bounded_delta

  sw = 0;
  sr = 0;
  for(x = 0; x < 3; x++){
    sw += pad[x] + end[x] + 1;
    sr += end[x] + 1;
#ifdef DEBUG
    fprintf(stderr, "since: padding[%d]: have=%d, expand=%d\n", x, end[x], pad[x]);
#endif
  }
#ifdef DEBUG
  fprintf(stderr, "since: total: %d->%d\n", sr, sw);
#endif
  if(sw >= MAX_FMT){
    fprintf(stderr, "since: major logic failure: want to rewrite to %d, limit at %d\n", sw, MAX_FMT);
    return -1;
  }

  for(i = 0; i < sn->s_size; i+= sr){
    ptr = sn->s_buffer + i;
    k = 0;
    for(x = 0; x < 3; x++){
      if(pad[x] > 0){
        memset(line + k, '0', pad[x]);
        k += pad[x];
      }
      memcpy(line + k, ptr, end[x]);
      if(ptr[end[x]] != sep[x]){
        fprintf(stderr, "since: data corruption: expected a separator, not 0x%x in line at %d in %s\n", ptr[end[x]], i, sn->s_name);
        return -1;
      }
      k += end[x];
      line[k] = sep[x];
      k++;
      ptr += end[x] + 1;
    }
    result = write(sn->s_fd, line, k);
    if(result != sw){
      fprintf(stderr, "since: unable to write line to %s: %s\n", sn->s_name, (result < 0) ? strerror(errno) : "incomplete write");
      return -1;
    }
  }

  sn->s_disk_device += (pad[0] / 2);
  sn->s_disk_inode += (pad[1] / 2);
  sn->s_disk_size += (pad[2] / 2);

  if(lseek(sn->s_fd, 0, SEEK_SET) != 0){
    fprintf(stderr, "since: unable to rewind tmp file to start: %s\n", strerror(errno));
    return -1;
  }

  return 0;
}

static int maybe_upgrade_state_file(struct since_state *sn)
{
  if((sn->s_disk_device >= sn->s_arch_device) &&
     (sn->s_disk_inode >= sn->s_arch_inode) &&
     (sn->s_disk_size >= sn->s_arch_size)){
    if(sn->s_verbose > 4){
      fprintf(stderr, "since: state file data fields greater or equal to architecture, no rewrite needed\n");
    }
    return 0;
  }

  if(sn->s_readonly){
    fprintf(stderr, "since: need to rewrite state file %s, but invoked as readonly\n", sn->s_name);
    return -1;
  }

  if((sn->s_buffer == NULL) || (sn->s_size == 0)){
    fprintf(stderr, "since: major logic failure, attempting to upgrade empty file\n");
    return -1;
  }

  if(tmp_state_file(sn, &internal_upgrade_state_file)){
    return -1;
  }

  forget_state_file(sn);

  return load_state_file(sn);
}

/* fmt logic ************************************************/

static int make_fmt_field(struct since_state *sn, int disk, int arch, int suffix)
{
  int have, pad, i, result;

  have = MAX_FMT - sn->s_fmt_used;

  if(disk < arch){
    return -1;
  }

  if(disk > arch){
    pad = (disk - arch) * 2;
    if(pad > have){
      return -1;
    }
    for(i = 0; i < pad; i++){
      sn->s_fmt[sn->s_fmt_used++] = '0';
    }
    have -= pad;
  }

  for(i = 0; fmt_table[i].f_bytes > 0; i++){
    if(fmt_table[i].f_bytes == arch){
      result = snprintf(sn->s_fmt + sn->s_fmt_used, have, "%%0%d%s%c", arch * 2, fmt_table[i].f_string, suffix);
      if((result < 0) || (result >= have)){
        return -1;
      }

      sn->s_fmt_used += result;
      return 0;
    }
  }

  return -1;
}

static int make_fmt_string(struct since_state *sn)
{
  int result;

  sn->s_fmt_used = 0;
  sn->s_fmt_output = 0;
  sn->s_fmt_prefix = 0;

  result = 0;

  result += make_fmt_field(sn, sn->s_disk_device, sn->s_arch_device, ':');
  result += make_fmt_field(sn, sn->s_disk_inode, sn->s_arch_inode, ':');
  /* meh, we wanted to use \n, but snprintf runs over the edge */
  result += make_fmt_field(sn, sn->s_disk_size, sn->s_arch_size, '\0');

  if(result){
    fprintf(stderr, "since: internal logic failure generating format string\n");
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "since: fmt string is %s", sn->s_fmt);
#endif

  sn->s_fmt_prefix = (2 * sn->s_disk_device) + 1 + (2 * sn->s_disk_inode);
  sn->s_fmt_output = sn->s_fmt_prefix + 1 + (2 * sn->s_disk_size); /* WARNING: missing \n, added manually */

  if((sn->s_fmt_output + 1) >= MAX_FMT){
    fprintf(stderr, "since: oversize fields: limit=%d, wanted=%d\n", MAX_FMT, sn->s_fmt_output + 1);
    return -1;
  }

  return 0;
}

/* datafile stuff *******************************************/

static char *ignore_suffix[] = { ".gz", ".bz2", ".Z", ".zip", NULL };

static int setup_data(struct since_state *sn, char *name)
{
  struct data_file *tmp;
  struct stat st;
  int fd, i;
  char *suffix;

  if(sn->s_nozip){
    suffix = strrchr(name, '.');
    if(suffix){
      for(i = 0; ignore_suffix[i]; i++){
        if(!strcmp(suffix, ignore_suffix[i])){
          if(sn->s_verbose > 4){
            fprintf(stderr, "since: not displaying presumed compressed file %s\n", name);
          }
          return 0;
        }
      }
    }
  }

  fd = open(name, O_RDONLY);
  if(fd < 0){
    fprintf(stderr, "since: unable to open %s: %s\n", name, strerror(errno));
    return 1;
  }

  if(fstat(fd, &st)){
    fprintf(stderr, "since: unable to fstat %s: %s\n", name, strerror(errno));
    return 1;
  }

  if(!(S_IFREG & st.st_mode)){
    fprintf(stderr, "since: unable to handle special file %s\n", name);
    return 1;
  }

  tmp = realloc(sn->s_data_files, sizeof(struct data_file) * (sn->s_data_count + 1));
  if(tmp == NULL){
    fprintf(stderr, "since: unable to allocate %u bytes for %s\n", sizeof(struct data_file) * (sn->s_data_count * 1), name);
    return -1;
  }

  sn->s_data_files = tmp;
  tmp = &(sn->s_data_files[sn->s_data_count]);
  sn->s_data_count++;

  tmp->d_name = name;
  tmp->d_fd = fd;

  tmp->d_dev = st.st_dev;
  tmp->d_ino = st.st_ino;

  tmp->d_had = 0;
  tmp->d_now = st.st_size;
  tmp->d_pos = 0;

  tmp->d_write = 0;
  tmp->d_jump = 0;
  tmp->d_deleted = 0;
  tmp->d_replaced = 0;
  tmp->d_moved = 0;
  tmp->d_notable = 1;

  tmp->d_offset = (-1);
  tmp->d_notify = (-1);

  return 0;
}

/* lookup stuff *********************************************/

static int lookup_entries(struct since_state *sn)
{
  char line[MAX_FMT], *end;
  int i, j, result;
  struct data_file *df;

  if((sn->s_buffer == NULL) || (sn->s_size == 0)){ /* file empty, nothing to look up */
    return 0;
  }

  /* could sort/index stuff to do better than O(n^2) */
  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);
    result = snprintf(line, MAX_FMT, sn->s_fmt, df->d_dev, df->d_ino, 0);
    if(result != sn->s_fmt_output){
      fprintf(stderr, "since: logic problem: expected state line to be %d bytes, printed %d\n", sn->s_fmt_output, result);
      return -1;
    }
    for(j = 0; j < sn->s_size; j += (sn->s_fmt_output + 1)){
      if(!memcmp(sn->s_buffer + j, line, sn->s_fmt_prefix)){
        df->d_offset = j;
        if(sn->s_arch_size > sizeof(unsigned long)){
          df->d_had = strtoull(sn->s_buffer + j + sn->s_fmt_prefix + 1, &end, 16);
        } else {
          df->d_had = strtoul(sn->s_buffer + j + sn->s_fmt_prefix + 1, &end, 16);
        }
        if(end[0] != '\n'){
          fprintf(stderr, "since: parse problem: unable to convert value at offset %d to number\n", j + sn->s_fmt_prefix + 1);
          return -1;
        }

        if(df->d_had > df->d_now){
          fprintf(stderr, "since: considering %s to be truncated, displaying from start\n", df->d_name);
          df->d_had = 0;
          df->d_write = 1;
        }

        if(df->d_pos != df->d_had){
          /* pos is the value which gets saved */
          df->d_pos = df->d_had;
          df->d_jump = 1;
        }

        if(sn->s_verbose > 3){
          /* this seems a bit risky, what if longs are bigger than wordsize ? */
#if _FILE_OFFSET_BITS > __WORDSIZE
          fprintf(stderr, "since: found record for %s at offset %d, now=%Lu, had=%Lu\n", df->d_name, j, df->d_now, df->d_had);
#else
          fprintf(stderr, "since: found record for %s at offset %d, now=%ld, had=%ld\n", df->d_name, j, df->d_now, df->d_had);
#endif
        }
        break;
      }
    }
  }

  return 0;
}

/* refresh using stat and sleep *****************************/

static int setup_watch(struct since_state *sn)
{
#ifdef USE_INOTIFY
  struct data_file *df;
  unsigned int i;

  sn->s_notify = inotify_init();
  if(sn->s_notify < 0){
    if(sn->s_verbose > 3){
      fprintf(stderr, "since: unable to use inotify: %s\n", strerror(errno));
    }
    return 1;
  }

  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);
    df->d_notify = inotify_add_watch(sn->s_notify, df->d_name, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
    if(df->d_notify < 0){
      fprintf(stderr, "since: unable to register notification for %s: %s\n", df->d_name, strerror(errno));
      close(sn->s_notify);
      sn->s_notify = (-1);
      return 1;
    }
  }
#endif
  return 0;
}

static int check_file(struct since_state *sn, struct data_file *df)
{
  struct stat st;
  int again;

  if(stat(df->d_name, &st) == 0){
    if((st.st_ino != df->d_ino) ||
       (st.st_dev != df->d_dev)){
      if(df->d_replaced == 0){
        df->d_replaced = 1;
        df->d_notable = 1;
      }
#ifdef DEBUG
      fprintf(stderr, "check: file %s no longer matches\n", df->d_name);
#endif
      /* TODO: could re-open filename here, redo lookup, etc */
      again = 1;
    } else {
      again = 0;
      /* TODO: could note a rename back to the old name */
      df->d_replaced = 0;
      df->d_moved = 0;
    }
  } else {
    switch(errno){
      case ENOENT :
        if(df->d_moved == 0){
          df->d_moved = 1;
          df->d_notable = 1;
        }
        break;
        /* TODO: could quit on some critical errors sooner */
    }
    again = 1;
  }

  if(again){
    if(fstat(df->d_fd, &st)){
      fprintf(stderr, "since: unable to stat %s: %s\n", df->d_name, strerror(errno));
      return -1;
    }
    if(st.st_nlink == 0){
      /* TODO: could delete entry to reduce inode collisions */
      if(df->d_deleted == 0){
        df->d_deleted = 1;
        df->d_notable = 1;
      }
    } else {
      if(df->d_moved == 0){
        df->d_moved = 1;
        df->d_notable = 1;
      }
    }
  }

#ifdef DEBUG
  fprintf(stderr, "update: new size=%Lu, old max=%Lu\n", st.st_size, df->d_now);
#endif

  if(st.st_size < df->d_now){
    fprintf(stderr, "since: considering %s to be truncated, displaying from start\n", df->d_name);
    df->d_had = 0;
    df->d_pos = 0;
    df->d_jump = 1;
    df->d_write = 1;
    df->d_notable = 1;
  }

  if(df->d_now < st.st_size){
    df->d_notable = 1;
  }
  df->d_now = st.st_size;

  return 0;
}

static int notify_watch(struct since_state *sn)
{
#ifdef USE_INOTIFY
  struct inotify_event update;
  struct data_file *df;
  char discard[PATH_MAX];
  unsigned int i;
  int result;

#ifdef DEBUG
  if(sn->s_notify < 0){
    fprintf(stderr, "since: major logic error, no notify file descriptor\n");
    abort();
  }
#endif

  sigprocmask(SIG_UNBLOCK, &(sn->s_set), NULL);
  result = read(sn->s_notify, &update, sizeof(struct inotify_event));
  sigprocmask(SIG_BLOCK, &(sn->s_set), NULL);

  if(result < 0){
    if(since_run == 0){
      return 1;
    }
    if(sn->s_verbose > 1){
      fprintf(stderr, "since: inotify failed: %s\n", strerror(errno));
    }
    return 0;
  }

  if(result != sizeof(struct inotify_event)){
    fprintf(stderr, "since: inotify weirdness: read partial notification\n");
    return -1;
  }

  if(sn->s_verbose > 4){
    fprintf(stderr, "since: inotify mask 0x%x, len %u\n", update.mask, update.len);
  }

  if(update.len > 0){
    result = read(sn->s_notify, discard, update.len);
    if(sn->s_verbose > 3){
      fprintf(stderr, "since: discarding %u bytes of unexpected path information from inotify\n", update.len);
    }
    if(result != update.len){
      fprintf(stderr, "since: inotify weirdness: unable to discard path information\n");
      return -1;
    }
  }

  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);
    if(update.wd == df->d_notify){

      if(check_file(sn, df) < 0){
        return -1;
      }
    }
  }

  return 0;
#else
#ifdef DEBUG
  fprintf(stderr, "since: major logic error, doing disabled notification\n");
  abort();
#endif
  return -1;
#endif
}

static int poll_watch(struct since_state *sn)
{
  unsigned int i;
  struct data_file *df;

  sigprocmask(SIG_UNBLOCK, &(sn->s_set), NULL);
  nanosleep(&(sn->s_delay), NULL);
  sigprocmask(SIG_BLOCK, &(sn->s_set), NULL);

  if(since_run == 0){
    return 1;
  }

  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);
    if(check_file(sn, df) < 0){
      return -1;
    }
  }

  return 0;
}

int run_watch(struct since_state *sn)
{
  if(sn->s_notify < 0){
    return poll_watch(sn);
  } else {
    return notify_watch(sn);
  }
}

/* display functions ****************************************/

static int display_header(struct since_state *sn, struct data_file *df, int single, int chuck)
{
  off_t delta;
  unsigned int value, z;
  int nada;
  char *suffixes[] = { "b", "kb", "Mb", "Gb", "Tb", NULL} ;

  switch(sn->s_verbose){
    case 0 : return 0;
    case 1 : if(single) return 0; /* WARNING: else fall */
    case 2 : if(df->d_notable == 0) return 0;
  }

  df->d_notable = 0;

  /* possibly do unlocked io here */
  fprintf(sn->s_header, "==> %s ", df->d_name);

  nada = 1;

  if(df->d_deleted){
    fprintf(sn->s_header, "[deleted] ");
    nada = 0;
  } else if(df->d_moved){
    fprintf(sn->s_header, "[moved] ");
    nada = 0;
  }

#if 0
  if(df->d_replaced){
    /* we know about this, but won't say anything (yet) */
    fprintf(sn->s_header, "[replaced]");
  }
#endif

  if(df->d_pos != df->d_now){
    if(chuck){
      fprintf(sn->s_header, "[discarded] ");
    }
    if(sn->s_verbose > 2){
      delta = df->d_now - df->d_pos;
      for(z = 0; (suffixes[z + 1]) && (delta > 9999); z++){
        delta /= 1024;
      }
      value = delta;
      fprintf(sn->s_header, "(+%u%s) ", value, suffixes[z]);
    }
    nada = 0;
  }

  if(nada){
    fprintf(sn->s_header, "[nothing new] ");
  }

  fprintf(sn->s_header, "<==\n");
  fflush(sn->s_header);

  return 0;
}

static int display_buffer(struct since_state *sn, struct data_file *df, char *buffer, unsigned int len)
{
  int wr, result;
  unsigned int wt, i, back;

#ifdef DEBUG
  sleep(1);

  fprintf(stderr, "display: need to display %u bytes for %s\n", len, df->d_name);
  if(len <= 0){
    fprintf(stderr, "since: logic failure: writing out empty string\n");
  }
#endif

  wt = 0;
  result = 1; /* used to infer signal */
  since_run = 1;
  sigprocmask(SIG_UNBLOCK, &(sn->s_set), NULL);

  while(since_run){
    wr = write(STDOUT_FILENO, buffer + wt, len - wt);
    switch(wr){
      case -1 :
#ifdef DEBUG
        fprintf(stderr, "display: write failed: %s\n", strerror(errno));
#endif
        switch(errno){
          case EINTR :
          case EPIPE :
            since_run = 0;
            result = 1;
          case EAGAIN : /* iffy */
            break;
          default :
            /* unlikely to do anything */
            fprintf(stderr, "since: unable to display output: %s\n", strerror(errno));
            since_run = 0;
            result = (-1);
        }
        break;
      case 0 :
        /* WARNING: unsure how to deal with this one, should we exit */
        break;
      default :
        wt += wr;
        if(wt >= len){
          since_run = 0;
          result = 0;
        }
        break;
    }
  }

  sigprocmask(SIG_BLOCK, &(sn->s_set), NULL);
  since_run = 1;

#ifdef DEBUG
  fprintf(stderr, "display: final code is %d, wt %u\n", result, wt);
#endif

  if(result < 0){ /* conventional error */
    return (-1);
  }

  if(result == 0){ /* success */
    df->d_pos += len;
    df->d_write = 1;
    return 0;
  }

  /* user interrupt */

  if(wt <= 0){
    return 1;
  }

  /* no guarantees, just trying to restart on a new line */
  back = (wt <= LINE_SEARCH) ? 0 : (wt - LINE_SEARCH);
  i = wt;

  do{
    i--;
    if(buffer[i] == '\n'){
#ifdef DEBUG
      fprintf(stderr, "recover: to newline at %u\n", i);
#endif
      wt = i + 1;
      break;
    }
  } while(i > back);

#ifdef DEBUG
  fprintf(stderr, "interrupt: at position %u\n", wt);
#endif

  df->d_pos += wt;
  df->d_write = 1;

  return 1;
}

static int display_file(struct since_state *sn, struct data_file *df, int single)
{
  char *ptr;
  char buffer[IO_BUFFER];
  int rr, result;
  off_t range, i;
  unsigned int fixup;

  /* WARNING: should not manipulate d_had here, should be done in lookup and refresh, maybe pos resets too */
  if(df->d_had > df->d_now){
    fprintf(stderr, "since: considering %s to be truncated, displaying from start\n", df->d_name);
    df->d_had = 0;
    /* WARNING: d_pos gets saved, not d_had */
    df->d_write = 1;
  }

  if(df->d_pos < df->d_had){
    df->d_jump = 1;
    df->d_pos = df->d_had;
  }

#ifdef DEBUG
  if(df->d_pos > df->d_now){
    fprintf(stderr, "since: logic failure - position pointer has overtaken length\n");
    abort();
  }
#endif

  range = df->d_now - df->d_pos;
  if(range == 0){
    display_header(sn, df, single, 0);
    return 0;
  }

  if((range > IO_BUFFER) && sn->s_domap){
    fixup = df->d_pos & (IO_BUFFER - 1);
    ptr = mmap(NULL, range + range, PROT_READ, MAP_PRIVATE, df->d_fd, df->d_pos - fixup);
    if((void *)(ptr) != MAP_FAILED){
      display_header(sn, df, single, 0);

      result = display_buffer(sn, df, ptr + fixup, range);
      df->d_jump = 1;
      munmap(ptr, range + fixup);

      return result;
#ifdef DEBUG
    } else {
      fprintf(stderr, "display: mmap of %d failed: %s\n", df->d_fd, strerror(errno));
#endif
    }
  }

  if(df->d_jump){
    if(lseek(df->d_fd, df->d_pos, SEEK_SET) != df->d_pos){
      fprintf(stderr, "since: unable to seek in file %s: %s\n", df->d_name, strerror(errno));
      return -1;
    }
    df->d_jump = 0;
  }

  display_header(sn, df, single, 0);
  i = 0;
  df->d_write = 1;
  while(df->d_pos < df->d_now){
    rr = read(df->d_fd, buffer, IO_BUFFER);
    switch(rr){
      case -1 :
        switch(errno){
          case EAGAIN :
          case EINTR  :
            break;
          default :
            fprintf(stderr, "since: unable to read from %s: %s\n", df->d_name, strerror(errno));
            return -1;
        }
        break;
      case  0 :
        fprintf(stderr, "since: unexpected eof while reading from %s\n", df->d_name);
        break;
      default :
        result = display_buffer(sn, df, buffer, rr);
        if(result != 0){
          return result;
        }
        break;
    }
  }

  if(df->d_now < df->d_pos){
    /* in case more gets append to file while reading */
    df->d_now = df->d_pos;
  }

  return 0;
}

static int display_files(struct since_state *sn)
{
  unsigned int i;
  int result, single;

#ifdef DEBUG
  fprintf(stderr, "display: have %u files to display\n", sn->s_data_count);
#endif

  single = (sn->s_data_count == 1)  ? 1 : 0;

  for(i = 0; i < sn->s_data_count; i++){
    result = display_file(sn, &(sn->s_data_files[i]), single);
    if(result){
      return result;
    }
  }

  return 0;
}

/* discard data **********************************************/

static void discard_files(struct since_state *sn)
{
  unsigned int i;
  struct data_file *df;
  int single;

#ifdef DEBUG
  fprintf(stderr, "display: have %u files to display\n", sn->s_data_count);
#endif

  single = (sn->s_data_count == 1)  ? 1 : 0;

  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);

    display_header(sn, df, single, 1);

    if(df->d_pos != df->d_now){
      df->d_pos = df->d_now;
      df->d_jump = 1;
      df->d_write = 1;
    }
  }

}

/* routines to write out new values to state file ************/

static int append_state_file(struct since_state *sn)
{
  int sofar, result;

  if((sn->s_append == NULL) || (sn->s_add == 0)){
    return 0;
  }

  if(lseek(sn->s_fd, sn->s_size, SEEK_SET) != sn->s_size){
    fprintf(stderr, "since: unable to seek to end of file %s: %s\n", sn->s_name, strerror(errno));
    return -1;
  }

  sofar = 0;
  while(sofar < sn->s_add){
    result = write(sn->s_fd, sn->s_append + sofar, sn->s_add - sofar);
    if(result < 0){
      switch(errno){
        case EAGAIN :
        case EINTR  :
          break;
        default :
          fprintf(stderr, "since: unable to write %d bytes to %s: %s\n", sn->s_add - sofar, sn->s_name, strerror(errno));
          return -1;
      }
    } else {
      sofar += result;
      if(result == 0){
        fprintf(stderr, "since: warning: wrote 0 bytes to file to %s\n", sn->s_name);
      }
    }
  }

  return 0;
}

static int internal_update_state_file(struct since_state *sn)
{
  int sofar, result;

#if 0 /* not needed, always run outof tmp where a new file is opened */
  if(lseek(sn->s_fd, 0, SEEK_SET) != 0){
    fprintf(stderr, "since: unable to seek to start of file %s: %s\n", sn->s_name, strerror(errno));
    return -1;
  }
#endif

  sofar = 0;
  while(sofar < sn->s_size){
    result = write(sn->s_fd, sn->s_buffer + sofar, sn->s_size - sofar);
    if(result < 0){
      switch(errno){
        case EAGAIN :
        case EINTR  :
          break;
        default :
          fprintf(stderr, "since: unable to rewrite %d bytes to %s: %s\n", sn->s_size - sofar, sn->s_name, strerror(errno));
          return -1;
      }
    } else {
      sofar += result;
      if(result == 0){
        fprintf(stderr, "since: warning: wrote 0 bytes to file to %s\n", sn->s_name);
      }
    }
  }

  return append_state_file(sn);
}

static int update_state_file(struct since_state *sn)
{
  /* WARNING: this invalidates the structure, forces a forget */
  char *tmp, *target;
  int i, j, result, redo, changed;
  struct data_file *df;

  if(sn->s_readonly){
    if(sn->s_verbose > 2){
      fprintf(stderr, "since: readonly, not updatating %s\n", sn->s_name);
    }
    return 1;
  }

  changed = 0;
  redo = sn->s_atomic;

  for(i = 0; i < sn->s_data_count; i++){
    df = &(sn->s_data_files[i]);
    if(df->d_write){
      changed = 1;
      if(df->d_offset < 0){
        /* awkwardly block duplicate entries */
        for(j = sn->s_data_count - 1; j > i; j--){
          if((sn->s_data_files[j].d_ino == df->d_ino) &&
             (sn->s_data_files[j].d_dev == df->d_dev) &&
             (sn->s_data_files[j].d_offset < 0) &&
             sn->s_data_files[j].d_write){
            break;
          }
        }
        if(i != j){
          continue; /* will append later, don't do it for this entry */
        }
        tmp = realloc(sn->s_append, sn->s_add + sn->s_fmt_output + 1);
        if(tmp == NULL){
          fprintf(stderr, "since: unable to allocate %d bytes for append buffer \n", sn->s_add + sn->s_fmt_output + 1);
          return -1;
        }
        sn->s_append = tmp;
        target = sn->s_append + sn->s_add;
        sn->s_add += sn->s_fmt_output + 1;
      } else {
        target = sn->s_buffer + df->d_offset;
        redo = sn->s_ismap ? sn->s_atomic : 1; /* rewrite file completely if not mmaped and we modify existing entries */
      }

      result = snprintf(target, sn->s_fmt_output + 1, sn->s_fmt, df->d_dev, df->d_ino, df->d_pos);
      target[sn->s_fmt_output] = '\n'; /* replace \0 with newline */
      if(result != sn->s_fmt_output){
        fprintf(stderr, "since: logic failure: wanted to print %d, got %d\n", sn->s_fmt_output, result);
        return -1;
      }
    }
  }

  if(changed){
    if(redo){
      result = tmp_state_file(sn, &internal_update_state_file);
    } else {
      result = append_state_file(sn);
    }
  } else {
    result = 0;
  }

  forget_state_file(sn);

  return result;
}

/* main related stuff ***************************************/

static void copying()
{
  printf("since: Copyright (c) 1998-2008 Marc Welz\n");
  printf("       May only be distributed in accordance with the terms of the GNU General\n");
  printf("       Public License v3 or newer as published by the Free Software Foundation\n");
}

static void usage(char *app)
{
  int i;

  printf("since displays the data appended to files since the last time since was run\n\n");

  printf("Usage: %s [option ...] file ...\n", app);

  printf("\nOptions\n");
  printf(" -a        update state file atomically\n");
  printf(" -d int    set the interval when following files\n");
  printf(" -e        print header lines to standard error\n");
  printf(" -f        follow files, periodically check if more data has been appended\n");
  printf(" -h        this help\n");
  printf(" -l        lax mode, do not fail if some files are inaccessible\n");
  printf(" -m        do not use mmap() to access files, use read()\n");
  printf(" -n        do not update since state file\n");
  printf(" -q        reduce verbosity to nothing\n");
  printf(" -s file   specify the state file, overriding SINCE variable and home directory\n");
  printf(" -v        increase verbosity, can be given multiple times\n");
  printf(" -x        ignore files with compressed extensions:");
  for(i = 0; ignore_suffix[i]; i++){
    printf(" %s", ignore_suffix[i]);
  }
  printf("\n");
  printf(" -z        discard initial output\n");
#ifdef VERSION
  printf(" -V        print version information\n");
#endif

  printf("\nExample\n");
  printf(" $ since -lz /var/log/*\n");
  printf(" $ logger foobar\n");
  printf(" $ since -lx /var/log/*\n");
}

int main(int argc, char *argv[])
{
  int i, j, result, dashes;
  struct since_state state, *sn;
  struct sigaction sag;
  char *state_file;

  sn = &state;
  state_file = NULL;

  init_state(sn);

  i = j = 1;
  dashes = 0;

  while (i < argc) {
    if((argv[i][0] == '-') && (dashes == 0)){
      switch (argv[i][j]) {
        case 'c' :
          copying();
          return EX_OK;
        case 'h' :
          usage(argv[0]);
          return EX_OK;
          break;
        case 's' :
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "since: -s needs a filename as parameter\n");
            return EX_USAGE;
          }
          state_file = argv[i] + j;
          i++;
          j = 1;
          break;
        case 'd':
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "since: -d needs an integer parameter\n");
            return EX_USAGE;
          }
          sn->s_delay.tv_sec = atoi(argv[i] + j);
          sn->s_delay.tv_nsec = 0;
          if(sn->s_delay.tv_sec < 0){
            sn->s_delay.tv_sec = 1;
          }
          i++;
          j = 1;
          break;
        case 'a' :
          j++;
          sn->s_atomic = 1;
          break;
        case 'e' :
          j++;
          sn->s_header = stderr;
          break;
        case 'f' :
          j++;
          sn->s_follow = 1;
          break;
        case 'l' :
          j++;
          sn->s_relaxed = 1;
          break;
        case 'm' :
          j++;
          sn->s_domap = 1 - sn->s_domap;
          break;
        case 'n' :
          j++;
          sn->s_readonly = 1;
          break;
        case 'q' :
          j++;
          sn->s_verbose = 0;
          break;
        case 'v' :
          j++;
          sn->s_verbose++;
          break;
        case 'x' :
          j++;
          sn->s_nozip++;
          break;
        case 'z' :
          j++;
          sn->s_discard++;
          break;
#ifdef VERSION
        case 'V' :
          j++;
          printf("since %s\n", VERSION);
          return EX_OK;
#endif
        case '-' :
          if((j == 1) && argv[i][j + 1] == '\0'){
            dashes = 1;
          }
          j++;
          break;
        case '\0':
          j = 1;
          i++;
          break;
        default:
          fprintf(stderr, "since: unknown option -%c (use -h for help)\n", argv[i][j]);
          return EX_USAGE;
      }
    } else {
      result = setup_data(sn, argv[i]);
      if((result < 0) || ((sn->s_relaxed == 0) && result > 0)){
        return EX_OSERR;
      }
      i++;
    }
  }

  if(sn->s_data_count <= 0){
    fprintf(stderr, "since: need at least one filename (use -h for help)\n");
    return EX_USAGE;
  }

  /* try to open a list of files */
  if(open_state_file(sn, state_file) < 0){
    return EX_OSERR;
  }

  /* attempt to load content of said file */
  if(load_state_file(sn) < 0){
    return EX_OSERR;
  }

  /* look at content, gather size of on disk fields */
  if(check_state_file(sn) < 0){
    return EX_DATAERR;
  }

  if(maybe_upgrade_state_file(sn) < 0){
    return EX_OSERR;
  }

  if(make_fmt_string(sn) < 0){
    return EX_SOFTWARE;
  }

  if(lookup_entries(sn) < 0){
    return EX_OSERR;
  }

#ifdef DEBUG
  for(i = 0; i < sn->s_data_count; i++){
    fprintf(stderr, "dump[%d]: name=%s, fd=%d\n", i, sn->s_data_files[i].d_name, sn->s_data_files[i].d_fd);
  }
#endif

  sigemptyset(&(sn->s_set));
  sigaddset(&(sn->s_set), SIGINT);
  sigaddset(&(sn->s_set), SIGPIPE);

  sigprocmask(SIG_BLOCK, &(sn->s_set), NULL);

  sag.sa_handler = &handle_signal;
  sigfillset(&(sag.sa_mask));
  sag.sa_flags = 0;
  sigaction(SIGPIPE, &sag, NULL);
  sigaction(SIGINT, &sag, NULL);

  if(sn->s_discard){
    discard_files(sn);
  } else {
    if(display_files(sn) < 0){
      return EX_OSERR;
    }
  }
  if(sn->s_follow){
    if(setup_watch(sn) < 0){
      return EX_OSERR;
    }
    do{
      result = run_watch(sn);
      if(result == 0){
        result = display_files(sn);
      }
    } while(result == 0);
    if(result < 0){
      return EX_OSERR;
    }
  }

  if(update_state_file(sn) < 0){
    return EX_OSERR;
  }

  destroy_state(sn);

  return EX_OK;
}
