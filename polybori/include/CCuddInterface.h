// -*- c++ -*-
//*****************************************************************************
/** @file CCuddInterface.h
 *
 * @author Alexander Dreyer
 * @date 2007-07-05
 *
 * This files defines a replacement for the decision diagram manager of CUDD's
 * C++ interface.
 *
 * @par Copyright:
 *   (c) 2007-2010 by The PolyBoRi Team
**/
//*****************************************************************************


// include basic definitions
#include "pbori_defs.h"

#include "cudd.h"
#include "cuddInt.h"

#include "pbori_func.h"         // handle_error
#include "CCallbackWrapper.h"

#include <vector>
#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_array.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "PBoRiError.h"

#include <stdexcept>
#include <algorithm>

#ifndef CCuddInterface_h_
#define CCuddInterface_h_

  // get cudd error texts
inline const char* error_text(DdManager* mgr) {
    switch (Cudd_ReadErrorCode(mgr)) {
    CUDD_MEMORY_OUT:
      return("Out of memory.");
    CUDD_TOO_MANY_NODES:
      return("To many nodes.");
    CUDD_MAX_MEM_EXCEEDED:
      return("Maximum memory exceeded.");
    CUDD_INVALID_ARG:
      return("Invalid argument.");
    CUDD_INTERNAL_ERROR:
      return("Internal error.");
    }
    return("Unexpected error.");
  }

/// Increment reference count
inline void 
intrusive_ptr_add_ref(DdManager* ptr){
  ++(ptr->hooks);
}

/// Release current pointer by decrementing reference counting
inline void 
intrusive_ptr_release(DdManager* ptr) {
  if (!(--(ptr->hooks))) {
    int retval = Cudd_CheckZeroRef(ptr);
    // Check for unexpected non-zero reference counts
    assert(retval == 0);

    Cudd_Quit(ptr);
  }
}


BEGIN_NAMESPACE_PBORI



/// @name Define templates for generating member functions from CUDD procedures
//@{
#define PB_CUDDMGR_READ(count, data, funcname) data funcname() const { \
  return BOOST_PP_CAT(Cudd_, funcname)(*this); }

#define PB_CUDDMGR_SWITCH(count, data, funcname) void funcname() { \
    BOOST_PP_CAT(Cudd_, funcname)(*this); }

#define PB_CUDDMGR_SET(count, data, funcname)  void funcname(data arg) { \
    BOOST_PP_CAT(Cudd_, funcname)(*this, arg); }
//@}

/** @class CCuddInterface
 * @brief This class defines a C++ interface to @c CUDD's decicion diagram
 * manager.
 *
 * The purpose of this wrapper is just to provide an efficient and save way of
 * handling the decision diagram management. It corrects some short-comings of
 * CUDD's built-in interface.
 *
 * @attention This class is intented for internal use only. 
 * Use the highlevel classes CDDManager<CCuddInterface> or BoolePolyRing
 * instead.
 **/

