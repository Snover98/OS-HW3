#include "Factory.h"

struct wrapper_struct{
    Factory* factory;
    int num_products;
    Product* products;
    int min_value;
    unsigned int fake_id;

    wrapper_struct(Factory* factory, int num_products, Product* products, int min_value = 0, unsigned int fake_id = 0):
            factory(factory), num_products(num_products), products(products), min_value(min_value), fake_id(fake_id){}
    wrapper_struct(Factory* factory, int num_products = 0, int min_value = 0, unsigned int fake_id = 0):
            factory(factory), num_products(num_products), products(NULL), min_value(min_value), fake_id(fake_id){}
    wrapper_struct(Factory* factory, int num_products, unsigned int fake_id):
            factory(factory), num_products(num_products), products(NULL), min_value(0), fake_id(fake_id){}

    ~wrapper_struct(){
        factory = NULL;
        products = NULL;
    }

};

void *prodWrapper(void* s_struct);
void *simpleWrapper(void* s_struct);
void *companyWrapper(void* s_struct);
void *thiefWrapper(void* s_struct);

class isAboveMinValueFunctor{
    int min_value;
public:
    isAboveMinValueFunctor(int min_value) : min_value(min_value){}
    bool operator()(Product& prod) { return (prod.getValue() >= min_value); }
};


Factory::Factory() : is_returning_open(true), is_factory_open(true), thieves_counter(0), waiting_for_return_counter(0),
                     waiting_thieves_counter(0), waiting_companies_counter(0),
                     threads_map(new std::unordered_map<unsigned int, pthread_t>),
                     available_products(new std::list<Product>), thefts(new std::list<std::pair<Product, int>>){
    //init mutex lock
    INIT_MUTEX_LOCK(factory_lock);
    INIT_MUTEX_LOCK(stolen_lock);
    INIT_MUTEX_LOCK(thieves_counter_lock);
    INIT_MUTEX_LOCK(returning_service_lock);

//    pthread_mutex_init(&factory_lock, NULL);
//    pthread_mutex_init(&stolen_lock, NULL);
//    pthread_mutex_init(&thieves_counter_lock, NULL);
//    pthread_mutex_init(&returning_service_lock, NULL);

    //init condition vars
    pthread_cond_init(&returning_open_condition, NULL);
    pthread_cond_init(&factory_open_condition, NULL);
    pthread_cond_init(&companies_condition, NULL);
}

Factory::~Factory(){
    //destroy mutex lock
    pthread_mutex_destroy(&factory_lock);
    pthread_mutex_destroy(&stolen_lock);
    pthread_mutex_destroy(&thieves_counter_lock);
    pthread_mutex_destroy(&returning_service_lock);

    //destroy condition vars
    pthread_cond_destroy(&returning_open_condition);
    pthread_cond_destroy(&factory_open_condition);
    pthread_cond_destroy(&companies_condition);

    //delete lists and map
    delete threads_map;
    delete available_products;
    delete thefts;
}

void Factory::startProduction(int num_products, Product* products,unsigned int id){
    //insert the new thread to the map;
    pthread_t& new_thread = (*threads_map)[id];

    //make wrapper struct
    wrapper_struct* s = new wrapper_struct(this, num_products, products);

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, prodWrapper , s);
}

void *prodWrapper(void* s_struct){
    wrapper_struct s = *static_cast<wrapper_struct*>(s_struct);
    delete static_cast<wrapper_struct*>(s_struct);

    s.factory->produce(s.num_products, s.products);
    pthread_exit(NULL);
}

void Factory::produce(int num_products, Product* products){
    //lock factory
    pthread_mutex_lock(&factory_lock);

    //add all products to factory
    for(int i=0; i<num_products; i++){
        available_products->push_back(products[i]);
    }

    //signal the next thread that it can take the lock
    factoryFreeSignal();

    //unlock factory
    pthread_mutex_unlock(&factory_lock);
}

void Factory::finishProduction(unsigned int id){
    //save thread id
    pthread_t prod_thread = (*threads_map)[id];

    //remove it from the map
    threads_map->erase(id);

    //join the thread
    pthread_join(prod_thread, NULL);
}

