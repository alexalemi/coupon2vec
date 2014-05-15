/*
 * My attempt at using the word2vec 
 * framework for doing the kaggle challenge
 *
 * Author: Alex Alemi
 * Date: 20140507
 */

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <time.h>
#include <math.h>
#include "dbg.h"
#include "qrand.h"


// num_customers=111137, num_products=42327
#define DATAFILE "data/transactions.csv"  // Note there are 142,221,150 lines
#define CUSTFILE "output/cust_vecs"
#define PRODFILE "output/prod_vecs"
#define WITH_ZLIB
#define MAX_STRING 1000
#define ARRAY_GROWTH 1000
#define D 256
#define CUSTS 311541    
#define PRODS 61318
#define LINES 349655789
#define ALPHA 0.000065
#define NEGS  5

#define EXP_TABLE_SIZE 1000
#define MAX_EXP 6

typedef double real;

real alpha = ALPHA;

real *customer_vecs;
real *product_vecs;
real *exp_table;

const int hash_size = 30000000;  // Maximum 30 * 0.7 = 21M words in the vocabulary
long *customer_hash;
long *product_hash;

typedef struct customer {
    long id;
} customer;

typedef struct product {
    long company, brand;
} product;

customer* customers;
product* products;

long num_customers = 0;
long num_products = 0;
long max_num_customers = ARRAY_GROWTH;
long max_num_products = ARRAY_GROWTH;

// define the hash functions
// for customers
unsigned int get_customer_hash(long id) {
    return (unsigned int)(id % hash_size);
}

// for products
unsigned int get_product_hash(long company, long brand) {
   return (unsigned int)((company + brand) % hash_size);
}

// finding functions
long find_customer(long id) {
    unsigned int hash = get_customer_hash(id);
    while (1) {
        long loc = customer_hash[hash];
        if (loc == -1) return -1;
        customer c = customers[loc];
        if (c.id == id) return loc;
        hash = (hash + 1) % hash_size;
    }
    return -1;
}

long find_product(long company, long brand) {
    unsigned int hash = get_product_hash(company, brand);
    while (1) {
        long loc = product_hash[hash];
        /* debug("Found loc %ld", loc); */
        if (loc == -1) return -1;
        product p = products[loc];
        if (p.company == company && p.brand == brand) return loc;
        hash = (hash + 1) % hash_size;
    }
    return -1;
}

// add a customer
long add_customer(long id) {
    /* debug("adding customer id: %ld", id); */
    customers[num_customers].id = id;
    num_customers++;
    // grow the array if necessary
    if (num_customers + 2 >= max_num_customers) {
        max_num_customers += ARRAY_GROWTH;
        customers = (customer *)realloc(customers, max_num_customers * sizeof(customer));
    }
    int hash = get_customer_hash(id);
    while (customer_hash[hash] != -1) hash = (hash + 1) % hash_size;
    customer_hash[hash] = num_customers - 1;
    return num_customers - 1;
}

// add a product, put it at the end of the products array
// and add its location to the hash lookup
long add_product(long company, long brand) {
    /* debug("adding product company: %ld, brand: %ld", company, brand); */
    products[num_products].company = company;
    products[num_products].brand = brand;
    num_products++;
    // grow the array if necessary
    if (num_products + 2 >= max_num_products) {
        max_num_products += ARRAY_GROWTH;
        products = (product *)realloc(products, max_num_products * sizeof(product));
    }
    unsigned int hash = get_product_hash(company, brand);
    while (product_hash[hash] != -1) hash = (hash + 1) % hash_size;
    product_hash[hash] = num_products - 1;
    return num_products - 1;
}


void initialize() {
    srand(time(NULL));
    sqrand(time(NULL));
    

    // set the hash bins to all be empty
    customer_hash = calloc(hash_size, sizeof(long));
    check(customer_hash, "Failed to create customer hash");
    product_hash = calloc(hash_size, sizeof(long));
    check(product_hash,"Failed to create product hash");
    for (long a=0; a<hash_size; a++) {
        customer_hash[a] = -1;
        product_hash[a] = -1;
    }

    // do the initial allocation for customers and products
    customers = calloc(ARRAY_GROWTH, sizeof(customer));
    check(customers, "Failed to create customers");
    products = calloc(ARRAY_GROWTH, sizeof(product));
    check(products, "Failed to create products");
    num_customers = 0;
    num_products = 0;
    max_num_customers = ARRAY_GROWTH;
    max_num_products = ARRAY_GROWTH;

    // initialize exp_table
    exp_table = (real *)malloc((EXP_TABLE_SIZE + 1) * sizeof(real));
    for (int i = 0; i < EXP_TABLE_SIZE; i++) {
        exp_table[i] = exp((i / (real)EXP_TABLE_SIZE * 2 - 1) * MAX_EXP);   // Precompute the exp() table
        exp_table[i] = exp_table[i] / (exp_table[i] + 1);                   // Precompute f(x) = x / (x + 1)
    }

    return;
error:
    log_err("Huston we have a problem!");
    exit(1);
}

