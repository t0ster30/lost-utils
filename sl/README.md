sl
-
sl takes the most common use of Unix ls, to display the files in a directory compactly in multiple columns, and makes it substantially more useful.

sl groups files by purpose so you can mentally organize many files quickly; for instance, it collects HTML and PHP files together, as opposed to leaving them mixed up with supporting images, CSS, and JavaScript. sl points out interesting files, which include those that have been recently modified, read relatively recently, are relatively large, have warnings, or need to be checked in to or out of version control.

sl is also aesthetically pleasing due to attention to layout and filtering as well as limiting color and text annotations to salient information.

Screen Shots
-
ls vs sl on WWW site:

![WWW](img/sl-1.png )

sl on a software development directory:

![C](img/sl-2.png)

sl on a collection (photos, audio, video, even apps):

![music](img/sl-3.png)

On this directory of music, which is managed by iTunes, we see all the albums by an artist. sl also shows /number of songs in each album, the relatively recent CD rips (which not coincidentally correspond to the latest two albums), which we ripped about 2 months ago (58 days) and 7 months ago (228 days). The summary line at bottom shows the total number of CDs and the (total number) of songs. Note that the directories were determined to hold audio material, and sorting works as it should with inital "The"s ignored. This display is useful for other kinds of hierarchy.

Features
-
    sort
        group by purpose to organize and make understandable so many files (like Apple II catalog arrangers, but automatically)
        For subdirectories, look at the files they contain and if warranted reclassify directory into image, audio, or video group.
        normalize: fold case for most groups (rather than all files starting with an uppercase letter followed by all lowercase files), ignore initial the/an/a, treat various word separators (space . _ -) as equivalent
        numbers ordered properly (8.jpg before 10.jpg ??? no need for leading 0s just for sorting) 
    mark
        important (highlight in inverse colors)
        autosearch for keywords, such as "urgent" or "password"
        quickly see what's new by looking at recently changed files (think red hot)
        or recent relative to other files in that directory (think once hot now still glowing orange)
        ??? files read relatively recently. The file you worked with more recently than the others is often the one you're looking for now.
            large files relative to other files in that directory (indent by a space ??? easy to pick out against column edge)
        e.g., largest fonts may have CJK or extensive Unicode, largest among source code may be site of heavy lifting, in directory of directories call out ones with most files
        classification by trailing character (like ls -F): directory/, link->, executable*, special_
        colorize directory, executable*, special (like ls -G, though more subtle since have groupings and warnings) 
    info
        spot info: brief, particularly relevant additional information on a highly limited number of files. Since few files are targeted, this is fast and avoids visual clutter. Standard spot info details the recently read (-age), recently changed (<age, with < implying it may be earlier if a download or sloppy copy reset the last modified time), and relatively large (size-in-bytes). Per-file customizations can display, for example, latest build time and build number next to Ant build.xml, count of critical bugs filed against source code file, number of lines in TODO list, warning if HTML has not been validated, you name it.
        /number of files in subdirectories, which can be a useful if rough survey (this is not slow)
        e.g., only 2 files, 1000 files, TOSORT/27, tests/27, Yosemite 2007 photos/316 vs Detroit photos/2
        summary line with counts and totals. Includes a count of .dotfiles, which are rare outside of the home directory. 
    filter
        ignore clutter: Emacs auto backup (like GNU ls -B), generated (Java .class, C .o), C .h, Macintosh Desktop DB, TAGS
        identify series (like audiobook ch 01.mp3 ... audiobook ch 27.mp3) and condense to first one plus count
        e.g., DSC00423.jpg, DSC00424.jpg, DSC00427.jpg ... DSC01072.jpg ??? DSC00423.jpg...227. Also look at /dev.
        elision of shared prefixes reduces the amount of text to read and implicitly clusters similar files. Here's one spectacular application.
        distill: If you are already familiar with a directory or it is very large, use the -only command-line option to distill the listing to only distinctive files. A file is considered distinctive if it's: recently changed, recently read, a warning, or spot info. sl -only on /usr/bin and OS X /Library/Fonts can be interesting. 
    layout
        column widths tailored to what's needed by individual columns (as opposed to uniform width by ls dictated by the single longest filename in the directory), giving a more natural appearance and freeing space for more columns
        if group title would be at bottom of column, bump to top of next column if room
        shorten very f...ing long names if necessary to achieve multiple columns. Shortened names retain the first characters of the file, file suffix, and the first number which is usually a series number or year or video resolution (1080p). 
    warnings
        broken link X (base file moved, renamed, or deleted): symlink, ~ file from Emacs backup or CVS previous version
        not readable by current user
        directory Writable by public
        directory not searchable/enterable (executable permission not set) by current user
        peculiar permissions: owner can't read, group or public can write or execute but not read, or owner has less permission than group or public. For example, a dropbox directory may display permissions rwx-wx-wx
        special permission bits: setuid, setgid, sticky. These are not errors, but something to be aware of.
        file 0-length or directory is empty, directory contains only 1 file (Strunk and White: "omit needless hierarchy")
        file has 2 or more ??? hard links. Under normal conditions, a file has exactly 1 (from its parent directory).
        file changed vis-a-vis version control: either local copy has been edited and needs to be uploaded^ to repository, or another worker updated the repository rendering the local copy stale and in need of downloadingv (stale files checked only in local repositories, not remote servers, for performance). Support for RCS and CVS is built in, and you can customize to add support for others. 