void Factory::startSimpleBuyer(unsigned int id){
    //insert the new thread to the map;
    pthread_t &simple_thread = (*threads_map)[id];

    //make wrapper struct
    wrapper_struct* s = new wrapper_struct(this);

    //create new thread using wrapper functions
    pthread_create(&simple_thread, NULL, simpleWrapper , s);
}

//Wrapper for correct usage of pthread_create
void *simpleWrapper(void* s_struct){
    //cast wrapper to correct struct
    wrapper_struct s = *static_cast<wrapper_struct*>(s_struct);
    delete static_cast<wrapper_struct*>(s_struct);
    //create pointer to return value (allocate with new)
    int* retval;
    //call try tryBuyOne
    retval = new int(s.factory->tryBuyOne());
    //return buy result
    pthread_exit((void*)retval);
}

int Factory::tryBuyOne(){
    //create a product with an id of -1 for the default return value
    Product bought = Product(-1, -1);

    if(pthread_mutex_trylock(&factory_lock) == 0){
        if(!available_products->empty() && is_factory_open) {
            //buy and remove the oldest product
            bought = available_products->front();
            available_products->pop_front();
        }

        //signal the next thread that it can take the lock
        factoryFreeSignal();

        //unlock the factory
        pthread_mutex_unlock(&factory_lock);
    }

    return bought.getId();
}

int Factory::finishSimpleBuyer(unsigned int id){
    //@TODO maybe we should use threads_map->at(id) instead so we'll find errors easier
    pthread_t simple_thread = (*threads_map)[id];

    //create pointer to result address and variable to copy to result to before freeing the memory
    int* buy_result_address = NULL;
    int buy_result;

    //remove it from the map
    threads_map->erase(id);

    //join the thread and save the result pointer in buy_result_address
    pthread_join(simple_thread, (void**)&buy_result_address);
    //copy the result to a local variable
    buy_result = *buy_result_address;
    //free the memory we allocated for the result
    delete buy_result_address;

    return buy_result;
}

void Factory::startCompanyBuyer(int num_products, int min_value,unsigned int id){
    //insert the new thread to the map;
    pthread_t &new_thread = (*threads_map)[id];

    //make wrapper struct
    wrapper_struct* s = new wrapper_struct(this, num_products, min_value);

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, companyWrapper , s);
}

void *companyWrapper(void* s_struct){
    //cast wrapper to correct struct
    wrapper_struct s = *static_cast<wrapper_struct*>(s_struct);
    delete static_cast<wrapper_struct*>(s_struct);

    int* num_returned;

    //buy products
    std::list<Product> bought_products = s.factory->buyProducts(s.num_products);

    //remove all products that should not be returned from the list
    isAboveMinValueFunctor pred = isAboveMinValueFunctor(s.min_value);
    bought_products.remove_if(pred);
    //save the number of products we will return, that will be our return value
    num_returned = new int(static_cast<int>(bought_products.size()));

    //return the products (id parameter is deprecated and can be passed any value)
    s.factory->returnProducts(bought_products, 0);

    pthread_exit((void*)num_returned);
}

std::list<Product> Factory::buyProducts(int num_products){
    //lock the factory
    pthread_mutex_lock(&factory_lock);
    pthread_mutex_lock(&thieves_counter_lock);
    //wait until there are enough products and no thieves are around
    while(available_products->size() < num_products || thieves_counter > 0 || !is_factory_open){
        //this company is now waiting
        ++waiting_companies_counter;
        pthread_mutex_unlock(&thieves_counter_lock);
        pthread_cond_wait(&companies_condition, &factory_lock);
        pthread_mutex_lock(&thieves_counter_lock);
        //this company is no longer waiting
        --waiting_companies_counter;
    }
    pthread_mutex_unlock(&thieves_counter_lock);

    //take the num_products oldest products
    std::list<Product> bought_products = takeOldestProducts(num_products);

    //signal that the factory is unlocked
    factoryFreeSignal();

    //unlock the factory
    pthread_mutex_unlock(&factory_lock);

    return bought_products;
}

