/*
    openDCM, dimensional constraint manager
    Copyright (C) 2013  Stefan Troeger <stefantroeger@gmx.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DCM_EDGE_GENERATOR_H
#define DCM_EDGE_GENERATOR_H

#include "property_generator.hpp"
#include "object_generator.hpp"
#include "extractor.hpp"

#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>

using namespace boost::spirit::karma;
namespace karma = boost::spirit::karma;
namespace phx = boost::phoenix;

namespace dcm {
namespace details {
  
template<typename Sys>
struct edge_generator : grammar<Iterator, std::vector<fusion::vector3<typename Sys::Cluster::edge_bundle, GlobalVertex, GlobalVertex> >()> {
  
      edge_generator();

      rule<Iterator, std::vector<fusion::vector3<typename Sys::Cluster::edge_bundle, GlobalVertex, GlobalVertex> >()> edge_range;
      rule<Iterator, fusion::vector3<typename Sys::Cluster::edge_bundle, GlobalVertex, GlobalVertex>()> edge;
      rule<Iterator, std::vector<typename Sys::Cluster::edge_bundle_single>&()> globaledge_range;
      rule<Iterator, typename Sys::Cluster::edge_bundle_single()> globaledge;
      details::edge_prop_gen<Sys> edge_prop;
      details::obj_gen<Sys> objects;	
      Extractor<Sys> ex;
};

template<typename Sys>
struct vertex_generator : grammar<Iterator, std::vector<typename Sys::Cluster::vertex_bundle>()> {
  
      vertex_generator();

      rule<Iterator, std::vector<typename Sys::Cluster::vertex_bundle>()> vertex_range;
      rule<Iterator, typename Sys::Cluster::vertex_bundle()> vertex;
      details::vertex_prop_gen<Sys> vertex_prop;
      details::obj_gen<Sys> objects;
      Extractor<Sys> ex;
};

}//details
}//dcm

#ifndef USE_EXTERNAL
  #include "edge_vertex_generator_imp.hpp"
#endif

#endif