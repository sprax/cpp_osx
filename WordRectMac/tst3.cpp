
#include <stdio.h>

#include "TracNode.hpp"

int retson()
{
	
	return 1;
}

template <typename MapT>
class AA
{
public:
	int mPub;
	MapT mT;
protected:
	int mPrtA;
	int mPrtB;
	int mPrtC;
	int mPrtD;
	int mPrtE;
private:
	int mPrv;
	
public:
	class BB : public AA<MapT>
	{
		void printPub() { 
			printf("pub:%d\n", AA<MapT>::mPub);
		}
		
		void setPrt(int a) {
			mPrtA = a;
		}
	};
	
};

template <typename U>
class BB : public AA<U>
{
	void printPub() { 
		printf("pub:%d\n", AA<U>::pub);
	}
	
	void setPrts(int a, int b, int c, int d, int e, U u) 
	{
		AA<U>::mPrtA = a;
		BB<U>::mPrtB = b;
		this->mPrtC = c;
		
#ifdef _MBCS
		using AA<U>::mPrtD;
		mPrtD = d;
		
		using BB<U>::mPrtE;
		mPrtE = e;
#endif
		
		this->mT = u;
		
		
		
		
	}
};


template <typename X, typename Y>
class Mult
{
    X mX;
    Y mY;
public:
    Mult(X x, Y y) {
        mX = x;
        mY = y;
    }
    Mult(Mult &);						// Prevent pass-by-value by not defining copy constructor.
    Mult& operator=(const Mult&);		// Prevent assignment by not defining this operator.
    ~Mult() { }   


    X fn(Y k)
    {
        return mX*k/mY;
    }

};

void test_Mult() {
    Mult<double, int> mult(3.14159, 2);
    double per = mult.fn(2);
    printf("mult result %f\n", per);
}


class Rye {
public:
    Rye() {
        printf("Wry ask Rye?\n");
    }
};

template <uint N>
class Ary : public Rye
{
    int mA[N];

public:
    Ary(int value, int dirk) {
        for (int n = N; --n > 0; )
            mA[n] = value;
    }

};

template <uint N>
class NotAry : public Ary<N>
{
public:
    NotAry(int size, int val) 
        : Ary<N>(size, val)
    { }
};

class MySize {
public:
    const int mS;
    MySize(int size) : mS(size) { }
};

static void test_NotAry()
{
    NotAry<7> notAry(5, 11);

//    const MySize mySize(8);
//    NotAry<mySize.mS> notSiz(66, 6666);

    const int six = 6;
    NotAry<six> notSix(66, 6666);

}


template <typename T, int begK>
class Mem {
    T *mData;
public:
    Mem(T size) {
        mData = new T[size] - begK;
    }
    ~Mem() {
        delete [] (mData + begK);
    }
};

void test_Mem()
{
    Mem<int, 7> mem(42); 
}