void initialize_vectors() {
    if (customer_vecs) free(customer_vecs);
    if (product_vecs) free(product_vecs);
    /* customer_vecs = malloc(CUSTS*D*sizeof(real)); */
    long long a = posix_memalign((void **)&customer_vecs, 128, (long long)CUSTS * D * sizeof(real));
    check(customer_vecs, "Failed to initialize customer vectors");
    /* product_vecs = malloc(PRODS*D*sizeof(real)); */
    a = posix_memalign((void **)&product_vecs, 128, (long long)PRODS * D * sizeof(real));
    check(product_vecs,"Failed to initialize product vectors");
    for (long a=0; a<CUSTS*D; a++) customer_vecs[a] = qrand_normal() / (2*D);
    for (long a=0; a<PRODS*D; a++) product_vecs[a] = qrand_normal() / (2*D);
    return;

error:
    log_err("Huston we have a problem!");
    exit(1);
}

real sigmoid(real x) {
    return 1./(1. + exp(-1.*x));
}

void print_customers(FILE *fp) {
    debug("Printing customer vecs..., num_customers=%ld", num_customers);
    for (int i=0; i<num_customers; i++) {
        fprintf(fp,"%ld", customers[i].id);
        for (int j=0; j<D; j++) {
            fprintf(fp,", %f", customer_vecs[i*D + j]);
        }
        fprintf(fp,"\n");
    }
    debug("Done.");
}

void print_products(FILE *fp) {
    debug("Printing product vecs..., num_products=%ld", num_products);
    for (int i=0; i<num_products; i++) {
        fprintf(fp,"%ld, %ld", products[i].company, products[i].brand);
        for (int j=0; j<D; j++) {
            fprintf(fp,", %f", product_vecs[i*D + j]);
        }
        fprintf(fp,"\n");
    }
    debug("Done.");
}



