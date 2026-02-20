#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void usage() {
    printf( "Usage:\n\n" );
    
    printf( "reportToCallgrind report_file callgrind_file\n\n" );

    printf( "Example:\n\n" );

    
    printf( "reportToCallgrind myReport.txt myReport.callgrind\n\n" );
    exit( 0 );
    }



char *stringDuplicate( char *inString ) {
    int len = strlen( inString );
    char *newString = new char[ len + 1 ];
    memcpy( newString, inString, len );
    newString[len] = '\0';
    return newString;
    }


typedef struct FunctionLine FunctionLine;

typedef struct FunctionCall {
        FunctionLine *callee;
        int sampleCount;
    } FunctionCall;

    

#define MAX_CALLEES 16                            

struct FunctionLine {
        char *fileName;
        char *functionName;
        int lineNum;
        int sampleCount;
        
        int numCallees;
        FunctionCall callees[ MAX_CALLEES ];
    };


int maxNumLines = 0;
int filledNumLines = 0;
FunctionLine *functionLines = NULL;



void updateCallee( FunctionLine *inLine, FunctionLine *inCallee, 
                   int inNumSamples ) {
    char found = false;
    // check for existing
    for( int i=0; i<inLine->numCallees; i++ ) {
        FunctionCall *c = &( inLine->callees[ i ] );
        
        if( strcmp( c->callee->functionName,
                    inCallee->functionName ) == 0 ) {
            c->sampleCount += inNumSamples;
            
            // note that FunctionLines track the position in the
            // target function where the sample was hit,
            // not the location of the start of the function
            
            // but when tracking callees, we actually want to
            // record the position in the source of the function itself
            
            // this is not actually listed in our stack traces
            // try to get as close as possible by taking the lowest
            // line number from inside the function body
            
            if( inCallee->lineNum < c->callee->lineNum ) {
                // replace
                c->callee = inCallee;
                }

            found = true;
            break;
            }
        }

    if( found ) {
        return;    
        }
    
    // add new one
    if( inLine->numCallees >= MAX_CALLEES ) {
        printf( "Out of space in function callee list for line:  %s:%s:%d\n",
               inLine->fileName,
               inLine->functionName,
               inLine->lineNum );
        return;
        }
    FunctionCall newCall = { inCallee, inNumSamples };
    
    inLine->callees[ inLine->numCallees ] = newCall;
    inLine->numCallees++;
    }

    


