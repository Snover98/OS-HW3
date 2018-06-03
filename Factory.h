#ifndef FACTORY_H_
#define FACTORY_H_

#include <pthread.h>
#include <list>
#include <unordered_map>
#include "Product.h"

class Factory{
private:
    //map of threads by id
    std::unordered_map<unsigned int, pthread_t> threads_map;

    //list of all available products from oldest produced to newest
    std::list<Product> available_products;

    /*list of all the filed thefts in the order they happened, each element in the list is a pair of
    stolen Product and the fake_id of the thief who stole it.*/
    std::list<std::pair<Product, int>> thefts;

    //lock for the factory
    pthread_mutex_t factory_lock;
    pthread_mutex_t stolen_lock;
    pthread_mutex_t thieves_counter_lock;

    //condition vars for the different thread types, used by the factory lock
    pthread_cond_t returning_open_condition;
    pthread_cond_t factory_open_condition;
    pthread_cond_t companies_condition;

    //condition vars for the different thread types, used by the factory lock
//    pthread_cond_t thieves_map_condition;
//    pthread_cond_t production_map_condition;
//    pthread_cond_t companies_map_condition;
//    pthread_cond_t simples_map_condition;

    //counters for thread types that need them
    int thieves_counter;
    int companies_counter;
//    int waiting_thieves_counter;
//    int waiting_companies_counter;

    //flag that is true when the returning service is open
    bool is_returning_open;

    //flag that is true when the factory is open
    bool is_factory_open;

    //calls the correct condition vars when map is free
//    void mapFreeSignal();
    //calls the correct condition vars when factory is free
    void factoryFreeSignal();
    //takes the num_products oldest products from available_products and returns them as a list (same order)
    std::list<Product> takeOldestProducts(int num_products);

public:
    Factory();
    ~Factory();
    
    void startProduction(int num_products, Product* products, unsigned int id);
    void produce(int num_products, Product* products);
    void finishProduction(unsigned int id);
    
    void startSimpleBuyer(unsigned int id);
    int tryBuyOne();
    int finishSimpleBuyer(unsigned int id);

    void startCompanyBuyer(int num_products, int min_value,unsigned int id);
    std::list<Product> buyProducts(int num_products);
    void returnProducts(std::list<Product> products,unsigned int id);
    int finishCompanyBuyer(unsigned int id);

    void startThief(int num_products,unsigned int fake_id);
    int stealProducts(int num_products,unsigned int fake_id);
    int finishThief(unsigned int fake_id);

    void closeFactory();
    void openFactory();
    
    void closeReturningService();
    void openReturningService();
    
    std::list<std::pair<Product, int>> listStolenProducts();
    std::list<Product> listAvailableProducts();

};
#endif // FACTORY_H_
