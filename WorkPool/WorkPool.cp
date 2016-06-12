/*
 *  WorkPool.cp
 *  WorkPool
 *
 *  Created by Sprax Lines on 10/7/12.
 *  Copyright 2012 self-employed. All rights reserved.
 *
 */

#include <iostream>
#include "WorkPool.h"
#include "WorkPoolPriv.h"

void WorkPool::HelloWorld(const char * s)
{
	 WorkPoolPriv *theObj = new WorkPoolPriv;
	 theObj->HelloWorldPriv(s);
	 delete theObj;
};

void WorkPoolPriv::HelloWorldPriv(const char * s) 
{
	std::cout << s << std::endl;
};



//  I think a good design should isolate the queuing and work/task separately. In your design, both are tightly coupled. 
//  This design is good if you want to create separate pool for every type of work/task.
//
//  Another approach is to create a separate Work class containing the process function.  
//  Then your MyService will extend Work. And WorkQueue class will accept Work and by that means any derived class too. 
//  This approach is more generic in nature. So same worker queue can accept different type of work/task. Below code illustration will clear more.
//
//  Just to add this approach can also be used if you want to have different pool for different type of data. It is more flexible in nature.

#include <string>
#include <boost/thread.hpp>
#include <boost/pool/pool.hpp>


using namespace std;
using namespace boost;

struct Service1Data{
    string item_data;
};

template <typename T> 
class Workqueue{
    
private:
    pool<T> *pThreadPool;
    
public:
    Workqueue       (int);        
    void Start      (T);        
    void Schedule   (T);
    virtual bool Process(T) {return true;}
};

template <typename T> Workqueue<T>::Workqueue(int thread_count){
    pThreadPool = new pool<T>(thread_count);
}

template <typename T> void Workqueue<T>::Start(T data){
    pThreadPool->schedule(boost::bind(&Workqueue::Process,this, data));  
    ////pThreadPool->wait();
}

template <typename T> void Workqueue<T>::Schedule(T data){
    pThreadPool->schedule(boost::bind(&Workqueue::Process,this, data));    
}

/**** Refinition ?
struct Service1Data{
    string item_data;
};
********/

class MyService : public Workqueue<Service1Data> {    
public:
    MyService       (int);
    bool Process    (Service1Data);        
};

MyService::MyService(int workers) : Workqueue<Service1Data>(workers) {}

bool MyService::Process(Service1Data service_data){
    cout << "in process (" << service_data.item_data << ")" << endl;     
    return true;
}

template <typename T> 
class Work{
    T data; // contains the actual data to work on
public:
    Work(T data) : data(data) {} // constructor to init data
    virtual bool Process(T) {return false;} // returns false to tell process failed
    T getData() { return data; } // get the data
};

class MyWork : public Work<Service1Data>
{
public:
    MyWork (Service1Data data) :
    Work(data) {}
    bool Process (Service1Data); // Implement your work specific process func
};

bool MyWork::Process(Service1Data service_data){
    cout << "in process (" << service_data.item_data << ")" << endl;     
    return true;
}

class Workqueue{
    
private:
    boost::pool            *pThreadPool;
    
public:
    Workqueue       (int);        
    void Start      (Work);        
    void Schedule   (Work);
};

Workqueue::Workqueue(int thread_count){
    pThreadPool = new pool(thread_count);
}

MyWork::Workoid Workqueue::Start(MyWork::Work workToDo){
    pThreadPool->schedule(boost::bind(&Work::Process,this, workToDo.getData()));  
    pThreadPool->wait();
}

void Workqueue::Schedule(MyWork::Work data){
    pThreadPool->schedule(boost::bind(&Work::Process,this, workToDo.getData()));    
}

void dirk() 
{
    
    MyService *service1 = new MyService(5);
    
    Service1Data x;
    x.item_data = "testing";
    service1->Start(x);
    
    // will wait until no more work.
    delete service1;
}