int main( int inNumArgs, char **inArgs ) {
    if( inNumArgs != 3 ) {
        usage();
        }
    
    FILE *reportFile = fopen( inArgs[1], "r" );
    
    if( reportFile == NULL ) {
        usage();
        }
    
    FILE *callgrindFile = fopen( inArgs[2], "w" );
    
    if( callgrindFile == NULL ) {
        fclose( reportFile );
        usage();
        }
    
    fseek( reportFile, 0, SEEK_END );
    int length = ftell( reportFile );
    fseek( reportFile, 0, SEEK_SET );
    
    printf( "Report file contains %d bytes\n", length );
    
    char *report = new char[ length + 1 ];
    if( report == NULL ) {
        printf( "Failed to allocate report buffer\n" );
        fclose( reportFile );
        fclose( callgrindFile );
        return 1;
        }
    fread( report, 1, length, reportFile );
    report[ length ] = '\0';
    

    const char *fullStackHeader =
        "Full stacks with at least one sample:";

    char *fullStackPos = 
        strstr( report, fullStackHeader );
    
    if( fullStackPos == NULL ) {
        printf( "Failed to parse report for full stack list\n" );
        
        delete [] report;
        
        
        fclose( reportFile );
        fclose( callgrindFile );
        return 1;
        }
    
    char *stackStartPos = &( fullStackPos[ strlen( fullStackHeader ) ] );


    // num lines an upper bound on number of unique function lines in profile
    char *nextLinePos = stackStartPos;
    
    int numLines = 0;
    while( nextLinePos != NULL ) {
        nextLinePos = strstr( &( nextLinePos[1] ), "\n" );
        numLines ++;
        }
    
    printf( "%d lines found in full stack portion of profile\n", numLines );

    maxNumLines = numLines;
    filledNumLines = 0;
    functionLines = new FunctionLine[ maxNumLines ];
    
    
    int startStackOffset = stackStartPos - report;
    
    fseek( reportFile, startStackOffset, SEEK_SET );

    char foundStack = true;
    int numStacksFound = 0;
    
    while( foundStack ) {
        foundStack = false;

        float percent;
        int numSamples;
        
        int numRead = 
            fscanf( reportFile, 
                    "%f%% ===================================== (%d samples)",
                    &percent, &numSamples );
        
        if( numRead == 2 ) {
            foundStack = true;
            numStacksFound ++;

            //printf( "%f percent, %d samples found in stack\n",
            //       percent, numSamples );
            
            
            int numRead = 4;
            
            int lastStackPos = 0;
            FunctionLine *lastStackFunctionLine = NULL;
            
            while( numRead == 4 ) {

                int stackPos;
                char funName[1024];
                char fileName[200];
                memset( funName, 0, sizeof(funName) );
                memset( fileName, 0, sizeof(fileName) );
                
                int curPos = ftell( reportFile );
                int lineNum;
                
                numRead = fscanf( reportFile, "%d: %1023[^\n]",
                                  &stackPos, funName );

                if (numRead != 2) {
                    // rewind
                    fseek( reportFile, curPos, SEEK_SET );
                    break;
                }

                char *at = strstr(funName, "   (at ");
                if (at) {
                    int numScanned = sscanf(at, "   (at %199[^:]:%d)", fileName, &lineNum);

                    if (numScanned == 2) {
                        numRead += numScanned;
                        }
                    else {
                        numScanned = sscanf(at, "   (at :%d)", &lineNum);
                        if(numScanned == 1) {
                            strcpy(fileName, "?");
                            numRead = 4;
                            }
                        else {
                            // rewind again
                            fseek( reportFile, curPos, SEEK_SET );
                            }
                        }

                    // terminate the function name
                    *at = '\0';
                    }

                if( numRead == 4 ) {
                    if( stackPos = lastStackPos + 1 ) {
                        lastStackPos = stackPos;
                        // next step in stack

                        char found = false;
                        
                        FunctionLine *thisStackFunctionLine = NULL;
                        
                        for( int f=0; f<filledNumLines; f++ ) {
                            FunctionLine *fl = 
                                &( functionLines[ f ] );
                            
                            if( fl->lineNum == lineNum 
                                && strcmp( fl->functionName, funName ) == 0 ) {
                                fl->sampleCount += numSamples;
                                found = true;
                                thisStackFunctionLine = fl;
                                break;
                                }
                            }
                        if( ! found ) {
                            FunctionLine newLine = {
                                stringDuplicate( fileName ),
                                stringDuplicate( funName ),
                                lineNum,
                                numSamples,
                                0 }; // no callees so far
                        
                            functionLines[ filledNumLines ] =
                                newLine;
                            thisStackFunctionLine = 
                                &( functionLines[ filledNumLines ] );
                            filledNumLines++;
                            }

                        if( lastStackFunctionLine != NULL ) {
                            // this line calls another line
                            
                            updateCallee( thisStackFunctionLine,
                                          lastStackFunctionLine,
                                          numSamples );
                            }
                        
                        lastStackFunctionLine = thisStackFunctionLine;

                        if( stackPos == 1 ) {
                            int curPos = ftell( reportFile );
                            // may be program source code context after this
                            // skip it
                            int sourceLine;
                            char sourceCode[300];
                            int numReadB = fscanf( reportFile,
                                                   "%d:|   %299[^\n]\n",
                                                   &sourceLine, sourceCode );
                            if( numReadB == 2 ) {
                                // source code line found, skip it
                                }
                            else {
                                // source code line not found
                                // rewind
                                fseek( reportFile, curPos, SEEK_SET );
                                }
                            }
                        }
                    }
                }
            //printf( "    Read up to stack line %d\n", lastStackPos );
            }
        }
    
    printf( "%d full stacks found\n", numStacksFound );
    
    /*

      # callgrind format
      events: Cycles Instructions Flops
      fl=file.f
      fn=main
      15 90 14 2
      16 20 12
     */
    fprintf( callgrindFile, "events: Samples\n" );
    
    printf( "%d unique function lines found\n", filledNumLines );
    
    for( int i=0; i<filledNumLines; i++ ) {
        int selfSampleCount = functionLines[i].sampleCount;
        
        // subtract sample count from function calls made from this line
        for( int c=0; c<functionLines[i].numCallees; c++ ) {
            selfSampleCount -= functionLines[i].callees[c].sampleCount;
            }

        fprintf( callgrindFile, "fl=%s\n", functionLines[i].fileName );
        fprintf( callgrindFile, "fn=%s\n", functionLines[i].functionName );
        fprintf( callgrindFile, "%d %d\n", 
                 functionLines[i].lineNum,
                 selfSampleCount );
        
        for( int c=0; c<functionLines[i].numCallees; c++ ) {
            /*
            cfi=file2.c
            cfn=func2
            calls=3 20
            16 400
            */
            fprintf( callgrindFile, "cfi=%s\n", 
                     functionLines[i].callees[c].callee->fileName );
            fprintf( callgrindFile, "cfn=%s\n", 
                     functionLines[i].callees[c].callee->functionName );
            fprintf( callgrindFile, "calls=1 %d\n", 
                     functionLines[i].callees[c].callee->lineNum );
            fprintf( callgrindFile, "%d %d\n", 
                     functionLines[i].lineNum,
                     functionLines[i].callees[c].sampleCount );
            }
        fprintf( callgrindFile, "\n\n" );
        }
    
    for( int i=0; i<filledNumLines; i++ ) {
        delete [] functionLines[i].fileName;
        delete [] functionLines[i].functionName;
        }

    delete [] functionLines;
    
    delete [] report;
    
    fclose( reportFile );
    fclose( callgrindFile );

    return 0;
    }
