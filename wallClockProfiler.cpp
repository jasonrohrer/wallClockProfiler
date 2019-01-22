#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/prctl.h>

#include <time.h>


#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SimpleVector.h"


static void usage() {
    printf( "\nDirect call usage:\n\n"
            "    wallClockProfiler samples_per_sec ./myProgram\n\n" );
    printf( "Attach to existing process (may require root):\n\n"
            "    wallClockProfiler samples_per_sec ./myProgram pid "
            "[detatch_sec]\n\n" );
    printf( "detatch_sec is the (optional) number of seconds before detatching and\n"
            "ending profiling (or -1 to stay attached forever, default)\n\n" );
    
    exit( 1 );
    }


int inPipe;
int outPipe;



char sendBuff[1024];


static void sendCommand( const char *inCommand ) {
    sprintf( sendBuff, "%s\n", inCommand );
    write( outPipe, sendBuff, strlen( sendBuff ) );
    }


char readBuff[4096];

char programExited = false;


static int fillBufferWithResponse() {
    int readSoFar = 0;
    while( true ) {
        int numRead = 
            read( inPipe, &( readBuff[readSoFar] ), 4095 - readSoFar );
        
        if( numRead > 0 ) {
            
            readSoFar += numRead;
            
            readBuff[ readSoFar ] = '\0';
        
            if( strstr( readBuff, "(gdb)" ) != NULL ) {
                // read full response
                return readSoFar;
                }
            }
        }
    }




static void checkProgramExited() {
    if( strstr( readBuff, "exited-normally" ) != NULL ) {
        programExited = true;
        }
    }


static void printGDBResponse() {
    int numRead = fillBufferWithResponse();
    
    if( numRead > 0 ) {
        checkProgramExited();
        printf( "\n\nRead from GDB:  %s", readBuff );
        }
    }



static void printGDBResponseToFile( FILE *inFile ) {
    int numRead = fillBufferWithResponse();
    
    if( numRead > 0 ) {
        checkProgramExited();
        fprintf( inFile, "\n\nRead from GDB:  %s", readBuff );
        }
    }



static void skipGDBResponse() {
    int numRead = fillBufferWithResponse();
    checkProgramExited();
    }




typedef struct StackFrame{
        void *address;
        char *funcName;
        char *fileName;
        int lineNum;
    } StackFrame;


typedef struct Stack {
        SimpleVector<StackFrame> frames;
        int sampleCount;
    } Stack;

    


SimpleVector<Stack> stackLog;



static char stackCompare( Stack *inA, Stack *inB ) {
    if( inA->frames.size() != inB->frames.size() ) {
        return false;
        }
    for( int i=0; i<inA->frames.size(); i++ ) {
        if( inA->frames.getElementDirect( i ).address !=
            inB->frames.getElementDirect( i ).address ) {
            return false;
            }    
        }
    return true;
    }


static void freeStack( Stack *inStack ) {
    for( int i=0; i<inStack->frames.size(); i++ ) {
        StackFrame f = inStack->frames.getElementDirect( i );
        delete [] f.funcName;
        delete [] f.fileName;
        }
    inStack->frames.deleteAll();
    }


    

static StackFrame parseFrame( char *inFrameString ) {
    StackFrame newF;
    
    char *openPos = strstr( inFrameString, "{" );
    
    if( openPos == NULL ) {
        printf( "Error parsing stack frame:  %s\n", inFrameString );
        exit( 1 );
        }
    openPos = &( openPos[1] );
    
    char *closePos = strstr( openPos, "}" );
    
    if( closePos == NULL ) {
        printf( "Error parsing stack frame:  %s\n", inFrameString );
        exit( 1 );
        }
    closePos[0] ='\0';
    
    int numVals;
    char **vals = split( openPos, ",", &numVals );
    
    void *address = NULL;
    
    for( int i=0; i<numVals; i++ ) {
        if( strstr( vals[i], "addr=" ) == vals[i] ) {
            sscanf( vals[i], "addr=\"%p\"", &address );
            break;
            }
        }

    newF.address = address;
    newF.lineNum = -1;
    newF.funcName = NULL;
    newF.fileName = NULL;
    
    for( int i=0; i<numVals; i++ ) {
        if( strstr( vals[i], "func=" ) == vals[i] ) {
            newF.funcName = new char[ 500 ];
            sscanf( vals[i], "func=\"%499s\"", newF.funcName );
            }
        else if( strstr( vals[i], "file=" ) == vals[i] ) {
            newF.fileName = new char[ 500 ];
            sscanf( vals[i], "file=\"%499s\"", newF.fileName );
            }
        else if( strstr( vals[i], "line=" ) == vals[i] ) {
            sscanf( vals[i], "line=\"%d\"", &newF.lineNum );
            }
        }
    
    if( newF.fileName == NULL ) {
        newF.fileName = stringDuplicate( "" );
        }
    if( newF.funcName == NULL ) {
        newF.funcName = stringDuplicate( "" );
        }
    
    char *quotePos = strstr( newF.fileName, "\"" );
    if( quotePos != NULL ) {
        quotePos[0] ='\0';
        }
    quotePos = strstr( newF.funcName, "\"" );
    if( quotePos != NULL ) {
        quotePos[0] ='\0';
        }


    for( int i=0; i<numVals; i++ ) {
        delete [] vals[i];
        }
    delete [] vals;

    return newF;
    }



