# wallClockProfiler
Sadly, this apparently ONLY profiler in existence on Linux that actually works.

By "actually works," I mean that a profiler is supposed to give you a rough estimate where your program is spending the most time.  Slow programs are slow for a variety of reasons, not just because of inefficient algorithms that waste CPU cycles.  Lots of programs are I/O bound.  If you profile such a program, you want to know where the I/O hotspots are, and you don't care where the CPU hotspots are, because they are tiny, by comparison.

As far as I can tell, after way too many hours of researching and testing every available tool, there is nothing on Linux that actually does this well.  I had a simple test program, written in C++, that looped over a big file on disk, fseeking randomly and reading from the file a bunch of times.  Obviously, such a program is spending almost all of its time in fseek.  But not a single existing profiler would tell me this.

Not perf.  Not gperftools.  Not OProfile.  Obviously not gprof.  None of the usual suspects.

Most of them were telling me that the majority of the running time was spent in rand (which was used to pick the random offset to fseek to).  The truth was, this was only about 1% of the simple program's wall-clock runtime.  If I want to speed this program up, optimizing rand is going to be a waste of time.

Valgrind's callgrind can do it, but it makes the program 50x slower (or something like that), so it's useless in production environments (for example, profiling a live server that has real users connecting to it).

And it turns out that what you actually want to do is really pretty simple.  So simple that some very smart people have suggested that you do it manually:

> Attach to your program in gdb.  Interrupt it at random a bunch of times.  See what stack traces repeat more often than you'd expect them to by random chance.  If 50% of your random interrupts land in the same exact stack trace, there's a good chance your program is spending a lot of time there.  Some people call this "poor man's profiling."


This profiler is a simple bit of C++ code that automates this task.  And it uses GDB, so it gets stack traces for you that are as real and informative as stack traces from GDB.  If your program is GDB-debuggable, this profiler will work on it.

And besides GDB and standard Linux system includes, it has no dependencies of any kind.  It's also a single CPP file, a file that's so simple that it doesn't even need a build script, just:

```
g++ -o wallClockProfiler wallClockProfiler.cpp 
```

It can either run your target program directly, or attach to an existing process.  It can sample however many times per second you want it to, and it can detach automatically after a certain number of seconds.

For example, if your server process is experiencing heavy load right now, you can attach to it for the next ten seconds, grab a few hundred stack samples, then see a nice little text report telling you exactly where your server is spending its time right now.

Overhead scales with your chosen sampling rate.  If you're looking for a big problem, a relatively low sampling rate (and thus a low overhead) will be sufficient to catch it. 

## Examples

Run ./myProgram and sample the stack 20 times per second until it exits:
```
./wallClockProfiler 20 ./myProgram
```

Attatch to an existing process ./myProgram (PID 3042) and sample the stack 20 times per second for 60 seconds:
```
./wallClockProfiler 20 ./myProgram 3042 60
```

## Sample Output
From profiling the aforementioned test program that does a lot of random fseeks:
```
Sampling stack while program runs...
Sampling 20 times per second, for 50000 usec between samples
Program exited normally
313 stack samples taken
3 unique stacks sampled



Report:

81.789% =====================================
        1: __kernel_vsyscall   (at :-1)
        2: __read_nocancel   (at ../sysdeps/unix/syscall-template.S:81)
        3: _IO_new_file_seekoff   (at fileops.c:1079)
        4: _IO_seekoff_unlocked   (at ioseekoff.c:69)
        5: __GI_fseek   (at fseek.c:39)
        6: readRandFileValue   (at testProf.cpp:15)
        7: main   (at testProf.cpp:41)


17.572% =====================================
        1: __kernel_vsyscall   (at :-1)
        2: __llseek   (at ../sysdeps/unix/sysv/linux/llseek.c:33)
        3: __GI__IO_file_seek   (at fileops.c:1214)
        4: _IO_new_file_seekoff   (at fileops.c:1072)
        5: _IO_seekoff_unlocked   (at ioseekoff.c:69)
        6: __GI_fseek   (at fseek.c:39)
        7: readRandFileValue   (at testProf.cpp:15)
        8: main   (at testProf.cpp:41)


 0.319% =====================================
        1: __GI__dl_debug_state   (at dl-debug.c:74)
        2: dl_main   (at rtld.c:2305)
        3: _dl_sysdep_start   (at ../elf/dl-sysdep.c:249)
        4: _dl_start_final   (at rtld.c:332)
        5: _dl_start   (at rtld.c:558)
        6: _start   (at :-1)
```
You can see that 99% of the samples occurred down inside `__GI-fseek`.  No existing profiler showed me anything like this.
