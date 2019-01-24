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

                            

typedef struct FunctionLine {
        char *fileName;
        char *functionName;
        int lineNum;
        int sampleCount;
    } FunctionLine;


int maxNumLines = 0;
int filledNumLines = 0;
FunctionLine *functionLines = NULL;

    


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
        "Full stacks with more than one sample:";

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
            while( numRead == 4 ) {

                int stackPos;
                char funName[200];
                char fileName[200];
                memset( funName, 0, 200 );
                memset( fileName, 0, 200 );
                
                int curPos = ftell( reportFile );
                int lineNum;
                numRead = fscanf( reportFile, "%d: %199s   (at %199[^:]:%d)",
                                  &stackPos, funName, fileName, &lineNum );
                
                if( numRead != 4 ) {
                    // rewind
                    fseek( reportFile, curPos, SEEK_SET );

                    // try again, allowing blank for file name
                    numRead = fscanf( reportFile, 
                                      "%d: %199s   (at :%d)",
                                      &stackPos, funName, &lineNum );
                    
                    if( numRead != 3 ) {
                        // rewind again
                        fseek( reportFile, curPos, SEEK_SET );
                        }
                    else {
                        fileName[0] = '?';
                        fileName[1] = '\0';
                        numRead = 4;
                        }
                    }

                if( numRead == 4 ) {
                    if( stackPos = lastStackPos + 1 ) {
                        lastStackPos = stackPos;
                        // next step in stack

                        char found = false;
                        
                        for( int f=0; f<filledNumLines; f++ ) {
                            FunctionLine *fl = 
                                &( functionLines[ f ] );
                            
                            if( fl->lineNum == lineNum 
                                && strcmp( fl->functionName, funName ) == 0 ) {
                                fl->sampleCount += numSamples;
                                found = true;
                                break;
                                }
                            }
                        if( ! found ) {
                            FunctionLine newLine = {
                                stringDuplicate( fileName ),
                                stringDuplicate( funName ),
                                lineNum,
                                numSamples };
                        
                            functionLines[ filledNumLines ] =
                                newLine;
                            filledNumLines++;
                            }

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
        fprintf( callgrindFile, "fl=%s\n", functionLines[i].fileName );
        fprintf( callgrindFile, "fn=%s\n", functionLines[i].functionName );
        fprintf( callgrindFile, "%d %d\n\n\n", 
                 functionLines[i].lineNum,
                 functionLines[i].sampleCount );
        

        delete [] functionLines[i].fileName;
        delete [] functionLines[i].functionName;
        }

    delete [] functionLines;
    
    delete [] report;
    
    fclose( reportFile );
    fclose( callgrindFile );

    return 0;
    }
