
#include <stdio.h>
#include <stdlib.h>


unsigned int MAX = 4294967295U;
double invMAXPlusOne = 1.0 / ( ( (float)MAX ) + 1.0 );

int getRandomBoundedInt( int inLow, int inHigh ) {
    double randFloat = (double)( rand() ) * invMAXPlusOne;

    int onePastRange = inHigh + 1;

    int magnitude = (int)( randFloat * ( onePastRange - inLow ) );
    
    return magnitude + inLow;
    }



int size = 200000000;

unsigned char readRandFileValue( FILE *inF ) {
    int pos = getRandomBoundedInt( 0, size - 1 );
    
    fseek( inF, pos, SEEK_SET );

    return fgetc( inF );
    }


int testCount = 10000000;


int main() {
    srand( 432490 );
    
    FILE *f = fopen( "testFile.bin", "r" );
    
    if( f != NULL ) {
        int sum = 0;
        for( int i=0; i<testCount; i++ ) {
            sum += readRandFileValue( f );
            }
        printf( "Sum = %d\n", sum );
        
        fclose( f );
        }
    return 0;
    }
