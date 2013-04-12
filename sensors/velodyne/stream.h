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
// 3. All advertising materials mentioning features or use of this software
//    must display the following acknowledgement:
//    This product includes software developed by the The University of Sydney.
// 4. Neither the name of the The University of Sydney nor the
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


#ifndef SNARK_SENSORS_VELODYNE_STREAM_H_
#define SNARK_SENSORS_VELODYNE_STREAM_H_

#include <stdlib.h>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <comma/math/compare.h>
#include <snark/sensors/velodyne/db.h>
#include <snark/sensors/velodyne/laser_return.h>
#include <snark/sensors/velodyne/impl/stream_traits.h>
#include <snark/sensors/velodyne/scan_tick.h>

namespace snark {  namespace velodyne {

/// velodyne point stream
template < typename S >
class stream : public boost::noncopyable
{
    public:
        /// constructor
        stream( S* stream, unsigned int rpm, bool outputInvalid = false, bool outputRaw = false );

        /// constructor
        stream( S* stream, bool outputInvalid = false, bool outputRaw = false );

        /// read point, return NULL, if end of stream
        laser_return* read();

        /// skip given number of scans including the current one
        /// @todo: the same for packets and points, once needed
        void skip_scan();

        /// return current scan number
        unsigned int scan() const;

        /// interrupt reading
        void close();

    private:
        boost::optional< double > m_angularSpeed;
        bool m_outputInvalid;
        bool m_outputRaw;
        boost::scoped_ptr< S > m_stream;
        boost::posix_time::ptime m_timestamp;
        const packet* m_packet;
        enum { m_size = 12 * 32 };
        struct index // quick and dirty
        {
            unsigned int idx;
            unsigned int block;
            unsigned int laser;
            index() : idx( 0 ), block( 0 ), laser( 0 ) {}
            const index& operator++()
            {
                ++idx;
                if( block & 0x1 )
                {
                    ++laser;
                    if( laser < 32 ) { --block; } else { laser = 0; ++block; }
                }
                else
                {
                    ++block;
                }
                return *this;
            }
            bool operator==( const index& rhs ) const { return idx == rhs.idx; }
        };
        index m_index;
        unsigned int m_scan;
        scan_tick m_tick;
        bool m_closed;
        laser_return m_laserReturn;
        double angularSpeed();
};

template < typename S >
inline stream< S >::stream( S* stream, unsigned int rpm, bool outputInvalid, bool outputRaw )
    : m_angularSpeed( ( 360 / 60 ) * rpm )
    , m_outputInvalid( outputInvalid )
    , m_outputRaw( outputRaw )
    , m_stream( stream )
    , m_scan( 0 )
    , m_closed( false )
{
    m_index.idx = m_size;
}

template < typename S >
inline stream< S >::stream( S* stream, bool outputInvalid, bool outputRaw )
    : m_outputInvalid( outputInvalid )
    , m_outputRaw( outputRaw )
    , m_stream( stream )
    , m_scan( 0 )
    , m_closed( false )
{
    m_index.idx = m_size;
}

template < typename S >
inline double stream< S >::angularSpeed()
{
    if( m_angularSpeed ) { return *m_angularSpeed; }
    double da = double( m_packet->blocks[0].rotation() - m_packet->blocks[11].rotation() ) / 100;
    double dt = double( ( impl::time_offset( 0, 0 ) - impl::time_offset( 11, 0 ) ).total_microseconds() ) / 1e6;
    return da / dt;
}

template < typename S >
inline laser_return* stream< S >::read()
{
    while( !m_closed )
    {
        if( m_index.idx >= m_size )
        {
            m_index = index();
            m_packet = reinterpret_cast< const packet* >( impl::stream_traits< S >::read( *m_stream, sizeof( packet ) ) );
            if( m_packet == NULL ) { return NULL; }
            if( m_tick.get( *m_packet ) )
            {
                m_scan++;
            }
            m_timestamp = impl::stream_traits< S >::timestamp( *m_stream );
        }
        // todo: scan number will be slightly different, depending on m_outputRaw value
        m_laserReturn = impl::getlaser_return( *m_packet, m_index.block, m_index.laser, m_timestamp, angularSpeed(), m_outputRaw );
        ++m_index;
        bool valid = !comma::math::equal( m_laserReturn.range, 0 );
        if( valid || m_outputInvalid ) { return &m_laserReturn; }
    }
    return NULL;
}

template < typename S >
inline unsigned int stream< S >::scan() const { return m_scan; }

template < typename S >
inline void stream< S >::close() { m_closed = true; impl::stream_traits< S >::close( *m_stream ); }

template < typename S >
inline void stream< S >::skip_scan()
{
    while( !m_closed )
    {
        m_index = index();
        m_packet = reinterpret_cast< const packet* >( impl::stream_traits< S >::read( *m_stream, sizeof( packet ) ) );
        if( m_packet == NULL ) { return; }
        if( m_tick.get( *m_packet ) )
        {
            m_scan++;
            return;
        }
    }
}

} } // namespace snark {  namespace velodyne {

#endif /*SNARK_SENSORS_VELODYNE_STREAM_H_*/