sl does not replace ls. Use ls to see all files and full metadata.
Software

for OS X, Solaris, Unix, and GNU/Linux
Licensed under the GNU Public License version 3. NO WARRANTY.

Install:
-
    Download software, probably to /usr/local/bin or ~/bin.
        v1.1.2 of February 16. Support filenames that are not UTF-8 and not ASCII (thanks Christian Neukirchen).
        v1.1 of February 12. Support file sizes larger than 4GB on 32-bit systems (thanks Giuseppe Merigo), tightened tolerances, new feature: autosearch.
        v1.0 of January 26, 2012 
    From the command line:

        chmod +x download-dir/sl
        unalias sl
        rehash

    Install Tcl, if needed (which tclsh comes up empty). Install into /usr/local/bin or change the first line of the sl script. Tcl is already installed in OS X. 

Use: Now more-useful listings are as convenient to type as the usual ls.

    sl directory-path

Convenience: Automatically see an overview and interesting files when switching to a new directory:

    alias cd  'cd \!*; sl'
    alias pd  'pushd \!*; sl'
    alias pdo 'pushd \!*; sl -only'

Customization
-
Customization is done via a startup file, at the path ~/.sl.tcl. You can control colors, new suffixes, localization of the most used text, switches that control system operation, and even exactly what is shown for every file. For example, here's a custom color scheme that makes files and directories brighter and blends the text annotations into the background.

The startup file is executed as Tcl code, so you can implement substantial changes, such as adding support for another version control system. Rather than hacking the source code, it is better to put customizations in the startup file so that you can easily update to new versions without reapplying your hacks. Tcl lets you go so far as redefining whole procedures, so any change you want can be done in the startup file.

    .sl.tcl sample startup file, download to your home directory. It shows how to make many of the most likely changes, as well as how to turn on features that are too mind blowing to be the default settings, including prefix and suffix elision. 

Support
-
Troubleshooting:

    If you see lots of garbage that looks like ^[[31m, enable color for your terminal or turn off color in your startup file.
    To change colors or bold on OS X, use Terminal's Preferences. Be sure to pick a font that has a bold variation, such as Menlo.
    To view color output with less, set the LESS environment variable to include --RAW-CONTROL-CHARS.
    Error reported no such file or directory, but file definitely exists. If you have a legacy filesystem with filenames that are not encoded in Unicode UTF-8 (or ASCII, which is a subset of UTF-8), you should migrate the names to UTF-8 with a tool such as convmv. sl tries to handle this situation, but for paths passed to it on the command line it is already too late.
    Previously, transposing the letters of ls was a misspelling. In some systems, it resulted in a Command not found error. Some shells prepared for this and aliased the transposition and other misspellings to in effect autocorrect to what the user meant to type. If the output still looks like ls, it's probably an alias. You can unalias in open terminals and take out the line in the shell startup file. In another case, a Linux distribution surprised you with ASCII art of a train. You can delete it, or if an ASCII train is a key part of your problem-solving toolkit you can rename it. 

Known bugs:
-
    On OS X, the Spotlight search engine indexes the contents of files. As a side effect of reading the content of a file, the file system updates the file's last accessed time (atime). Because Spotlight is continuously indexing, often within seconds of a file being changed, almost all files have very recent atimes. For sl, this makes the atime useless for showing recently accessed files: because everybody's special, nobody's special. Spotlight should consider its work to be stealthy and reset the atime.
    Some file systems do not update atime, which is used to determine recently read files. 

> Send suggestions and bug reports to
>
> Invented by Tom Phelps on December 30, 2011.
