#ifndef KOBJARRAY_H_INCLUDED
#define KOBJARRAY_H_INCLUDED
#include "kmalloc.h"

template< class T >
class KObjArray 
{
public:
    KObjArray(int cap)
    {
        this->cap = 0;
        this->size = 0;
        obj = NULL;
        set_capacity(cap);
    };
    ~KObjArray() {
        if (obj) {
            free(obj);
        }
    };
    T* begin()
    {
        return obj;
    }
    T* operator[](int index) {
        if (index >= 0 && index < size)
            return obj+index;
        else
            return NULL;
    }
    T* get_new()
    {
        if (guarantee(size + 1)!=0) {
            return NULL;
        }
        T* new_obj = this->obj+size;
        size++;
        return new_obj;
    }
    T* new_first()
    {
        size++;
        return obj;
    }
    int get_size()
    {
        return size;
    }
private:
    int set_capacity(int num_obj)
    {
        if (cap < num_obj) {
            void* new_array = realloc(obj, num_obj * sizeof(T));
            if (!new_array) {
                return -1;
            }
            obj = (T *)new_array;
            cap = num_obj;
        }
        return 0;
    }
    int guarantee(int num_obj)
    {
        if (cap - size >= num_obj) {
            return 0;
        }
        return set_capacity(num_obj + size + 8);
    }
    T* obj;
    int size;
    int cap;
};

#endif