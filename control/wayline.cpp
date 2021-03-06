// This file is part of snark, a generic and flexible library for robotics research
// Copyright (c) 2011 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cmath>
#include <iostream>
#include <boost/static_assert.hpp>
#include <comma/base/exception.h>
#include "wayline.h"

namespace snark { namespace control {

wayline_t::wayline_t() {}
wayline_t::wayline_t( const vector_t& start, const vector_t& end ) :
      v( normalise( end - start ) )
    , line( Eigen::ParametrizedLine< double, dimensions >::Through( start, end ) )
    , perpendicular_line_at_end( v, end )
    , heading( atan2( v.y(), v.x() ) )
    {
        BOOST_STATIC_ASSERT( dimensions == 2 );
    }

bool wayline_t::is_past_endpoint( const vector_t& location ) const
{
    return perpendicular_line_at_end.signedDistance( location ) > 0;
}

double wayline_t::cross_track_error( const vector_t& location ) const
{
    return -line.signedDistance( location );
}

double wayline_t::heading_error( double yaw, double heading_offset ) const
{
    return wrap_angle( heading + heading_offset - yaw );
}

} } // namespace snark { namespace control {
