#include "Factory.h"

typedef struct{
    Factory* factory;
    int num_products;
    Product* products;
    int min_value;
    unsigned int fake_id;
} wrapper_struct;

void *prodWrapper(void* s_struct);
void *simpleWrapper(void* s_struct);
void *companyWrapper(void* s_struct);
void *thiefWrapper(void* s_struct);

class isAboveMinValueFunctor{
    int min_value;
public:
    isAboveMinValueFunctor(int min_value) : min_value(min_value){}
    bool isAboveMinValue(Product& prod) { return (prod.getValue() >= min_value); }
};


Factory::Factory() : is_returning_open(true), is_factory_open(true), thieves_counter(0), companies_counter(0){
    //init mutex lock
    //@TODO maybe should not be NULL
    pthread_mutex_init(&factory_lock, NULL);
    pthread_mutex_init(&stolen_lock, NULL);

    //init condition vars
    pthread_cond_init(&thieves_condition, NULL);
    pthread_cond_init(&production_condition, NULL);
    pthread_cond_init(&companies_condition, NULL);
    pthread_cond_init(&thieves_map_condition, NULL);
    pthread_cond_init(&production_map_condition, NULL);
    pthread_cond_init(&companies_map_condition, NULL);
    pthread_cond_init(&simples_map_condition, NULL);
}

Factory::~Factory(){
    //destroy mutex lock
    pthread_mutex_destroy(&factory_lock);
    pthread_mutex_destroy(&stolen_lock);

    //destroy condition vars
    pthread_cond_destroy(&thieves_condition);
    pthread_cond_destroy(&production_condition);
    pthread_cond_destroy(&companies_condition);
    pthread_cond_destroy(&thieves_map_condition);
    pthread_cond_destroy(&production_map_condition);
    pthread_cond_destroy(&companies_map_condition);
    pthread_cond_destroy(&simples_map_condition);
}

void Factory::startProduction(int num_products, Product* products,unsigned int id){
    //make new thread in the map
    pthread_t &new_thread = threads_map[id];

    //make wrapper struct
    wrapper_struct s = {.factory = this, .products = products, .num_products = num_products};

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, prodWrapper , &s);
}

void *prodWrapper(void* s_struct){
    wrapper_struct* s;
    s = static_cast<wrapper_struct*>(s_struct);
    s->factory->produce(s->num_products, s->products);
    pthread_exit(NULL);
}

void Factory::produce(int num_products, Product* products){
    //lock factory
    pthread_mutex_lock(&factory_lock);

    //add all products to factory
    for(int i=0; i<num_products; i++){
        available_products.push_back(products[i]);
    }

    //signal the next thread that it can take the lock
    factoryFreeSignal();

    //unlock factory
    pthread_mutex_unlock(&factory_lock);
}

void Factory::finishProduction(unsigned int id){
    //save thread id
    pthread_t prod_thread = threads_map[id];

    //remove it from the map
    threads_map.erase(id);

    //join the thread
    pthread_join(prod_thread, NULL);
}

void Factory::startSimpleBuyer(unsigned int id){
    //if it can't take the map lock, return
//    pthread_mutex_lock(&map_lock);

    pthread_t &new_thread = threads_map[id];
//    new_thread = (pthread_t*)malloc(sizeof(pthread_t));

    //tell the next thread that it can lock the map
//    mapFreeSignal();
    //give up the map lock
//    pthread_mutex_unlock(&map_lock);

    wrapper_struct s = {.factory = this};

    pthread_create(&new_thread, NULL, simpleWrapper , &s);
}

//@TODO Wrapper for correct usage of pthread_create
void *simpleWrapper(void* s_struct){
    wrapper_struct* s;
    s = static_cast<wrapper_struct*>(s_struct);
    pthread_exit((void*)s->factory->tryBuyOne());
}

int Factory::tryBuyOne(){
    Product bought;

    if(pthread_mutex_trylock(&factory_lock) == 0 && !available_products.empty() && thieves_counter == 0 &&
            companies_counter == 0 && is_factory_open){
        //buy and remove the oldest product
        bought = available_products.front();
        available_products.pop_front();

        //signal the next thread that it can take the lock
        factoryFreeSignal();

        //unlock the factory
        pthread_mutex_unlock(&factory_lock);

        //return value is the bought product's id
        return bought.getId();
    }

    return -1;
}

