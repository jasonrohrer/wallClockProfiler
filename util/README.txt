Converts the text report output by wallClockProfiler into the .callgrind format
for visualization with kcachegrind.



Build like this:

g++ -o reportToCallgrind reportToCallgrind.cpp



Redirect your profile report to a file like this:

./wallClockProfiler  10  ./myProgram  >  report.txt



Convert like this:

./reportToCallgrind  report.txt   report.callgrind



Visualize like this:

kcachegrind  report.callgrind
