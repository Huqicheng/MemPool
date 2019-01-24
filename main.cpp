#include <iostream>
#include <MemPool.h>
using namespace std;

MemPool::CMemoryPool *g_ptrMemPool = NULL;

int main() {
    MemPool::CMemoryPool pool;
    g_ptrMemPool = new MemPool::CMemoryPool() ;

    if(g_ptrMemPool) delete g_ptrMemPool ;
}