// learn some stuff
void run(FILE *fp) {
    clock_t start = clock();
    clock_t now;
    int label;

    debug("Populating Hashes...");
    long id, company, brand, quantity;
    rewind(fp);
    char dump[MAX_STRING+1];
    // get the first line
    fgets(dump, MAX_STRING, fp);

    // create the temporary arrays
    real *custupdate = calloc(D, sizeof(real));
    real *produpdate = calloc(D, sizeof(real));
    real *cv;
    real *pv;
    real *randcv;
    real *randpv;

    real dot, mult, mymult;
    long customer_loc, product_loc;

    long linenum = 1; 
    // file is in format <id,chain,dept,category,company,brand,date,productsize,productmeasure,purchasequantity,purchaseamount>
    while (!feof(fp)) {
        // get the next line from the file
        fgets(dump, MAX_STRING, fp);
        linenum++;
        sscanf(dump, "%ld,%*ld,%*ld,%*ld,%ld,%ld,%*25[^,],%*ld,%*30[^,],%ld,%*s", &id, &company, &brand, &quantity);
        quantity = 1;
        /* debug("Found id: %ld, company: %ld, brand: %ld", id, company, brand); */

        customer_loc =  find_customer(id);
        if (customer_loc == -1) customer_loc = add_customer(id);
        product_loc = find_product(company, brand);
        if (product_loc == -1) product_loc = add_product(company, brand);

        // Do the update
        label = 1;
        cv = customer_vecs + customer_loc*D;
        pv = product_vecs  + product_loc*D;
        /* alpha = ALPHA; */
        /* alpha = ALPHA * (1. - linenum / (real)(LINES + 1.)); */
        /* if (alpha < ALPHA * 0.0001) alpha = ALPHA * 0.0001; */

        dot = 0.;
        mult = 0.;
        // get the dot product
        for (int i=0; i<D; i++) dot += cv[i]*pv[i];

        // get the multiplier
        if (dot > MAX_EXP) mult = (label - 1.)*alpha;
        else if (dot < -MAX_EXP) mult = (label - 0.)*alpha;
        else mult = (label-exp_table[(int)((dot + MAX_EXP) * (EXP_TABLE_SIZE / MAX_EXP / 2))])*alpha;

        /* debug("Looking at customer: %ld, product: %ld, dot: %g, mult: %g", custint, prodint, dot, mult); */
        // adjust the weights
        mymult = quantity*mult;
        for (int i=0; i<D; i++)  custupdate[i] = mymult*pv[i];
        for (int i=0; i<D; i++)  produpdate[i] = mymult*cv[i];

        label = 0.;
        for (int i=0; i<quantity*NEGS; i++) {
            long randp = (lqrand()%PRODS);
            randpv = product_vecs + D*randp;
            dot = 0.;
            // get the dot product
            for (int i=0; i<D; i++) dot += cv[i]*randpv[i];

            // get the multiplier
            if (dot > MAX_EXP) mult = (label - 1.)*alpha;
            else if (dot < -MAX_EXP) mult = (label - 0.)*alpha;
            else mult = (label-exp_table[(int)((dot + MAX_EXP) * (EXP_TABLE_SIZE / MAX_EXP / 2))])*alpha/(NEGS+0.);

            // adjust the weights
            for (int i=0; i<D; i++)  custupdate[i] += mult*randpv[i];
            for (int i=0; i<D; i++)  randpv[i] += mult*cv[i];
        }
        for (int i=0; i<quantity*NEGS; i++) {
            long randc = (lqrand()%CUSTS);
            randcv = customer_vecs + D*randc;
            dot = 0.;
            // get the dot product
            for (int i=0; i<D; i++) dot += cv[i]*randpv[i];

            // get the multiplier
            if (dot > MAX_EXP) mult = (label - 1.)*alpha;
            else if (dot < -MAX_EXP) mult = (label - 0.)*alpha;
            else mult = (label-exp_table[(int)((dot + MAX_EXP) * (EXP_TABLE_SIZE / MAX_EXP / 2))])*alpha/(NEGS+0.);

            // adjust the weights
            for (int i=0; i<D; i++)  produpdate[i] += mult*randcv[i];
            for (int i=0; i<D; i++)  randcv[i] += mult*pv[i];
        }




        // apply updates
        for (int i=0; i<D; i++) cv[i] += custupdate[i];
        for (int i=0; i<D; i++) pv[i] += produpdate[i];

        for (int i=0; i<D; i++)
            if (isnan(cv[i]) || isnan(pv[i])) {
                log_err("We've hit a nan!!!!, linenum=%ld, line=%s", linenum, dump);
                exit(1);
            }

        if (linenum%10000 == 0) { 
            double totcupdate = 0.;
            double totpupdate = 0.;
            for (int i=0; i<D; i++) totcupdate += custupdate[i]*custupdate[i];
            for (int i=0; i<D; i++) totpupdate += produpdate[i]*produpdate[i];
            double totcv = 0.;
            double totpv = 0.;
            for (int i=0; i<D; i++) totcv += cv[i]*cv[i];
            for (int i=0; i<D; i++) totpv += pv[i]*pv[i];
            double totcsize = 0.;
            double totpsize = 0.;
            for (long i=0; i<D*CUSTS; i++) totcsize += customer_vecs[i]*customer_vecs[i];
            for (long i=0; i<D*CUSTS; i++) totpsize += product_vecs[i]*product_vecs[i];

            now = clock();
            int seconds_remaining = (int)((now - start)/(CLOCKS_PER_SEC+0.)*LINES/(linenum+0.));
            int hours = seconds_remaining/(60*60);
            seconds_remaining -= hours*60*60;
            int minutes = seconds_remaining/60;
            seconds_remaining -= minutes*60;
            printf("%c%ldK lines processed. %.2f%% done. alpha=%g, num_customers=%ld, num_products=%ld. est time remaining %dh%2dm, csize=%g,%g,%g psize=%g,%g,%g             ", 
                    13, linenum/1000, linenum/(LINES+0.)*100., alpha, num_customers, num_products, hours, minutes, 
                    sqrt(totcupdate), sqrt(totcv), sqrt(totcsize), sqrt(totpupdate), sqrt(totpv), sqrt(totpsize) );
            fflush(stdout);
        }

        if (linenum%1000000 == 0) {
            FILE *fc = fopen(CUSTFILE,"w");
            print_customers(fc);
            fclose(fc);
            FILE *fp = fopen(PRODFILE,"w");
            print_products(fp);
            fclose(fp);
        }

        if (linenum>LINES-1) break;

        /* if (linenum > 100000) break; */
    }
    printf("\n");
}



int main(int argc, char *argv[]) {
    initialize();
    initialize_vectors();
    FILE *datfile = fopen(DATAFILE,"r");

    run(datfile);

    FILE *fc = fopen(CUSTFILE,"w");
    print_customers(fc);
    fclose(fc);
    FILE *fp = fopen(PRODFILE,"w");
    print_products(fp);
    fclose(fp);

    debug("done ish");
    fclose(datfile);
}