static void logGDBStackResponse() {
    int numRead = fillBufferWithResponse();
    
    if( numRead == 0 ) {
        return;
        }
    

    checkProgramExited();
        
    if( programExited ) {
        return;
        }
    
    const char *stackStartMarker = ",stack=[";
    
    char *stackStartPos = strstr( readBuff, ",stack=[" );
        
    if( stackStartPos == NULL ) {
        return;
        }
    
    char *stackStart = &( stackStartPos[ strlen( stackStartMarker ) ] );
    
    char *closeBracket = strstr( stackStart, "]\n" );
    
    if( closeBracket == NULL ) {
        return;
        }
    
    // terminate at close
    closeBracket[0] = '\0';
    
    const char *frameMarker = "frame=";
    
    if( strstr( stackStart, frameMarker ) != stackStart ) {
        return;
        }
    
    // skip first
    stackStart = &( stackStart[ strlen( frameMarker ) ] );
    
    int numFrames;
    char **frames = split( stackStart, frameMarker, &numFrames );

    Stack thisStack;
    thisStack.sampleCount = 1;
    for( int i=0; i<numFrames; i++ ) {
        thisStack.frames.push_back( parseFrame( frames[i] ) );
        delete [] frames[i];
        }
    delete [] frames;

    
    char match = false;
    for( int i=0; i<stackLog.size(); i++ ) {
        Stack *inOld = stackLog.getElement( i );
        
        if( stackCompare( inOld, &thisStack ) ) {
            match = true;
            inOld->sampleCount++;
            break;
            }
        }
    
    if( match ) {
        freeStack( &thisStack );
        }
    else {
        stackLog.push_back( thisStack );
        }
    }





