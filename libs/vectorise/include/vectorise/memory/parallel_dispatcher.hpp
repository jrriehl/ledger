#pragma once

#include "vectorise/memory/details.hpp"
#include "vectorise/memory/range.hpp"
#include "vectorise/platform.hpp"
#include "vectorise/vectorise.hpp"

#include<type_traits>

namespace fetch {
namespace memory {

template< typename T>
class ConstParallelDispatcher {
public:    
  typedef T type;

  enum { vector_size = platform::VectorRegisterSize<type>::value };     
  typedef typename vectorize::VectorRegister<type, vector_size>
  vector_register_type;
  typedef vectorize::VectorRegisterIterator<type, vector_size>
  vector_register_iterator_type;
    
  ConstParallelDispatcher(type *ptr, std::size_t const&size)
    : pointer_(ptr), size_(size)
  {
  }
    
  type Reduce(vector_register_type(*vector_reduction)(vector_register_type const&,
      vector_register_type const&) ) const {
    vector_register_type a, b(type(0));
    vector_register_iterator_type iter(this->pointer(), this->size());

    std::size_t N = this->size();
    
    for (std::size_t i = 0; i < N; i +=  vector_register_type::E_BLOCK_COUNT) {
      iter.Next(a);
      b = vector_reduction(a, b);      
    }

    return reduce(b) ;
  }


  template< typename F >
  type SumReduce(F &&vector_reduce) {
    vector_register_iterator_type self_iter(this->pointer(), this->size());    
    vector_register_type c( type(0) ), tmp, self;
    
    std::size_t N = this->size();
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      self_iter.Next(self);
      tmp = vector_reduce(self);     
      c = c + tmp;
    }
    
    return reduce(c);
  }

  
  template< typename... Args >
  type
  SumReduce(typename details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::signature_type &&kernel,
    Args &&... args) {

    vector_register_type regs[sizeof...(args)];
    vector_register_iterator_type iters[sizeof...(args)];
    InitializeVectorIterators(0, this->size(), iters, std::forward<Args>(args)...);

    vector_register_iterator_type self_iter(this->pointer(), this->size());    
    vector_register_type c( type(0) ), tmp, self;
    
    std::size_t N = this->size();
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      self_iter.Next(self);
      tmp = details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::Apply(self, regs, std::move(kernel));
      c = c + tmp;
    }

    return reduce(c);  
  }

