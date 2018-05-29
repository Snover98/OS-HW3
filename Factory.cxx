#include "Factory.h"

typedef struct{
    Factory* factory;
    int num_products;
    Product* products;
} prod_struct;

typedef struct{
    Factory* factory;
} simple_struct;

typedef struct{
    Factory* factory;
    int num_products;
    int min_value;
} company_struct;

typedef struct{
    Factory* factory;
    int num_products;
    unsigned int fake_id;
} thief_struct;

void *prodWrapper(void*);
void *simpleWrapper(void*);
void *companyWrapper(void*);
void *thiefWrapper(void*);


Factory::Factory() : is_returning_open(true), is_factory_open(true), thieves_counter(0), companies_counter(0){
    //init mutex lock
    //@TODO maybe should not be NULL
    pthread_mutex_init(&factory_lock, NULL);
    pthread_mutex_init(&map_lock, NULL);

    //init condition vars
    pthread_cond_init(&thieves_condition, NULL);
    pthread_cond_init(&production_condition, NULL);
    pthread_cond_init(&companies_condition, NULL);
    pthread_cond_init(&thieves_map_condition, NULL);
    pthread_cond_init(&production_map_condition, NULL);
    pthread_cond_init(&companies_map_condition, NULL);
}

Factory::~Factory(){
    //destroy mutex lock
    pthread_mutex_destroy(&factory_lock);
    pthread_mutex_destroy(&map_lock);

    //destroy condition vars
    pthread_cond_destroy(&thieves_condition);
    pthread_cond_destroy(&production_condition);
    pthread_cond_destroy(&companies_condition);
    pthread_cond_destroy(&thieves_map_condition);
    pthread_cond_destroy(&production_map_condition);
    pthread_cond_destroy(&companies_map_condition);
}

void Factory::startProduction(int num_products, Product* products,unsigned int id){

}

void Factory::produce(int num_products, Product* products){

}

void Factory::finishProduction(unsigned int id){

}

void Factory::startSimpleBuyer(unsigned int id){
    pthread_t* new_thread;
    new_thread = (pthread_t*)malloc(sizeof(pthread_t));
    threads_map[id] = new_thread;

    simple_struct s = {.factory = this};

    pthread_create(new_thread, NULL, simpleWrapper , &s);
}

//@TODO Wrapper for correct usage of pthread_create
void *simpleWrapper(void* s_struct){
    simple_struct* s;
    s = static_cast<simple_struct*>(s_struct);
    pthread_exit((void*)s->factory->tryBuyOne());
}

int Factory::tryBuyOne(){
    //try lock return value
    int lock_res;
    //value that we will return, default is -1 (failure)
    int retval = -1;
    Product bought;

    lock_res = pthread_mutex_trylock(&factory_lock);
    if(lock_res == 0 && !available_products.empty() && thieves_counter == 0 && companies_counter == 0 && is_factory_open){
        //buy and remove the oldest product
        bought = available_products.front();
        available_products.pop_front();

        //return value is the bought product's id
        retval = bought.getId();
    }

    if(lock_res == 0){
        pthread_mutex_unlock(&factory_lock);
    }

    return retval;
}

int Factory::finishSimpleBuyer(unsigned int id){

    return -1;
}

void Factory::startCompanyBuyer(int num_products, int min_value,unsigned int id){

}

std::list<Product> Factory::buyProducts(int num_products){

    return std::list<Product>();
}

void Factory::returnProducts(std::list<Product> products,unsigned int id){

}

int Factory::finishCompanyBuyer(unsigned int id){
    return 0;
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
