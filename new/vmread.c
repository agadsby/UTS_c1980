/*
 *  Vmread
 *  System interface to the virtual reader.
 *
 *  Usage: vmread [filename] [-c -h -r -s]
 * 
 * GADSBY 22Jan26 Changed magic number handling
 *
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tag.h>
#include <sys/dir.h>
#include <utmp.h>
#include <acct.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <stdioerr.h>
#include <setjmp.h>
/* #include "spooler/vmpunch.h" */

#define  STREAM  1
#define  RAW     2
#define  QUIET   4
#define  EXECUTE 8

#define ECENTSIGN       0x17    /* this is what an EBCDIC cent-sign */
				/* translates to in ascii */

#define APERM 0666              /* mode bits for all permissions */
#define WPERM 0222              /* mode bits for full write permission */

#define min(a, b) (a < b? a : b)

struct passwd *pw, *getpwnam();
struct passwd *kpw, unkusr;

struct sfblok sfblock;
struct acct_io rd;

char    usage[] =       "[filename] [-c -h -r -s]";
char    mandline[] =    "/usr/spool/bin/";
char    parcel[] =      "Parcel received under";
char    spoolrdr[] =    "/usr/spool/rdr";
char    devtag[] =      "/dev/tag";
char    reader[] =      "reader";
char    root[] =        "root";
char    unknown[] =     "unknown";
char    msg[BUFSIZ];

extern  char    etatab[];
extern  char    *cmdname;

FILE    *rdrfp;                 /* the reader */
FILE    *outfp;                 /* the output file pointer */
char    outfile[BUFSIZ];        /* name of the output file */
char    userid[10];             /* userid to receive the file */

char    ourbuf[BUFSIZ];         /* buffer to unget the magic number */
int     ourcnt;                 /* count of ungotten chars */

int     halt;                   /* gracefully termination flag */
int     magic;                  /* the first word */
int     bytecnt;                /* number of bytes read so far */

jmp_buf env;                    /* to recover from disk full signal */


main(argc, argv)
int     argc;
char    **argv;
{
	int     mode, isaygo, headers;
	int     mitt();
	char    *rdr;
	char    modestr[10];
	char    filename[BUFSIZ];
	char    header[HDRSIZ];
	char    *finddev();
	FILE    *sffp;          /* the spool fileblock interface */
	FILE    *getfn();

	/*
	 * Catch or ignore  all signals.
	 */
	argv++;
	for (mode = SIGHUP; mode <= SIGQUIT; mode++)
		signal(mode, SIG_IGN);
	for (mode = SIGINS; mode <= NSIG; mode++)
		signal(mode, mitt);
	onerror(ENOTDIR, EIGNORE);
	mode = isaygo = headers = 0;
	/*
	 * If no output file use standard output, else open file.
	 */
	if (**argv == '-')
		outfp = stdout;
	else {
		outfp = fopen(*argv, "w");
		if (outfp == NULL) {
                        sprintf(msg, "%s: unable to open", *argv);
			die(msg);
		}
		argv++;
		argc--;
	}
	/*
	 * Read flags.
	 */
	while (**argv == '-' && --argc) {
		switch( (*argv)[1] ) {
		case 'r' :
			mode |= RAW;
			break;
		case 's' :
			mode |= STREAM;
			break;
		case 'c' :
			isaygo = 1;
			break;
		case 'h' :
			headers = 1;
			break;
		default :
			die(usage);
		}
		argv++;
	}
	kpw = getpwnam(unknown);
	if (kpw != NULL) {
		unkusr = *kpw;
		kpw = &unkusr;
	}
	/*
	 * Open tag and reader.
	 */
	if ((sffp = fopen(devtag, "r")) == NULL) {
		sprintf(msg, "%s: unable to open\n", devtag);
		die(msg);
	}
	rdr = finddev(reader);
	if ((rdrfp = fopen(rdr, "r")) == NULL) {
		sprintf(msg, "%s: unable to open\n", rdr);
		die(msg);
	}
	/*
	 *  If reading in continuous mode this is an infinite loop,
	 *  otherwise the process will simply fall through
	 */
        read(fileno(sffp), (char *) &sfblock, sizeof(struct sfblok));
        magic = getw(rdrfp);
	do {
		setjmp(env);            /* return here on disk full */
                bytecnt = 0;
                ourcnt = 0;
		if (headers) {
                        mode = 0;
                        filename[0] = EOS;
                        userid[0] = EOS;
			if (feof(rdrfp)) {
                                clearerr(sffp);
                                clearerr(rdrfp);
                                read(fileno(sffp), (char *) &sfblock, sizeof(struct sfblok));
                                magic = getw(rdrfp);
			}
			if (magic == MAGIC1) {
				bytecnt += sizeof(magic);
				rdheader(rdrfp, header);
                                sscanf(header, "%s %s %s", modestr, userid, filename);
                                mode = getmode(modestr);
                        } else {
                                *(int *) ourbuf = magic;
                                ourcnt = sizeof(magic);
			}
                        outfp = getfn(userid, filename, outfile, mode);
                        if (outfp == NULL) {
                                mode |= QUIET;
                                mode &= ~EXECUTE;
                                outfp = fopen("/dev/null", "w");
                        }
		}
		readin(rdrfp, outfp, mode);
		if (stdout != outfp)
			fclose(outfp);
		if (mode & EXECUTE)
			doit(filename, outfile);
		else if ( !(mode & QUIET) ) {
			if (userid[0] != EOS) {
                                sprintf(msg, "%s \"%s\".", parcel, outfile);
                                mynotify(userid, msg, 0);
			}
		}
		if (headers)
			acct();
		/*
		 * Check for 'clean' stop.
		 */
		if (halt)
			isaygo = 0;
	} while (isaygo);
	fclose(rdrfp);
	fclose(sffp);
}
/*
 *  rdheader - read the header line.
 *  The header is in upper case EBCDIC.  Translate it to
 *  lower case ASCII.  Characters preceeded by a backslash
 *  are not converted to lower case.
 */
