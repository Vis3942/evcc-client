/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2010 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          psal_assert.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Interface for Platform Services Abstraction Layer
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef PSAL_ASSERT_H
#define PSAL_ASSERT_H



/* --------------------------------------------------------------------------------------------------
 *                                          Includes
 * --------------------------------------------------------------------------------------------------
 */
#include "psal.h"
#include "platform_test_config.h"
#if defined(STDLIB_BUILD)
#include <assert.h>
#endif


/* --------------------------------------------------------------------------------------------------
 *                                          Defines
 * --------------------------------------------------------------------------------------------------
 */
#define PSAL_ASSERT_HANDLER(func, file, line)   PSAL_HazardLights(func, file, line)


/* --------------------------------------------------------------------------------------------------
 *                                            Macros
 * --------------------------------------------------------------------------------------------------
 */

/*
 *  PSAL_ASSERT( x )            - Assert that the given expression is true.  If the expression
 *                                evaluates to false, the assert handler is called.
 *
 *  PSAL_ASSERT_FORCE()         - Forces call of the assert handler.
 *
 *  PSAL_ASSERT_COMPILE( x )    - Code that compiles if asserts are on.  Useful to remove variable
 *                                declarations not needed when assert not enabled.
 *
 *  PSAL_ASSERT_STATIC( x )     - Compile time version of assert.  If assert fails, a compile
 *                                time error of "negative array size" results.
 *                                Does not add to code size.  Can use "sizeof" in expression.
 *
 *  NOTE:  To disable PSAL_ASSERT and PSAL_ASSERT_COMPILE for release code, define NDEBUG in the project.
 *
 */
#if defined (NDEBUG)            //---------------- Start NDEBUG Section
  #define PSAL_ASSERT(expr)
  #define PSAL_ASSERT_COMPILE(x)
  #if defined (STDLIB_BUILD)      // Linuxy builds
    #define PSAL_ASSERT_FORCE()           exit(EXIT_FAILURE)
  #else                           // Non-Linuxy builds
    #define PSAL_ASSERT_FORCE()           exit(EXIT_FAILURE)
  #endif
#else                           //---------------- Start undef NDEBUG Section
  #if defined (PSAL_DEBUG) || !defined(STDLIB_BUILD)   // PSAL_DEBUG enabled or non-Linuxy builds
    #define PSAL_ASSERT(x)                do {  if( !( x ) ) PSAL_ASSERT_HANDLER(__func__,__FILE__,__LINE__);  }  while( 0 )
    #define PSAL_ASSERT_COMPILE(x)        x
    #define PSAL_ASSERT_FORCE()           PSAL_ASSERT_HANDLER(__func__,__FILE__,__LINE__)
  #else
    #define PSAL_ASSERT(x)                assert(x)
    #define PSAL_ASSERT_COMPILE(x)        x
    #define PSAL_ASSERT_FORCE()           assert(0)
  #endif
#endif

#define PSAL_ASSERT_STATIC(x)         void dummyPrototype( int dummyArray[( x )?1:-1] )

/* Implementation notes:
 *
 *   PSAL_ASSERT() uses a do-while(0) construct to guarantee an atomic C statement.
 *
 *   PSAL_ASSERT_STATIC() creates a dummy function prototype with a parameter that
 *                        has a negative size array if assert fails.
 */



/* --------------------------------------------------------------------------------------------------
 */
#endif  /* PSAL_ASSERT_H */