void Factory::returnProducts(std::list<Product> products,unsigned int id){
    //if there are no products to return, exit the function
    if(products.empty()){
        return;
    }

    //lock the returning service
    pthread_mutex_lock(&returning_service_lock);
    //if the returning service is closed, wait until it opens
    if(!is_returning_open){
        //this company is waiting, so increase the counter
        ++waiting_for_return_counter;
        pthread_cond_wait(&returning_open_condition, &returning_service_lock);
        //this company is no longer waiting, so decrease the counter
        --waiting_for_return_counter;
    }
    //unlock the returning service
    pthread_mutex_unlock(&returning_service_lock);

    //lock the factory
    pthread_mutex_lock(&factory_lock);
    pthread_mutex_lock(&thieves_counter_lock);
    //wait until no thieves are around
    while(thieves_counter > 0){
        //this company is now waiting
        ++waiting_companies_counter;
        pthread_mutex_unlock(&thieves_counter_lock);
        pthread_cond_wait(&companies_condition, &factory_lock);
        pthread_mutex_lock(&thieves_counter_lock);
        //this company is no longer waiting
        --waiting_companies_counter;
    }
    pthread_mutex_unlock(&thieves_counter_lock);

    //return the products
    available_products->splice(available_products->end(), products);

    //signal that the factory is unlocked
    factoryFreeSignal();

    //unlock the factory
    pthread_mutex_unlock(&factory_lock);
}

int Factory::finishCompanyBuyer(unsigned int id){
    //save the thread
    pthread_t company_thread = (*threads_map)[id];

    //create pointer to result address and variable to copy to result to before freeing the memory
    int* num_returned_address = NULL;
    int num_returned = 0;

    //remove it from the map
    threads_map->erase(id);

    //join the thread and save the result pointer in num_returned_address
    pthread_join(company_thread, (void**)&num_returned_address);
    //copy the result to a local variable
    num_returned = *num_returned_address;
    //free the memory we allocated for the result
    delete num_returned_address;

    return num_returned;
}

void Factory::startThief(int num_products,unsigned int fake_id){
    //make new thread in the map
    pthread_t &new_thread = (*threads_map)[fake_id];

    //make wrapper struct
    wrapper_struct* s = new wrapper_struct(this, num_products, fake_id);

    //update companies counter
    pthread_mutex_lock(&thieves_counter_lock);
    ++thieves_counter;
    pthread_mutex_unlock(&thieves_counter_lock);

    //create new thread using wrapper functions
    pthread_create(&new_thread, NULL, thiefWrapper , s);
}

void *thiefWrapper(void* s_struct){
    //cast wrapper to correct struct
    wrapper_struct s = *static_cast<wrapper_struct*>(s_struct);
    delete static_cast<wrapper_struct*>(s_struct);

    //create pointer to return value (allocate with new)
    int* retval;

    //call stealProducts
    retval = new int(s.factory->stealProducts(s.num_products, s.fake_id));

    //return the theft value
    pthread_exit((void*)retval);
}

int Factory::stealProducts(int num_products,unsigned int fake_id){
    //lock factory
    pthread_mutex_lock(&factory_lock);

    //take the reported thefts' lock
    pthread_mutex_lock(&stolen_lock);

    if(!is_factory_open){
        //thief is now waiting
        ++waiting_thieves_counter;
        //wait until the factory opens
        pthread_mutex_unlock(&stolen_lock);
        pthread_cond_wait(&factory_open_condition, &factory_lock);
        pthread_mutex_lock(&stolen_lock);
        //thief is no longer waiting
        --waiting_thieves_counter;
    }

    //calculate how many products will be stolen
    int num_stolen = std::min(num_products, static_cast<int>(available_products->size()));
    //steal the products
    std::list<Product> stolen = takeOldestProducts(num_stolen);

    //the thief is no longer in the factory so we should decrease the counter
    pthread_mutex_lock(&thieves_counter_lock);
    --thieves_counter;
    pthread_mutex_unlock(&thieves_counter_lock);

    //signal the next thread that it can take the lock
    factoryFreeSignal();

    //free the factory lock
    pthread_mutex_unlock(&factory_lock);

    //report the thefts
    for (auto& product : stolen){
        thefts->emplace_back(product, static_cast<int>(fake_id));
    }

    //unlock reported thefts' lock
    pthread_mutex_unlock(&stolen_lock);

    return num_stolen;
}