rdheader(rdrfp, buf)
FILE    *rdrfp;
char    *buf;
{
	char    *b, *b1;
	int     c;

	fread(buf, HDRSIZ, 1, rdrfp);
	bytecnt += HDRSIZ;
	b = b1 = buf;
	while (b < &buf[HDRSIZ]) {
                c = etatab[*b++];
                if (c == '\\' || c == ECENTSIGN)
			c = etatab[*b++];
                else if (isalpha(c))
                        c = tolower(c);
                *b1++ = c;
        }
	*b1 = EOS;
}
/*
 *  Returns a pointer to file which will get the
 *  results of the reading.
 */
FILE  *
getfn(userid, filename, outfile, mode)
char  *userid;
char  *filename;
char  *outfile;
int   mode;
{
	int     ukneek = 0;
	int     len;
        struct  stat statstuf;
	FILE    *fdback;

        pw = getpwnam(userid);
	if (pw == NULL || pw->pw_uid == 0) {
                if (kpw == NULL) {
                        sprintf(msg, "%s: %s user not found.  File %s discarded", cmdname, unknown, filename);
                        mynotify(root, msg, 0);
                        return(NULL);
                }
	}
	if ((mode & EXECUTE) || (filename[0] == EOS))
		strcpy(outfile, spoolrdr);
	else if (filename[0] != '/') {
		if (pw == NULL)
                        strcpy(outfile, spoolrdr);
		else {
			strcpy(outfile, pw->pw_dir);
			strcat(outfile, "/");
			strcat(outfile, filename);
		}
	} else
		strcpy(outfile, filename);
	/*
	 * Determine if file named can be used. If not, come up with a
	 * new, unique name for it. If this fails, write to the user's
	 * home directory.
	 */
        if (stat(outfile, &statstuf) == -1) {
                if ((fdback = fopen(outfile, "w")) != NULL) {
			chmod(outfile, APERM);
			if (pw != NULL && pw->pw_uid != 0)
				chown(outfile, pw->pw_uid, pw->pw_gid);
                        else
				chown(outfile, kpw->pw_uid, kpw->pw_gid);
			return(fdback);
		}
		/*
                 *  Cannot create file, use home directory or
		 *  /usr/spool/rdr
		 */
                if (pw != NULL)
                        strcpy(outfile, pw->pw_dir);
		else
                        strcpy(outfile, spoolrdr);
                if (stat(outfile, &statstuf) == -1)
			return(NULL);
	}
        if ((statstuf.st_mode & S_IFMT) == S_IFDIR)
                strcat(outfile, "/file");
        else if ((statstuf.st_mode & WPERM) == WPERM)
                return(fopen(outfile, "w"));     /* note: no chown */
	/*
	 *  Got here because file is a directory, or name isn't writable.
	 *  We will stick a unique number on it.
	 */
	len = strlen(outfile);
	do {
		sprintf(outfile, "%.*s%d", len, outfile, ukneek++);
	} while (stat(outfile, &statstuf) != -1);

	fdback = fopen(outfile, "w");
	chmod(outfile, APERM);
	if (pw != NULL && pw->pw_uid != 0)
	        chown(outfile, pw->pw_uid, pw->pw_gid);
        else
                chown(outfile, kpw->pw_uid, kpw->pw_gid);
	return(fdback);
}
/*
 * Read from the virtual reader driver.
 *
 * If 'stream'.
 * <two byte length> <data> <two byte length> <data>...
 *
 * If not 'stream'.
 *       /-------------------------
 *    /-------------------------- |
 * /--------------------------  | |
 * |data on virtual cards    |  |_|
 * |(fixed record size)      |__|
 * |_________________________|
 *
 * Data may be either ASCII or EBCDIC in either mode.
 */