int main( int inNumArgs, char **inArgs ) {
    
    if( inNumArgs != 3 && inNumArgs != 4 && inNumArgs != 5 ) {
        usage();
        }
    
    int samplesPerSecond = 100;
    
    sscanf( inArgs[1], "%d", &samplesPerSecond );
    


    int readPipe[2];
    int writePipe[2];
    
    pipe( readPipe );
    pipe( writePipe );
    
    

    int childPID = fork();
    
    if( childPID == -1 ) {
        printf( "Failed to fork\n" );
        return 1;
        }
    else if( childPID == 0 ) {
        // child
        dup2( writePipe[0], STDIN_FILENO );
        dup2( readPipe[1], STDOUT_FILENO );
        dup2( readPipe[1], STDERR_FILENO );

        while( false && true ) {
            printf( "test\n" );
            }
        
        //ask kernel to deliver SIGTERM in case the parent dies
        prctl( PR_SET_PDEATHSIG, SIGTERM );
        
        if( inNumArgs == 4 || inNumArgs == 5 ) {
            // attatch to PID
            execlp( "gdb", "gdb", "-nx", "--interpreter=mi", 
                    inArgs[2], inArgs[3], NULL );
            }
        else {
            execlp( "gdb", "gdb", "-nx", "--interpreter=mi", inArgs[2], NULL );
            }
        exit( 0 );
        }
    
    // else parent
    printf( "Forked GDB child on PID=%d\n", childPID );
    
    
    //close unused pipe ends
    close( writePipe[0] );
    close( readPipe[1] );
    
    inPipe = readPipe[0];
    outPipe = writePipe[1];

    fcntl( inPipe, F_SETFL, O_NONBLOCK );

    skipGDBResponse();
    

    if( inNumArgs == 3 ) {
        printf( "\n\nStarting gdb program with 'run', "
                "redirecting program output to wcOut.tx\n" );
        
        sendCommand( "run > wcOut.txt" );
        }
    else {
        printf( "\n\nResuming attached gdb program with '-exec-continue'\n" );
        
        sendCommand( "-exec-continue" );
        }
    
    usleep( 100000 );

    skipGDBResponse();
    
    printf( "Debugging program '%s'\n", inArgs[2] );

    char rawProgramName[100];
    
    char *endOfPath = strrchr( inArgs[2], '/' );

    char *progName = inArgs[2];
    
    if( endOfPath != NULL ) {
        progName = &( endOfPath[1] );
        }
    
    char *pidCall = autoSprintf( "pidof %s", progName );\

    FILE *pidPipe = popen( pidCall, "r" );
    
    delete [] pidCall;
    
    if( pidPipe == NULL ) {
        printf( "Failed to open pipe to pidof to get debugged app pid\n" );
        return 1;
        }

    int pid = -1;
    
    // if there are multiple GDP procs, they are printed in newest-first order
    // this will get the pid of the latest one (our GDB child)
    int numRead = fscanf( pidPipe, "%d", &pid );
    
    pclose( pidPipe );

    if( numRead != 1 ) {
        printf( "Failed to read PID of debugged app\n" );
        return 1;
        }
    
    printf( "PID of debugged process = %d\n", pid );
    

    printf( "Sampling stack while program runs...\n" );

    
    int numSamples = 0;
    

    int usPerSample = 1000000 / samplesPerSecond;
    

    printf( "Sampling %d times per second, for %d usec between samples\n",
            samplesPerSecond, usPerSample );
    
    time_t startTime = time( NULL );
    
    int detatchSeconds = -1;
    
    if( inNumArgs == 5 ) {
        sscanf( inArgs[4], "%d", &detatchSeconds );
        }
    if( detatchSeconds != -1 ) {
        printf( "Will detatch automatically after %d seconds\n",
                detatchSeconds );
        }
    

    while( !programExited &&
           ( detatchSeconds == -1 ||
             time( NULL ) < startTime + detatchSeconds ) ) {
        usleep( usPerSample );
    
        // interrupt
        kill( pid, SIGINT );
        numSamples++;
        
        skipGDBResponse();
        

        // sample stack
        sendCommand( "-stack-list-frames" );
        //skipGDBResponse();
        logGDBStackResponse();
        
        
        // continue running
        
        sendCommand( "-exec-continue" );
        skipGDBResponse();
        }

    if( programExited ) {
        printf( "Program exited normally\n" );
        }
    else {
        printf( "Detatching from program\n" );
        kill( pid, SIGINT );
        sendCommand( "-gdb-exit" );
        }
    
    printf( "%d stack samples taken\n", numSamples );

    printf( "%d unique stacks sampled\n", stackLog.size() );
    
    
    // simple insertion sort
    SimpleVector<Stack> sortedStacks;
    
    while( stackLog.size() > 0 ) {
        int max = 0;
        Stack maxStack;
        int maxInd = -1;
        for( int i=0; i<stackLog.size(); i++ ) {
            Stack s = stackLog.getElementDirect( i );
            
            if( s.sampleCount > max ) {
                maxStack = s;
                max = s.sampleCount;
                maxInd = i;
                }
            }  
        sortedStacks.push_back( maxStack );
        stackLog.deleteElement( maxInd );
        }
    
    printf( "\n\n\nReport:\n\n" );
    
    for( int i=0; i<sortedStacks.size(); i++ ) {
        Stack s = sortedStacks.getElementDirect( i );
        printf( "%6.3f%% =====================================\n"
                "      %3d: %s   (at %s:%d)\n", 
                100 * s.sampleCount / (float )numSamples,
                1,
                s.frames.getElement( 0 )->funcName, 
                s.frames.getElement( 0 )->fileName, 
                s.frames.getElement( 0 )->lineNum );
        // print stack for context below
        for( int j=1; j<s.frames.size(); j++ ) {
            StackFrame f = s.frames.getElementDirect( j );
            printf( "      %3d: %s   (at %s:%d)\n", 
                    j + 1,
                    f.funcName, 
                    f.fileName, 
                    f.lineNum );
            }
        printf( "\n\n" );

        freeStack( &s );
        }
    


    
    return 0;
    }
