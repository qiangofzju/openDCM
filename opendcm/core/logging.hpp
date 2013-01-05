/*
    openDCM, dimensional constraint manager
    Copyright (C) 2012  Stefan Troeger <stefantroeger@gmx.net>

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

#ifndef GCM_LOGGING_H
#define GCM_LOGGING_H

#ifdef USE_LOGGING

#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/attributes/attribute_def.hpp>
#include <boost/log/utility/init/to_file.hpp>
#include <boost/log/utility/init/common_attributes.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/formatters.hpp>
#include <boost/log/filters.hpp>

#include <boost/shared_ptr.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace fmt = boost::log::formatters;
namespace flt = boost::log::filters;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace dcm {

static int counter = 0;
typedef sinks::synchronous_sink< sinks::text_file_backend > sink_t;

inline boost::shared_ptr< sink_t > init_log() {

    //create the filename
    std::stringstream str;
    str<<"openDCM_"<<counter<<"_%N.log";
    counter++;

    boost::shared_ptr< logging::core > core = logging::core::get();

    boost::shared_ptr< sinks::text_file_backend > backend =
        boost::make_shared< sinks::text_file_backend >(
            keywords::file_name = str.str(),             //file name pattern
            keywords::rotation_size = 10 * 1024 * 1024 	//Rotation size is 10MB
        );

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    boost::shared_ptr< sink_t > sink(new sink_t(backend));
    
    sink->set_formatter(
        fmt::stream <<"[" << fmt::attr<std::string>("Tag") <<"] "
        << fmt::if_(flt::has_attr<std::string>("ID")) [
            fmt::stream << "["<< fmt::attr< std::string >("ID")<<"] "]
        << fmt::message()
    );

    core->add_sink(sink);
    return sink;
};

inline void stop_log(boost::shared_ptr< sink_t >& sink) {

    boost::shared_ptr< logging::core > core = logging::core::get();

    // Remove the sink from the core, so that no records are passed to it
    core->remove_sink(sink);
    sink.reset();
};

}; //dcm

#endif //USE_LOGGING

#endif //GCM_LOGGING_H
