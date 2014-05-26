// Let's see how many things there are with quantity

#include <stdio.h>
#include <stdlib.h>

#define DATAFILE "data/transactions.csv"  // Note there are 142,221,150 lines
#define MAX_STRING 1000
long long count = 0;

int main(int argc, char *argv[]) {

    long quantity = 0;
    long long total = 0;

    FILE *datfile = fopen(DATAFILE,"r");
    // get the first line
    char dump[MAX_STRING+1];
    fgets(dump, MAX_STRING, datfile);

    long linenum = 0;

    while (!feof(datfile)) {
        linenum++;

        if (linenum%1000000 == 0) printf("current total: %lld\n", total);
        // file is in format <id,chain,dept,category,company,brand,date,productsize,productmeasure,purchasequantity,purchaseamount>
        fgets(dump, MAX_STRING, datfile);
        sscanf(dump, "%*ld,%*ld,%*ld,%*ld,%*ld,%*ld,%*25[^,],%*ld,%*30[^,],%ld,%*s", &quantity);
        total += quantity; 
    }

    printf("Total quantity count: %lld\n", total);

}
