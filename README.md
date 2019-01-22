# wallClockProfiler
Sadly, this apparently ONLY profiler in existence on Linux that actually works.

By "actually works," I mean that a profiler is supposed to give you a rough estimate where your program is spending the most time.  Slow programs are slow for a variety of reasons, not just because of inefficient algorithms that waste CPU cycles.  Lots of programs are I/O bound.  If you profile such a program, you want to know where the I/O hotspots are, and you don't care where the CPU hotspots are, because they are tiny, by comparison.

As far as I can tell, after way too many hours of researching and testing every available tool, there is nothing on Linux that actually does this well.  I had a simple test program, written in C++, that looped over a big file on disk, fseeking randomly and reading from the file a bunch of times.  Obviously, such a program is spending almost all of its time in fseek.  But not a single existing profiler would tell me this.

Not perf.  Not gperftools.  Not OProfile.  Obviously not gprof.  None of the usual suspects.

Most of them were telling me that the majority of the running time was spent in rand (which was used to pick the random offset to fseek to).  The truth was, this was only about 5% of the simple program's wall-clock runtime.  If I want to speed this program up, optimizing rand is going to be a waste of time.

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