readin(rdrfp, outfp, mode)
FILE    *rdrfp;
FILE    *outfp;
int     mode;
{
	static  char    hopper[BUFSIZ];
	register int     i, length, bks;
	int     remain;
	short   size;

	rd.ai_recs = 0;
	if (mode & STREAM) {
		/*
		 * 'Stream' data
		 */
		myread((char *)&size, 1, sizeof(size), rdrfp);
		bytecnt += sizeof(size);
		while (size && !feof(rdrfp)) {
			length = min(size, BUFSIZ);
			size -= length;
			i = myread(hopper, 1, length, rdrfp);
			bytecnt += i;
			rd.ai_recs++;
			if (  !(mode & RAW) )
				for (i = 0; i < length; i++)
					hopper[i] = etatab[hopper[i]];
			fwrite(hopper, 1, length, outfp);
			if ( !(mode & RAW) )
				putc('\n', outfp);
			if (size == 0) {
			        myread((char *)&size, 1, sizeof(size), rdrfp);
                                bytecnt += sizeof(size);
                        }
		}
		/*
		 *  Get to the beginning of the next record because
                 *  there might be more than one file in the spool file
		 */
		if (!feof(rdrfp)) {
			remain = sfblock.sf_recsize - bytecnt%sfblock.sf_recsize;
                        if (remain < sfblock.sf_recsize)
                                for (i = 0; i < remain; i++)
                                        getc(rdrfp);
			magic = getw(rdrfp);
		}
	} else {
		/*
		 * Card image data. (length from tag)
		 */
		bks = sfblock.sf_recsize;
		while (myread(hopper, 1, bks, rdrfp) == bks) {
			rd.ai_recs++;
			if ( !(mode & RAW) ) {
				for (i = 0; i < bks; i++)
					hopper[i] = etatab[hopper[i]];
				i = bks;
				while (hopper[i-1] == ' ') {
                                        i--;
					if (i == 0)
						break;
				}
                                hopper[i] = '\n';
				fwrite(hopper, i + 1, 1, outfp);
			}
			else
				fwrite(hopper, bks, 1, outfp);
		}
		while (!feof(rdrfp)) getc(rdrfp);
	}
	return;
}

/*
 * Change mode character to numeric representation.
 */
getmode(modestr)
char *modestr;
{
	struct  modetab *mp;
	char    *s;
	int     nmode = 0;

	for (s = modestr; *s; s++)
                for (mp = modes; mp->m_char; mp++)
			if (*s == mp->m_char)
				nmode |= mp->m_bits;
	return(nmode);
}

/*
 * Use the file sent as input to the program name in the header.
 * The program name in the header has the following format:
 *         cmd:arg1:arg2:arg3:arg4:arg5:arg6
 *
 * Where:
 *         cmd   - an executable module in the /usr/spool/bin directory.
 *         argN  - specify up to 6 optional arguments
 *
 * The owner of the process will be changed to the userid
 * specified in the header.
 */
