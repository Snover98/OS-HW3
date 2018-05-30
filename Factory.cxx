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


Factory::Factory() : is_returning_open(true), is_factory_open(true), thieves_counter(0), companies_counter(0), {
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
    pthread_t new_thread;
//    new_thread = (pthread_t*)malloc(sizeof(pthread_t));
    threads_map[id] = new_thread;

    //tell the next thread that it can lock the map
//    mapFreeSignal();
    //give up the map lock
//    pthread_mutex_unlock(&map_lock);

    wrapper_struct s = {.factory = this, .products = products, .num_products = num_products};

    pthread_create(&threads_map[id], NULL, prodWrapper , &s);

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

}

void Factory::startSimpleBuyer(unsigned int id){
    //if it can't take the map lock, return
//    pthread_mutex_lock(&map_lock);

    pthread_t new_thread;
//    new_thread = (pthread_t*)malloc(sizeof(pthread_t));
    threads_map[id] = new_thread;

    //tell the next thread that it can lock the map
//    mapFreeSignal();
    //give up the map lock
//    pthread_mutex_unlock(&map_lock);

    wrapper_struct s = {.factory = this};

    pthread_create(&threads_map[id], NULL, simpleWrapper , &s);
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