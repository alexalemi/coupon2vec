// Let's see how many things there are with quantity


#define DATAFILE "data/transactions.csv"  // Note there are 142,221,150 lines
long long count = 0;

int main(int argc, char *argv[]) {

    long quantity = 0;
    long long total = 0;

    FILE *datfile = fopen(DATAFILE,"r");
    // get the first line
    fgets(dump, MAX_STRING, datfile);

    while (!feof(datfile)) {
        // file is in format <id,chain,dept,category,company,brand,date,productsize,productmeasure,purchasequantity,purchaseamount>
        fgets(dump, MAX_STRING, datfile);
        sscanf(dump, "%*ld,%*ld,%*ld,%*ld,%*ld,%*ld,%*25[^,],%*ld,%*30[^,],%ld,%*s", &quantity);
        total += quantity; 
    }

    printf("Total quantity count: %lld\n", total);

}
