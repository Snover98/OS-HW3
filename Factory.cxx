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
    pthread_cond_init(&returning_open_condition, NULL);
    pthread_cond_init(&factory_open_condition, NULL);
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
    pthread_cond_destroy(&returning_open_condition);
    pthread_cond_destroy(&factory_open_condition);
    pthread_cond_destroy(&companies_condition);
    pthread_cond_destroy(&thieves_map_condition);
    pthread_cond_destroy(&production_map_condition);
    pthread_cond_destroy(&companies_map_condition);
    pthread_cond_destroy(&simples_map_condition);
}

void Factory::startProduction(int num_products, Product* products,unsigned int id){
    if(num_products <= 0) return;

    pthread_t new_thread;

    //make wrapper struct
    wrapper_struct s = {.factory = this, .products = products, .num_products = num_products};

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, prodWrapper , &s);

    //insert the new thread to the map;
    threads_map[id] = new_thread;
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

    //unlock factory
    pthread_mutex_unlock(&factory_lock);

    //signal the next thread that it can take the lock
    factoryFreeSignal();
}

void Factory::finishProduction(unsigned int id){
    //TODO: dont we need to lock the mutex before ending the prod_thread? maybe the same thread is still producing

    //save thread id
    pthread_t prod_thread = threads_map[id];

    //remove it from the map
    threads_map.erase(id);

    //join the thread
    pthread_join(prod_thread, NULL);
}

void Factory::startSimpleBuyer(unsigned int id){
    pthread_t simple_thread;

    wrapper_struct s = {.factory = this};

    pthread_create(&simple_thread, NULL, simpleWrapper , &s);

    threads_map[id] = simple_thread;

    finishSimpleBuyer(id);
}

//@TODO Wrapper for correct usage of pthread_create
void *simpleWrapper(void* s_struct){
    wrapper_struct* s;
    s = static_cast<wrapper_struct*>(s_struct);
    pthread_exit((void*)s->factory->tryBuyOne());
}

int Factory::tryBuyOne(){
    Product bought;
    int isLocked = pthread_mutex_trylock(&factory_lock) == 0 ? 1 : 0;
    int isBought=0;

    if(isLocked){
        if(!available_products.empty() && thieves_counter == 0 &&
               companies_counter == 0 && is_factory_open) {
            //buy and remove the oldest product
            bought = available_products.front();
            available_products.pop_front();
            isBought = 1;
        }

        //unlock the factory
        pthread_mutex_unlock(&factory_lock);

        //signal the next thread that it can take the lock
        factoryFreeSignal();

        //return value is the bought product's id
        if(isBought) {
            return bought.getId();
        }
    }

    return -1;
}

int Factory::finishSimpleBuyer(unsigned int id){
    //@TODO maybe we should use threads_map.at(id) instead so we'll find errors easier
    pthread_t simple_thread = threads_map[id];

    int buy_result;

    //remove it from the map
    threads_map.erase(id);

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
    while(available_products.size() < num_products || thieves_counter > 0 || !is_factory_open){
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
    while(thieves_counter > 0 || !is_returning_open){
        pthread_cond_t current_condition = is_returning_open ? returning_open_condition : companies_condition;
        pthread_cond_wait(&current_condition, &factory_lock);
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
    pthread_t company_thread = threads_map[id];

    int num_returned = 0;

    //remove it from the map
    threads_map.erase(id);

    //decrease counter by 1
    companies_counter--;

    //join the thread
    pthread_join(company_thread, (void**)&num_returned);

    return num_returned;
}

void Factory::startThief(int num_products,unsigned int fake_id){
    //make new thread in the map
    pthread_t &new_thread = threads_map[fake_id];

    //make wrapper struct
    wrapper_struct s = {.factory = this, .fake_id = fake_id, .num_products = num_products};

    //update companies counter
    thieves_counter++;

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, thiefWrapper , &s);
}

void *thiefWrapper(void* s_struct){
    wrapper_struct* s;
    s = static_cast<wrapper_struct*>(s_struct);
    //return the theft value
    pthread_exit((void*)s->factory->stealProducts(s->num_products, s->fake_id));
}

int Factory::stealProducts(int num_products,unsigned int fake_id){
    //lock factory
    pthread_mutex_lock(&factory_lock);
    while(!is_factory_open){
        pthread_cond_wait(&factory_open_condition, &factory_lock);
    }

    //calculate how many products will be stolen
    int num_stolen = std::min(num_products, static_cast<int>(available_products.size()));
    //steal the products
    std::list<Product> stolen = takeOldestProducts(num_stolen);

    //signal the next thread that it can take the lock
    factoryFreeSignal();
    //free the factory lock
    pthread_mutex_unlock(&factory_lock);

    //take the reported thefts' lock
    pthread_mutex_lock(&stolen_lock);

    //report the thefts
    for (auto& product : stolen){
        thefts.emplace_back(product, static_cast<int>(fake_id));
    }

    //unlock reported thefts' lock
    pthread_mutex_unlock(&stolen_lock);

    return num_stolen;
}

int Factory::finishThief(unsigned int fake_id){
    //save the thread
    pthread_t thief_thread = threads_map[fake_id];

    int num_stolen = 0;

    //remove it from the map
    threads_map.erase(fake_id);

    //decrease counter by 1
    thieves_counter--;

    //join the thread
    pthread_join(thief_thread, (void**)&num_stolen);

    return num_stolen;
}

void Factory::closeFactory(){
    is_factory_open = false;
}

void Factory::openFactory(){
    is_factory_open = true;
    pthread_cond_broadcast(&factory_open_condition);
    factoryFreeSignal();
}

void Factory::closeReturningService(){
    is_returning_open = false;
}

void Factory::openReturningService(){
    is_returning_open = true;
    pthread_cond_broadcast(&returning_open_condition);
}

std::list<std::pair<Product, int>> Factory::listStolenProducts(){
    //take the reported thefts' lock
    pthread_mutex_lock(&stolen_lock);

    //copy the thefts' list
    std::list<std::pair<Product, int>> thefts_copy = thefts;

    //unlock the reported thefts' lock
    pthread_mutex_unlock(&stolen_lock);

    //return the copy
    return thefts_copy;
}

std::list<Product> Factory::listAvailableProducts(){
    //lock the factory lock
    pthread_mutex_lock(&factory_lock);

    //copy the available products list
    std::list<Product> available_products_copy = available_products;

    //unlock the reported thefts' lock
    pthread_mutex_unlock(&factory_lock);

    //return the copy
    return available_products_copy;
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