doit(exec, datafile)
char    *exec;
char    *datafile;
{
	int i, j;
	char arg[7][20], *argv[7], bin[40];


	i = fork();
	if (i == 0) {
		for (i = 0; i < 15 ;i++)
			close(i);
		open(datafile, 0);
                open("/dev/null", 1);
		if (pw != NULL) {
		        setgid(pw->pw_gid);
		        setuid(pw->pw_uid);
			chdir(pw->pw_dir);
		}
		/*
		 * Believe it or not, this parses the arg list.
		 */
		i = sscanf(exec, "%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]",
                        arg[0], arg[1], arg[2], arg[3], arg[4],
			arg[5], arg[6]);
		for (j = 0; j < i; j++)
			argv[j] = arg[j];
		argv[i] = NULL;
		sprintf(bin, "%s%s", mandline, argv[0]);
		execv(bin, argv);
		unlink(datafile);
		exit(1);
	}
	while (wait(&j) != i);
	unlink(datafile);
}

/*
 *  mitt - signal catching routine
 *  Tell root about the signal.
 */
mitt(sig)
int     sig;
{
	int mitt();
	char msg[30];
	/*
	 * Reset signals.
	 * For SIGTERM, set flag to halt gracefully.
	 */
	if (sig == SIGTERM || sig == SIGNOSPC)
                signal(sig, mitt);
	sprintf(msg, "%s: caught sig(%d)", cmdname, sig);
	mynotify(root, msg, 0);
	if (sig == SIGTERM)
		halt++;
	/*
	 *  If out of disk space, unlink the file, notify the user,
         *  and get the end of the spool file
	 */
        if (sig == SIGNOSPC) {
		fclose(outfp);
		unlink(outfile);
		if (userid[0] != EOS) {
			sprintf(msg, "Parcel NOT received in %s: disk full\n", outfile);
			mynotify(userid, msg, 0);
		}
		while (!feof(rdrfp))
			getc(rdrfp);
		longjmp(env, 0);
	}
	return;
}

/*
 * acct: write out the accounting record
 */
acct()
{
	rd.ai_rid = A_READ;
	if (pw == NULL) {
		rd.ai_uid = 0;
		rd.ai_gid = 0;
	} else {
		rd.ai_uid = pw->pw_uid;
		rd.ai_gid = pw->pw_gid;
	}
	rd.ai_ctime = time();
	acctwrt(&rd, sizeof rd);
}

/*
 *  myread - a small front end to fread
 */
myread(buf, size, n, rdrfp)
char    *buf;
int     size;
int     n;
FILE    *rdrfp;
{
	int     bytes;
	int     ret;

	if (ourcnt > 0) {
		bytes = size*n;
		if (ourcnt >= bytes) {
			strncpy(buf, ourbuf, bytes);
			ourcnt -= bytes;
			strncpy(ourbuf, &ourbuf[bytes], ourcnt);
			ret = n;
		} else {
			strncpy(buf, ourbuf, ourcnt);
			ret = fread(&buf[ourcnt], bytes - ourcnt, 1, rdrfp);
			ret = (ret == 1)? n: 0;
			ourcnt = 0;
		}
	} else
		ret = fread(buf, size, n, rdrfp);
	return(ret);
}
/*
 *  Send an error message to root and die
 */
die(s)
char    *s;
{
	char    buf[BUFSIZ];

	sprintf(buf, "%s: %s", cmdname, s);
        mynotify(root, buf, 1);
	exit(1);
}
/*
 *  mynotify - a frontend to notify
 *  Do a double fork to avoid hanging on a terminal with a full
 *  output queue.  The second fork avoids zombie processes.
 */
mynotify(userid, msg, mode)
char    *userid;
char    *msg;
int     mode;
{
	if (fork() == 0) {
		if (fork() == 0)
			notify(userid, msg, mode);
		exit(0);
	} else
                wait(NULL);
}
/*
 *  finddev makes a pass through the /dev directory, looking for
 *  a device whose name matches the specified string.
 *  It returns a file descriptor for first found.
 */
char *
finddev(fordev)
char    *fordev;
{
        FILE    *devdir;         /* fp for the /dev */
        struct  direct  elem;    /* directory element */
        int     len;             /* length for match */
        static  char    fullname[30];    /* place to build the name */

        devdir = fopen("/dev", "r");
        len = strlen(fordev);
        while (fread((char *)&elem, sizeof(struct direct), 1, devdir) == 1) {
                if (elem.d_ino == 0)
			continue;
                if (strncmp(elem.d_name, fordev, len) == 0) {
                        strcpy(fullname, "/dev/");
                        strncat(fullname, elem.d_name, DIRSIZ);
                        return(fullname);
                }
	}
        fclose(devdir);
        return(NULL);
}