  template< typename F >
  type
  SumReduce(TrivialRange const &range, F &&vector_reduce) {

    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    int SIMDSize = STU - SFL;
          
    vector_register_iterator_type self_iter(this->pointer() + std::size_t(SFL), std::size_t( SIMDSize ));    
    vector_register_type tmp, self;

    type ret = 0;    
    
    if( SFL != SF) {
      self_iter.Next(self);

      tmp = vector_reduce(self);     

      int Q = vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()));
      for(int i=0; i < vector_register_type::E_BLOCK_COUNT; ++i) {
        if(Q <= i ) {
          ret +=  first_element( tmp );
        }
        tmp = shift_elements_right( tmp );        
      }
    }

        
    vector_register_type c( type(0) );
    
    for (int i = SF; i < ST; i += vector_register_type::E_BLOCK_COUNT) {    

      self_iter.Next(self);
      tmp = vector_reduce(self);      
      c = c + tmp;
    }
    
    for(std::size_t i=0; i < vector_register_type::E_BLOCK_COUNT ; ++i)  {
      ret += first_element(c);      
      c = shift_elements_right( c );
    }

    if( STU != ST) {
      self_iter.Next(self);
      tmp = vector_reduce(self);  

      int Q = (int(range.to()) -  ST - 1) ;      
      for(int i=0; i <= Q; ++i) {
        
        ret += first_element( tmp );
        tmp = shift_elements_right( tmp );
      }     
    }    
    
    return ret;    
  }


  

  template< typename... Args >
  type
  SumReduce(TrivialRange const &range, typename details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::signature_type &&reduce,
    Args &&... args) {

    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    int SIMDSize = STU - SFL;
      
    
    vector_register_type regs[sizeof...(args)];
    vector_register_iterator_type iters[sizeof...(args)];

    InitializeVectorIterators(std::size_t(SFL), std::size_t( SIMDSize ), iters, std::forward<Args>(args)...);

    vector_register_iterator_type self_iter(this->pointer() + std::size_t(SFL), std::size_t( SIMDSize ));    
    vector_register_type tmp, self;

    type ret = 0;    
    
    if( SFL != SF) {
      
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      self_iter.Next(self);
      
      tmp = details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::Apply(self, regs, std::move(reduce));

      int Q = vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()));
      for(int i=0; i < vector_register_type::E_BLOCK_COUNT; ++i) {
        if(Q <= i ) {
          ret +=  first_element( tmp );
        }
        tmp = shift_elements_right( tmp );        
      }
    }

        
    vector_register_type c( type(0) );
    
    for (int i = SF; i < ST; i += vector_register_type::E_BLOCK_COUNT) {    
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      self_iter.Next(self);
      tmp = details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::Apply(self, regs, std::move(reduce));
      c = c + tmp;
    }
    
    for(std::size_t i=0; i < vector_register_type::E_BLOCK_COUNT ; ++i)  {
      ret += first_element(c);      
      c = shift_elements_right( c );
    }

    if( STU != ST) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      self_iter.Next(self);
      tmp = details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::Apply(self, regs, std::move(reduce));      

      int Q = (int(range.to()) -  ST - 1) ;      
      for(int i=0; i <= Q; ++i) {
        
        ret += first_element( tmp );
        tmp = shift_elements_right( tmp );
      }     
    }    
    
    return ret;    
  }

  

  template< typename... Args >
  type
  ProductReduce(typename details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::signature_type &&kernel,
    Args &&... args) {

    vector_register_type regs[sizeof...(args)];
    vector_register_iterator_type iters[sizeof...(args)];
    InitializeVectorIterators(0, this->size(), iters, std::forward<Args>(args)...);

    vector_register_iterator_type self_iter(this->pointer(), this->size());    
    vector_register_type c( type(0) ), tmp, self;
    
    std::size_t N = this->size();
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      self_iter.Next(self);
      tmp = details::MatrixReduceFreeFunction<vector_register_type >::template Unroll< Args...>::Apply(self, regs, std::move(kernel));
      c = c * tmp;
    }

    return reduce(c); 
  }

  
  

  type Reduce(TrivialRange const &range,
    vector_register_type(*vector_reduction)(vector_register_type const&,
      vector_register_type const&) ) const {
    

    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    int SIMDSize = STU - SFL;
      

    vector_register_type a, b(type(0));
    vector_register_iterator_type iter(this->pointer() + SFL, std::size_t( SIMDSize ));

    
    if(SFL != SF) {
      iter.Next(a);
      a = vector_zero_below_element( a, vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()))  );
      b = vector_reduction(a, b);  
    }
    
        
    for (int i = SF; i < ST; i +=  vector_register_type::E_BLOCK_COUNT) {
      iter.Next(a);
      b = vector_reduction(a, b);      
    }

    if(STU != ST) {
      iter.Next(a);
      a = vector_zero_above_element( a, (int(range.to()) -  ST - 1)  );
      b = vector_reduction(a, b);  
    }

    // TODO: Make reduction tree / Wallace tree
    type ret = 0;
    for(std::size_t i=0; i < vector_register_type::E_BLOCK_COUNT ; ++i)  {
      vector_register_type c(ret);
      c = vector_reduction(c, b);
      ret = first_element(c);      
      b = shift_elements_right( b );      
    }

    return ret;
  }

  
  type Reduce( type(*register_reduction)(type const&, type const&)) const {
    type ret = 0;
    std::size_t N = this->size();
    
    for (std::size_t i = 0; i < N; ++i)
    {
      ret = register_reduction( ret,  pointer_[i] );
    }

    return ret;
  }

  
  type Reduce(TrivialRange const &range, type(*register_reduction)(type const&, type const&)) const {
    type ret = 0;
    for (std::size_t i = range.from(); i < range.to(); ++i)
    {
      ret = register_reduction( ret,  pointer_[i] );
    }
    return ret;
  }



    


  

  type const *pointer() const { return pointer_; }    
  std::size_t const &size() const { return size_; }
  std::size_t &size() { return size_; }  
protected:
  type *pointer() { return pointer_; }
  type *pointer_;
  std::size_t size_;

  template< typename G, typename... Args >
  static void InitializeVectorIterators(std::size_t const &offset, std::size_t const &size,
    vector_register_iterator_type *iters,
    G &next,
    Args... remaining)
  {

    assert(next.size() >= offset + size); 
    (*iters) = vector_register_iterator_type( next.pointer() + offset,  size );
    InitializeVectorIterators(offset, size, iters + 1, remaining... );
  }
    
  template< typename G >
  static void InitializeVectorIterators(std::size_t const &offset, std::size_t const &size,
    vector_register_iterator_type *iters, G &next)
  {
    assert(next.padded_size() >= offset + size);
    (*iters) = vector_register_iterator_type( next.pointer() + offset,  size );
  }
    
  static void InitializeVectorIterators(std::size_t const &offset, std::size_t const &size, vector_register_iterator_type *iters)
  {  
  }


  template< typename G, typename... Args >
  static void SetPointers(std::size_t const &offset, std::size_t const &size,
    type const **regs,
    G &next,
    Args... remaining)
  {

    assert(next.size() >= offset + size);
    *regs = next.pointer() + offset;
    SetPointers(offset, size, regs + 1, remaining... );
  }
    
  template< typename G >
  static void SetPointers(std::size_t const &offset, std::size_t const &size,
    type const **regs, G &next)
  {
    assert(next.size() >= offset + size);
    *regs = next.pointer() + offset;    
  }
    
  static void SetPointers(std::size_t const &offset, std::size_t const &size, vector_register_iterator_type *iters)
  {  
  }

  
  

};