class CCuddInterface:
  public CTypes::auxtypes_type {

  /// Type of *this
  typedef CCuddInterface self;

public:
  /// Type of Cudd's decision diagrams
  typedef DdNode* node_ptr;

  /// Type of Cudd decision diagram manager
  typedef DdManager mgr_type;
  typedef int cudd_idx_type;

  typedef node_ptr (*unary_int_function)(mgr_type*, cudd_idx_type);
  typedef node_ptr (*void_function)(mgr_type*);


  /// Smart pointer to Cudd manager
  typedef boost::intrusive_ptr<mgr_type> mgr_ptr;

  /// Initialize CUDD-like decision diagram manager
  CCuddInterface(size_type numVars, size_type numVarsZ, 
                 size_type numSlots = PBORI_UNIQUE_SLOTS,
                 size_type cacheSize = PBORI_CACHE_SLOTS, 
                 unsigned long maxMemory = PBORI_MAX_MEMORY):
    p_mgr(init(numVars, numVarsZ, numSlots, cacheSize, maxMemory)),
    m_vars(numVarsZ) {
    for (idx_type idx = 0; idx < numVarsZ; ++idx) initVar(m_vars[idx], idx);
  }

  /// Copy constructor
  CCuddInterface(const self& rhs): p_mgr(rhs.p_mgr), m_vars(rhs.m_vars) {
    std::for_each(m_vars.begin(), m_vars.end(), Cudd_Ref);
  }

  /// Destructor
  ~CCuddInterface() {
    std::for_each(m_vars.begin(), m_vars.end(),
                  callBack(&self::recursiveDeref));
  }

  /// Get pure CUDD structure
  mgr_type* getManager() const { return p_mgr.operator->(); }

  /// Get (shared) pointer to initialized manager
  mgr_ptr pManager() const { return p_mgr; }

  /// Print statistical information
  void info() const { checkedResult(Cudd_PrintInfo(*this,stdout)); }

  /// Assignment operation 
  self& operator=(const self & right) {
    p_mgr = right.p_mgr;
    return *this;
  }

  /// Get ZDD variable 
  node_ptr zddVar(idx_type idx) const { return apply(Cudd_zddIthVar, idx); }

  /// Get 1-terminal for ZDDs
  node_ptr zddOne(idx_type iMax) const  { return apply(Cudd_ReadZddOne, iMax); }

  /// Get 0-terminal for ZDDs
  node_ptr zddZero() const { return apply(Cudd_ReadZero); }

  /// Get 1-terminal for ZDDs
  node_ptr zddOne() const {  
    return checkedResult(DD_ONE(getManager()));
  }

  /// @name Member functions mimicking/interfacing with CUDD procedures 
  /// @note See preprocessor generated members below
  //@{
  int ReorderingStatusZdd(Cudd_ReorderingType * method) const {
    return Cudd_ReorderingStatusZdd(*this, method);
  }

  /// @note unused (do not use permutations if the variables)
  idx_type ReadPermZdd(idx_type idx) const { 
    return Cudd_ReadPermZdd(*this, idx); 
  }

  /// @note unused (do not use permutations if the variables)
  idx_type ReadInvPermZdd(idx_type idx) const { 
    return Cudd_ReadInvPermZdd(*this, idx);
  }

  void AddHook(DD_HFP f, Cudd_HookType where) { 
    checkedResult(Cudd_AddHook(*this, f, where));
  }
  void RemoveHook(DD_HFP f, Cudd_HookType where) { 
    checkedResult(Cudd_RemoveHook(*this, f, where)); 
  }
  int IsInHook(DD_HFP f, Cudd_HookType where) const { 
    return Cudd_IsInHook(*this, f, where); 
  }
  void EnableReorderingReporting() { 
    checkedResult(Cudd_EnableReorderingReporting(*this)); 
  }
  void DisableReorderingReporting() { 
    checkedResult(Cudd_DisableReorderingReporting(*this)); 
  }

  void DebugCheck(){ checkedResult(Cudd_DebugCheck(*this)); }
  void CheckKeys(){ checkedResult(Cudd_CheckKeys(*this)); }
  void PrintLinear() { checkedResult(Cudd_PrintLinear(*this)); }

  int ReadLinear(int x, int y) { return Cudd_ReadLinear(*this, x, y); }

  size_type Prime(size_type pr) const { return Cudd_Prime(pr); }

  void PrintVersion(FILE * fp) const { std::cout.flush(); Cudd_PrintVersion(fp); }

  MtrNode* MakeZddTreeNode(size_type low, size_type size, size_type type) {
    return Cudd_MakeZddTreeNode(*this, low, size, type);
  }
  void zddPrintSubtable() const{ 
    std::cout.flush();
    Cudd_zddPrintSubtable(*this);
  }

  void zddReduceHeap(Cudd_ReorderingType heuristic, int minsize) {
    checkedResult(Cudd_zddReduceHeap(*this, heuristic, minsize));
  }
  void zddShuffleHeap(int * permutation) { 
    checkedResult(Cudd_zddShuffleHeap(*this, permutation));
  }
  void zddSymmProfile(int lower, int upper) const {
    Cudd_zddSymmProfile(*this, lower, upper);
  }

  /// @note Preprocessor generated members
  /// @code
  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_SET, size_type, 
    (SetMinHit)(SetLooseUpTo)(SetMaxCacheHard)(SetMaxLive) )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_SET, int, 
    (SetSiftMaxVar)(SetSiftMaxSwap)(SetRecomb)(SetSymmviolation)
    (SetArcviolation)(SetPopulationSize)(SetNumberXovers)
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_SET, FILE*, (SetStdout)(SetStderr))

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_SWITCH, BOOST_PP_NIL, 
    (zddRealignEnable)(zddRealignDisable)
    (AutodynDisableZdd)(FreeZddTree)
    (EnableGarbageCollection)(DisableGarbageCollection)
    (TurnOnCountDead)(TurnOffCountDead)(ClearErrorCode)  
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, double,
    (ReadCacheUsedSlots)(ReadCacheLookUps)(ReadCacheHits) 
    (ReadSwapSteps)(ReadMaxGrowth)(AverageDistance)
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, size_type,
    (ReadCacheSlots)(ReadMinHit)(ReadLooseUpTo)(ReadMaxCache)
    (ReadMaxCacheHard)(ReadSlots)(ReadKeys)(ReadDead)(ReadMinDead)
    (ReadNextReordering)(ReadMaxLive)
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, int,
    (zddRealignmentEnabled)(ReadZddSize)(ReadReorderings)(ReadSiftMaxVar)
    (ReadSiftMaxSwap)(ReadGarbageCollections)(GarbageCollectionEnabled)
    (DeadAreCounted)(ReadRecomb)
    (ReadPopulationSize)(ReadSymmviolation)(ReadArcviolation)
    (ReadNumberXovers)(ReorderingReporting)(ReadErrorCode)
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, long,
    (ReadReorderingTime)(ReadGarbageCollectionTime)
    (ReadPeakNodeCount)(zddReadNodeCount)
  )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, large_size_type, 
    (ReadMemoryInUse)(ReadMaxMemory) )

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, FILE*, (ReadStdout)(ReadStderr))

  BOOST_PP_SEQ_FOR_EACH(PB_CUDDMGR_READ, MtrNode*, (ReadZddTree))

  PB_CUDDMGR_SET(BOOST_PP_NIL, Cudd_ReorderingType, AutodynEnableZdd)
  PB_CUDDMGR_SET(BOOST_PP_NIL, unsigned long, SetMaxMemory)
  PB_CUDDMGR_SET(BOOST_PP_NIL, double, SetMaxGrowth)
  PB_CUDDMGR_SET(BOOST_PP_NIL, MtrNode*, SetZddTree)
  /** @endcode */
  /// 
  //@}


  node_ptr getVar(idx_type idx) const {
    if UNLIKELY(idx >= m_vars.size())
      throw PBoRiError(CTypes::out_of_bounds);
    return  m_vars[idx];
  }

  /// Get number of managed variables
  size_type nVariables() const { return (size_type)ReadZddSize(); }

  /// clear all temporarily stored data
  void cacheFlush() {  cuddCacheFlush(*this); }

