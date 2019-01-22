
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

int main() {

    FILE *f = fopen( "testFile.bin", "w" );
    
    if( f != NULL ) {
        
        for( int i=0; i<size; i++ ) {
            fputc( getRandomBoundedInt( 0, 255 ), f );
            }
        

        fclose( f );
        }
    
    return 0;
    }
