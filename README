# coupon2vec

An attempt at hacking together a word2vec type version for doing
the Acquire Valued Shoppers Challenge challenge.

## Data

You should extract `transactions.csv` to `data/`.

The format is:

id,chain,dept,category,company,brand,date,productsize,productmeasure,purchasequantity,purchaseamount

 * id - customer id
 * chain - store chain
 * dept - dept of store
 * category - some thing in a store
 * company - the company that makes the product
 * brand - a particular product brand
 * purchasequantity - amount purchased.

A customer is identified by (id)
A product is (company, brand)

learn a bunch of vectors, v(id),  and w(company,brand), s.t.
    max over the dataset log p(w|v)

3/1/13 is where the coupon junk starts

The `coupon2vec.full` reads the unpacked data file, assuming its in /data/ and writes its results to output
The `coupon2vec` exectuble reads the compressed file
