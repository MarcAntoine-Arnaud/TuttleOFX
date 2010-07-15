#include "MemoryPool.hpp"
#include <boost/throw_exception.hpp>
#include <algorithm>

namespace tuttle {
namespace host {
namespace core {

IPool::~IPool() {}

class PoolData : public IPoolData
{
private:
	PoolData(); ///< No default Ctor
	PoolData( const PoolData& ); ///< No copy Ctor
	friend class MemoryPool;

public:
	PoolData( IPool& pool, const size_t size )
		: _pool( pool ),
		_id( _count++ ),
		_reservedSize( size ),
		_size( size ),
		_pData( new char[size] ),
		_refCount( 0 )
	{}

	~PoolData()
	{
		delete [] _pData;
	}

public:
	bool operator==( const PoolData& other ) const
	{
		return _id == other._id;
	}

	virtual void addRef();
	virtual void release();

	virtual char*        data()               { return _pData; }
	virtual const char*  data() const         { return _pData; }
	virtual const size_t size() const         { return _size; }
	virtual const size_t reservedSize() const { return _reservedSize; }

private:
	static size_t _count; ///< unique id generator
	IPool& _pool; ///< ref to the owner pool
	const size_t _id; ///< unique id to identify one memory data
	const size_t _reservedSize; ///< memory allocated
	size_t _size; ///< memory requested
	char* const _pData; ///< own the data
	int _refCount; ///< counter on clients currently using this data
};

void intrusive_ptr_add_ref( IPoolData* pData )
{
	pData->addRef();
}

void intrusive_ptr_release( IPoolData* pData )
{
	pData->release();
}

size_t PoolData::_count = 0;

void PoolData::addRef()
{
	if( ++_refCount == 1 )
		_pool.referenced( this );
}

void PoolData::release()
{
	if( --_refCount == 0 )
		_pool.released( this );
}

MemoryPool::MemoryPool( const size_t maxSize )
	: _memoryAuthorized( maxSize )
{}

MemoryPool::~MemoryPool()
{}

void MemoryPool::referenced( PoolData* pData )
{
//	DataList::iterator it = std::find( _dataUnused.begin(), _dataUnused.end(), pData );
	DataList::iterator it = _dataUnused.find( pData );
	if( it != _dataUnused.end() )
	{
		_dataUnused.erase( it );
	}
	else // a really new data
	{
		_allDatas.push_back( pData );
		_dataMap[pData->data()] = pData;
	}
	_dataUsed.insert( pData );
}

void MemoryPool::released( PoolData* pData )
{
	_dataUsed.erase( pData );
	_dataUnused.insert( pData );
}

namespace  {

struct DataFitSize : public std::unary_function<PoolData*, void>
{
	DataFitSize( size_t size )
		: _size( size ),
		_bestMatchDiff( ULONG_MAX ),
		_pBestMatch( NULL )
	{}

	void operator()( PoolData* pData )
	{
		const size_t dataSize = pData->reservedSize();

		if( _size > dataSize )
			return;
		const size_t diff = dataSize - _size;
		if( diff >= _bestMatchDiff )
			return;
		_bestMatchDiff = diff;
		_pBestMatch    = pData;
	}

	PoolData* bestMatch()
	{
		return _pBestMatch;
	}

private:
	const size_t _size;
	size_t _bestMatchDiff;
	PoolData* _pBestMatch;
};

}  // namespace

boost::intrusive_ptr<IPoolData> MemoryPool::allocate( const size_t size ) throw( std::bad_alloc, std::length_error )
{
	// checking within unused data
	PoolData* const pData = std::for_each( _dataUnused.begin(), _dataUnused.end(), DataFitSize( size ) ).bestMatch();

	if( pData != NULL )
	{
		pData->_size = size;
		return pData;
	}
	if( size > getAvailableMemorySize() )
	{
		std::stringstream s;
		s << "MemoryPool can't allocate size:" << size << " because memorySizeAvailable=" << getAvailableMemorySize();
		BOOST_THROW_EXCEPTION( std::length_error( s.str() ) );
	}
	return new PoolData( *this, size );
}

namespace  {

size_t accumulateReservedSize( const size_t& sum, const IPoolData* pData )
{
	return sum + pData->reservedSize();
}

size_t accumulateWastedSize( const size_t& sum, const IPoolData* pData )
{
	return sum + ( pData->reservedSize() - pData->size() );
}

}  // namespace

size_t MemoryPool::getUsedMemorySize() const
{
	return std::accumulate( _dataUsed.begin(), _dataUsed.end(), 0, &accumulateReservedSize );
}

size_t MemoryPool::getAllocatedMemorySize() const
{
	return getUsedMemorySize() + std::accumulate( _dataUnused.begin(), _dataUnused.end(), 0, &accumulateReservedSize );
}

size_t MemoryPool::getMaxMemorySize() const
{
	return _memoryAuthorized;
}

size_t MemoryPool::getAvailableMemorySize() const
{
	return getMaxMemorySize() - getUsedMemorySize();
}

size_t MemoryPool::getWastedMemorySize() const
{
	return std::accumulate( _dataUsed.begin(), _dataUsed.end(), 0, std::ptr_fun( &accumulateWastedSize ) );
}

void MemoryPool::clear( size_t size )
{}

void MemoryPool::clearOne()
{}

void MemoryPool::clearAll()
{
	_dataUnused.clear();
}

}
}
}
