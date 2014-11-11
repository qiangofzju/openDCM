/*
    openDCM, dimensional constraint manager
    Copyright (C) 2012-2014  Stefan Troeger <stefantroeger@gmx.net>

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

#ifndef DCM_GEOMETRY_3D_H
#define DCM_GEOMETRY_3D_H

#include <opendcm/core/geometry.hpp>
#include <opendcm/core/defines.hpp>

#include <functional>

#include <boost/fusion/include/at_c.hpp>
#include <boost/fusion/include/nview.hpp>

namespace dcm {
namespace geometry {

template<typename Kernel, bool MappedType = true>
struct Cluster3 : public Geometry<Kernel, MappedType,
        storage::Vector<3>, storage::Matrix<3,3>> {

    typedef typename Kernel::Scalar Scalar;
    using Geometry<Kernel, MappedType, storage::Vector<3>, storage::Matrix<3,3>>::m_storage;

    auto translation()->decltype(fusion::at_c<0>(m_storage)) {
        return fusion::at_c<0>(m_storage);
    };

    auto rotation()->decltype(fusion::at_c<1>(m_storage)) {
        return fusion::at_c<1>(m_storage);
    };
    
    details::Transform<Scalar, 3> transform() {
        return details::Transform<Scalar, 3>(Eigen::Quaternion<Scalar>(rotation()),
                                             Eigen::Translation<Scalar, 3>(translation()));
    };
    
    Eigen::Transform<Scalar, 3, Eigen::AffineCompact> eigenTransform() {
        return Eigen::Transform<Scalar, 3, Eigen::AffineCompact>(rotation())
                   * Eigen::Translation<Scalar, 3>(translation());
    };
};

} //geometry

namespace numeric {

#define NQFAKTOR 0.5    //the faktor by which the norm quaternion is multiplied with to get the RealScalar
//norm quaternion to generate the unit quaternion

template<typename Kernel, template<class, bool> class Base>
struct Cluster3Geometry : public DependendGeometry<Kernel, Base, geometry::Cluster3> {
    
    typedef typename Kernel::Scalar Scalar;
    typedef DependendGeometry<Kernel, Base, geometry::Cluster3> Inherited;
    
    void transformLocal(const Eigen::Transform<Scalar, 3, Eigen::AffineCompact>& t) {
        m_local.transform(t);
    };
    
    void recalculate() {

        dcm_assert(Inherited::m_base);
        Inherited::m_value = m_local.transformed(Inherited::m_base->eigenTransform());
        
        typename Inherited::DerivativeIterator it = Inherited::derivatives().begin();
        for(typename Inherited::DependendDerivative& d : Inherited::m_base->derivatives()) {

            dcm_assert(it != Inherited::derivatives().end());
            it->first = m_local.transformed(d.first.eigenTransform());        
            ++it;
        };
    };
        
protected:
    Base<Kernel, false> m_local; //the local value in the cluster (m_value is global)
};    
    
template< typename Kernel>
struct Cluster3 : public ParameterGeometry<Kernel, geometry::Cluster3,
        geometry::storage::Vector<3>, geometry::storage::Vector<3>> {

    typedef ParameterGeometry<Kernel, geometry::Cluster3,
            geometry::storage::Vector<3>, geometry::storage::Vector<3>> Inherited;

    typedef typename Kernel::Scalar Scalar;
    
    virtual void init(LinearSystem<Kernel>& sys) {

        //the normal parameter initialisation is actually good enough...
        Inherited::init(sys);

        //we just want to set the derivatives for the translational parameters already in the init
        //function as they stay constant, no need to rewrite them every time
        std::reverse_iterator<typename Inherited::DerivativeIterator> it = Inherited::derivatives().rbegin();

        for(int i=2; i>=0; --i) {
            it->first.translation()(i) = 1;
            ++it;
        }
    };

    void recalculate() {

        //calculate the quaternion and rotation matrix from the parameter vector
        const Eigen::Quaternion<Scalar> Q = calculateTransform();
        std::cout<<"transform: "<<m_transformation<<std::endl;
        Inherited::rotation() = Q.toRotationMatrix();
                
        const Eigen::Matrix<Scalar, 3, 1>& normQ = pRotation();

        // now calculate the gradient quaternions and calculate the diff rotation matrices
        // normQ = (a,b,c)
        // n = ||normQ||
        //
        // Q = (a/n sin(n), b/n sin(n), c/n sin(n), cos(n))
        //

        //n=||normQ||, sn = sin(n)/n, sn3 = sin(n)/n^3, cn = cos(n)/n, divn = 1/n;
        const Scalar n    = normQ.norm();
        const Scalar sn   = std::sin(NQFAKTOR*n)/n;
        const Scalar mul  = (NQFAKTOR*std::cos(NQFAKTOR*n)-sn)/std::pow(n,2);

        //dxa = dQx/da
        const Scalar dxa = sn + std::pow(normQ(0),2)*mul;
        const Scalar dxb = normQ(0)*normQ(1)*mul;
        const Scalar dxc = normQ(0)*normQ(2)*mul;

        const Scalar dya = normQ(1)*normQ(0)*mul;
        const Scalar dyb = sn + std::pow(normQ(1),2)*mul;
        const Scalar dyc = normQ(1)*normQ(2)*mul;

        const Scalar dza = normQ(2)*normQ(0)*mul;
        const Scalar dzb = normQ(2)*normQ(1)*mul;
        const Scalar dzc = sn + std::pow(normQ(2),2)*mul;

        const Scalar dwa = -sn*NQFAKTOR*normQ(0);
        const Scalar dwb = -sn*NQFAKTOR*normQ(1);
        const Scalar dwc = -sn*NQFAKTOR*normQ(2);

        dcm_assert(Inherited::derivatives().size()==6);
        typename Inherited::DerivativeIterator it = Inherited::derivatives().begin();

        //dQ/dx
        Eigen::Matrix<Scalar,3,3>& diffx = it->first.rotation();
        diffx(0,0) = -4.0*(Q.y()*dya+Q.z()*dza);
        diffx(0,1) = -2.0*(Q.w()*dza+dwa*Q.z())+2.0*(Q.x()*dya+dxa*Q.y());
        diffx(0,2) = 2.0*(dwa*Q.y()+Q.w()*dya)+2.0*(dxa*Q.z()+Q.x()*dza);
        diffx(1,0) = 2.0*(Q.w()*dza+dwa*Q.z())+2.0*(Q.x()*dya+dxa*Q.y());
        diffx(1,1) = -4.0*(Q.x()*dxa+Q.z()*dza);
        diffx(1,2) = -2.0*(dwa*Q.x()+Q.w()*dxa)+2.0*(dya*Q.z()+Q.y()*dza);
        diffx(2,0) = -2.0*(dwa*Q.y()+Q.w()*dya)+2.0*(dxa*Q.z()+Q.x()*dza);
        diffx(2,1) = 2.0*(dwa*Q.x()+Q.w()*dxa)+2.0*(dya*Q.z()+Q.y()*dza);
        diffx(2,2) = -4.0*(Q.x()*dxa+Q.y()*dya);

        //dQ/dy
        ++it;
        Eigen::Matrix<Scalar,3,3>& diffy = it->first.rotation();
        diffy(0,0) = -4.0*(Q.y()*dyb+Q.z()*dzb);
        diffy(0,1) = -2.0*(Q.w()*dzb+dwb*Q.z())+2.0*(Q.x()*dyb+dxb*Q.y());
        diffy(0,2) = 2.0*(dwb*Q.y()+Q.w()*dyb)+2.0*(dxb*Q.z()+Q.x()*dzb);
        diffy(1,0) = 2.0*(Q.w()*dzb+dwb*Q.z())+2.0*(Q.x()*dyb+dxb*Q.y());
        diffy(1,1) = -4.0*(Q.x()*dxb+Q.z()*dzb);
        diffy(1,2) = -2.0*(dwb*Q.x()+Q.w()*dxb)+2.0*(dyb*Q.z()+Q.y()*dzb);
        diffy(2,0) = -2.0*(dwb*Q.y()+Q.w()*dyb)+2.0*(dxb*Q.z()+Q.x()*dzb);
        diffy(2,1) = 2.0*(dwb*Q.x()+Q.w()*dxb)+2.0*(dyb*Q.z()+Q.y()*dzb);
        diffy(2,2) = -4.0*(Q.x()*dxb+Q.y()*dyb);

        //dQ/dz
        ++it;
        Eigen::Matrix<Scalar,3,3>& diffz = it->first.rotation();
        diffz(0,0) = -4.0*(Q.y()*dyc+Q.z()*dzc);
        diffz(0,1) = -2.0*(Q.w()*dzc+dwc*Q.z())+2.0*(Q.x()*dyc+dxc*Q.y());
        diffz(0,2) = 2.0*(dwc*Q.y()+Q.w()*dyc)+2.0*(dxc*Q.z()+Q.x()*dzc);
        diffz(1,0) = 2.0*(Q.w()*dzc+dwc*Q.z())+2.0*(Q.x()*dyc+dxc*Q.y());
        diffz(1,1) = -4.0*(Q.x()*dxc+Q.z()*dzc);
        diffz(1,2) = -2.0*(dwc*Q.x()+Q.w()*dxc)+2.0*(dyc*Q.z()+Q.y()*dzc);
        diffz(2,0) = -2.0*(dwc*Q.y()+Q.w()*dyc)+2.0*(dxc*Q.z()+Q.x()*dzc);
        diffz(2,1) = 2.0*(dwc*Q.x()+Q.w()*dxc)+2.0*(dyc*Q.z()+Q.y()*dzc);
        diffz(2,2) = -4.0*(Q.x()*dxc+Q.y()*dyc);

        //set the translation (this is one additional and not very elegant copy, but currently there
        //is no more elegant way witout braking the architecture)
        Inherited::translation() = pTranslation();
        //the translation differentials stay fixed, no need to write them every time...

        //recalculate all geometries
        for(auto& fct : m_recalculateables)
            fct();
    };

    template<template<class, bool> class Base>
    void addClusterGeometry(Cluster3Geometry<Kernel, Base>* g) {
        m_recalculateables.emplace_back(std::bind(&Cluster3Geometry<Kernel, Base>::recalculate, g));
        m_transformables.emplace_back(std::bind(&Cluster3Geometry<Kernel, Base>::transformLocal, g, 
                                                std::placeholders::_1));
    };
    
protected:

    using Inherited::m_parameterStorage;
    
    auto pTranslation() -> decltype(fusion::at_c<0>(m_parameterStorage)) {
        return fusion::at_c<1>(m_parameterStorage);
    };

    auto pRotation() -> decltype(fusion::at_c<1>(m_parameterStorage)) {
        return fusion::at_c<0>(m_parameterStorage);
    };

    typename Eigen::Quaternion<Scalar> calculateTransform() {

        Scalar norm = pRotation().norm();
        m_transformation.setIdentity();

        if(norm < 0.1) {
            if(norm == 0) {
                m_transformation *= typename details::Transform<Scalar, 3>::Translation(pTranslation());
                resetClusterRotation(m_transformation);
            }
            else {
                const Scalar fac = std::sin(NQFAKTOR*norm)/norm;
                m_transformation =  Eigen::Quaternion<Scalar>(std::cos(NQFAKTOR*norm), pRotation()(0)*fac,
                                    pRotation()(1)*fac,pRotation()(2)*fac);

                m_transformation *= Eigen::Translation<Scalar,3>(pTranslation());
                resetClusterRotation(m_transformation);
            }

            transformToMaps(m_transformation);
            Eigen::Quaternion<Scalar> q = m_transformation.rotation();
            return q;
        }

        const Scalar fac = std::sin(NQFAKTOR*norm)/norm;
        const Eigen::Quaternion<Scalar> Q(std::cos(NQFAKTOR*norm), pRotation()(0)*fac, pRotation()(1)*fac,
                                            pRotation()(2)*fac);
        m_transformation = Q;
        m_transformation *= typename details::Transform<Scalar, 3>::Translation(pTranslation());

        return Q;

    };

    void resetClusterRotation(details::Transform<Scalar, 3>& trans) {

#ifdef DCM_USE_LOGGING
        BOOST_LOG_SEV(log, information) << "Reset cluster rotation:"<<std::endl<<trans;
#endif

        trans = m_resetTransform.inverse()*trans;

        const auto& t = m_resetTransform.transformation();
        for(auto& fct : m_transformables)
            fct(t);
    };

    void transformToMaps(const details::Transform<Scalar, 3>& trans) {

        const Eigen::Quaternion<Scalar>& m_quaternion = trans.rotation();

        if(m_quaternion.w() < 1.) {
            Scalar s = std::acos(m_quaternion.w())/std::sin(std::acos(m_quaternion.w()));
            pRotation() = m_quaternion.vec()*s;
            pRotation() /= NQFAKTOR;
        }
        else {
            pRotation().setZero();
        }

        pTranslation() = trans.translation().vector();
    };


    details::Transform<Scalar, 3> m_transformation;
    details::Transform<Scalar, 3> m_resetTransform = details::Transform<Scalar, 3>(
            Eigen::Quaternion<Scalar>(Eigen::AngleAxisd(M_PI*2./3.,
            Eigen::Vector3d(1,1,1).normalized())));
    
    std::vector<std::function<void()>>                                  m_recalculateables;
    std::vector<std::function<void(const Eigen::Transform<Scalar,3, 
                                   Eigen::AffineCompact>&)>>            m_transformables;
};

} //numeric
} //dcm


#endif //GCM_GEOMETRY_3D_H
