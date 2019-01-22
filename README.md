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

## Sample output from other profilers

First, perf.  From calling:
```
perf record -g ./testProf
perf report
```
Output:
```
Samples: 59K of event 'cycles', Event count (approx.): 3556233224               
+  47.12%  testProf  [kernel.kallsyms]  [k] __copy_to_user_ll
+   9.35%  testProf  [kernel.kallsyms]  [k] find_get_page
+   5.12%  testProf  [kernel.kallsyms]  [k] radix_tree_lookup_element
+   3.95%  testProf  [kernel.kallsyms]  [k] native_flush_tlb_single
+   3.51%  testProf  [kernel.kallsyms]  [k] sysenter_past_esp
+   3.36%  testProf  [kernel.kallsyms]  [k] generic_file_aio_read
+   2.37%  testProf  [vdso]             [.] 0x00000428
+   2.13%  testProf  libc-2.19.so       [.] _IO_file_seekoff@@GLIBC_2.1
+   1.25%  testProf  [kernel.kallsyms]  [k] ext4_llseek
+   1.24%  testProf  [kernel.kallsyms]  [k] fget_light
+   1.09%  testProf  [kernel.kallsyms]  [k] sys_llseek
+   1.08%  testProf  [kernel.kallsyms]  [k] _copy_to_user
+   1.05%  testProf  libc-2.19.so       [.] _IO_getc
+   1.02%  testProf  [kernel.kallsyms]  [k] common_file_perm
+   0.99%  testProf  [kernel.kallsyms]  [k] do_sync_read
+   0.98%  testProf  testProf           [.] getRandomBoundedInt(int, int)
+   0.91%  testProf  [kernel.kallsyms]  [k] vfs_read
+   0.88%  testProf  [kernel.kallsyms]  [k] current_kernel_time
+   0.77%  testProf  [kernel.kallsyms]  [k] fsnotify
+   0.75%  testProf  [kernel.kallsyms]  [k] kmap_atomic_prot
+   0.73%  testProf  [kernel.kallsyms]  [k] file_read_actor
+   0.71%  testProf  libc-2.19.so       [.] fseek
```
Next, gperftools.  From calling:

```
env CPUPROFILE=prof.out ./testProf
pprof --text ./testProf ./prof.out
```
Output:
```
Total: 1458 samples
    1341  92.0%  92.0%     1341  92.0% 0xb77d7428
      34   2.3%  94.3%       34   2.3% _IO_new_file_seekoff
      21   1.4%  95.7%       25   1.7% RandomSource32::getRandomBoundedInt
      11   0.8%  96.5%       11   0.8% __GI_fseek
       9   0.6%  97.1%        9   0.6% __llseek
       8   0.5%  97.7%        8   0.5% _IO_getc
       7   0.5%  98.1%        7   0.5% _IO_acquire_lock_fct
       6   0.4%  98.6%        6   0.4% 0xb77d7429
       4   0.3%  98.8%        4   0.3% JenkinsRandomSource::genRand32
       4   0.3%  99.1%       56   3.8% main
       3   0.2%  99.3%        3   0.2% __GI__IO_file_seek
       2   0.1%  99.5%        2   0.1% _IO_seekoff_unlocked
       2   0.1%  99.6%        2   0.1% __GI__IO_file_read
       2   0.1%  99.7%        2   0.1% __read_nocancel
       2   0.1%  99.9%        2   0.1% __x86.get_pc_thunk.bx
       1   0.1%  99.9%        1   0.1% 0xb77d741d
       1   0.1% 100.0%       26   1.8% readRandFileValue
       0   0.0% 100.0%       56   3.8% __libc_start_main
       0   0.0% 100.0%        1   0.1% _init
```
Here at least we see that 92% of our runtime was... down inside some mystery function.  But that mystery function was called from somewhere, right?  Even outputing to a full visual callgraph viewer (using the `--callgrind` output option for pprof) still shows that 92% of our execution time was spent in a function with no callers.
