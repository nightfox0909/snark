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

/// @authors abdallah kassir, vsevolod vlaskine

#include <iostream>
#include <string>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>
#include <Eigen/Dense>
#include <comma/application/command_line_options.h>
#include <comma/application/signal_flag.h>
#include <comma/csv/stream.h>
#include <comma/csv/traits.h>
#include <comma/io/select.h>
#include <comma/io/stream.h>
#include <comma/name_value/parser.h>
#include "../../math/roll_pitch_yaw.h"
#include "../../math/rotation_matrix.h"
#include "../../math/geometry/polygon.h"
#include "../../math/geometry/polytope.h"
#include "../../math/applications/frame.h"
#include "../../visiting/traits.h"

struct bounds_t
{
    bounds_t(): front(0), back(0), right(0), left(0), top(0), bottom(0) {}
    double front;
    double back;
    double right;
    double left;
    double top;
    double bottom;
};

struct point
{
    point(): coordinates( Eigen::Vector3d::Zero() ), block(0), id(0), flag(1){}
    boost::posix_time::ptime timestamp;
    Eigen::Vector3d coordinates;
    comma::uint32 block;
    comma::uint32 id;
    comma::uint32 flag;
};

struct position
{
    Eigen::Vector3d coordinates;
    snark::roll_pitch_yaw orientation;
    
    position() : coordinates( Eigen::Vector3d::Zero() ) {}
};

struct filter_input : public Eigen::Vector3d
{
    filter_input() : Eigen::Vector3d( Eigen::Vector3d::Zero() ) {}
    ::position filter; // quick and dirty
};

struct filter_output
{
    filter_output( bool included = false ) : included( included ) {}
    bool included;
};

namespace comma { namespace visiting {
    
template <> struct traits< ::position >
{
    template < typename K, typename V > static void visit( const K& k, ::position& t, V& v )
    {
        v.apply( "coordinates", t.coordinates );
        v.apply( "orientation", t.orientation );
    }
    
    template < typename K, typename V > static void visit( const K& k, const ::position& t, V& v )
    {
        v.apply( "coordinates", t.coordinates );
        v.apply( "orientation", t.orientation );
    }
};
    
template <> struct traits< filter_input >
{
    template < typename K, typename V > static void visit( const K& k, filter_input& t, V& v )
    {
        traits< Eigen::Vector3d >::visit( k, t, v );
        v.apply( "filter", t.filter );
    }
    
    template < typename K, typename V > static void visit( const K& k, const filter_input& t, V& v )
    {
        traits< Eigen::Vector3d >::visit( k, t, v );
        v.apply( "filter", t.filter );
    }
};

template <> struct traits< filter_output >
{ 
    template < typename K, typename V > static void visit( const K& k, const filter_output& t, V& v )
    {
        v.apply( "included", t.included );
    }
};

} } // namespace comma { namespace visiting { 

static void usage( bool verbose = false )
{
    std::cerr << std::endl;
    std::cerr << "filter points from dynamic objects represented by a position and orientation stream" << std::endl;
    std::cerr << "input: point-cloud, bounding stream" << std::endl;
    std::cerr << "       bounding data may either be joined to each point or provided through a separate stream in which points-grep will time-join the two streams" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat points.csv | points-grep <what> [<options>]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<what>" << std::endl;
    std::cerr << "     polytope: todo" << std::endl;
    std::cerr << "     box: todo" << std::endl;
    std::cerr << std::endl;
    exit( 0 );
}

snark::geometry::convex_polytope transform( const snark::geometry::convex_polytope& polytope, const ::position& position ) { return polytope.transformed( position.coordinates, position.orientation ); } 

template < typename Shape > int run( const Shape& shape, const comma::command_line_options& options )
{
    comma::csv::options csv( options );
    csv.full_xpath = true;
    if( csv.fields.empty() ) { csv.fields = "x,y,z"; }
    std::vector< std::string > fields = comma::split( csv.fields, ',' );
    for( unsigned int i = 0; i < fields.size(); ++i )
    {
        if( fields[i] == "filter/x" ) { fields[i] = "filter/coordinates/x"; }
        else if( fields[i] == "filter/y" ) { fields[i] = "filter/coordinates/y"; }
        else if( fields[i] == "filter/z" ) { fields[i] = "filter/coordinates/z"; }
        else if( fields[i] == "filter/roll" ) { fields[i] = "filter/orientation/roll"; }
        else if( fields[i] == "filter/pitch" ) { fields[i] = "filter/orientation/pitch"; }
        else if( fields[i] == "filter/yaw" ) { fields[i] = "filter/orientation/yaw"; }
    }
    csv.fields = comma::join( fields, ',' );
    bool output_all = options.exists( "--output-all" );
    filter_input default_input;
    default_input.filter = comma::csv::ascii< ::position >().get( options.value< std::string >( "--position", "0,0,0,0,0,0" ) );
    boost::optional< Shape > transformed;
    if( !csv.has_field( "filter,filter/coordinates,filter/coordinates/x,filter/coordinates/y,filter/coordinates/z,filter/orientation,filter/orientation/roll,filter/orientation/pitch,filter/orientation/yaw" ) ) { transformed = transform( shape, default_input.filter ); }
    comma::csv::input_stream< filter_input > istream( std::cin, csv, default_input );
    comma::csv::output_stream< filter_output > ostream( std::cout, csv.binary() );
    comma::csv::passed< filter_input > passed( istream, std::cout );
    comma::csv::tied< filter_input, filter_output > tied( istream, ostream );
    while( istream.ready() || ( std::cin.good() && !std::cin.eof() ) )
    {
        const filter_input* p = istream.read();
        if( !p ) { break; }
        bool keep = transformed ? transformed->has( *p ) : transform( shape, p->filter ).has( *p );
        if( output_all ) { tied.append( filter_output( keep ) ); }
        else if( keep ) { passed.write(); }
        if( csv.flush ) { std::cout.flush(); }
    }
    return 0;
}