template< typename T>
class ParallelDispatcher : public ConstParallelDispatcher< T >{
public:    
  typedef T type;

  typedef ConstParallelDispatcher< T > super_type;
    
  enum { vector_size = platform::VectorRegisterSize<type>::value };     
  typedef typename vectorize::VectorRegister<type, vector_size>
  vector_register_type;
  typedef vectorize::VectorRegisterIterator<type, vector_size>
  vector_register_iterator_type;
    
  ParallelDispatcher(type *ptr, std::size_t const&size)
    : super_type(ptr, size)
  {
  }


  template< typename F >
  void Apply(F&& apply ) {
    vector_register_type c;


    std::size_t N = this->size();
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      assert( i < this->size());
      apply(c);
     
      c.Store(this->pointer() + i);
    }
  }


  template< typename... Args >
  void
  Apply(typename details::MatrixApplyFreeFunction<vector_register_type ,
    void >::template Unroll< Args...>::signature_type &&apply,
    Args &&... args) {

    vector_register_type regs[sizeof...(args)], c;
    vector_register_iterator_type iters[sizeof...(args)];
    ConstParallelDispatcher<T>::InitializeVectorIterators(0, this->size(), iters, std::forward<Args>(args)...);



    std::size_t N = this->size();
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      
      details::MatrixApplyFreeFunction<vector_register_type , void >::template Unroll< Args...>::Apply(regs, std::move(apply), c);
      
      c.Store(this->pointer() + i);
    }
  }



  template< typename F >
  void Apply(TrivialRange const &range, F &&apply) {
    
    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    
    vector_register_type c;

    if( SFL != SF) {
      apply(c);
        
      int Q = vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()));
      for(int i=0; i < vector_register_type::E_BLOCK_COUNT; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        if(Q <= i ) {
          this->pointer()[ SFL + i ] = value;
        }        
      }     
    }

    

    for (int i = SF; i < ST; i += vector_register_type::E_BLOCK_COUNT) {
      apply(c);      
      c.Store(this->pointer() + i);
    }

    if( STU != ST) {
      apply(c);
      
      int Q = (int(range.to()) -  ST - 1) ;      
      for(int i=0; i <= Q; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        this->pointer()[ ST + i ] = value;
      }     
    }    
  }


  
  template< typename... Args >
  void Apply(TrivialRange const &range, typename details::MatrixApplyFreeFunction<vector_register_type ,
    void >::template Unroll< Args...>::signature_type &&apply,
    Args &&... args) {
    
    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    int SIMDSize = STU - SFL;
      
    
    vector_register_type regs[sizeof...(args)], c;
    vector_register_iterator_type iters[sizeof...(args)];

    ConstParallelDispatcher<T>::InitializeVectorIterators(std::size_t(SFL), std::size_t( SIMDSize ), iters, std::forward<Args>(args)...);

    if( SFL != SF) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      details::MatrixApplyFreeFunction<vector_register_type , void >::template Unroll< Args...>::Apply(regs, std::move(apply), c);

      int Q = vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()));
      for(int i=0; i < vector_register_type::E_BLOCK_COUNT; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        if(Q <= i ) {

          this->pointer()[ SFL + i ] = value;
        }        
      }     
    }

    

    for (int i = SF; i < ST; i += vector_register_type::E_BLOCK_COUNT) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      details::MatrixApplyFreeFunction<vector_register_type , void >::template Unroll< Args...>::Apply(regs, std::move(apply), c);
      
      c.Store(this->pointer() + i);
    }

    if( STU != ST) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      details::MatrixApplyFreeFunction<vector_register_type , void >::template Unroll< Args...>::Apply(regs, std::move(apply), c);

      int Q = (int(range.to()) -  ST - 1) ;      
      for(int i=0; i <= Q; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        this->pointer()[ ST + i ] = value;
      }     
    }    
  }
  


  template< class C, typename... Args >
  void Apply(C const &cls,
    typename details::MatrixApplyClassMember<C, vector_register_type,
    void >::template Unroll< Args...>::signature_type &&fnc,
    Args &&... args) {
    
    vector_register_type regs[sizeof...(args)], c;
    vector_register_iterator_type iters[sizeof...(args)];
    std::size_t N = super_type::size();
    ConstParallelDispatcher<T>::InitializeVectorIterators(0, N, iters, std::forward<Args>(args)...);
    for (std::size_t i = 0; i < N; i += vector_register_type::E_BLOCK_COUNT) {
      assert( i < N );
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);
      details::MatrixApplyClassMember<C, vector_register_type , void >::template Unroll< Args...>::Apply(regs, cls, std::move(fnc), c);
      
      c.Store(this->pointer() + i);
    }

  }

  template< class C, typename... Args >
  void Apply(C const &cls,
    typename details::MatrixApplyClassMember<C, type,
    void >::template Unroll< Args...>::signature_type &&fnc,
    Args &&... args) {
    
    constexpr std::size_t R = sizeof...(args);
    type const* regs[R];
    type c;
    
    std::size_t N = super_type::size();
    
    ConstParallelDispatcher<T>::SetPointers(0, N, regs, std::forward<Args>(args)...);
    

    for (std::size_t i = 0; i < N; ++i)
    {
      
      details::MatrixApplyClassMember<C, type , void >::template Unroll< Args...>::Apply(regs, cls, std::move(fnc), c);

      this->pointer()[i] = c;
      
      for(std::size_t j=0; j < R; ++j)
      {
        ++regs[j];
      }

    }    
  }
  
  template< class C, typename... Args >
  typename std::enable_if<std::is_same<  decltype(&C::operator()),  typename details::MatrixApplyClassMember<C, type, void >::template Unroll< Args...>::signature_type >::value, void >::type
  Apply(C const &cls, Args &&... args) 
  {
    return Apply(cls, &C::operator(), std::forward<Args>(args)...);    
  }

  template< class C, typename... Args >
  typename std::enable_if<std::is_same<  decltype(&C::operator()),  typename details::MatrixApplyClassMember<C, vector_register_type, void >::template Unroll< Args...>::signature_type >::value, void >::type
  Apply(C const &cls, Args &&... args) 
  {

    return Apply(cls, &C::operator(), std::forward<Args>(args)...);    
  }
  
    
  template< class C, typename... Args >
  void Apply(TrivialRange const &range, C const &cls,
    typename details::MatrixApplyClassMember<C, vector_register_type,
    void >::template Unroll< Args...>::signature_type &fnc,
    Args &&... args) {

    int SFL = int( range.SIMDFromLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int SF = int( range.SIMDFromUpper< vector_register_type::E_BLOCK_COUNT >() );
    int ST = int( range.SIMDToLower< vector_register_type::E_BLOCK_COUNT >() );
    
    int STU = int( range.SIMDToUpper< vector_register_type::E_BLOCK_COUNT >() );
    int SIMDSize = STU - SFL;
      

    vector_register_type regs[sizeof...(args)], c;
    vector_register_iterator_type iters[sizeof...(args)];
    ConstParallelDispatcher<T>::InitializeVectorIterators(std::size_t(SFL), std::size_t( SIMDSize ), iters, std::forward<Args>(args)...);
    

    if( SFL != SF) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters); 
      details::MatrixApplyClassMember<C, vector_register_type , void >::template Unroll< Args...>::Apply(regs, cls, fnc, c);

      int Q = vector_register_type::E_BLOCK_COUNT - (SF- int(range.from()));
      for(int i=0; i < vector_register_type::E_BLOCK_COUNT; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        if(Q <= i ) {
          this->pointer()[ SFL + i ] = value;
        }        
      }     
    }


    for (int i = SF; i < ST; i += vector_register_type::E_BLOCK_COUNT) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters);      
      details::MatrixApplyClassMember<C, vector_register_type , void >::template Unroll< Args...>::Apply(regs, cls, fnc, c);      
      c.Store(this->pointer() + i);
    }

    if( STU != ST) {
      details::UnrollNext< sizeof...(args), vector_register_type, vector_register_iterator_type>::Apply(regs, iters); 
      details::MatrixApplyClassMember<C, vector_register_type , void >::template Unroll< Args...>::Apply(regs, cls, fnc, c);

      int Q = (int(range.to()) -  ST - 1) ;      
      for(int i=0; i <= Q; ++i) {
        type value = first_element( c );
        c = shift_elements_right( c );
        this->pointer()[ ST + i ] = value;
      }     
    }
  }
  
  type *pointer() { return super_type::pointer(); }    
private:

    
};

    
}
}

