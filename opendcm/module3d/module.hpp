/*
    openDCM, dimensional constraint manager
    Copyright (C) 2012  Stefan Troeger <stefantroeger@gmx.net>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this library; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DCM_MODULE_3D_H
#define DCM_MODULE_3D_H

#include "opendcm/core/object.hpp"
#include "opendcm/core/module.hpp"
#include "opendcm/core/utilities.hpp"
#include "opendcm/core/geometry.hpp"
#include "opendcm/core/constraint.hpp"
#include "opendcm/core/typeadaption.hpp"

#include <type_traits>

struct symbol;
namespace dcm {
    
template<typename T>
void pretty(const T& t) {
    std::cout<<__PRETTY_FUNCTION__<<std::endl;
};
    
template<typename ... types>
struct Module3D {

    typedef boost::mpl::int_<5> ID;

    template<typename Final, typename Stacked>
    struct type : public Stacked {

        /**
         * @brief Container for 3D user geometry
         * 
         * 
         */
        struct Geometry3D : public Stacked::ObjectBase, public utilities::Variant<types...> {

            typedef utilities::Variant<types...> InheritedV;
            typedef typename Stacked::ObjectBase InheritedO;
                       
            typedef symbolic::GeometryProperty GeometryProperty;
            typedef graph::VertexProperty      VertexProperty;
            DCM_OBJECT_ADD_PROPERTIES( Final, (GeometryProperty)(VertexProperty) )
            
        public:            
            Geometry3D(Final* system) 
                : Stacked::ObjectBase( Final::template objectTypeID<typename Final::Geometry3D>::ID::value ),
                m_system(system), m_type(-1) {};
            
            /**
             * @brief Set the content and stores the user type
             * 
             * This function is used to set the content of the Geometry3D container. It extract the contents of 
             * the given \a geometry and stores it in an accessible manner for the solver. Furhtermore it stores
             * the supplied user type for later access. The exact behaviour of this feature depends on the specified
             * qualifiers of the storable types as given as the Module3D template arguments. If the type was given
             * without any special qualifiers then a copy of the given user type is strored.
            */
            template<typename T>
            void set(const T& geometry) {
                
                typedef typename Final::Kernel  Kernel;
                typedef typename Kernel::Scalar Scalar;
            
                BOOST_MPL_ASSERT((mpl::contains<mpl::vector<types...>, T>));
                
                //store the type
                m_type = Final::template geometryID<typename geometry_traits<T>::type>::value;
                
                //hold the given type in our variant
                InheritedV::m_variant = geometry;
                
                //ensure correct property initialization
                symbolic::Geometry* sg = InheritedO::template getProperty<GeometryProperty>();
                if(sg) {delete sg;}                
                sg = new symbolic::TypeGeometry<Kernel, geometry_traits<T>::type::template type>;
                InheritedO::template setProperty<GeometryProperty>(sg);

                //store the value in internal data structure
                (typename geometry_traits<T>::modell()).template extract<Scalar,
                typename geometry_traits<T>::accessor >(geometry, 
                    static_cast<symbolic::TypeGeometry<Kernel,geometry_traits<T>::type::template type>*>(sg)->getPrimitveGeometry());
            
                //setup the tree
                typename Final::Graph* cluster = static_cast<typename Final::Graph*>(m_system->getGraph());
                fusion::vector<graph::LocalVertex, graph::GlobalVertex> res = cluster->addVertex();                     
                cluster->template setProperty<GeometryProperty> (fusion::at_c<0>(res), sg);
                InheritedO::template setProperty<VertexProperty>(fusion::at_c<1>(res));               
            };
            
            /**
             * @brief Check if the Geometry3D holds a geometry type
             * 
             * As a Geometry3D is only a container for user geometry it is only valid if a user type was set. 
             * To check if this already happend this function can be used. If it returns \a false then the 
             * \ref set function neets to be used to initialize the container before using it to setup a system.
             * \note When the geometry is created through the systems \ref addGeometry3D funcion it is always 
             * propperly initialized, false is only possible if the user creates the Geometry3D itself.
             * 
             * @return bool true if a valid geometry type was set
             */
            bool holdsGeometry() {
                return InheritedV::holdsType();
            };
            
            /**
             * @brief Check if a certain geometry type is stored
             * 
             * As a Geometry3D is only a container for user types it is often needed to query the type currently 
             * stored. This can be done with this function in two ways. First it is possible to query by the dcm
             * geometry type, for example dcm::Point3. Valid types are the one used in the geometry traits. The 
             * alternative is to directly provide the user types. Valid values are the ones used to construct the
             * Module3D. See the following example for the usage (given MyCustomPointType was regirstered before 
             * as dcm::Point3 via geometry_traits)
             * @code
             * boost::shared_ptr<Geometry3D> g(new Geometry3D(system));
             * g->set(MyCustomPointType())
             * assert(g->holdsGeometryType<dcm::Point3>())
             * assert(!g->holdsGeometryType<dcm::Line3>())
             * assert(g->holdsGeometryType<MyCustomPointType>())
             * assert(!g->holdsGeometryType<MyCustomLineType>())
             * @endcode
             * \note Unregisterd user types are not supported, the function cannot be compiled in such a 
             * situation
             */
            template<typename T>
            typename boost::enable_if<mpl::contains<mpl::vector<types...>, T>, bool>::type 
            holdsGeometryType() {
                return m_type == Final::template geometryID<typename geometry_traits<T>::type>::value;
            };
            template<typename T>
            typename boost::disable_if<mpl::contains<mpl::vector<types...>, T>, bool>::type 
            holdsGeometryType() {
                return m_type == Final::template geometryID<T>::value;
            };
           
            
        protected:
            Final* m_system;
            int    m_type;
        };
        
        struct Constraint3D : public Stacked::ObjectBase {

            typedef symbolic::ConstraintProperty ConstraintProperty;
            DCM_OBJECT_ADD_PROPERTIES( Final, (ConstraintProperty) )
            
            Constraint3D(Final* system) 
                : Stacked::ObjectBase( Final::template objectTypeID<typename Final::Geometry3D>::ID::value ),
                m_system(system), m_type(-1){
                    
            };    
            
            template<typename ...Constraints>
            set(const Constraints&... cons) {
                
                //we create one global edge for each constraint
                
            };
            
        protected:
            Final* m_system;
            int    m_type;
            
            
        };
        
        DCM_MODULE_ADD_OBJECTS(Stacked, (Geometry3D)(Constraint3D))
        
        template<typename T>
        std::shared_ptr<Geometry3D> addGeometry3D(const T& geom) {
            
            std::shared_ptr<Geometry3D> g(new Geometry3D(static_cast<Final*>(this)));
            g->set(geom);
            return g;
        };
    };
    
};

}//dcm

#endif //DCM_GEOMETRY3D_H