int Factory::finishSimpleBuyer(unsigned int id){
    //wait for the map lock
//    pthread_mutex_lock(&map_lock);

    //@TODO maybe we should use threads_map.at(id) instead so we'll find errors easier
    pthread_t simple_thread = threads_map[id];

    int buy_result = 0;

    //remove it from the map
    threads_map.erase(id);
    //tell the next thread that it can lock the map
//    mapFreeSignal();
    //unlock the map
//    pthread_mutex_unlock(&map_lock);
    //join the thread
    pthread_join(simple_thread, (void**)&buy_result);

    return buy_result;
}

void Factory::startCompanyBuyer(int num_products, int min_value,unsigned int id){
    //make new thread in the map
    pthread_t &new_thread = threads_map[id];

    //make wrapper struct
    wrapper_struct s = {.factory = this, .min_value = min_value, .num_products = num_products};

    //update companies counter
    companies_counter++;

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, companyWrapper , &s);

}

void *companyWrapper(void* s_struct){
    int num_returned;
    wrapper_struct* s;
    s = static_cast<wrapper_struct*>(s_struct);

    //buy products
    std::list<Product> bought_products = s->factory->buyProducts(s->num_products);

    //remove all products that should not be returned from the list
    isAboveMinValueFunctor pred = isAboveMinValueFunctor(s->min_value);
    bought_products.remove_if(pred.isAboveMinValue);
    //save the number of products we will return, that will be our return value
    num_returned = static_cast<int>(bought_products.size());

    //return the products (id parameter is deprecated and can be passed any value)
    s->factory->returnProducts(bought_products, 0);

    pthread_exit((void*)num_returned);
}

std::list<Product> Factory::buyProducts(int num_products){
    //lock the factory
    pthread_mutex_lock(&factory_lock);
    //wait until there are enough products and no thieves are around
    while(available_products.size() < num_products || thieves_counter > 0){
        pthread_cond_wait(&companies_condition, &factory_lock);
    }

    //take the num_products oldest products
    std::list<Product> bought_products = takeOldestProducts(num_products);

    //signal that the factory is unlocked
    factoryFreeSignal();

    //unlock the factory
    pthread_mutex_unlock(&factory_lock);

    return bought_products;
}

void Factory::returnProducts(std::list<Product> products,unsigned int id){
    //lock the factory
    pthread_mutex_lock(&factory_lock);
    //wait until no thieves are around
    while(thieves_counter > 0){
        pthread_cond_wait(&companies_condition, &factory_lock);
    }

    //return the products
    available_products.splice(available_products.end(), products);

    //signal that the factory is unlocked
    factoryFreeSignal();

    //unlock the factory
    pthread_mutex_unlock(&factory_lock);
}

int Factory::finishCompanyBuyer(unsigned int id){
    //save the thread
    pthread_t simple_thread = threads_map[id];

    int num_returned = 0;

    //remove it from the map
    threads_map.erase(id);

    //decrease counter by 1
    companies_counter--;

    //join the thread
    pthread_join(simple_thread, (void**)&num_returned);

    return num_returned;
}

void Factory::startThief(int num_products,unsigned int fake_id){

}

int Factory::stealProducts(int num_products,unsigned int fake_id){

    return 0;
}

int Factory::finishThief(unsigned int fake_id){

    return 0;
}

void Factory::closeFactory(){

}

void Factory::openFactory(){

}

void Factory::closeReturningService(){

}

void Factory::openReturningService(){

}

std::list<std::pair<Product, int>> Factory::listStolenProducts(){
    return std::list<std::pair<Product, int>>();
}

std::list<Product> Factory::listAvailableProducts(){
    return std::list<Product>();
}

//void Factory::mapFreeSignal(){
//    if(waiting_thieves_counter > 0){
//        pthread_cond_signal(&thieves_map_condition);
//    } else if(waiting_companies_counter > 0){
//        pthread_cond_signal(&companies_map_condition);
//    }
//
//    pthread_cond_signal(&production_map_condition);
//}

void Factory::factoryFreeSignal(){
    if(thieves_counter == 0){
        pthread_cond_broadcast(&companies_condition);
    }
}

std::list<Product> Factory::takeOldestProducts(int num_products){
    if(num_products == 0){
        return std::list<Product>();
    }

    std::list<Product> new_list;

    //create an iterator pointing to the last product we want to take
    auto last_to_take = available_products.begin();
    std::advance(last_to_take, num_products-1);

    //take all of the elements from the beginning of available_products to the num_products element
    new_list.splice(new_list.begin(), available_products, available_products.begin(), last_to_take);

    return new_list;
}