/*
 * My attempt at using the word2vec 
 * framework for doing the kaggle challenge
 * This version loads all of the training examples
 * into memory and then shuffles them
 * before training
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
/* #define DATAFILE "data/shuftransactions.csv"  // Note there are 142,221,150 lines */
#define DATAFILE "data/shuftransactions.csv"  // Note there are 142,221,150 lines
#define CUSTFILE "output/cust_vecs_nob10q"
#define PRODFILE "output/prod_vecs_nob10q"
#define WITH_ZLIB
#define MAX_STRING 1000
#define ARRAY_GROWTH 1000
#define D 256
#define CUSTS 311541    
#define PRODS 61318
#define LINES 349655789
#define INTERACTIONS 587467189
/* #define ALPHA 0.000065 */
/* #define ALPHA 0.025 */
/* #define ALPHA 0.0025 */
#define ALPHA 0.025
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
long linenum = 0;

real *custupdate;
real *produpdate;

typedef struct customer {
    long id;
} customer;

typedef struct product {
    long company, brand;
} product;

typedef struct purchase {
    customer* custp;
    product* prodp;
} purchase;

purchase* purchases;
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
    
    custupdate = calloc(D, sizeof(real));
    produpdate = calloc(D, sizeof(real));

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

    // initialize the purchases array
    purchases = malloc(INTERACTIONS * sizeof(purchase));
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


double inline getmult(double label, double dot) {
    double mult;
    // get the multiplier
    if (dot > MAX_EXP) mult = (label - 1.);
    else if (dot < -MAX_EXP) mult = (label - 0.);
    else mult = (label-exp_table[(int)((dot + MAX_EXP) * (EXP_TABLE_SIZE / MAX_EXP / 2))]);
    return mult;
}

// learn a single step
void onestep(purchase p) {
    memset(custupdate, 0, D*sizeof(real));
    memset(produpdate, 0, D*sizeof(real));
    real *cv;
    real *pv;
    real *randcv;
    real *randpv;
    real dot, mult;
    long customer_loc, product_loc;
    int label;

    linenum++;
    long id = p.custp->id;
    customer_loc =  find_customer(id);
    long company = p.prodp->company;
    long brand = p.prodp->brand;
    product_loc = find_product(company, brand);

    // Do the update
    label = 1;
    cv = customer_vecs + customer_loc*D;
    pv = product_vecs  + product_loc*D;
    alpha = ALPHA * (1. - linenum / (real)(LINES + 1.));
    if (alpha < ALPHA * 0.0001) alpha = ALPHA * 0.0001;

    /* debug("Looking at customer: %ld, product: %ld, dot: %g, mult: %g", customer_loc, product_loc, dot, mult); */
    // adjust the weights
    dot = 0.;
    for (int i=0; i<D; i++) dot += cv[i]*pv[i];
    mult = getmult(1., dot)*alpha;
    for (int i=0; i<D; i++)  custupdate[i] = mult*pv[i];
    for (int i=0; i<D; i++)  produpdate[i] = mult*cv[i];

    for (int i=0; i<NEGS; i++) {
        long randp = (lqrand()%PRODS);
        randpv = product_vecs + D*randp;
        // get the dot product
        dot = 0.;
        for (int i=0; i<D; i++) dot += cv[i]*randpv[i];
        // get the multiplier
        /* mult = getmult(0., dot)*alpha/(NEGS+0.); */
        mult = getmult(0., dot)*alpha;
        // adjust the weights
        for (int i=0; i<D; i++)  custupdate[i] += mult*randpv[i];
        for (int i=0; i<D; i++)  randpv[i] += mult*cv[i];
    }
    for (int i=0; i<NEGS; i++) {
        long randc = (lqrand()%CUSTS);
        randcv = customer_vecs + D*randc;
        // get the dot product
        dot = 0.;
        for (int i=0; i<D; i++) dot += randcv[i]*pv[i];
        // get the multiplier
        /* mult = getmult(0., dot)*alpha/(NEGS+0.); */
        mult = getmult(0., dot)*alpha;
        // adjust the weights
        for (int i=0; i<D; i++)  produpdate[i] += mult*randcv[i];
        for (int i=0; i<D; i++)  randcv[i] += mult*pv[i];
    }

    // apply updates
    for (int i=0; i<D; i++) cv[i] += custupdate[i];
    for (int i=0; i<D; i++) pv[i] += produpdate[i];

}

// shuffle all of the purchases
// using a fisher yates shuffle
void shuffle_purchases() {
    for (long i=INTERACTIONS-1; i>0; i--) {
        long j = lqrand() % i;
        purchase temp = purchases[j];
        purchases[j] = purchases[i];
        purchases[i] = temp;
    }
}

// learn some stuff
// shuffle the array of purchases
// and then consume all of it
void run() {
    clock_t start = clock();
    clock_t now;
    debug("Shuffling...");
    shuffle_purchases();
    debug("Finished shuffle.");
    linenum = 0;

    for (long i=0; i<INTERACTIONS; i++) {
        onestep(purchases[i]);

        if (i%10000==0) {
            now = clock();
            int seconds_remaining = (int)((now - start)/(CLOCKS_PER_SEC+0.)*INTERACTIONS/(linenum+0.));
            int hours = seconds_remaining/(60*60);
            seconds_remaining -= hours*60*60;
            int minutes = seconds_remaining/60;
            seconds_remaining -= minutes*60;
            printf("%c%ldK lines processed. %.2f%% done. est time remaining %dh%2dm             ", 
                    13, linenum/1000, linenum/(INTERACTIONS+0.)*100., hours, minutes);
            fflush(stdout);
        }
    }
}


// read in the input file
// and figure out pairs that
// we want to train on
void readpurchases(FILE* fp) {
    long long pk = 0;
    long id, company, brand, quantity;
    long customer_loc, product_loc;

    char dump[MAX_STRING+1];
    // get the first line
    fgets(dump, MAX_STRING, fp);

    while (!feof(fp)) {
        fgets(dump, MAX_STRING, fp);
        // file is in format <id,chain,dept,category,company,brand,date,productsize,productmeasure,purchasequantity,purchaseamount>
        sscanf(dump, "%ld,%*ld,%*ld,%*ld,%ld,%ld,%*25[^,],%*ld,%*30[^,],%ld,%*s", &id, &company, &brand, &quantity);
        customer_loc =  find_customer(id);
        if (customer_loc == -1) customer_loc = add_customer(id);
        product_loc = find_product(company, brand);
        if (product_loc == -1) product_loc = add_product(company, brand);
        customer* custp = &customers[customer_loc];
        product* prodp = &products[product_loc];

        for (int i=0; i<quantity; i++) {
            purchases[pk].custp = custp;
            purchases[pk].prodp = prodp;
            pk++;
        }
    }

}



int main(int argc, char *argv[]) {
    initialize();
    initialize_vectors();
    FILE *datfile = fopen(DATAFILE,"r");
    debug("Reading purchases...");
    readpurchases(datfile);
    debug("Finished read purchases.");

    for (int i=0; i<10; i++) {
        debug("ON RUN %i", i);
        run();
    }

    FILE *fc = fopen(CUSTFILE,"w");
    print_customers(fc);
    fclose(fc);
    FILE *fp = fopen(PRODFILE,"w");
    print_products(fp);
    fclose(fp);

    debug("done ish");
    fclose(datfile);
}
