//
// Created by LYS on 3/29/2025.
//

#pragma once

#include <stdexcept>

#include <cpptrace/cpptrace.hpp>
#include <spdlog/spdlog.h>

#define THROW_WITH_STACK( MSG ) throw std::runtime_error( MSG + ( "\n" + cpptrace::generate_trace().to_string(  ) ) );
#define LOG_WITH_STACK( MSG )   spdlog::error( MSG + ( "\n" + cpptrace::generate_trace().to_string(  ) ) );

#define FILLER                                 static_assert( false )
#define EX_ASSERTIONS( x )                     x
#define MA_ASSERTIONS( _1, _2, _3, NAME, ... ) NAME

#define MAKE_SURE_COND_STR( COND, STR ) \
    if ( !( COND ) ) [[unlikely]]       \
    {                                   \
        THROW_WITH_STACK( STR )         \
    }
#define MAKE_SURE_COND( COND ) MAKE_SURE_COND_STR( COND, #COND )
#define MAKE_SURE( ... )       EX_ASSERTIONS( MA_ASSERTIONS( __VA_ARGS__, FILLER, MAKE_SURE_COND_STR, MAKE_SURE_COND )( __VA_ARGS__ ) )


#ifdef NDEBUG
#    define CHECK_COND( COND )
#else
#    define CHECK_COND_STR( COND, STR ) \
        if ( !( COND ) ) [[unlikely]]   \
        {                               \
            THROW_WITH_STACK( STR )     \
        }
#    define CHECK_COND( COND ) CHECK_COND_STR( COND, #COND )
#endif
#define CHECK( ... ) EX_ASSERTIONS( MA_ASSERTIONS( __VA_ARGS__, FILLER, CHECK_COND_STR, CHECK_COND )( __VA_ARGS__ ) )

#define NOT_NULL( VAL )                     \
    if ( VAL == nullptr ) [[unlikely]]      \
    {                                       \
        THROW_WITH_STACK( #VAL " is null" ) \
    }                                       \
    ( VAL )

#define VERIFY_COND_STR_ACTION( COND, STR, ACTION ) \
    if ( !( COND ) ) [[unlikely]]                   \
    {                                               \
        LOG_WITH_STACK( STR )                       \
        ACTION;                                     \
    }
#define VERIFY_COND_ACTION( COND, ACTION ) VERIFY_COND_STR_ACTION( COND, #COND, ACTION )
#define VERIFY_COND( COND )                VERIFY_COND_STR_ACTION( COND, #COND, ; )
#define VERIFY( ... )                      EX_ASSERTIONS( MA_ASSERTIONS( __VA_ARGS__, VERIFY_COND_STR_ACTION, VERIFY_COND_ACTION, VERIFY_COND )( __VA_ARGS__ ) )