int main( int argc, char** argv )
{
    try
    {
        comma::command_line_options options( argc, argv, usage );
        const std::vector< std::string >& unnamed = options.unnamed("--output-all,--verbose,-v,--flush","-.*");
        std::string what = unnamed[0];
        Eigen::Vector3d inflate_by = comma::csv::ascii< Eigen::Vector3d >().get( options.value< std::string >( "--offset,--inflate-by", "0,0,0" ) );
        if( what == "polytope" || what == "convex-polytope" )
        {
            std::cerr << "points-grep: polytope: todo" << std::endl; return 1;
//             const std::string& normals = options.value< std::string >( "--normals", "" );
//             const std::string& planes = options.value< std::string >( "--planes", "" );
//             if( !normals.empty() )
//             {
//                 // todo
//             }
//             else if( !planes.empty() )
//             {
//                 comma::csv::options shape_csv = comma::name_value::parser().get< comma::csv::options >( planes );
//                 comma::io::istream is( &shape_csv.filename[0], shape_csv.binary() ? comma::io::mode::binary : comma::io::mode::ascii );
//                 comma::csv::input_stream< snark::triangle > istream( is(), shape_csv );
//                 // todo
//             }
//             else
//             {
//                 std::cerr << "points-grep: polytope: please specify --planes or --normals" << std::endl;
//                 return 1;
//             }
            
            // todo
            
            //snark::geometry::convex_polytope polytope;
            //return run( polytope, options );
            return 1;
        }
        else if( what == "box" )
        {
            boost::optional< Eigen::Vector3d > origin;
            boost::optional< Eigen::Vector3d > end;
            boost::optional< Eigen::Vector3d > size;
            Eigen::Vector3d centre = comma::csv::ascii< Eigen::Vector3d >().get( options.value< std::string >( "--center,--centre", "0,0,0" ) );
            if( options.exists( "--origin,--begin" ) ) { origin = comma::csv::ascii< Eigen::Vector3d >().get( options.value< std::string >( "--origin,--begin" ) ); }
            if( options.exists( "--end" ) ) { end = comma::csv::ascii< Eigen::Vector3d >().get( options.value< std::string >( "--end" ) ); }
            if( options.exists( "--size" ) ) { size = comma::csv::ascii< Eigen::Vector3d >().get( options.value< std::string >( "--size" ) ); }
            if( !origin )
            {
                if( end && size ) { origin = *end - *size; }
                else if( size ) { origin = centre - *size / 2; }
                else { std::cerr << "points-grep: box: please specify --origin or --size" << std::endl; return 1; }
            }
            if( !end )
            {
                if( origin && size ) { end = *origin + *size; }
                else if( size ) { end = centre + *size / 2; }
                else { std::cerr << "points-grep: box: please specify --end or --size" << std::endl; return 1; }
            }
            centre = ( *origin + *end ) / 2 ;
            Eigen::Vector3d radius = ( *end - *origin ) / 2 + inflate_by;
            Eigen::MatrixXd normals( 6, 3 );
            normals <<  0,  0,  1,
                        0,  0, -1,
                        0,  1,  0,
                        0, -1,  0,
                        1,  0,  0,
                       -1,  0,  0;
            Eigen::VectorXd distances( 6 );
            distances << radius.x(), radius.x(), radius.y(), radius.y(), radius.z(), radius.z();
            return run( snark::geometry::convex_polytope( normals, distances ), options );
        }
        std::cerr << "points-grep: expected filter name, got: \"" << what << "\"" << std::endl;
        return 1;
    }
    catch( std::exception& ex ) { std::cerr << "points-grep: " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << "points-grep: unknown exception" << std::endl; }
    return 1;
}
