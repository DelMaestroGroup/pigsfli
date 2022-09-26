#ifndef RNG_H
#define RNG_H
#include <random>
#include <memory>
#include <optional>
#include <functional>
#include "MersenneTwister.h"
#include "pcg_random.hpp"
#include "boost/random.hpp"
#include "boost/generator_iterator.hpp"


typedef unsigned long uint32;

class RNG
 	{
 	public:
 	
 	virtual ~RNG(){};
	virtual void seed(){};
	virtual void seed( const uint32 oneSeed ){};
	virtual uint32 randInt( const uint32 n ){return 0;}
	virtual uint32 randInt(){return 0;}
	virtual double rand(){return 0.0;}
	virtual double randExc(){return 0.0;}
	virtual double randExc(const double n){return 0.0;}
	virtual double randNorm( const double mean , const double stddev ){return 0.0;}
	virtual double randDblExc(){ return  0.0; };
	virtual double randDblExc( const double n ) { return 0.0; };
	
	// Saving and loading generator state
    virtual std::stringstream save()  {
    	std::stringstream stateStrStrm;
        stateStrStrm.str("");
        
        return stateStrStrm;
    }; 
    
    virtual void load(std::istream&) {};
        
    };
	

class MTFromPIMC:public RNG
	{

	public:
	MTFromPIMC();
	MTFromPIMC(const uint32 );
	void seed();
	void seed( const uint32 );
	uint32 randInt();
	uint32 randInt( const uint32 );
	double rand();
	double randExc();
	double randExc(const double );
	double randNorm( const double , const double );
	double randDblExc();
	double randDblExc( const double );
	
    // Saving and loading generator state
    std::stringstream save();  // to array of size SAVE
    void load(std::istream&);  // from such array
    
	private:
	MTRand random;

	};

class MTFromSTL:public RNG
	{
	public:
	MTFromSTL();
	MTFromSTL(const uint32 );
	void seed();
	void seed( const uint32 );
	uint32 randInt();
	uint32 randInt( const uint32 );
	double rand();
	double randExc();
	double randExc(const double );
	double randNorm( const double , const double );
	double randDblExc();
	double randDblExc( const double );
	
		    // Saving and loading generator state
    std::stringstream save();  // to array of size SAVE
    
    void load(std::istream&);  // from such array
	
	private:
	std::mt19937 generator;
	std::uniform_real_distribution<double> dis{};
	std::uniform_int_distribution<uint32> disInt{};
    std::normal_distribution<double> disNorm{};
};

class MTFromPCG:public RNG
	{
	public:
	MTFromPCG();
	MTFromPCG(const uint32 );
	void seed();
	void seed( const uint32 );
	uint32 randInt();
	uint32 randInt( const uint32 );
	double rand();
	double randExc();
	double randExc(const double );
	double randNorm( const double , const double );
	double randDblExc();
	double randDblExc( const double );
	
		    // Saving and loading generator state
    std::stringstream save();  // to array of size SAVE
    
    void load(std::istream&);  // from such array
	
	private:
	pcg32 generator;
	std::uniform_real_distribution<double> dis{};
	std::uniform_int_distribution<uint32> disInt{};
    std::normal_distribution<double> disNorm{};
};

class MTFromBOOST:public RNG
	{
	public:
	MTFromBOOST();
	MTFromBOOST(const uint32 );
	void seed();
	void seed( const uint32 );
	uint32 randInt();
	uint32 randInt( const uint32 );
	double rand();
	double randExc();
	double randExc(const double );
	double randNorm( const double , const double );
	double randDblExc();
	double randDblExc( const double );
	
    std::stringstream save();  // to array of size SAVE
    
    void load(std::istream&);
	
	private:
	boost::random::mt19937 generator;
	boost::random::uniform_real_distribution<double> dis{};
	boost::random::uniform_int_distribution<uint32> disInt{};
	boost::random::normal_distribution<double> disNorm{};
	
	};

/**
This functions selects whether to call the constructor with a seed or not depending on 
whether the optional seed s is given

@tparam T the type that we want to create
@param s an optional seed
@return An object of type T.
*/
template <typename T>
inline std::unique_ptr<RNG> lambdatemplate(std::optional<uint32> s = std::nullopt)
{
    if (s)
	return std::make_unique<T>(*s);
    else
	return std::make_unique<T>();
};

// Here we set up a jump table such that we can jump upon an integer to the respective function.
static std::function<std::unique_ptr<RNG>(std::optional<uint32>) > typeswitcher[] {
    [](std::optional<uint32> s = std::nullopt){return lambdatemplate<MTFromPIMC>(s);},
    [](std::optional<uint32> s = std::nullopt){return lambdatemplate<MTFromSTL>(s);},
    [](std::optional<uint32> s = std::nullopt){return lambdatemplate<MTFromBOOST>(s);},
    [](std::optional<uint32> s = std::nullopt){return lambdatemplate<MTFromPCG>(s);}
    };

/**************************************************************************/
    inline std::unique_ptr<RNG> GetRNG( int sourceID )
    {
	return typeswitcher[sourceID - 1] (std::optional<uint32>() );
    };


    inline std::unique_ptr<RNG> GetRNG( int sourceID, const uint32 seed  )
    {
	return typeswitcher[sourceID - 1] (seed);
    };

//###################################################
    inline RNG GetRNGFromPtr( RNG &random )
    {
        return random ;
    };

/************************************************************/  

#endif