int Factory::finishThief(unsigned int fake_id){
    //save the thread
    pthread_t thief_thread = (*threads_map)[fake_id];

    //create pointer to result address and variable to copy to result to before freeing the memory
    int* num_stolen_address = NULL;
    int num_stolen = 0;

    //remove it from the map
    threads_map->erase(fake_id);

    //join the thread and save the result pointer in buy_result_address
    pthread_join(thief_thread, (void**)&num_stolen_address);
    //copy the result to a local variable
    num_stolen = *num_stolen_address;
    //free the memory we allocated for the result
    delete num_stolen_address;

    return num_stolen;
}

void Factory::closeFactory(){
    if(is_factory_open){
        pthread_mutex_lock(&factory_lock);
        is_factory_open = false;
        pthread_mutex_unlock(&factory_lock);
    }
}

void Factory::openFactory(){
    if(!is_factory_open){
        pthread_mutex_lock(&factory_lock);
        is_factory_open = true;
        if(waiting_thieves_counter > 0){
            pthread_cond_broadcast(&factory_open_condition);
        }
        factoryFreeSignal();
        pthread_mutex_unlock(&factory_lock);
    }
}

void Factory::closeReturningService(){
    if(is_returning_open){
        pthread_mutex_lock(&returning_service_lock);
        is_returning_open = false;
        pthread_mutex_unlock(&returning_service_lock);
    }
}

void Factory::openReturningService(){
    if(!is_returning_open){
        pthread_mutex_lock(&returning_service_lock);
        is_returning_open = true;
        if(waiting_for_return_counter > 0){
            pthread_cond_broadcast(&returning_open_condition);
        }
        pthread_mutex_unlock(&returning_service_lock);
    }
}

std::list<std::pair<Product, int>> Factory::listStolenProducts(){
    //take the reported thefts' lock
    pthread_mutex_lock(&stolen_lock);

    //copy the thefts' list
    std::list<std::pair<Product, int>> thefts_copy = *thefts;

    //unlock the reported thefts' lock
    pthread_mutex_unlock(&stolen_lock);

    //return the copy
    return thefts_copy;
}

std::list<Product> Factory::listAvailableProducts(){
    //lock the factory lock
    pthread_mutex_lock(&factory_lock);

    //copy the available products list
    std::list<Product> available_products_copy = *available_products;

    //unlock the reported thefts' lock
    pthread_mutex_unlock(&factory_lock);

    //return the copy
    return available_products_copy;
}

void Factory::factoryFreeSignal(){
    //lock thieves_counter_lock so we can look at thieves_counter,
    //waiting_companies_counter is under factory_lock which is always locked before calling this function
    pthread_mutex_lock(&thieves_counter_lock);
    //if there are now thieves waiting for the factory lock, and there are companies waiting for it
    //and the factory is open broadcast to them
    if(is_factory_open && thieves_counter == 0 && waiting_companies_counter > 0){
        pthread_cond_broadcast(&companies_condition);
    }
    pthread_mutex_unlock(&thieves_counter_lock);
}
//factory is always locked when this function is called
std::list<Product> Factory::takeOldestProducts(int num_products){
    if(num_products == 0){
        return std::list<Product>();
    }

    std::list<Product> new_list;

    //create an iterator pointing to the last product we want to take
    auto last_to_take = available_products->begin();
    std::advance(last_to_take, num_products);

    //take all of the elements from the beginning of available_products to the num_products element
    new_list.splice(new_list.begin(), *available_products, available_products->begin(), last_to_take);

    return new_list;
}