protected:

  /// initialized CUDD decision diagrma manager, check it and start reference counting
  mgr_ptr init(size_type numVars,size_type numVarsZ, size_type numSlots,
                 size_type cacheSize, large_size_type maxMemory) {

    DdManager* ptr = Cudd_Init(numVars, numVarsZ, numSlots, cacheSize, maxMemory);
    if UNLIKELY(ptr==NULL)
      throw PBoRiError(CTypes::failed);
    
    ptr->hooks = NULL;          // abusing hooks pointer for reference counting

    return ptr;
  }
  /// Generate check result of previous node operation and convert 
  node_ptr checkedResult(node_ptr result) const  { 
    checkedResult(int(result != NULL));
    return result;
  }

  /// Generate check numerical result of previous operation
  void checkedResult(int result) const  {
    if UNLIKELY(result == 0) {
      throw std::runtime_error(error_text(*this));
    } 
  }

  /// Apply function to given index
  node_ptr apply(unary_int_function func, idx_type idx) const  { 
    return checkedResult(func(*this, idx) );
  }

  /// Call function 
  node_ptr apply(void_function func) const { 
    return checkedResult(func(*this) );
  }

protected:
  /// Dereferencing of diagram node
  void recursiveDeref(node_ptr node) const { 
    Cudd_RecursiveDerefZdd(*this, node);
  }

  /// Generate raw variable
  void initVar(node_ptr& node, idx_type idx) const {
    Cudd_Ref(node = cuddUniqueInterZdd(*this, 
                                       idx, zddOne(),  zddZero()));
  }

  /// Wrapping memeber function as functional
  template <class MemberFuncPtr>
  CCallbackWrapper<MemberFuncPtr>
  callBack(MemberFuncPtr ptr) {
    return CCallbackWrapper<MemberFuncPtr>(*this, ptr);
  }

private:
  /// Implicit cast to pure CUDD structure (only accesible here)
  operator mgr_type*() const { return getManager(); }

  /// Smart pointer to Cudd maanger
  mgr_ptr p_mgr;  

  /// Variable cache
  std::vector<node_ptr> m_vars;
}; // CCuddInterface


#undef PB_CUDDMGR_READ
#undef PB_CUDDMGR_SWITCH
#undef PB_CUDDMGR_SET

END_NAMESPACE_PBORI

#endif


