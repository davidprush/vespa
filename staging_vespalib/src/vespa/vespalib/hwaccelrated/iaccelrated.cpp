// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// Copyright (C) 2003 Fast Search & Transfer ASA
// Copyright (C) 2003 Overture Services Norway AS

#include <vespa/vespalib/hwaccelrated/iaccelrated.h>
#include <vespa/vespalib/hwaccelrated/generic.h>
#include <vespa/vespalib/hwaccelrated/sse2.h>
#include <vespa/vespalib/hwaccelrated/avx.h>
#include <vespa/vespalib/hwaccelrated/avx2.h>
#include <assert.h>

namespace vespalib {

namespace hwaccelrated {

namespace {

class Factory {
public:
    virtual ~Factory() { }
    virtual IAccelrated::UP create() const = 0;
};

class GenericFactory :public Factory{
public:
    virtual IAccelrated::UP create() const { return IAccelrated::UP(new GenericAccelrator()); }
};

class Sse2Factory :public Factory{
public:
    virtual IAccelrated::UP create() const { return IAccelrated::UP(new Sse2Accelrator()); }
};

class AvxFactory :public Factory{
public:
    virtual IAccelrated::UP create() const { return IAccelrated::UP(new AvxAccelrator()); }
};

class Avx2Factory :public Factory{
public:
    virtual IAccelrated::UP create() const { return IAccelrated::UP(new Avx2Accelrator()); }
};

template<typename T>
void verifyAccelrator(const IAccelrated & accel)
{
    const size_t testLength(127);
    T * a = new T[testLength];
    T * b = new T[testLength];
    for (size_t j(0); j < 0x20; j++) {
        T sum(0);
        for (size_t i(j); i < testLength; i++) {
            a[i] = b[i] = i;
            sum += i*i;
        }
        T hwComputedSum(accel.dotProduct(&a[j], &b[j], testLength - j));
        assert(sum == hwComputedSum);
    }
    delete [] a;
    delete [] b;
}

class RuntimeVerificator
{
public:
    RuntimeVerificator();
};

RuntimeVerificator::RuntimeVerificator()
{
   GenericAccelrator generic;
   verifyAccelrator<float>(generic); 
   verifyAccelrator<double>(generic); 
   verifyAccelrator<int32_t>(generic); 
   verifyAccelrator<int64_t>(generic); 

   IAccelrated::UP thisCpu(IAccelrated::getAccelrator());
   verifyAccelrator<float>(*thisCpu); 
   verifyAccelrator<double>(*thisCpu); 
   verifyAccelrator<int32_t>(*thisCpu); 
   verifyAccelrator<int64_t>(*thisCpu); 
   
}

class Selector
{
public:
    Selector() __attribute__((noinline));
    IAccelrated::UP create() { return _factory->create(); }
private:
    std::unique_ptr<Factory> _factory;
};

Selector::Selector() :
    _factory(new GenericFactory())
{
    __builtin_cpu_init ();
    if (__builtin_cpu_supports("avx2")) {
        _factory.reset(new Avx2Factory());
    } else if (__builtin_cpu_supports("avx")) {
        _factory.reset(new AvxFactory());
    } else if (__builtin_cpu_supports("sse2")) {
        _factory.reset(new Sse2Factory());
    }
}

}

static Selector _G_selector;

RuntimeVerificator _G_verifyAccelrator;


IAccelrated::UP
IAccelrated::getAccelrator()
{
    return _G_selector.create();
}

}
}
